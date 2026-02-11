#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef SL_TEST_SL_H
#define SL_TEST_SL_H

#include "sp.h"
#include "utest.h"
#include "stevelock.h"

#include <signal.h>

#define ut (*utest_fixture)
#define ur (*utest_result)

#define SP_TEST_REPORT(fmt, ...) \
  do { \
    sp_str_t formatted = sp_format_str(fmt, ##__VA_ARGS__); \
    UTEST_PRINTF("%s\n", sp_str_to_cstr(formatted)); \
  } while (0)

#define SP_TEST_STREQ(a, b, is_assert) \
  UTEST_SURPRESS_WARNING_BEGIN do { \
    if (!sp_str_equal((a), (b))) { \
      const c8* __sp_test_file_lval = __FILE__; \
      const u32 __sp_test_line_lval = __LINE__; \
      sp_str_builder_t __sp_test_builder = SP_ZERO_INITIALIZE(); \
      sp_str_builder_append_fmt_str(&__sp_test_builder, SP_LIT("{}:{} Failure:"), SP_FMT_CSTR(__sp_test_file_lval), SP_FMT_U32(__sp_test_line_lval)); \
      sp_str_builder_new_line(&__sp_test_builder); \
      sp_str_builder_indent(&__sp_test_builder); \
      sp_str_builder_append_fmt_str(&__sp_test_builder, SP_LIT("{} != {}"), SP_FMT_QUOTED_STR((a)), SP_FMT_QUOTED_STR((b))); \
      SP_TEST_REPORT(sp_str_builder_to_str(&__sp_test_builder)); \
      *utest_result = UTEST_TEST_FAILURE; \
      if (is_assert) { \
        return; \
      } \
    } \
  } while (0) \
  UTEST_SURPRESS_WARNING_END

#define SP_EXPECT_STR_EQ_CSTR(a, b) SP_TEST_STREQ((a), SP_CSTR(b), false)
#define SP_EXPECT_STR_EQ(a, b) SP_TEST_STREQ((a), (b), false)
#define SP_EXPECT_ERR(err) EXPECT_EQ(sp_err_get(), err)

typedef struct {
  const c8* const* read_dirs;
  u32 num_read_dirs;
  const c8* const* write_dirs;
  u32 num_write_dirs;
  u32 network;
} sl_test_state_t;

typedef struct {
  const c8* cmd;
  const c8* const* args;
  u32 num_args;
  sl_env_t env;
  sp_str_t stdin_data;
  bool close_stdin;
} sl_test_process_t;

typedef struct {
  sl_test_state_t state;
  sl_test_process_t process;
  bool kill_after_spawn;
  s32 kill_signal;
} sl_test_exec_t;

typedef struct {
  sl_err_t spawn;
  s32 kill;
  s32 wait;
  c8 out_buffer[4096];
  c8 err_buffer[4096];
} sl_test_exec_result_t;

typedef struct {
  sl_test_state_t state;
  sl_test_process_t process;
  s32 wait;
  const c8* out;
  const c8* err;
} sl_test_io_case_t;

typedef struct {
  sl_test_state_t state;
  sl_test_process_t process;
  s32 wait;
  const c8* out_contains;
} sl_test_env_case_t;

typedef struct {
  sl_test_state_t state;
  sl_test_process_t process;
  s32 wait;
  const c8* out;
} sl_test_args_case_t;

typedef struct {
  sl_test_state_t state;
  sl_test_process_t process;
  s32 wait;
} sl_test_wait_case_t;

typedef struct {
  sl_test_state_t state;
  sl_test_process_t process;
  s32 signal;
  s32 kill;
  s32 wait;
} sl_test_kill_case_t;

typedef enum {
  SL_EXPECT_ALLOW,
  SL_EXPECT_BLOCK,
} sl_test_expect_t;

typedef enum {
  SL_EXPECT_PATH_ANY,
  SL_EXPECT_PATH_EXISTS,
  SL_EXPECT_PATH_MISSING,
} sl_test_path_expect_t;

typedef struct {
  const c8* path;
  const c8* content;
  sl_test_expect_t expect;
  sl_test_path_expect_t path_expect;
  const c8* expected_content;
} sl_test_write_op_t;

typedef struct {
  const c8* path;
  const c8* content;
} sl_test_file_fixture_t;

typedef struct {
  const c8* path;
  const c8* target;
} sl_test_symlink_fixture_t;

typedef struct {
  const c8* name;
  struct {
    const c8* read_dirs[8];
    u32 num_read_dirs;
    const c8* write_dirs[8];
    u32 num_write_dirs;
    u32 network;
  } state;
  struct {
    const c8* dirs[8];
    sl_test_file_fixture_t files[8];
    sl_test_symlink_fixture_t symlinks[8];
  } fs;
  struct {
    sl_test_write_op_t write[8];
  } ops;
} sl_test_sandbox_case_t;

typedef struct stevelock {
  sp_mem_arena_t* arena;
} stevelock;

UTEST_F_SETUP(stevelock) {
  utest_fixture->arena = sp_mem_arena_new(64 * 1024);
  if (!utest_fixture->arena) {
    *utest_result = UTEST_TEST_FAILURE;
    return;
  }
  sp_context_push_arena(utest_fixture->arena);
}

UTEST_F_TEARDOWN(stevelock) {
  if (!utest_fixture->arena) {
    return;
  }
  sp_context_pop();
  sp_mem_arena_destroy(utest_fixture->arena);
}

static sb_opts_t sl_test_state_to_opts(sl_test_state_t state) {
  return (sb_opts_t){
    .read = {
      .dirs = (c8**)state.read_dirs,
      .num_dirs = state.num_read_dirs,
    },
    .write = {
      .dirs = (c8**)state.write_dirs,
      .num_dirs = state.num_write_dirs,
    },
    .network = state.network,
  };
}

static sp_str_t sl_test_read_fd(s32 fd, c8* buffer, u32 capacity) {
  u32 used = 0;

  if (!capacity) {
    return (sp_str_t)SL_ZERO;
  }

  for (;;) {
    u64 remaining = (u64)(capacity - 1 - used);
    if (!remaining) {
      break;
    }

    s64 n = sp_read(fd, buffer + used, remaining);
    if (n <= 0) {
      break;
    }
    used += (u32)n;
  }

  buffer[used] = 0;
  return sp_str(buffer, used);
}

static sl_test_exec_result_t sl_test_run_exec(sl_test_exec_t spec) {
  sp_err_clear();

  sl_test_exec_result_t result = {
    .spawn = SL_ERROR,
    .kill = 0,
    .wait = -1,
  };

  sb_opts_t opts = sl_test_state_to_opts(spec.state);
  sl_ctx_t* sb = sb_create(&opts);
  if (!sb) {
    return result;
  }

  result.spawn = sb_spawn(sb, spec.process.cmd, spec.process.args, spec.process.num_args, spec.process.env);
  if (result.spawn) {
    sb_destroy(sb);
    return result;
  }

  if (!sp_str_empty(spec.process.stdin_data)) {
    s32 fd = sb_stdin_fd(sb);
    const c8* data = spec.process.stdin_data.data;
    u32 remaining = spec.process.stdin_data.len;

    while (remaining > 0) {
      s64 n = sp_write(fd, data, remaining);
      if (n <= 0) {
        break;
      }
      data += n;
      remaining -= (u32)n;
    }

    if (spec.process.close_stdin) {
      sp_close(fd);
    }
  }

  if (sp_str_empty(spec.process.stdin_data) && spec.process.close_stdin) {
    sp_close(sb_stdin_fd(sb));
  }

  if (spec.kill_after_spawn) {
    sp_os_sleep_ms(20);
    result.kill = sb_kill(sb, spec.kill_signal);
  }

  result.wait = sb_wait(sb);
  sl_test_read_fd(sb_stdout_fd(sb), result.out_buffer, SP_CARR_LEN(result.out_buffer));
  sl_test_read_fd(sb_stderr_fd(sb), result.err_buffer, SP_CARR_LEN(result.err_buffer));
  sb_destroy(sb);
  return result;
}

static void sl_test_run_io_cases(int* utest_result, const sl_test_io_case_t* cases, u32 num_cases) {
  sl_for(it, num_cases) {
    sl_test_exec_result_t result = sl_test_run_exec((sl_test_exec_t){
      .state = cases[it].state,
      .process = cases[it].process,
    });

    EXPECT_EQ(result.spawn, SL_OK);
    EXPECT_EQ(result.wait, cases[it].wait);
    EXPECT_TRUE(sp_str_equal_cstr(SP_CSTR(result.out_buffer), cases[it].out));
    EXPECT_TRUE(sp_str_equal_cstr(SP_CSTR(result.err_buffer), cases[it].err));
  }
}

static void sl_test_run_env_cases(int* utest_result, const sl_test_env_case_t* cases, u32 num_cases) {
  sl_for(it, num_cases) {
    sl_test_exec_result_t result = sl_test_run_exec((sl_test_exec_t){
      .state = cases[it].state,
      .process = cases[it].process,
    });

    EXPECT_EQ(result.spawn, SL_OK);
    EXPECT_EQ(result.wait, cases[it].wait);
    EXPECT_TRUE(sp_str_contains(SP_CSTR(result.out_buffer), SP_CSTR(cases[it].out_contains)));
  }
}

static void sl_test_run_args_cases(int* utest_result, const sl_test_args_case_t* cases, u32 num_cases) {
  sl_for(it, num_cases) {
    sl_test_exec_result_t result = sl_test_run_exec((sl_test_exec_t){
      .state = cases[it].state,
      .process = cases[it].process,
    });

    EXPECT_EQ(result.spawn, SL_OK);
    EXPECT_EQ(result.wait, cases[it].wait);
    EXPECT_TRUE(sp_str_equal_cstr(SP_CSTR(result.out_buffer), cases[it].out));
  }
}

static void sl_test_run_wait_cases(int* utest_result, const sl_test_wait_case_t* cases, u32 num_cases) {
  sl_for(it, num_cases) {
    sb_opts_t opts = sl_test_state_to_opts(cases[it].state);
    sl_ctx_t* sb = sb_create(&opts);
    ASSERT_TRUE(sb != SL_NULLPTR);

    sl_err_t err = sb_spawn(sb, cases[it].process.cmd, cases[it].process.args, cases[it].process.num_args, cases[it].process.env);
    ASSERT_EQ(err, SL_OK);

    s32 first = sb_wait(sb);
    s32 second = sb_wait(sb);

    EXPECT_EQ(first, cases[it].wait);
    EXPECT_EQ(second, cases[it].wait);

    sb_destroy(sb);
  }
}

static void sl_test_run_kill_cases(int* utest_result, const sl_test_kill_case_t* cases, u32 num_cases) {
  sl_for(it, num_cases) {
    sl_test_exec_result_t result = sl_test_run_exec((sl_test_exec_t){
      .state = cases[it].state,
      .process = cases[it].process,
      .kill_after_spawn = true,
      .kill_signal = cases[it].signal,
    });

    EXPECT_EQ(result.spawn, SL_OK);
    EXPECT_EQ(result.kill, cases[it].kill);
    EXPECT_EQ(result.wait, cases[it].wait);
  }
}

static u32 sl_test_cstr_list_len(const c8* const* items, u32 max) {
  u32 len = 0;
  while (len < max && items[len]) {
    len++;
  }
  return len;
}

static u32 sl_test_file_fixture_len(const sl_test_file_fixture_t* items, u32 max) {
  u32 len = 0;
  while (len < max && items[len].path) {
    len++;
  }
  return len;
}

static u32 sl_test_symlink_fixture_len(const sl_test_symlink_fixture_t* items, u32 max) {
  u32 len = 0;
  while (len < max && items[len].path) {
    len++;
  }
  return len;
}

static bool sl_test_path_is_absolute(const c8* path) {
  if (!path) {
    return false;
  }
  return path[0] == '/';
}

static sp_str_t sl_test_case_path(sp_str_t root, const c8* path) {
  if (sl_test_path_is_absolute(path)) {
    return SP_CSTR(path);
  }
  return sp_fs_join_path(root, SP_CSTR(path));
}

static sp_str_t sl_test_net_probe_path() {
  sp_str_t dir = sp_fs_get_exe_path();
  return sp_fs_join_path(dir, SP_LIT("stevelock_net_probe"));
}

static sp_str_t sl_test_testbox_path() {
  sp_str_t dir = sp_fs_get_exe_path();
  return sp_fs_join_path(dir, SP_LIT("stevelock_testbox"));
}

static u32 sl_test_write_op_len(const sl_test_write_op_t* items, u32 max) {
  u32 len = 0;
  while (len < max && items[len].path) {
    len++;
  }
  return len;
}

static void sl_test_run_sandbox_case(int* utest_result, const sl_test_sandbox_case_t* spec) {
  sp_str_t suite_root = SP_LIT("/tmp/stevelock");
  sp_fs_create_dir(suite_root);

  c8 root_template[256] = SL_ZERO;
  const c8* name = spec->name;
  if (!name) {
    name = "sandbox_case";
  }

  s32 template_len = snprintf(root_template, SP_CARR_LEN(root_template), "/tmp/stevelock/%s-XXXXXX", name);
  ASSERT_TRUE(template_len > 0);
  ASSERT_TRUE(template_len < (s32)SP_CARR_LEN(root_template));

  c8* root_dir = mkdtemp(root_template);
  ASSERT_TRUE(root_dir != SL_NULLPTR);

  sp_str_t root = SP_CSTR(root_dir);

  u32 fs_dir_count = sl_test_cstr_list_len(spec->fs.dirs, SP_CARR_LEN(spec->fs.dirs));
  sl_for(it, fs_dir_count) {
    sp_str_t rel = SP_CSTR(spec->fs.dirs[it]);
    sp_str_t abs = sp_fs_join_path(root, rel);
    sp_fs_create_dir(abs);
  }

  u32 fs_file_count = sl_test_file_fixture_len(spec->fs.files, SP_CARR_LEN(spec->fs.files));
  sl_for(it, fs_file_count) {
    sp_str_t abs = sl_test_case_path(root, spec->fs.files[it].path);
    sp_str_t parent = sp_fs_parent_path(abs);
    sp_fs_create_dir(parent);

    sp_io_writer_t writer = sp_io_writer_from_file(abs, SP_IO_WRITE_MODE_OVERWRITE);
    if (spec->fs.files[it].content) {
      sp_io_write_cstr(&writer, spec->fs.files[it].content);
    }
    sp_io_writer_close(&writer);
  }

  u32 fs_link_count = sl_test_symlink_fixture_len(spec->fs.symlinks, SP_CARR_LEN(spec->fs.symlinks));
  sl_for(it, fs_link_count) {
    sp_str_t link_path = sl_test_case_path(root, spec->fs.symlinks[it].path);
    sp_str_t link_parent = sp_fs_parent_path(link_path);
    sp_fs_create_dir(link_parent);

    sp_str_t target = sl_test_case_path(root, spec->fs.symlinks[it].target);
    sp_err_t err = sp_fs_create_sym_link(target, link_path);
    EXPECT_EQ(err, SP_ERR_OK);
  }

  const c8* write_dirs_raw[8] = SL_ZERO;
  sp_str_t write_dirs_storage[8] = SL_ZERO;
  const c8* read_dirs_raw[8] = SL_ZERO;
  sp_str_t read_dirs_storage[8] = SL_ZERO;

  u32 read_dir_count = spec->state.num_read_dirs;
  sl_for(it, read_dir_count) {
    sp_str_t abs = sl_test_case_path(root, spec->state.read_dirs[it]);
    read_dirs_storage[it] = sp_str_null_terminate(abs);
    read_dirs_raw[it] = read_dirs_storage[it].data;
  }

  u32 write_dir_count = spec->state.num_write_dirs;
  sl_for(it, write_dir_count) {
    sp_str_t abs = sl_test_case_path(root, spec->state.write_dirs[it]);
    write_dirs_storage[it] = sp_str_null_terminate(abs);
    write_dirs_raw[it] = write_dirs_storage[it].data;
  }

  sl_test_state_t state = {
    .read_dirs = read_dirs_raw,
    .num_read_dirs = read_dir_count,
    .write_dirs = write_dirs_raw,
    .num_write_dirs = write_dir_count,
    .network = spec->state.network,
  };

  sp_str_t testbox = sl_test_testbox_path();
  sp_str_t testbox_cstr = sp_str_null_terminate(testbox);

  u32 write_op_count = sl_test_write_op_len(spec->ops.write, SP_CARR_LEN(spec->ops.write));
  sl_for(it, write_op_count) {
    sp_str_t abs = sl_test_case_path(root, spec->ops.write[it].path);
    sp_str_t abs_cstr = sp_str_null_terminate(abs);

    const c8* args[] = {
      "write-file",
      "--path",
      abs_cstr.data,
    };
    sl_test_exec_result_t result = sl_test_run_exec((sl_test_exec_t){
      .state = state,
      .process = {
        .cmd = testbox_cstr.data,
        .args = args,
        .num_args = SP_CARR_LEN(args),
        .stdin_data = spec->ops.write[it].content ? SP_CSTR(spec->ops.write[it].content) : SP_LIT(""),
        .close_stdin = true,
      },
    });

    EXPECT_EQ(result.spawn, SL_OK);

    if (spec->ops.write[it].expect == SL_EXPECT_ALLOW) {
      EXPECT_EQ(result.wait, 0);
    }
    if (spec->ops.write[it].expect == SL_EXPECT_BLOCK) {
      EXPECT_NE(result.wait, 0);
    }

    if (spec->ops.write[it].path_expect == SL_EXPECT_PATH_EXISTS) {
      EXPECT_TRUE(sp_fs_exists(abs));
    }
    if (spec->ops.write[it].path_expect == SL_EXPECT_PATH_MISSING) {
      EXPECT_FALSE(sp_fs_exists(abs));
    }

    if (spec->ops.write[it].expected_content) {
      sp_str_t content = sp_io_read_file(abs);
      EXPECT_TRUE(sp_str_equal_cstr(content, spec->ops.write[it].expected_content));
    }

    sp_free((void*)abs_cstr.data);
  }

  sl_for(it, write_dir_count) {
    sp_free((void*)write_dirs_storage[it].data);
  }

  sl_for(it, read_dir_count) {
    sp_free((void*)read_dirs_storage[it].data);
  }

  sp_fs_remove_dir(root);
}

#endif
