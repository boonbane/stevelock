#define SP_IMPLEMENTATION
#include "sl.h"

UTEST_F(stevelock, process_io) {
  sp_str_t cmd = sl_test_testbox_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);

  const c8* args_echo[] = {
    "echo",
    "ready",
  };

  const c8* args_cat[] = {
    "cat",
  };

  sl_test_io_case_t cases[] = {
    {
      .state = {
        .network = 0,
      },
      .process = {
        .cmd = cmd_cstr.data,
        .args = args_cat,
        .num_args = SP_CARR_LEN(args_cat),
        .stdin_data = SP_LIT("hello-input"),
        .close_stdin = true,
      },
      .wait = 0,
      .out = "hello-input",
      .err = "",
    },
    {
      .state = {
        .network = 0,
      },
      .process = {
        .cmd = cmd_cstr.data,
        .args = args_echo,
        .num_args = SP_CARR_LEN(args_echo),
      },
      .wait = 0,
      .out = "ready",
      .err = "",
    },
  };

  sl_test_run_io_cases(utest_result, cases, SP_CARR_LEN(cases));
}

UTEST_F(stevelock, process_environment) {
  sp_str_t cmd = sl_test_testbox_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);

  const c8* env[] = {
    "SL_ONLY=env-value",
    SL_NULLPTR,
  };

  const c8* args[] = {
    "print-env",
    "--key",
    "SL_ONLY",
  };

  sl_test_env_case_t cases[] = {
    {
      .state = {
        .network = 0,
      },
      .process = {
        .cmd = cmd_cstr.data,
        .args = args,
        .num_args = SP_CARR_LEN(args),
        .env = env,
      },
      .wait = 0,
      .out_contains = "SL_ONLY=env-value",
    },
  };

  sl_test_run_env_cases(utest_result, cases, SP_CARR_LEN(cases));
}

UTEST_F(stevelock, process_args) {
  sp_str_t cmd = sl_test_testbox_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);

  const c8* args[] = {
    "echo",
    "--sep",
    "|",
    "alpha",
    "beta",
    "gamma",
  };

  sl_test_args_case_t cases[] = {
    {
      .state = {
        .network = 0,
      },
      .process = {
        .cmd = cmd_cstr.data,
        .args = args,
        .num_args = SP_CARR_LEN(args),
      },
      .wait = 0,
      .out = "alpha|beta|gamma",
    },
  };

  sl_test_run_args_cases(utest_result, cases, SP_CARR_LEN(cases));
}

UTEST_MAIN();
