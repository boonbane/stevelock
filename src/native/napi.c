#include <node_api.h>
#include <stdlib.h>
#include <string.h>

#include "stevelock.h"

void n_finalize(napi_env env, void* ptr, void* hint) {

}

#define NAPI_CALL(call)                                                        \
    do {                                                                        \
        napi_status _s = (call);                                               \
        if (_s != napi_ok) {                                                   \
            napi_throw_error(env, NULL, #call " failed");                      \
            return NULL;                                                        \
        }                                                                      \
    } while (0)

static napi_value n_create(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value argv[1];
    NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));

    if (argc < 1) {
        napi_throw_error(env, NULL, "create requires an options object");
        return NULL;
    }

    napi_value opts_obj = argv[0];

    /* opts.writable (required string[]) */
    const char **writable = NULL;
    int writable_count = 0;
    napi_value v_writable;
    NAPI_CALL(napi_get_named_property(env, opts_obj, "writable", &v_writable));
    {
        bool is_arr = false;
        napi_is_array(env, v_writable, &is_arr);
        if (!is_arr) {
            napi_throw_error(env, NULL, "opts.writable must be an array");
            return NULL;
        }
        uint32_t len;
        napi_get_array_length(env, v_writable, &len);
        if (len == 0) {
            napi_throw_error(env, NULL, "opts.writable must have at least one path");
            return NULL;
        }
        writable_count = (int)len;
        writable = calloc(len, sizeof(char *));
        for (uint32_t i = 0; i < len; i++) {
            napi_value el;
            napi_get_element(env, v_writable, i, &el);
            char *s = malloc(1024);
            size_t slen;
            napi_get_value_string_utf8(env, el, s, 1024, &slen);
            writable[i] = s;
        }
    }

    /* opts.allowNet (optional, default false) */
    int allow_net = 0;
    napi_value v_net;
    if (napi_get_named_property(env, opts_obj, "allowNet", &v_net) == napi_ok) {
        napi_get_value_bool(env, v_net, (bool *)&allow_net);
    }

    /* opts.readable (optional string[]) */
    const char **readable = NULL;
    int readable_count = 0;
    napi_value v_readable;
    if (napi_get_named_property(env, opts_obj, "readable", &v_readable) == napi_ok) {
        bool is_arr = false;
        napi_is_array(env, v_readable, &is_arr);
        if (is_arr) {
            uint32_t len;
            napi_get_array_length(env, v_readable, &len);
            readable_count = (int)len;
            readable = calloc(len, sizeof(char *));
            for (uint32_t i = 0; i < len; i++) {
                napi_value el;
                napi_get_element(env, v_readable, i, &el);
                char *s = malloc(1024);
                size_t slen;
                napi_get_value_string_utf8(env, el, s, 1024, &slen);
                readable[i] = s;
            }
        }
    }

    sb_opts_t opts = {
        .writable = writable,
        .writable_count = writable_count,
        .allow_net = allow_net,
        .readable = readable,
        .readable_count = readable_count,
    };

    sb_t *sb = sb_create(&opts);

    /* free writable copies */
    for (int i = 0; i < writable_count; i++) free((void *)writable[i]);
    free(writable);
    /* free readable copies */
    for (int i = 0; i < readable_count; i++) free((void *)readable[i]);
    free(readable);

    if (!sb) {
        napi_throw_error(env, NULL, "sb_create failed");
        return NULL;
    }

    napi_value external;
    NAPI_CALL(napi_create_external(env, sb, n_finalize, NULL, &external));
    return external;
}

/* --- spawn(handle, cmd, args) ------------------------------------------- */

static napi_value n_spawn(napi_env env, napi_callback_info info) {
    size_t argc = 3;
    napi_value argv[3];
    NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));

    sb_t *sb;
    NAPI_CALL(napi_get_value_external(env, argv[0], (void **)&sb));

    /* cmd */
    char cmd[1024];
    size_t cmd_len;
    NAPI_CALL(napi_get_value_string_utf8(env, argv[1], cmd, sizeof(cmd), &cmd_len));

    /* args array */
    uint32_t args_len = 0;
    bool is_arr = false;
    napi_is_array(env, argv[2], &is_arr);
    if (is_arr) napi_get_array_length(env, argv[2], &args_len);

    /* build argv: [cmd, ...args, NULL] */
    const char **c_argv = calloc(args_len + 2, sizeof(char *));
    c_argv[0] = cmd;
    for (uint32_t i = 0; i < args_len; i++) {
        napi_value el;
        napi_get_element(env, argv[2], i, &el);
        char *s = malloc(4096);
        size_t slen;
        napi_get_value_string_utf8(env, el, s, 4096, &slen);
        c_argv[i + 1] = s;
    }
    c_argv[args_len + 1] = NULL;

    int ret = sb_spawn(sb, cmd, c_argv, NULL);

    for (uint32_t i = 0; i < args_len; i++) free((void *)c_argv[i + 1]);
    free(c_argv);

    if (ret != 0) {
        const char *err = sb_error(sb);
        napi_throw_error(env, NULL, err ? err : "sb_spawn failed");
        return NULL;
    }

    napi_value undef;
    napi_get_undefined(env, &undef);
    return undef;
}

/* --- simple getters ----------------------------------------------------- */

static napi_value n_pid(napi_env env, napi_callback_info info) {
    size_t argc = 1; napi_value argv[1];
    NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    sb_t *sb; NAPI_CALL(napi_get_value_external(env, argv[0], (void **)&sb));
    napi_value result;
    NAPI_CALL(napi_create_int32(env, sb_pid(sb), &result));
    return result;
}

static napi_value n_stdin_fd(napi_env env, napi_callback_info info) {
    size_t argc = 1; napi_value argv[1];
    NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    sb_t *sb; NAPI_CALL(napi_get_value_external(env, argv[0], (void **)&sb));
    napi_value result;
    NAPI_CALL(napi_create_int32(env, sb_stdin_fd(sb), &result));
    return result;
}

static napi_value n_stdout_fd(napi_env env, napi_callback_info info) {
    size_t argc = 1; napi_value argv[1];
    NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    sb_t *sb; NAPI_CALL(napi_get_value_external(env, argv[0], (void **)&sb));
    napi_value result;
    NAPI_CALL(napi_create_int32(env, sb_stdout_fd(sb), &result));
    return result;
}

static napi_value n_stderr_fd(napi_env env, napi_callback_info info) {
    size_t argc = 1; napi_value argv[1];
    NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    sb_t *sb; NAPI_CALL(napi_get_value_external(env, argv[0], (void **)&sb));
    napi_value result;
    NAPI_CALL(napi_create_int32(env, sb_stderr_fd(sb), &result));
    return result;
}

/* --- wait(handle) ------------------------------------------------------- */

static napi_value n_wait(napi_env env, napi_callback_info info) {
    size_t argc = 1; napi_value argv[1];
    NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    sb_t *sb; NAPI_CALL(napi_get_value_external(env, argv[0], (void **)&sb));
    int code = sb_wait(sb);
    if (code < 0) {
        napi_throw_error(env, NULL, sb_error(sb));
        return NULL;
    }
    napi_value result;
    NAPI_CALL(napi_create_int32(env, code, &result));
    return result;
}

/* --- kill(handle, signal) ----------------------------------------------- */

static napi_value n_kill(napi_env env, napi_callback_info info) {
    size_t argc = 2; napi_value argv[2];
    NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    sb_t *sb; NAPI_CALL(napi_get_value_external(env, argv[0], (void **)&sb));
    int sig; NAPI_CALL(napi_get_value_int32(env, argv[1], &sig));
    if (sb_kill(sb, sig) != 0) {
        napi_throw_error(env, NULL, sb_error(sb));
        return NULL;
    }
    napi_value undef; napi_get_undefined(env, &undef);
    return undef;
}

/* --- destroy(handle) ---------------------------------------------------- */

static napi_value n_destroy(napi_env env, napi_callback_info info) {
    size_t argc = 1; napi_value argv[1];
    NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
    sb_t *sb; NAPI_CALL(napi_get_value_external(env, argv[0], (void **)&sb));

    sb_destroy(sb);

    napi_value undef; napi_get_undefined(env, &undef);
    return undef;
}

/* --- module init -------------------------------------------------------- */

#define EXPORT_FN(name, fn)                                                    \
    do {                                                                        \
        napi_value _fn;                                                        \
        napi_create_function(env, name, NAPI_AUTO_LENGTH, fn, NULL, &_fn);     \
        napi_set_named_property(env, exports, name, _fn);                      \
    } while (0)

static napi_value sb_napi_init(napi_env env, napi_value exports) {
    EXPORT_FN("create", n_create);
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
