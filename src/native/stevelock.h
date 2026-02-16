/*
 *
 * ███████╗████████╗███████╗██╗   ██╗███████╗██╗      ██████╗  ██████╗██╗  ██╗
 * ██╔════╝╚══██╔══╝██╔════╝██║   ██║██╔════╝██║     ██╔═══██╗██╔════╝██║ ██╔╝
 * ███████╗   ██║   █████╗  ██║   ██║█████╗  ██║     ██║   ██║██║     █████╔╝
 * ╚════██║   ██║   ██╔══╝  ╚██╗ ██╔╝██╔══╝  ██║     ██║   ██║██║     ██╔═██╗
 * ███████║   ██║   ███████╗ ╚████╔╝ ███████╗███████╗╚██████╔╝╚██████╗██║  ██╗
 * ╚══════╝   ╚═╝   ╚══════╝  ╚═══╝  ╚══════╝╚══════╝ ╚═════╝  ╚═════╝╚═╝  ╚═╝

 * TODO:
 *   - Do not depend on libc
 *     - Replace syscall() with a little ASM for x64 and ARM
 *     - Replace allocation with mmap + memory arena
 *
 *
*/
#ifndef SL_STEVELOCK_H
#define SL_STEVELOCK_H

// ██████╗ ██╗      █████╗ ████████╗███████╗ ██████╗ ██████╗ ███╗   ███╗
// ██╔══██╗██║     ██╔══██╗╚══██╔══╝██╔════╝██╔═══██╗██╔══██╗████╗ ████║
// ██████╔╝██║     ███████║   ██║   █████╗  ██║   ██║██████╔╝██╔████╔██║
// ██╔═══╝ ██║     ██╔══██║   ██║   ██╔══╝  ██║   ██║██╔══██╗██║╚██╔╝██║
// ██║     ███████╗██║  ██║   ██║   ██║     ╚██████╔╝██║  ██║██║ ╚═╝ ██║
// ╚═╝     ╚══════╝╚═╝  ╚═╝   ╚═╝   ╚═╝      ╚═════╝ ╚═╝  ╚═╝╚═╝     ╚═╝
#ifdef _WIN32
#define SL_WIN32
#endif

#ifdef __APPLE__
#define SL_MACOS
#define SL_POSIX
#endif

#ifdef __linux__
#define SL_LINUX
#define SL_POSIX
#endif

#ifdef __COSMOPOLITAN__
#define SL_COSMO
#define SL_POSIX
#endif

#ifdef __cplusplus
#define SL_CPP
#endif

// ███╗   ███╗ █████╗  ██████╗██████╗  ██████╗ ███████╗
// ████╗ ████║██╔══██╗██╔════╝██╔══██╗██╔═══██╝██╔════╝
// ██╔████╔██║███████║██║     ██████╔╝██║   ██║███████╗
// ██║╚██╔╝██║██╔══██║██║     ██╔══██╗██║   ██║╚════██║
// ██║ ╚═╝ ██║██║  ██║╚██████╗██║  ██║╚██████╔╝███████║
// ╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝
#ifdef SL_CPP
  #define SL_RVAL(T) (T)
  #define SL_THREAD_LOCAL thread_local
  #define SL_BEGIN_EXTERN_C() extern "C" {
  #define SL_END_EXTERN_C() }
  #define SL_ZERO {}
  #define SL_NULL 0
  #define SL_NULLPTR nullptr
#else
  #define SL_RVAL(T) (T)
  #define SL_THREAD_LOCAL _Thread_local
  #define SL_BEGIN_EXTERN_C()
  #define SL_END_EXTERN_C()
  #define SL_ZERO {0}
  #define SL_NULL 0
  #define SL_NULLPTR ((void*)0)
#endif

#define sp_for(it, n) for (u32 it = 0; it < n; it++)
#define sp_for_range(it, low, high) for (u32 it = (low); it < (high); it++)

#if defined(SP_USE_ASSERTS)
#define sp_try(expr) sp_assert(!expr)
#define sp_try_as(expr, err) sp_assert(!expr)
#define sp_try_as_void(expr) sp_assert(!expr)
#else
#define sp_try(expr)                                                           \
  do {                                                                         \
    s32 _sp_result = (expr);                                                   \
    if (_sp_result)                                                            \
      return _sp_result;                                                       \
  } while (0)
#define sp_try_as(expr, err)                                                   \
  do {                                                                         \
    if (expr)                                                                  \
      return err;                                                              \
  } while (0)
#define sp_try_as_void(expr)                                                   \
  do {                                                                         \
    if (expr)                                                                  \
      return;                                                                  \
  } while (0)
#endif

#define sl_for(it, n) for (u32 it = 0; it < n; it++)
#define sl_for_range(it, low, high) for (u32 it = (low); it < (high); it++)


// ██╗███╗   ██╗ ██████╗██╗     ██╗   ██╗██████╗ ███████╗
// ██║████╗  ██║██╔════╝██║     ██║   ██║██╔══██╗██╔════╝
// ██║██╔██╗ ██║██║     ██║     ██║   ██║██║  ██║█████╗
// ██║██║╚██╗██║██║     ██║     ██║   ██║██║  ██║██╔══╝
// ██║██║ ╚████║╚██████╗███████╗╚██████╔╝██████╔╝███████╗
// ╚═╝╚═╝  ╚═══╝ ╚═════╝╚══════╝ ╚═════╝ ╚═════╝ ╚══════╝
// @include

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>


// ████████╗██╗   ██╗██████╗ ███████╗███████╗
// ╚══██╔══╝╚██╗ ██╔╝██╔══██╗██╔════╝██╔════╝
//    ██║    ╚████╔╝ ██████╔╝█████╗  ███████╗
//    ██║     ╚██╔╝  ██╔═══╝ ██╔══╝  ╚════██║
//    ██║      ██║   ██║     ███████╗███████║
//    ╚═╝      ╚═╝   ╚═╝     ╚══════╝╚══════╝
// @types
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;
typedef char c8;

///////////
// ERROR //
///////////
typedef enum {
  SL_OK = 0,
  SL_ERROR = 1,
  SL_ERROR_UNSUPPORTED_KERNEL = 2,
  SL_ERROR_RULESET_CREATE = 3,
  SL_ERROR_RULESET_ADD = 4,
  SL_ERROR_PIPE = 5,
  SL_ERROR_FORK = 6,
  SL_ERROR_INVALID_CONTEXT = 7,
  SL_ERROR_INVALID_COMMAND = 8,
  SL_ERROR_INVALID_SCOPE = 9,
} sl_err_t;


const c8* sl_err_to_string(sl_err_t err);


///////////////
// ALLOCATOR //
///////////////
typedef enum {
  SL_ALLOC_MODE_ALLOC,
  SL_ALLOC_MODE_REALLOC,
  SL_ALLOC_MODE_FREE,
} sl_alloc_mode_t;

typedef void* (*sl_alloc_fn_t)(void*, sl_alloc_mode_t, u64, void*);

typedef struct {
  sl_alloc_fn_t on_alloc;
  void* user_data;
} sl_allocator_t;

#define sl_alloc_n(t, n) sl_alloc((sizeof(t)) * (n))
#define sl_alloc_t(t) sl_alloc_n(t, 1)

void* sl_allocator_alloc(sl_allocator_t allocator, u64 size);
void* sl_allocator_realloc(sl_allocator_t allocator, void* ptr, u64 size);
void  sl_allocator_free(sl_allocator_t allocator, void* ptr);
void* sl_malloc_allocator(void* user_data, sl_alloc_mode_t mode, u64 size, void* ptr);
void* sl_alloc(u64 size);
void  sl_free(void* ptr);


/////////////
// CONTEXT //
/////////////
#if defined(SL_LINUX)
typedef struct {
  s32 abi;
} sl_platform_t;

#elif defined(SL_MACOS)
typedef struct {
  const c8* profile;
} sl_platform_t;

#else
#error ("stevelock.h only supports Linux and macOS")
#endif

typedef struct {
  c8** dirs;
  u32 num_dirs;
} sl_scope_t;

typedef struct {
  s32 pid;
  s32 stdin_fd;
  s32 stdout_fd;
  s32 stderr_fd;
  s32 exited;
  s32 destroyed;
  s32 exit_code;

  sl_scope_t read;
  sl_scope_t write;
  u32 network;
  char error[256];

  sl_platform_t platform;
} sl_ctx_t;


/////////
// API //
/////////
typedef struct {
  sl_allocator_t gpa;
} sl_runtime_t;
extern sl_runtime_t sl_rt;

typedef struct {
  sl_scope_t read;
  sl_scope_t write;
  u32 network;
} sb_opts_t;

typedef const c8* const* sl_env_t;

sl_ctx_t* sb_create(const sb_opts_t* opts);
sl_err_t  sb_spawn(sl_ctx_t* sb, const c8* cmd, const c8* const* args, u32 num_args, sl_env_t env);
pid_t     sb_pid(const sl_ctx_t* sb);
int       sb_stdin_fd(const sl_ctx_t* sb);
int       sb_stdout_fd(const sl_ctx_t* sb);
int       sb_stderr_fd(const sl_ctx_t* sb);
int       sb_wait(sl_ctx_t* sb);
int       sb_kill(sl_ctx_t* sb, int sig);
void      sb_destroy(sl_ctx_t* sb);
const c8* sb_error(const sl_ctx_t* sb);

#ifdef STEVELOCK_IMPLEMENTATION

sl_runtime_t sl_rt = {
  .gpa =
    {
      .on_alloc = sl_malloc_allocator,
      .user_data = SL_NULLPTR,
    },
};
////////////
// STRING //
////////////
typedef struct {
  const c8* data;
  u32 len;
} sl_str_t;

#define sl_str(data, len)                                                                                              \
  (sl_str_t) { data, len }

static u32 sl_cstr_len(const c8* str);
static c8* sl_cstr_copy(const c8* src);


////////
// IO //
////////
#define SL_CHILD_PRE_EXEC_FAILURE 126
#define SL_CHILD_POST_EXEC_FAILURE 127
#define SL_NULL_PIPE {-1, -1}
#define SL_NULL_PIPES {SL_NULL_PIPE, SL_NULL_PIPE, SL_NULL_PIPE}

typedef struct {
  s32 in[2];
  s32 out[2];
  s32 err[2];
} sl_pipes_t;

static void sl_child_fail(s32 exit_code);
static bool sl_is_child(s32 pid);
static bool sl_is_parent(s32 pid);
static void sl_pipe_try_close(s32 pipes[2]);
static void sl_pipes_try_close(sl_pipes_t* pipes);

const c8* sl_err_to_string(sl_err_t err) {
  switch (err) {
  case SL_OK:
    return "SL_OK";
  case SL_ERROR:
    return "SL_ERROR";
  case SL_ERROR_UNSUPPORTED_KERNEL:
    return "SL_ERROR_UNSUPPORTED_KERNEL";
  case SL_ERROR_RULESET_CREATE:
    return "SL_ERROR_RULESET_CREATE";
  case SL_ERROR_RULESET_ADD:
    return "SL_ERROR_RULESET_ADD";
  case SL_ERROR_PIPE:
    return "SL_ERROR_PIPE";
  case SL_ERROR_FORK:
    return "SL_ERROR_FORK";
  case SL_ERROR_INVALID_CONTEXT:
    return "SL_ERROR_INVALID_CONTEXT";
  case SL_ERROR_INVALID_COMMAND:
    return "SL_ERROR_INVALID_COMMAND";
  case SL_ERROR_INVALID_SCOPE:
    return "SL_ERROR_INVALID_SCOPE";
  }
  return "SL_ERROR_UNKNOWN";
}

void* sl_allocator_alloc(sl_allocator_t allocator, u64 size) {
  return allocator.on_alloc(allocator.user_data, SL_ALLOC_MODE_ALLOC, size, SL_NULLPTR);
}

void* sl_allocator_realloc(sl_allocator_t allocator, void* ptr, u64 size) {
  return allocator.on_alloc(allocator.user_data, SL_ALLOC_MODE_REALLOC, size, ptr);
}

void sl_allocator_free(sl_allocator_t allocator, void* ptr) {
  allocator.on_alloc(allocator.user_data, SL_ALLOC_MODE_FREE, 0, ptr);
}

void* sl_malloc_allocator(void* user_data, sl_alloc_mode_t mode, u64 size, void* ptr) {
  (void)user_data;
  switch (mode) {
  case SL_ALLOC_MODE_ALLOC:
    return calloc(size, 1);
  case SL_ALLOC_MODE_REALLOC:
    return realloc(ptr, size);
  case SL_ALLOC_MODE_FREE: {
    free(ptr);
    return SL_NULLPTR;
  }
  }
  return SL_NULLPTR;
}

void* sl_alloc(u64 size) {
  void* memory = sl_allocator_alloc(sl_rt.gpa, size);
  if (memory && size > 0) {
    memset(memory, 0, size);
  }
  return memory;
}

void sl_free(void* ptr) {
  if (!ptr) {
    return;
  }
  sl_allocator_free(sl_rt.gpa, ptr);
}

u32 sl_cstr_len(const c8* str) {
  if (!str) return 0;

  u32 len = 0;
  while (str[len]) {
    len++;
  }
  return len;
}

static c8* sl_cstr_copy(const c8* src) {
  if (!src) return SL_NULLPTR;

  u32 len = sl_cstr_len(src) + 1;
  c8* dst = (c8*)sl_alloc(len);
  if (!dst) {
    return SL_NULLPTR;
  }

  memcpy(dst, src, len);
  return dst;
}

static sl_err_t sl_validate_scope(const sl_scope_t* scope, const c8* kind, c8* err, u64 err_len) {
  if (!scope || !scope->num_dirs) {
    return SL_OK;
  }

  for (u32 i = 0; i < scope->num_dirs; i++) {
    const c8* path = scope->dirs[i];
    if (!path || !path[0]) {
      snprintf(err, err_len, "%s scope[%u] is empty", kind, i);
      return SL_ERROR_INVALID_SCOPE;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
      snprintf(err, err_len, "%s scope[%u] stat(%s): %s", kind, i, path, strerror(errno));
      return SL_ERROR_INVALID_SCOPE;
    }

    if (!S_ISDIR(st.st_mode)) {
      snprintf(err, err_len, "%s scope[%u] is not a directory: %s", kind, i, path);
      return SL_ERROR_INVALID_SCOPE;
    }
  }

  return SL_OK;
}

static sl_err_t sl_validate_ctx_scopes(sl_ctx_t* sb) {
  if (!sb) {
    return SL_ERROR_INVALID_CONTEXT;
  }

  sp_try(sl_validate_scope(&sb->read, "read", sb->error, sizeof(sb->error)));
  sp_try(sl_validate_scope(&sb->write, "write", sb->error, sizeof(sb->error)));
  return SL_OK;
}

void sl_child_fail(s32 exit_code) { _exit(exit_code); }

bool sl_is_child(s32 pid) { return pid == 0; }

bool sl_is_parent(s32 pid) { return pid > 0; }

void sl_pipe_try_close(s32 pipes[2]) {
  sp_for(it, 2) {
    if (pipes[it] >= 0) {
      close(pipes[it]);
    }
  }
}

void sl_pipes_try_close(sl_pipes_t* pipes) {
  sl_pipe_try_close(pipes->in);
  sl_pipe_try_close(pipes->out);
  sl_pipe_try_close(pipes->err);
}

#if defined(SL_LINUX)

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

#ifndef LANDLOCK_ACCESS_FS_REFER
#define LANDLOCK_ACCESS_FS_REFER 0
#endif

#ifndef LANDLOCK_ACCESS_FS_TRUNCATE
#define LANDLOCK_ACCESS_FS_TRUNCATE 0
#endif

#if defined(LANDLOCK_ACCESS_NET_BIND_TCP) && defined(LANDLOCK_ACCESS_NET_CONNECT_TCP)
#define SL_HAS_LANDLOCK_NET 1
#else
#define SL_HAS_LANDLOCK_NET 0
#endif

static inline int landlock_create_ruleset(const struct landlock_ruleset_attr* attr, u64 size, u32 flags) {
  return (int)syscall(__NR_landlock_create_ruleset, attr, size, flags);
}

static inline int landlock_add_rule(int ruleset_fd, enum landlock_rule_type type, const void* attr, __u32 flags) {
  return (int)syscall(__NR_landlock_add_rule, ruleset_fd, type, attr, flags);
}

static inline int landlock_restrict_self(int ruleset_fd, __u32 flags) {
  return (int)syscall(__NR_landlock_restrict_self, ruleset_fd, flags);
}

/* --- ABI negotiation ---------------------------------------------------- */

/* All FS access rights up through ABI v5 (IOCTL_DEV). */
#define ACCESS_FS_ROUGHLY_READ (LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR)

#define ACCESS_FS_ROUGHLY_WRITE                                                                                        \
  (LANDLOCK_ACCESS_FS_WRITE_FILE | LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE |                    \
   LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG |                          \
   LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK |                       \
   LANDLOCK_ACCESS_FS_MAKE_SYM | LANDLOCK_ACCESS_FS_REFER | LANDLOCK_ACCESS_FS_TRUNCATE)

#define ACCESS_FS_ALL (ACCESS_FS_ROUGHLY_READ | ACCESS_FS_ROUGHLY_WRITE)

static u64 get_fs_mask() {
  s32 abi = landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);

  u64 mask = ACCESS_FS_ALL;
  if (abi < 2) mask &= ~LANDLOCK_ACCESS_FS_REFER;
  if (abi < 3) mask &= ~LANDLOCK_ACCESS_FS_TRUNCATE;

  return mask;
}

/* --- sandbox struct ----------------------------------------------------- */

/* --- helpers ------------------------------------------------------------ */

/*
 * Open a path with O_PATH and add a LANDLOCK_RULE_PATH_BENEATH rule
 * granting `access` under that path. Returns 0 on success, -1 on error.
 */
static int add_path_rule(int ruleset_fd, const char* path, __u64 access, char* errbuf, size_t errbuf_sz) {
  int fd = open(path, O_PATH | O_CLOEXEC);
  if (fd < 0) {
    snprintf(errbuf, errbuf_sz, "open(%s, O_PATH): %s", path, strerror(errno));
    return -1;
  }

  struct landlock_path_beneath_attr pb = {
    .allowed_access = access,
    .parent_fd = fd,
  };

  int ret = landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH, &pb, 0);
  int saved = errno;
  close(fd);

  if (ret < 0) {
    snprintf(errbuf, errbuf_sz, "landlock_add_rule(%s): %s", path, strerror(saved));
    return -1;
  }
  return 0;
}

static sl_err_t add_path_rule_safe(s32 ruleset_fd, const char* path, __u64 access, sl_ctx_t* sb) {
  if (add_path_rule(ruleset_fd, path, access, (char*)sb->error, sizeof(sb->error)) < 0) {
    close(ruleset_fd);
    return SL_ERROR_RULESET_ADD;
  }
  return SL_OK;
}

/*
 * Build a landlock ruleset fd configured per `sb`.
 * Returns SL_OK and writes the fd to out_fd.
 */
static sl_err_t build_ruleset(sl_ctx_t* sb, s32* out_fd) {
  *out_fd = -1;

  u64 mask = get_fs_mask();

  struct landlock_ruleset_attr attr = {
    .handled_access_fs = mask,
#if SL_HAS_LANDLOCK_NET
    .handled_access_net = 0,
#endif
  };

  /* If network is denied, handle TCP bind+connect so they're blocked
   * unless we add explicit allow rules (which we won't). */
  int abi = landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
#if SL_HAS_LANDLOCK_NET
  if (!sb->network && abi >= 4) {
    attr.handled_access_net = LANDLOCK_ACCESS_NET_BIND_TCP | LANDLOCK_ACCESS_NET_CONNECT_TCP;
  }
#endif

  s32 ruleset_fd = landlock_create_ruleset(&attr, sizeof(attr), 0);
  if (ruleset_fd < 0) {
    snprintf((char*)sb->error, sizeof(sb->error), "landlock_create_ruleset: %s", strerror(errno));
    return SL_ERROR_RULESET_CREATE;
  }

  /* Allow read+execute on / (the whole filesystem) */
  __u64 read_access = ACCESS_FS_ROUGHLY_READ & mask;
  sp_try(add_path_rule_safe(ruleset_fd, "/", read_access, sb));

  /* Allow full access (read+write) to each writable directory */
  __u64 full_access = mask;
  for (u32 i = 0; i < sb->write.num_dirs; i++) {
    sp_try(add_path_rule_safe(ruleset_fd, sb->write.dirs[i], full_access, sb));
  }

  /* Allow writes to /dev (for /dev/null, /dev/tty, etc.) */
  sp_try(add_path_rule_safe(ruleset_fd, "/dev", full_access, sb));

  /* Additional readable paths */
  for (u32 i = 0; i < sb->read.num_dirs; i++) {
    sp_try(add_path_rule_safe(ruleset_fd, sb->read.dirs[i], read_access, sb));
  }

  /* If network allowed, add rules for all TCP ports */
  if (sb->network && abi >= 4) {
    /* Not handling net means net is unrestricted - already the case
     * since we didn't set handled_access_net. Nothing to do. */
  }

  *out_fd = ruleset_fd;
  return SL_OK;
}

/* --- public API --------------------------------------------------------- */

sl_ctx_t* sb_create(const sb_opts_t* opts) {
  if (!opts) return SL_NULLPTR;
  if (opts->write.num_dirs > 0 && !opts->write.dirs) return SL_NULLPTR;
  if (opts->read.num_dirs > 0 && !opts->read.dirs) return SL_NULLPTR;

  s32 abi = landlock_create_ruleset(NULL, 0, LANDLOCK_CREATE_RULESET_VERSION);
  if (abi < 0) return SL_NULLPTR;

  sl_ctx_t* sl = sl_alloc(sizeof(sl_ctx_t));
  if (!sl) return SL_NULLPTR;

  *sl = (sl_ctx_t){
    .pid = -1,
    .stdin_fd = -1,
    .stdout_fd = -1,
    .stderr_fd = -1,
    .write = {.dirs = sl_alloc_n(c8*, opts->write.num_dirs), .num_dirs = opts->write.num_dirs},
    .read = {.dirs = sl_alloc_n(c8*, opts->read.num_dirs), .num_dirs = opts->read.num_dirs},
    .network = opts->network,
    .platform = {
      .abi = abi,
    },
  };
  if (sl->write.num_dirs && !sl->write.dirs) {
    sl_free((void*)sl->read.dirs);
    sl_free(sl);
    return SL_NULLPTR;
  }
  if (sl->read.num_dirs && !sl->read.dirs) {
    sl_free((void*)sl->write.dirs);
    sl_free(sl);
    return SL_NULLPTR;
  }

  sl_for(it, sl->write.num_dirs) {
    sl->write.dirs[it] = sl_cstr_copy(opts->write.dirs[it]);
    if (!sl->write.dirs[it]) {
      sb_destroy(sl);
      return SL_NULLPTR;
    }
  }

  sl_for(it, sl->read.num_dirs) {
    sl->read.dirs[it] = sl_cstr_copy(opts->read.dirs[it]);
    if (!sl->read.dirs[it]) {
      sb_destroy(sl);
      return SL_NULLPTR;
    }
  }

  return sl;
}

sl_err_t sb_spawn(sl_ctx_t* sb, const c8* cmd, const c8* const* args, u32 num_args, sl_env_t env) {
  if (!sb) return SL_ERROR_INVALID_CONTEXT;
  if (!cmd) return SL_ERROR_INVALID_COMMAND;
  if (num_args && !args) return SL_ERROR_INVALID_COMMAND;
  if (sb->pid != -1) {
    snprintf(sb->error, sizeof(sb->error), "already spawned");
    return SL_ERROR;
  }

  sp_try(sl_validate_ctx_scopes(sb));

  s32 ruleset = SL_ZERO;
  sp_try(build_ruleset(sb, &ruleset));

  sl_pipes_t pipes = SL_NULL_PIPES;
  if (pipe(pipes.in) || pipe(pipes.out) || pipe(pipes.err)) {
    snprintf(sb->error, sizeof(sb->error), "pipe: %s", strerror(errno));
    sl_pipes_try_close(&pipes);
    close(ruleset);
    return SL_ERROR_PIPE;
  }

  const c8** argv = sl_alloc_n(const c8*, num_args + 2);
  if (!argv) {
    snprintf(sb->error, sizeof(sb->error), "alloc argv failed");
    sl_pipes_try_close(&pipes);
    close(ruleset);
    return SL_ERROR;
  }

  argv[0] = cmd;
  sl_for(it, num_args) { argv[it + 1] = args[it]; }
  argv[num_args + 1] = SL_NULLPTR;

  pid_t pid = fork();

  if (sl_is_parent(pid)) {
    close(pipes.in[0]);
    close(pipes.out[1]);
    close(pipes.err[1]);
    close(ruleset);

    sb->pid = pid;
    sb->stdin_fd = pipes.in[1];
    sb->stdout_fd = pipes.out[0];
    sb->stderr_fd = pipes.err[0];
    sb->exited = 0;

    sl_free((void*)argv);
    return SL_OK;
  }

  if (sl_is_child(pid)) {
    dup2(pipes.in[0], STDIN_FILENO);
    dup2(pipes.out[1], STDOUT_FILENO);
    dup2(pipes.err[1], STDERR_FILENO);
    sl_pipes_try_close(&pipes);

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
      fprintf(stderr, "prctl(NO_NEW_PRIVS): %s\n", strerror(errno));
      sl_child_fail(SL_CHILD_PRE_EXEC_FAILURE);
    }

    if (landlock_restrict_self(ruleset, 0)) {
      fprintf(stderr, "landlock_restrict_self: %s\n", strerror(errno));
      sl_child_fail(SL_CHILD_PRE_EXEC_FAILURE);
    }

    close(ruleset);

    if (env) {
      execve(cmd, (char* const*)argv, (char* const*)env);
    }

    if (!env) {
      extern char** environ;
      execve(cmd, (char* const*)argv, environ);
    }

    sl_child_fail(SL_CHILD_POST_EXEC_FAILURE);
  }

  sl_pipes_try_close(&pipes);
  close(ruleset);
  sl_free((void*)argv);
  snprintf(sb->error, sizeof(sb->error), "fork: %s", strerror(errno));
  return SL_ERROR_FORK;
}

pid_t sb_pid(const sl_ctx_t* sb) { return sb ? sb->pid : -1; }
int sb_stdin_fd(const sl_ctx_t* sb) { return sb ? sb->stdin_fd : -1; }
int sb_stdout_fd(const sl_ctx_t* sb) { return sb ? sb->stdout_fd : -1; }
int sb_stderr_fd(const sl_ctx_t* sb) { return sb ? sb->stderr_fd : -1; }

int sb_wait(sl_ctx_t* sb) {
  if (!sb || sb->pid < 0) return -1;
  if (sb->exited) return sb->exit_code;

  int status;
  if (waitpid(sb->pid, &status, 0) < 0) {
    snprintf(sb->error, sizeof(sb->error), "waitpid: %s", strerror(errno));
    return -1;
  }

  sb->exited = 1;
  sb->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
  return sb->exit_code;
}

int sb_kill(sl_ctx_t* sb, int sig) {
  if (!sb || sb->pid < 0 || sb->exited) return -1;
  if (kill(sb->pid, sig) < 0) {
    snprintf(sb->error, sizeof(sb->error), "kill: %s", strerror(errno));
    return -1;
  }
  return 0;
}

void sb_destroy(sl_ctx_t* sb) {
  if (!sb || sb->destroyed) return;
  sb->destroyed = 1;

  if (sb->pid > 0 && !sb->exited) {
    kill(sb->pid, SIGKILL);
    int status;
    waitpid(sb->pid, &status, 0);
  }

  if (sb->stdin_fd >= 0) close(sb->stdin_fd);
  if (sb->stdout_fd >= 0) close(sb->stdout_fd);
  if (sb->stderr_fd >= 0) close(sb->stderr_fd);
  for (u32 i = 0; i < sb->write.num_dirs; i++)
    sl_free((void*)sb->write.dirs[i]);
  sl_free((void*)sb->write.dirs);
  for (u32 i = 0; i < sb->read.num_dirs; i++)
    sl_free((void*)sb->read.dirs[i]);
  sl_free((void*)sb->read.dirs);
  sl_free(sb);
}

const c8* sb_error(const sl_ctx_t* sb) {
  if (!sb) return "null sandbox";
  return sb->error[0] ? sb->error : SL_NULLPTR;
}

#endif

#if defined(SL_MACOS)

#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* --- private sandbox API ------------------------------------------------ */

typedef int (*sandbox_init_fn)(const char* profile, uint64_t flags, const char* const params[], char** errorbuf);
typedef void (*sandbox_free_error_fn)(char* errorbuf);

static sandbox_init_fn sb_init_fn;
static sandbox_free_error_fn sb_free_fn;

static int sb_load_dylib(void) {
  static int loaded = -1;
  if (loaded != -1) return loaded;

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

/* --- profile generation ------------------------------------------------- */

typedef struct {
  c8* data;
  u64 len;
  u64 cap;
} sl_profile_builder_t;

static bool sl_profile_reserve(sl_profile_builder_t* builder, u64 append_len) {
  if (!builder) {
    return false;
  }

  u64 required = builder->len + append_len + 1;
  if (required <= builder->cap) {
    return true;
  }

  u64 cap = builder->cap;
  if (!cap) {
    cap = 4096;
  }

  while (cap < required) {
    if (cap > (UINT64_MAX / 2)) {
      return false;
    }
    cap *= 2;
  }

  c8* next = sl_allocator_realloc(sl_rt.gpa, builder->data, cap);
  if (!next) {
    return false;
  }

  builder->data = next;
  builder->cap = cap;
  return true;
}

static bool sl_profile_append_n(sl_profile_builder_t* builder, const c8* value, u64 len) {
  if (!len) {
    return true;
  }
  if (!value) {
    return false;
  }
  if (!sl_profile_reserve(builder, len)) {
    return false;
  }

  memcpy(builder->data + builder->len, value, len);
  builder->len += len;
  builder->data[builder->len] = 0;
  return true;
}

static bool sl_profile_append(sl_profile_builder_t* builder, const c8* value) {
  if (!value) {
    return false;
  }
  return sl_profile_append_n(builder, value, sl_cstr_len(value));
}

static bool sl_profile_append_ch(sl_profile_builder_t* builder, c8 ch) {
  return sl_profile_append_n(builder, &ch, 1);
}

static bool sl_profile_append_escaped_path(sl_profile_builder_t* builder, const c8* path) {
  if (!path || !path[0]) {
    return false;
  }

  for (u32 it = 0; path[it]; it++) {
    c8 ch = path[it];
    if (ch == '\\' || ch == '"') {
      if (!sl_profile_append_ch(builder, '\\')) {
        return false;
      }
    }
    if (!sl_profile_append_ch(builder, ch)) {
      return false;
    }
  }

  return true;
}

static bool sl_profile_append_subpath(sl_profile_builder_t* builder, const c8* action, const c8* path) {
  if (!action || !path || !path[0]) {
    return false;
  }

  if (!sl_profile_append_ch(builder, '(')) {
    return false;
  }
  if (!sl_profile_append(builder, action)) {
    return false;
  }
  if (!sl_profile_append(builder, " (subpath \"")) {
    return false;
  }
  if (!sl_profile_append_escaped_path(builder, path)) {
    return false;
  }
  if (!sl_profile_append(builder, "\"))")) {
    return false;
  }

  return true;
}

static bool sl_profile_append_subpath_resolved(sl_profile_builder_t* builder, const c8* action, const c8* path) {
  if (!sl_profile_append_subpath(builder, action, path)) {
    return false;
  }

  c8 resolved[PATH_MAX] = SL_ZERO;
  if (!realpath(path, resolved)) {
    return true;
  }
  if (!strcmp(path, resolved)) {
    return true;
  }

  return sl_profile_append_subpath(builder, action, resolved);
}

static c8* build_profile(const sb_opts_t* opts) {
  if (!opts) {
    return SL_NULLPTR;
  }

  sl_profile_builder_t profile = {
    .data = sl_alloc_n(c8, 4096),
    .cap = 4096,
  };
  if (!profile.data) {
    return SL_NULLPTR;
  }
  profile.data[0] = 0;

  if (!sl_profile_append(&profile, "(version 1)")) goto fail;
  if (!sl_profile_append(&profile, "(deny default (with no-log))")) goto fail;
  if (!sl_profile_append(&profile, "(allow process*)")) goto fail;
  if (!sl_profile_append(&profile, "(allow sysctl-read)")) goto fail;
  if (!sl_profile_append(&profile, "(allow mach*)")) goto fail;
  if (!sl_profile_append(&profile, "(allow ipc*)")) goto fail;
  if (!sl_profile_append(&profile, "(allow signal)")) goto fail;

  if (!sl_profile_append(&profile, "(allow file-read*)")) goto fail;

  sl_for(it, opts->read.num_dirs) {
    if (!sl_profile_append_subpath_resolved(&profile, "allow file-read*", opts->read.dirs[it])) {
      goto fail;
    }
  }

  sl_for(it, opts->write.num_dirs) {
    if (!sl_profile_append_subpath_resolved(&profile, "allow file-write*", opts->write.dirs[it])) {
      goto fail;
    }
  }

  if (!sl_profile_append_subpath(&profile, "allow file-write*", "/dev")) goto fail;

  if (opts->network) {
    if (!sl_profile_append(&profile, "(allow network*)")) goto fail;
  }

  return profile.data;

fail:
  sl_free(profile.data);
  return SL_NULLPTR;
}

/* --- public API --------------------------------------------------------- */

sl_ctx_t* sb_create(const sb_opts_t* opts) {
  if (!opts) return SL_NULLPTR;
  if (opts->write.num_dirs > 0 && !opts->write.dirs) return SL_NULLPTR;
  if (opts->read.num_dirs > 0 && !opts->read.dirs) return SL_NULLPTR;
  if (!sb_load_dylib()) return SL_NULLPTR;

  sl_ctx_t* sb = sl_alloc(sizeof(sl_ctx_t));
  if (!sb) return SL_NULLPTR;

  sb->pid = -1;
  sb->stdin_fd = -1;
  sb->stdout_fd = -1;
  sb->stderr_fd = -1;
  sb->write = (sl_scope_t){
    .dirs = sl_alloc_n(c8*, opts->write.num_dirs),
    .num_dirs = opts->write.num_dirs,
  };
  sb->read = (sl_scope_t){
    .dirs = sl_alloc_n(c8*, opts->read.num_dirs),
    .num_dirs = opts->read.num_dirs,
  };

  if (sb->write.num_dirs && !sb->write.dirs) {
    sl_free(sb);
    return SL_NULLPTR;
  }

  if (sb->read.num_dirs && !sb->read.dirs) {
    sl_free((void*)sb->write.dirs);
    sl_free(sb);
    return SL_NULLPTR;
  }

  sl_for(it, sb->write.num_dirs) {
    sb->write.dirs[it] = sl_cstr_copy(opts->write.dirs[it]);
    if (!sb->write.dirs[it]) {
      sb_destroy(sb);
      return SL_NULLPTR;
    }
  }

  sl_for(it, sb->read.num_dirs) {
    sb->read.dirs[it] = sl_cstr_copy(opts->read.dirs[it]);
    if (!sb->read.dirs[it]) {
      sb_destroy(sb);
      return SL_NULLPTR;
    }
  }

  sb->network = opts->network;

  sb->platform.profile = build_profile(opts);
  if (!sb->platform.profile) {
    sb_destroy(sb);
    return SL_NULLPTR;
  }

  return sb;
}

sl_err_t sb_spawn(sl_ctx_t* sb, const c8* cmd, const c8* const* args, u32 num_args, sl_env_t env) {
  if (!sb) return SL_ERROR_INVALID_CONTEXT;
  if (!cmd) return SL_ERROR_INVALID_COMMAND;
  if (num_args && !args) return SL_ERROR_INVALID_COMMAND;
  if (sb->pid != -1) {
    snprintf(sb->error, sizeof(sb->error), "already spawned");
    return SL_ERROR;
  }

  sp_try(sl_validate_ctx_scopes(sb));

  /* pipes: parent -> child stdin, child stdout -> parent, child stderr ->
   * parent */
  sl_pipes_t pipes = SL_NULL_PIPES;
  if (pipe(pipes.in) || pipe(pipes.out) || pipe(pipes.err)) {
    snprintf(sb->error, sizeof(sb->error), "pipe: %s", strerror(errno));
    sl_pipes_try_close(&pipes);
    return SL_ERROR_PIPE;
  }

  const c8** argv = sl_alloc_n(const c8*, num_args + 2);
  if (!argv) {
    snprintf(sb->error, sizeof(sb->error), "alloc argv failed");
    sl_pipes_try_close(&pipes);
    return SL_ERROR;
  }

  argv[0] = cmd;
  sl_for(it, num_args) { argv[it + 1] = args[it]; }
  argv[num_args + 1] = SL_NULLPTR;

  pid_t pid = fork();
  if (pid < 0) {
    snprintf(sb->error, sizeof(sb->error), "fork: %s", strerror(errno));
    sl_pipes_try_close(&pipes);
    sl_free((void*)argv);
    return SL_ERROR_FORK;
  }

  if (pid == 0) {
    /* --- child --- */

    /* wire up stdio */
    dup2(pipes.in[0], STDIN_FILENO);
    dup2(pipes.out[1], STDOUT_FILENO);
    dup2(pipes.err[1], STDERR_FILENO);
    sl_pipes_try_close(&pipes);

    /* apply sandbox */
    char* sberr = NULL;
    if (sb_init_fn(sb->platform.profile, 0, NULL, &sberr) != 0) {
      fprintf(stderr, "sandbox_init: %s\n", sberr ? sberr : "unknown");
      if (sberr) sb_free_fn(sberr);
      _exit(126);
    }

    /* exec */
    if (env) {
      execve(cmd, (char* const*)argv, (char* const*)env);
    }

    if (!env) {
      extern char** environ;
      execve(cmd, (char* const*)argv, environ);
    }

    /* exec failed */
    fprintf(stderr, "execve(%s): %s\n", cmd, strerror(errno));
    _exit(127);
  }

  /* --- parent --- */
  close(pipes.in[0]);
  close(pipes.out[1]);
  close(pipes.err[1]);

  sb->pid = pid;
  sb->stdin_fd = pipes.in[1];
  sb->stdout_fd = pipes.out[0];
  sb->stderr_fd = pipes.err[0];
  sb->exited = 0;

  sl_free((void*)argv);
  return SL_OK;
}

pid_t sb_pid(const sl_ctx_t* sb) { return sb ? sb->pid : -1; }
int sb_stdin_fd(const sl_ctx_t* sb) { return sb ? sb->stdin_fd : -1; }
int sb_stdout_fd(const sl_ctx_t* sb) { return sb ? sb->stdout_fd : -1; }
int sb_stderr_fd(const sl_ctx_t* sb) { return sb ? sb->stderr_fd : -1; }

int sb_wait(sl_ctx_t* sb) {
  if (!sb || sb->pid < 0) return -1;
  if (sb->exited) return sb->exit_code;

  int status;
  if (waitpid(sb->pid, &status, 0) < 0) {
    snprintf(sb->error, sizeof(sb->error), "waitpid: %s", strerror(errno));
    return -1;
  }

  sb->exited = 1;
  sb->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
  return sb->exit_code;
}

int sb_kill(sl_ctx_t* sb, int sig) {
  if (!sb || sb->pid < 0 || sb->exited) return -1;
  if (kill(sb->pid, sig) < 0) {
    snprintf(sb->error, sizeof(sb->error), "kill: %s", strerror(errno));
    return -1;
  }
  return 0;
}

void sb_destroy(sl_ctx_t* sb) {
  if (!sb || sb->destroyed) return;
  sb->destroyed = 1;

  if (sb->pid > 0 && !sb->exited) {
    kill(sb->pid, SIGKILL);
    int status;
    waitpid(sb->pid, &status, 0);
  }

  if (sb->stdin_fd >= 0) close(sb->stdin_fd);
  if (sb->stdout_fd >= 0) close(sb->stdout_fd);
  if (sb->stderr_fd >= 0) close(sb->stderr_fd);
  for (u32 i = 0; i < sb->write.num_dirs; i++)
    sl_free((void*)sb->write.dirs[i]);
  sl_free((void*)sb->write.dirs);
  for (u32 i = 0; i < sb->read.num_dirs; i++)
    sl_free((void*)sb->read.dirs[i]);
  sl_free((void*)sb->read.dirs);
  sl_free((void*)sb->platform.profile);
  sl_free(sb);
}

const c8* sb_error(const sl_ctx_t* sb) {
  if (!sb) return "null sandbox";
  return sb->error[0] ? sb->error : SL_NULLPTR;
}

#endif

#endif

#endif // SL_STEVELOCK_H
