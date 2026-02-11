/*
 * sb_darwin.c - macOS sandbox implementation using Seatbelt
 *
 * Uses sandbox_init_with_parameters (private but stable API from
 * libsystem_sandbox.dylib) applied in a forked child before exec.
 */

#ifdef __APPLE__

#include "stevelock.h"

#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* --- private sandbox API ------------------------------------------------ */

typedef int (*sandbox_init_fn)(const char* profile, uint64_t flags,
                               const char* const params[], char** errorbuf);
typedef void (*sandbox_free_error_fn)(char* errorbuf);

static sandbox_init_fn sb_init_fn;
static sandbox_free_error_fn sb_free_fn;

static int sb_load_dylib(void) {
  static int loaded = -1;
  if (loaded != -1)
    return loaded;

  void* lib = dlopen("/usr/lib/system/libsystem_sandbox.dylib", RTLD_LAZY);
  if (!lib) {
    loaded = 0;
    return 0;
  }

  sb_init_fn = (sandbox_init_fn)dlsym(lib, "sandbox_init_with_parameters");
  sb_free_fn = (sandbox_free_error_fn)dlsym(lib, "sandbox_free_error");
  loaded = (sb_init_fn && sb_free_fn) ? 1 : 0;
  return loaded;
}

/* --- sandbox struct ----------------------------------------------------- */

struct sb {
  char* profile; /* compiled SBPL string */
  pid_t pid;     /* child pid, -1 if not spawned */
  int stdin_fd;  /* parent writes here */
  int stdout_fd; /* parent reads here */
  int stderr_fd; /* parent reads here */
  int exited;    /* 1 if we already waited */
  int destroyed; /* 1 if sb_destroy already called */
  int exit_code;
  char errbuf[256];
};

/* --- profile generation ------------------------------------------------- */

static char* build_profile(const sb_opts_t* opts) {
  /*
   * We build an SBPL profile string. Strategy:
   *   - deny default
   *   - allow process control (fork/exec/signal)
   *   - allow reads to system paths (and optionally user-specified)
   *   - deny reads to /Users
   *   - allow writes only to opts->root and /dev
   *   - optionally allow network
   */
  size_t cap = 2048;
  char* p = malloc(cap);
  if (!p)
    return NULL;

  int off = 0;

  off += snprintf(p + off, cap - off,
                  "(version 1)"
                  "(deny default (with no-log))"
                  "(allow process*)"
                  "(allow sysctl-read)"
                  "(allow mach*)"
                  "(allow ipc*)"
                  "(allow signal)");

  /* file reads: allow system, deny /Users */
  off += snprintf(p + off, cap - off,
                  "(allow file-read*)"
                  "(deny file-read* (subpath \"/Users\"))");

  /* if any writable dir is under /Users, re-allow reads there */
  for (int i = 0; i < opts->writable_count; i++) {
    if (strncmp(opts->writable[i], "/Users", 6) == 0) {
      off += snprintf(p + off, cap - off, "(allow file-read* (subpath \"%s\"))",
                      opts->writable[i]);
    }
  }

  /* additional readable paths */
  for (int i = 0; i < opts->readable_count; i++) {
    off += snprintf(p + off, cap - off, "(allow file-read* (subpath \"%s\"))",
                    opts->readable[i]);
  }

  /* file writes: only the writable dirs + /dev + /private/var (for tmp) */
  for (int i = 0; i < opts->writable_count; i++) {
    off += snprintf(p + off, cap - off, "(allow file-write* (subpath \"%s\"))",
                    opts->writable[i]);
  }
  off += snprintf(p + off, cap - off,
                  "(allow file-write* (subpath \"/dev\"))"
                  "(allow file-write* (subpath \"/private/var/folders\"))");

  /* network */
  if (opts->allow_net) {
    off += snprintf(p + off, cap - off, "(allow network*)");
  }

  return p;
}

/* --- public API --------------------------------------------------------- */

sb_t* sb_create(const sb_opts_t* opts) {
  if (!opts || !opts->writable || opts->writable_count < 1)
    return NULL;
  if (!sb_load_dylib())
    return NULL;

  sb_t* sb = calloc(1, sizeof(sb_t));
  if (!sb)
    return NULL;

  sb->pid = -1;
  sb->stdin_fd = -1;
  sb->stdout_fd = -1;
  sb->stderr_fd = -1;

  sb->profile = build_profile(opts);
  if (!sb->profile) {
    free(sb);
    return NULL;
  }

  return sb;
}

int sb_spawn(sb_t* sb, const char* cmd, const char* const* argv,
             const char* const* env) {
  if (!sb || !cmd)
    return -1;
  if (sb->pid != -1) {
    snprintf(sb->errbuf, sizeof(sb->errbuf), "already spawned");
    return -1;
  }

  /* pipes: parent -> child stdin, child stdout -> parent, child stderr ->
   * parent */
  int pin[2], pout[2], perr[2];
  if (pipe(pin) || pipe(pout) || pipe(perr)) {
    snprintf(sb->errbuf, sizeof(sb->errbuf), "pipe: %s", strerror(errno));
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    snprintf(sb->errbuf, sizeof(sb->errbuf), "fork: %s", strerror(errno));
    close(pin[0]);
    close(pin[1]);
    close(pout[0]);
    close(pout[1]);
    close(perr[0]);
    close(perr[1]);
    return -1;
  }

  if (pid == 0) {
    /* --- child --- */

    /* wire up stdio */
    dup2(pin[0], STDIN_FILENO);
    dup2(pout[1], STDOUT_FILENO);
    dup2(perr[1], STDERR_FILENO);
    close(pin[0]);
    close(pin[1]);
    close(pout[0]);
    close(pout[1]);
    close(perr[0]);
    close(perr[1]);

    /* apply sandbox */
    char* sberr = NULL;
    if (sb_init_fn(sb->profile, 0, NULL, &sberr) != 0) {
      fprintf(stderr, "sandbox_init: %s\n", sberr ? sberr : "unknown");
      if (sberr)
        sb_free_fn(sberr);
      _exit(126);
    }

    /* exec */
    if (env) {
      execve(cmd, (char* const*)argv, (char* const*)env);
    } else {
      extern char** environ;
      execve(cmd, (char* const*)argv, environ);
    }

    /* exec failed */
    fprintf(stderr, "execve(%s): %s\n", cmd, strerror(errno));
    _exit(127);
  }

  /* --- parent --- */
  close(pin[0]);
  close(pout[1]);
  close(perr[1]);

  sb->pid = pid;
  sb->stdin_fd = pin[1];
  sb->stdout_fd = pout[0];
  sb->stderr_fd = perr[0];
  sb->exited = 0;

  return 0;
}

pid_t sb_pid(const sb_t* sb) { return sb ? sb->pid : -1; }
int sb_stdin_fd(const sb_t* sb) { return sb ? sb->stdin_fd : -1; }
int sb_stdout_fd(const sb_t* sb) { return sb ? sb->stdout_fd : -1; }
int sb_stderr_fd(const sb_t* sb) { return sb ? sb->stderr_fd : -1; }

int sb_wait(sb_t* sb) {
  if (!sb || sb->pid < 0)
    return -1;
  if (sb->exited)
    return sb->exit_code;

  int status;
  if (waitpid(sb->pid, &status, 0) < 0) {
    snprintf(sb->errbuf, sizeof(sb->errbuf), "waitpid: %s", strerror(errno));
    return -1;
  }

  sb->exited = 1;
  sb->exit_code =
    WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
  return sb->exit_code;
}

int sb_kill(sb_t* sb, int sig) {
  if (!sb || sb->pid < 0 || sb->exited)
    return -1;
  if (kill(sb->pid, sig) < 0) {
    snprintf(sb->errbuf, sizeof(sb->errbuf), "kill: %s", strerror(errno));
    return -1;
  }
  return 0;
}

void sb_destroy(sb_t* sb) {
  if (!sb || sb->destroyed)
    return;
  sb->destroyed = 1;

  if (sb->pid > 0 && !sb->exited) {
    kill(sb->pid, SIGKILL);
    int status;
    waitpid(sb->pid, &status, 0);
  }

  if (sb->stdin_fd >= 0)
    close(sb->stdin_fd);
  if (sb->stdout_fd >= 0)
    close(sb->stdout_fd);
  if (sb->stderr_fd >= 0)
    close(sb->stderr_fd);
  free(sb->profile);
  free(sb);
}

const char* sb_error(const sb_t* sb) {
  if (!sb)
    return "null sandbox";
  return sb->errbuf[0] ? sb->errbuf : NULL;
}

#endif /* __APPLE__ */
