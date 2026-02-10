/*
 * sb_linux.c - Linux sandbox implementation using Landlock
 *
 * Uses Landlock LSM (Linux 5.13+) to restrict filesystem access,
 * applied in a forked child via prctl(PR_SET_NO_NEW_PRIVS) +
 * landlock_restrict_self before exec.
 */

#ifdef __linux__

#define SP_IMPLEMENTATION
#include "sp.h"

#include "stevelock.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/landlock.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

/* --- landlock syscall wrappers ------------------------------------------ */

static inline int landlock_create_ruleset(
    const struct landlock_ruleset_attr *attr, size_t size, __u32 flags)
{
    return (int)syscall(__NR_landlock_create_ruleset, attr, size, flags);
}

static inline int landlock_add_rule(
    int ruleset_fd, enum landlock_rule_type type, const void *attr,
    __u32 flags)
{
    return (int)syscall(__NR_landlock_add_rule, ruleset_fd, type, attr, flags);
}

static inline int landlock_restrict_self(int ruleset_fd, __u32 flags)
{
    return (int)syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}

/* --- ABI negotiation ---------------------------------------------------- */

/* All FS access rights up through ABI v5 (IOCTL_DEV). */
#define ACCESS_FS_ROUGHLY_READ ( \
    LANDLOCK_ACCESS_FS_EXECUTE | \
    LANDLOCK_ACCESS_FS_READ_FILE | \
    LANDLOCK_ACCESS_FS_READ_DIR)

#define ACCESS_FS_ROUGHLY_WRITE ( \
    LANDLOCK_ACCESS_FS_WRITE_FILE | \
    LANDLOCK_ACCESS_FS_REMOVE_DIR | \
    LANDLOCK_ACCESS_FS_REMOVE_FILE | \
    LANDLOCK_ACCESS_FS_MAKE_CHAR | \
    LANDLOCK_ACCESS_FS_MAKE_DIR | \
    LANDLOCK_ACCESS_FS_MAKE_REG | \
    LANDLOCK_ACCESS_FS_MAKE_SOCK | \
    LANDLOCK_ACCESS_FS_MAKE_FIFO | \
    LANDLOCK_ACCESS_FS_MAKE_BLOCK | \
    LANDLOCK_ACCESS_FS_MAKE_SYM | \
    LANDLOCK_ACCESS_FS_REFER | \
    LANDLOCK_ACCESS_FS_TRUNCATE)

#define ACCESS_FS_ALL (ACCESS_FS_ROUGHLY_READ | ACCESS_FS_ROUGHLY_WRITE)

/*
 * Query the running kernel's Landlock ABI version and return
 * the appropriate handled_access_fs mask. Older kernels don't
 * support newer flags, so we strip them.
 */
static __u64 get_fs_mask(void)
{
    int abi = landlock_create_ruleset(NULL, 0,
                                     LANDLOCK_CREATE_RULESET_VERSION);
    if (abi < 0)
        return 0;

    __u64 mask = ACCESS_FS_ALL;

    /* ABI < 2: no REFER */
    if (abi < 2)
        mask &= ~LANDLOCK_ACCESS_FS_REFER;
    /* ABI < 3: no TRUNCATE */
    if (abi < 3)
        mask &= ~LANDLOCK_ACCESS_FS_TRUNCATE;

    return mask;
}

/* --- sandbox struct ----------------------------------------------------- */

struct sb {
    char **writable;       /* writable directories (copies) */
    int    writable_count;
    char **readable;       /* additional readable paths (copies) */
    int    readable_count;
    int    allow_net;
    pid_t  pid;
    int    stdin_fd;
    int    stdout_fd;
    int    stderr_fd;
    int    exited;
    int    destroyed;
    int    exit_code;
    char   errbuf[256];
};

/* --- helpers ------------------------------------------------------------ */

/*
 * Open a path with O_PATH and add a LANDLOCK_RULE_PATH_BENEATH rule
 * granting `access` under that path. Returns 0 on success, -1 on error.
 */
static int add_path_rule(int ruleset_fd, const char *path, __u64 access,
                         char *errbuf, size_t errbuf_sz)
{
    int fd = open(path, O_PATH | O_CLOEXEC);
    if (fd < 0) {
        snprintf(errbuf, errbuf_sz, "open(%s, O_PATH): %s",
                 path, strerror(errno));
        return -1;
    }

    struct landlock_path_beneath_attr pb = {
        .allowed_access = access,
        .parent_fd = fd,
    };

    int ret = landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH,
                                &pb, 0);
    int saved = errno;
    close(fd);

    if (ret < 0) {
        snprintf(errbuf, errbuf_sz, "landlock_add_rule(%s): %s",
                 path, strerror(saved));
        return -1;
    }
    return 0;
}

/*
 * Build and return a landlock ruleset fd configured per `sb`.
 * Returns fd >= 0 on success, -1 on error (with errbuf set).
 */
static int build_ruleset(const struct sb *sb)
{
    __u64 fs_mask = get_fs_mask();
    if (!fs_mask) {
        snprintf((char *)sb->errbuf, sizeof(sb->errbuf),
                 "landlock not supported by this kernel");
        return -1;
    }

    struct landlock_ruleset_attr attr = {
        .handled_access_fs = fs_mask,
        .handled_access_net = 0,
    };

    /* If network is denied, handle TCP bind+connect so they're blocked
     * unless we add explicit allow rules (which we won't). */
    int abi = landlock_create_ruleset(NULL, 0,
                                     LANDLOCK_CREATE_RULESET_VERSION);
    if (!sb->allow_net && abi >= 4) {
        attr.handled_access_net =
            LANDLOCK_ACCESS_NET_BIND_TCP |
            LANDLOCK_ACCESS_NET_CONNECT_TCP;
    }

    int ruleset_fd = landlock_create_ruleset(&attr, sizeof(attr), 0);
    if (ruleset_fd < 0) {
        snprintf((char *)sb->errbuf, sizeof(sb->errbuf),
                 "landlock_create_ruleset: %s", strerror(errno));
        return -1;
    }

    /* Allow read+execute on / (the whole filesystem) */
    __u64 read_access = ACCESS_FS_ROUGHLY_READ & fs_mask;
    if (add_path_rule(ruleset_fd, "/", read_access,
                      (char *)sb->errbuf, sizeof(sb->errbuf)) < 0) {
        close(ruleset_fd);
        return -1;
    }

    /* Allow full access (read+write) to each writable directory */
    __u64 full_access = fs_mask;
    for (int i = 0; i < sb->writable_count; i++) {
        if (add_path_rule(ruleset_fd, sb->writable[i], full_access,
                          (char *)sb->errbuf, sizeof(sb->errbuf)) < 0) {
            close(ruleset_fd);
            return -1;
        }
    }

    /* Allow writes to /dev (for /dev/null, /dev/tty, etc.) */
    if (add_path_rule(ruleset_fd, "/dev", full_access,
                      (char *)sb->errbuf, sizeof(sb->errbuf)) < 0) {
        close(ruleset_fd);
        return -1;
    }

    /* Additional readable paths */
    for (int i = 0; i < sb->readable_count; i++) {
        if (add_path_rule(ruleset_fd, sb->readable[i], read_access,
                          (char *)sb->errbuf, sizeof(sb->errbuf)) < 0) {
            close(ruleset_fd);
            return -1;
        }
    }

    /* If network allowed, add rules for all TCP ports */
    if (sb->allow_net && abi >= 4) {
        /* Not handling net means net is unrestricted - already the case
         * since we didn't set handled_access_net. Nothing to do. */
    }

    return ruleset_fd;
}

/* --- public API --------------------------------------------------------- */

sb_t *sb_create(const sb_opts_t *opts)
{
    if (!opts || !opts->writable || opts->writable_count < 1) return NULL;

    /* Check landlock support early */
    int abi = landlock_create_ruleset(NULL, 0,
                                     LANDLOCK_CREATE_RULESET_VERSION);
    if (abi < 0) return NULL;

    sb_t *sb = calloc(1, sizeof(sb_t));
    if (!sb) return NULL;

    sb->pid = -1;
    sb->stdin_fd = -1;
    sb->stdout_fd = -1;
    sb->stderr_fd = -1;

    sb->writable = calloc(opts->writable_count, sizeof(char *));
    if (!sb->writable) { free(sb); return NULL; }
    sb->writable_count = opts->writable_count;
    for (int i = 0; i < opts->writable_count; i++) {
        sb->writable[i] = strdup(opts->writable[i]);
    }

    sb->allow_net = opts->allow_net;

    if (opts->readable_count > 0 && opts->readable) {
        sb->readable = calloc(opts->readable_count, sizeof(char *));
        if (!sb->readable) { free(sb); return NULL; }
        sb->readable_count = opts->readable_count;
        for (int i = 0; i < opts->readable_count; i++) {
            sb->readable[i] = strdup(opts->readable[i]);
        }
    }

    return sb;
}

int sb_spawn(sb_t *sb, const char *cmd, const char *const *argv,
             const char *const *env)
{
    if (!sb || !cmd) return -1;
    if (sb->pid != -1) {
        snprintf(sb->errbuf, sizeof(sb->errbuf), "already spawned");
        return -1;
    }

    /* Build the ruleset fd in the parent so we can report errors. */
    int ruleset_fd = build_ruleset(sb);
    if (ruleset_fd < 0) return -1;

    /* pipes: parent -> child stdin, child stdout -> parent, child stderr -> parent */
    int pin[2], pout[2], perr[2];
    if (pipe(pin) || pipe(pout) || pipe(perr)) {
        snprintf(sb->errbuf, sizeof(sb->errbuf), "pipe: %s", strerror(errno));
        close(ruleset_fd);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        snprintf(sb->errbuf, sizeof(sb->errbuf), "fork: %s", strerror(errno));
        close(pin[0]); close(pin[1]);
        close(pout[0]); close(pout[1]);
        close(perr[0]); close(perr[1]);
        close(ruleset_fd);
        return -1;
    }

    if (pid == 0) {
        /* --- child --- */

        /* wire up stdio */
        dup2(pin[0], STDIN_FILENO);
        dup2(pout[1], STDOUT_FILENO);
        dup2(perr[1], STDERR_FILENO);
        close(pin[0]); close(pin[1]);
        close(pout[0]); close(pout[1]);
        close(perr[0]); close(perr[1]);

        /* Required for landlock_restrict_self without CAP_SYS_ADMIN */
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
            fprintf(stderr, "prctl(NO_NEW_PRIVS): %s\n", strerror(errno));
            _exit(126);
        }

        /* Apply the landlock ruleset */
        if (landlock_restrict_self(ruleset_fd, 0)) {
            fprintf(stderr, "landlock_restrict_self: %s\n", strerror(errno));
            _exit(126);
        }
        close(ruleset_fd);

        /* exec */
        if (env) {
            execve(cmd, (char *const *)argv, (char *const *)env);
        } else {
            extern char **environ;
            execve(cmd, (char *const *)argv, environ);
        }

        /* exec failed */
        fprintf(stderr, "execve(%s): %s\n", cmd, strerror(errno));
        _exit(127);
    }

    /* --- parent --- */
    close(pin[0]);
    close(pout[1]);
    close(perr[1]);
    close(ruleset_fd);

    sb->pid = pid;
    sb->stdin_fd = pin[1];
    sb->stdout_fd = pout[0];
    sb->stderr_fd = perr[0];
    sb->exited = 0;

    return 0;
}

pid_t sb_pid(const sb_t *sb)       { return sb ? sb->pid : -1; }
int   sb_stdin_fd(const sb_t *sb)  { return sb ? sb->stdin_fd : -1; }
int   sb_stdout_fd(const sb_t *sb) { return sb ? sb->stdout_fd : -1; }
int   sb_stderr_fd(const sb_t *sb) { return sb ? sb->stderr_fd : -1; }

int sb_wait(sb_t *sb) {
    if (!sb || sb->pid < 0) return -1;
    if (sb->exited) return sb->exit_code;

    int status;
    if (waitpid(sb->pid, &status, 0) < 0) {
        snprintf(sb->errbuf, sizeof(sb->errbuf), "waitpid: %s",
                 strerror(errno));
        return -1;
    }

    sb->exited = 1;
    sb->exit_code = WIFEXITED(status) ?
                    WEXITSTATUS(status) : 128 + WTERMSIG(status);
    return sb->exit_code;
}

int sb_kill(sb_t *sb, int sig) {
    if (!sb || sb->pid < 0 || sb->exited) return -1;
    if (kill(sb->pid, sig) < 0) {
        snprintf(sb->errbuf, sizeof(sb->errbuf), "kill: %s",
                 strerror(errno));
        return -1;
    }
    return 0;
}

void sb_destroy(sb_t *sb) {
    if (!sb || sb->destroyed) return;
    sb->destroyed = 1;

    if (sb->pid > 0 && !sb->exited) {
        kill(sb->pid, SIGKILL);
        int status;
        waitpid(sb->pid, &status, 0);
    }

    if (sb->stdin_fd >= 0)  close(sb->stdin_fd);
    if (sb->stdout_fd >= 0) close(sb->stdout_fd);
    if (sb->stderr_fd >= 0) close(sb->stderr_fd);
    for (int i = 0; i < sb->writable_count; i++) free(sb->writable[i]);
    free(sb->writable);
    for (int i = 0; i < sb->readable_count; i++) free(sb->readable[i]);
    free(sb->readable);
    free(sb);
}

const char *sb_error(const sb_t *sb) {
    if (!sb) return "null sandbox";
    return sb->errbuf[0] ? sb->errbuf : NULL;
}

#endif /* __linux__ */
