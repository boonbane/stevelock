#include "js_native_api.h"
#include "js_native_api_types.h"
#include <node_api.h>

#define STEVELOCK_IMPLEMENTATION
#include "stevelock.h"

typedef enum {
  SL_NAPI_OK = 0,
  SL_NAPI_ERROR = 1,
  SL_NAPI_BAD_ARG = 2,
  SL_NAPI_FAILED_ALLOC = 3,
} sl_napi_err_t;

typedef struct {
  sl_ctx_t* sb;
} n_sb_handle_t;

void n_finalize(napi_env env, void* ptr, void* hint) {
  (void)env;
  (void)hint;

  if (!ptr) {
    return;
  }

  n_sb_handle_t* h = (n_sb_handle_t*)ptr;
  if (h->sb) {
    sb_destroy(h->sb);
    h->sb = NULL;
  }

  sl_free(h);
}

#define NAPI_CALL(call)  \
  do {  \
    napi_status _s = (call);  \
    if (_s != napi_ok) {  \
      napi_throw_error(env, NULL, #call " failed");  \
      return NULL;  \
    }  \
  } while (0)

static n_sb_handle_t* n_get_handle(napi_env env, napi_value v) {
  n_sb_handle_t* h = NULL;
  NAPI_CALL(napi_get_value_external(env, v, (void**)&h));
  if (!h || !h->sb) {
    napi_throw_error(env, NULL, "sandbox destroyed");
    return NULL;
  }
  return h;
}

typedef struct {
  napi_value value;
  napi_value read;
  napi_value write;
  napi_value network;
} sl_napi_options_t;

static void sl_napi_free_scope(sl_scope_t* scope) {
  if (!scope) {
    return;
  }

  if (!scope->dirs) {
    scope->num_dirs = 0;
    return;
  }

  sl_for(i, scope->num_dirs) { sl_free(scope->dirs[i]); }

  sl_free(scope->dirs);
  *scope = (sl_scope_t)SL_ZERO;
}

static s32 sl_napi_copy_str(napi_env env, napi_value value, c8** str) {
  size_t len = 0;
  sp_try(napi_get_value_string_utf8(env, value, NULL, 0, &len));

  struct {
    c8* data;
    u32 len;
  } buffer = {
    .len = len + 1,
  };

  buffer.data = sl_alloc_n(c8, buffer.len);
  if (!buffer.data) return SL_NAPI_FAILED_ALLOC;

  napi_status status = napi_get_value_string_utf8(env, value, buffer.data, buffer.len, &len);
  if (status) {
    sl_free(buffer.data);
    return status;
  }

  *str = buffer.data;
  return SL_OK;
}

static s32 sl_napi_copy_scope(napi_env napi, napi_value value, sl_scope_t* scope) {
  bool is_array = false;
  sp_try(napi_is_array(napi, value, &is_array));
  if (!is_array) return SL_NAPI_BAD_ARG;

  sp_try(napi_get_array_length(napi, value, &scope->num_dirs));
  if (!scope->num_dirs) return SL_NAPI_OK;

  scope->dirs = sl_alloc_n(const c8*, scope->num_dirs);
  if (!scope->dirs) return SL_NAPI_FAILED_ALLOC;

  sl_for(it, scope->num_dirs) {
    napi_value dir;
    sp_try(napi_get_element(napi, value, it, &dir));

    c8* str = SL_ZERO;
    sp_try(sl_napi_copy_str(napi, dir, &str));
    scope->dirs[it] = str;
  }

  return 0;
}

#define SL_NAPI_MAX_ARGS 8

typedef struct {
  u64 num_args;
  napi_valuetype types [SL_NAPI_MAX_ARGS];
} sl_napi_cb_desc_t;

typedef struct {
  u64 num_args;
  napi_value args [8];
  napi_status error;
} sl_napi_cb_t;

sl_napi_cb_t sl_napi_get_cb_info(napi_env env, napi_callback_info info, u64 num_args) {
  sl_napi_cb_t cb = { .num_args = num_args };
  cb.error = napi_get_cb_info(env, info, &cb.num_args, cb.args, SL_NULLPTR, SL_NULLPTR);
  if (cb.num_args < num_args) {
    cb.error = napi_generic_failure;
  }
  return cb;
}

sl_napi_cb_t sl_napi_validate_cb(napi_env env, napi_callback_info info, sl_napi_cb_desc_t c) {
  sl_napi_cb_t cb = { .num_args = c.num_args };
  cb.error = napi_get_cb_info(env, info, &cb.num_args, cb.args, SL_NULLPTR, SL_NULLPTR);
  if (cb.num_args != c.num_args) {
    cb.error = napi_generic_failure;
    return cb;
  }

  sl_for(it, cb.num_args) {
    napi_valuetype type = napi_undefined;
    cb.error = napi_typeof(env, cb.args[it], &type);
    if (cb.error) return cb;
    if (type != c.types[it]) return cb;
  }

  return cb;
}

static napi_value sl_napi_create(napi_env env, napi_callback_info info) {
  napi_value result = SL_ZERO;

  sl_napi_cb_t cb = sl_napi_validate_cb(env, info, (sl_napi_cb_desc_t) {
    .num_args = 1,
    .types = { napi_object }
  });
  if (cb.error) {
    return SL_NULLPTR;
  }

  sl_napi_options_t v = {
    .value = cb.args[0],
  };
  sb_opts_t opts = {
    .read = SL_ZERO,
    .write = SL_ZERO,
    .network = 0,
  };

  if (!napi_get_named_property(env, v.value, "read", &v.read)) {
    if (sl_napi_copy_scope(env, v.read, &opts.read)) {
      goto done;
    }
  }

  if (!napi_get_named_property(env, v.value, "write", &v.write)) {
    if (sl_napi_copy_scope(env, v.write, &opts.write)) {
      goto done;
    }
  }

  if (napi_get_named_property(env, v.value, "network", &v.network) == napi_ok) {
    bool network = false;
    if (napi_get_value_bool(env, v.network, &network) != napi_ok) {
      goto done;
    }
    opts.network = network ? 1 : 0;
  }

  sl_ctx_t* sb = sb_create(&opts);
  if (!sb) {
    goto done;
  }

  n_sb_handle_t* handle = sl_alloc_t(n_sb_handle_t);
  if (!handle) {
    sb_destroy(sb);
    goto done;
  }
  handle->sb = sb;

  if (napi_create_external(env, handle, n_finalize, NULL, &result) != napi_ok) {
    n_finalize(env, handle, NULL);
    return NULL;
  }

done:
  sl_napi_free_scope(&opts.write);
  sl_napi_free_scope(&opts.read);
  return result;
}

///////////
// SPAWN //
///////////
static napi_value n_spawn(napi_env env, napi_callback_info info) {
  napi_value out = NULL;
  const c8* msg = SL_NULLPTR;
  c8* cmd = SL_ZERO;
  c8** argv = SL_ZERO;
  u32 num_argv = SL_ZERO;
  u32 num_filled = SL_ZERO;

  u64 num_args = 3;
  napi_value args[3] = SL_ZERO;
  if (napi_get_cb_info(env, info, &num_args, args, NULL, NULL) != napi_ok) {
    return NULL;
  }

  n_sb_handle_t* handle = n_get_handle(env, args[0]);
  if (!handle) {
    return SL_NULL;
  }

  sl_ctx_t* sb = handle->sb;

  if (sl_napi_copy_str(env, args[1], &cmd)) {
    msg = "spawn command must be a string";
    goto done;
  }

  /* args array */
  bool is_arr = false;
  if (napi_is_array(env, args[2], &is_arr) != napi_ok) {
    msg = "spawn args must be an array";
    goto done;
  }

  if (is_arr) {
    if (napi_get_array_length(env, args[2], &num_argv) != napi_ok) {
      msg = "spawn args must be an array";
      goto done;
    }
  }

  if (num_argv) {
    argv = sl_alloc_n(c8*, num_argv);
    if (!argv) {
      msg = "failed to allocate spawn args";
      goto done;
    }
  }

  sl_for(it, num_argv) {
    napi_value value = SL_ZERO;
    if (napi_get_element(env, args[2], it, &value) != napi_ok) {
      msg = "spawn args must be strings";
      goto done;
    }

    c8* arg = SL_ZERO;
    if (sl_napi_copy_str(env, value, &arg)) {
      msg = "spawn args must be strings";
      goto done;
    }

    argv[it] = arg;
    num_filled++;
  }

  sl_err_t err = sb_spawn(sb, cmd, (const c8* const*)argv, num_argv, NULL);
  if (err) {
    msg = sl_err_to_string(err);
    goto done;
  }

  if (napi_get_undefined(env, &out) != napi_ok) {
    out = NULL;
  }

done:
  sl_for(it, num_filled) { sl_free(argv[it]); }
  sl_free(argv);
  sl_free(cmd);

  if (msg) {
    napi_throw_error(env, NULL, msg);
    return NULL;
  }

  return out;
}

/* --- simple getters ----------------------------------------------------- */

static napi_value n_pid(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  n_sb_handle_t* h = n_get_handle(env, argv[0]);
  if (!h) return NULL;
  napi_value result;
  NAPI_CALL(napi_create_int32(env, sb_pid(h->sb), &result));
  return result;
}

static napi_value n_stdin_fd(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  n_sb_handle_t* h = n_get_handle(env, argv[0]);
  if (!h) return NULL;
  napi_value result;
  NAPI_CALL(napi_create_int32(env, sb_stdin_fd(h->sb), &result));
  return result;
}

static napi_value n_stdout_fd(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  n_sb_handle_t* h = n_get_handle(env, argv[0]);
  if (!h) return NULL;
  napi_value result;
  NAPI_CALL(napi_create_int32(env, sb_stdout_fd(h->sb), &result));
  return result;
}

static napi_value n_stderr_fd(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  n_sb_handle_t* h = n_get_handle(env, argv[0]);
  if (!h) return NULL;
  napi_value result;
  NAPI_CALL(napi_create_int32(env, sb_stderr_fd(h->sb), &result));
  return result;
}

/* --- wait(handle) ------------------------------------------------------- */

static napi_value n_wait(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  n_sb_handle_t* h = n_get_handle(env, argv[0]);
  if (!h) return NULL;
  int code = sb_wait(h->sb);
  if (code < 0) {
    napi_throw_error(env, NULL, sb_error(h->sb));
    return NULL;
  }
  napi_value result;
  NAPI_CALL(napi_create_int32(env, code, &result));
  return result;
}

/* --- kill(handle, signal) ----------------------------------------------- */

static napi_value n_kill(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2];
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  n_sb_handle_t* h = n_get_handle(env, argv[0]);
  if (!h) return NULL;
  int sig;
  NAPI_CALL(napi_get_value_int32(env, argv[1], &sig));
  if (sb_kill(h->sb, sig) != 0) {
    napi_throw_error(env, NULL, sb_error(h->sb));
    return NULL;
  }
  napi_value undef;
  napi_get_undefined(env, &undef);
  return undef;
}

/* --- destroy(handle) ---------------------------------------------------- */

static napi_value n_destroy(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  n_sb_handle_t* h = NULL;
  NAPI_CALL(napi_get_value_external(env, argv[0], (void**)&h));

  if (h && h->sb) {
    sb_destroy(h->sb);
    h->sb = NULL;
  }

  napi_value undef;
  napi_get_undefined(env, &undef);
  return undef;
}

/* --- module init -------------------------------------------------------- */

#define EXPORT_FN(name, fn)  \
  do {  \
    napi_value _fn;  \
    napi_create_function(env, name, NAPI_AUTO_LENGTH, fn, NULL, &_fn);  \
    napi_set_named_property(env, exports, name, _fn);  \
  } while (0)

static napi_value sb_napi_init(napi_env env, napi_value exports) {
  EXPORT_FN("create", sl_napi_create);
  EXPORT_FN("spawn", n_spawn);
  EXPORT_FN("pid", n_pid);
  EXPORT_FN("wait", n_wait);
  EXPORT_FN("kill", n_kill);
  EXPORT_FN("destroy", n_destroy);
  EXPORT_FN("stdinFd", n_stdin_fd);
  EXPORT_FN("stdoutFd", n_stdout_fd);
  EXPORT_FN("stderrFd", n_stderr_fd);
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, sb_napi_init)
