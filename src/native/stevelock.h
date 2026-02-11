#ifndef SL_STEVELOCK_H
#define SL_STEVELOCK_H

#include <sys/types.h>

#include "sp.h"

typedef struct sb sb_t;

typedef struct {
  const char** writable;
  s32 writable_count;

  /* allow network access (default: denied) */
  int allow_net;

  /* paths readable by the sandboxed process (NULL = all) */
  const char** readable;
  int readable_count;
} sb_opts_t;

/* create a sandbox config. does not spawn anything yet. */
sb_t* sb_create(const sb_opts_t* opts);

/* spawn a process inside the sandbox. returns 0 on success, -1 on error.
 * after this call, sb_pid() returns the child pid.
 * env may be NULL to inherit the parent environment. */
int sb_spawn(sb_t* sb, const char* cmd, const char* const* argv,
             const char* const* env);

/* get the child pid (-1 if not spawned) */
pid_t sb_pid(const sb_t* sb);

/* get stdout/stderr fds for the child (-1 if not spawned).
 * stdin_fd is the fd you write to, stdout/stderr are fds you read from. */
int sb_stdin_fd(const sb_t* sb);
int sb_stdout_fd(const sb_t* sb);
int sb_stderr_fd(const sb_t* sb);

/* blocking wait for exit. returns exit code, or -1 on error. */
int sb_wait(sb_t* sb);

/* send a signal (e.g. SIGTERM, SIGKILL). returns 0 or -1. */
int sb_kill(sb_t* sb, int sig);

/* kill (if running) and free all resources. */
void sb_destroy(sb_t* sb);

/* last error message (static string, valid until next sb_ call on this sb). */
const char* sb_error(const sb_t* sb);

#endif // SL_STEVELOCK_H
