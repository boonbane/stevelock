#define SP_IMPLEMENTATION
#include "sl.h"

UTEST_F(stevelock, wait_behavior) {
  sp_str_t cmd = sl_test_testbox_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);
  const c8* args_ok[] = {
    "status",
    "--code",
    "0",
  };
  const c8* args_fail[] = {
    "status",
    "--code",
    "1",
  };

  sl_test_wait_case_t cases[] = {
    {
      .state = {
        .network = 0,
      },
      .process = {
        .cmd = cmd_cstr.data,
        .args = args_ok,
        .num_args = SP_CARR_LEN(args_ok),
      },
      .wait = 0,
    },
    {
      .state = {
        .network = 0,
      },
      .process = {
        .cmd = cmd_cstr.data,
        .args = args_fail,
        .num_args = SP_CARR_LEN(args_fail),
      },
      .wait = 1,
    },
  };

  sl_test_run_wait_cases(utest_result, cases, SP_CARR_LEN(cases));
}

UTEST_F(stevelock, kill_behavior) {
  sp_str_t cmd = sl_test_testbox_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);

  const c8* args[] = {
    "sleep",
  };

  sl_test_kill_case_t cases[] = {
    {
      .state = {
        .network = 0,
      },
      .process = {
        .cmd = cmd_cstr.data,
        .args = args,
        .num_args = SP_CARR_LEN(args),
      },
      .signal = SIGKILL,
      .kill = 0,
      .wait = 128 + SIGKILL,
    },
  };

  sl_test_run_kill_cases(utest_result, cases, SP_CARR_LEN(cases));
}

UTEST_F(stevelock, api_validation) {
  sp_str_t cmd = sl_test_testbox_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);

  const c8* args[] = {
    "status",
    "--code",
    "0",
  };

  EXPECT_TRUE(sb_create(SL_NULLPTR) == SL_NULLPTR);

  {
    sb_opts_t opts = {
      .write = {
        .dirs = SL_NULLPTR,
        .num_dirs = 1,
      },
    };
    EXPECT_TRUE(sb_create(&opts) == SL_NULLPTR);
  }

  {
    sb_opts_t opts = {
      .read = {
        .dirs = SL_NULLPTR,
        .num_dirs = 1,
      },
    };
    EXPECT_TRUE(sb_create(&opts) == SL_NULLPTR);
  }

  EXPECT_EQ(sb_spawn(SL_NULLPTR, "ignored", SL_NULLPTR, 0, SL_NULLPTR), SL_ERROR_INVALID_CONTEXT);

  {
    sb_opts_t opts = SL_ZERO;
    sl_ctx_t* sb = sb_create(&opts);
    ASSERT_TRUE(sb != SL_NULLPTR);

    EXPECT_EQ(sb_spawn(sb, SL_NULLPTR, SL_NULLPTR, 0, SL_NULLPTR), SL_ERROR_INVALID_COMMAND);
    EXPECT_EQ(sb_spawn(sb, cmd_cstr.data, SL_NULLPTR, 1, SL_NULLPTR), SL_ERROR_INVALID_COMMAND);

    sl_err_t ok = sb_spawn(sb, cmd_cstr.data, args, SP_CARR_LEN(args), SL_NULLPTR);
    EXPECT_EQ(ok, SL_OK);
    EXPECT_EQ(sb_wait(sb), 0);

    sb_destroy(sb);
  }
}

UTEST_F(stevelock, descriptor_semantics) {
  sp_str_t cmd = sl_test_testbox_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);

  sb_opts_t opts = SL_ZERO;
  sl_ctx_t* sb = sb_create(&opts);
  ASSERT_TRUE(sb != SL_NULLPTR);

  EXPECT_LT(sb_pid(sb), 0);
  EXPECT_LT(sb_stdin_fd(sb), 0);
  EXPECT_LT(sb_stdout_fd(sb), 0);
  EXPECT_LT(sb_stderr_fd(sb), 0);

  EXPECT_LT(sb_pid(SL_NULLPTR), 0);
  EXPECT_LT(sb_stdin_fd(SL_NULLPTR), 0);
  EXPECT_LT(sb_stdout_fd(SL_NULLPTR), 0);
  EXPECT_LT(sb_stderr_fd(SL_NULLPTR), 0);

  const c8* args[] = {
    "emit",
    "--stdout",
    "descriptor-check",
  };

  sl_err_t err = sb_spawn(sb, cmd_cstr.data, args, SP_CARR_LEN(args), SL_NULLPTR);
  ASSERT_EQ(err, SL_OK);

  EXPECT_GT(sb_pid(sb), 0);
  EXPECT_GE(sb_stdin_fd(sb), 0);
  EXPECT_GE(sb_stdout_fd(sb), 0);
  EXPECT_GE(sb_stderr_fd(sb), 0);

  EXPECT_EQ(sb_wait(sb), 0);

  EXPECT_GT(sb_pid(sb), 0);
  EXPECT_GE(sb_stdin_fd(sb), 0);
  EXPECT_GE(sb_stdout_fd(sb), 0);
  EXPECT_GE(sb_stderr_fd(sb), 0);

  sb_destroy(sb);
}

UTEST_MAIN();
