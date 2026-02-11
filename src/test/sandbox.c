#define SP_IMPLEMENTATION
#include "sl.h"

static sp_str_t sl_test_make_case_root(int* utest_result, const c8* name, c8* root_template, u32 root_template_size) {
  (void)utest_result;

  sp_str_t suite_root = SP_LIT("/tmp/stevelock");
  sp_fs_create_dir(suite_root);

  s32 template_len = snprintf(root_template, root_template_size, "/tmp/stevelock/%s-XXXXXX", name);
  if (template_len <= 0 || template_len >= (s32)root_template_size) {
    *utest_result = UTEST_TEST_FAILURE;
    return (sp_str_t)SL_ZERO;
  }

  c8* root_dir = mkdtemp(root_template);
  if (!root_dir) {
    *utest_result = UTEST_TEST_FAILURE;
    return (sp_str_t)SL_ZERO;
  }
  return SP_CSTR(root_dir);
}

static sl_test_exec_result_t sl_test_run_box_exec(sl_test_state_t state, const c8* const* args, u32 num_args, sp_str_t stdin_data) {
  sp_str_t cmd = sl_test_testbox_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);

  return sl_test_run_exec((sl_test_exec_t){
    .state = state,
    .process = {
      .cmd = cmd_cstr.data,
      .args = args,
      .num_args = num_args,
      .stdin_data = stdin_data,
      .close_stdin = true,
    },
  });
}

UTEST_F(stevelock, sandbox_write_restrictions) {
  sl_test_run_sandbox_case(utest_result, &(sl_test_sandbox_case_t){
    .name = "sandbox_write_restrictions",
    .state = {
      .write_dirs = { "sandbox/allow" },
      .num_write_dirs = 1,
      .network = 0,
    },
    .fs = {
      .dirs = {
        "sandbox/allow",
        "sandbox/block",
      }
    },
    .ops = {
      .write = {
        { "sandbox/allow/ok.txt", "content", SL_EXPECT_ALLOW, SL_EXPECT_PATH_EXISTS },
        { "sandbox/block/blocked.txt", "content", SL_EXPECT_BLOCK, SL_EXPECT_PATH_MISSING },
      }
    }
  });
}

UTEST_F(stevelock, sandbox_write_no_allowed_dirs) {
  sl_test_run_sandbox_case(utest_result, &(sl_test_sandbox_case_t){
    .name = "sandbox_write_no_allowed_dirs",
    .state = {
      .num_write_dirs = 0,
      .network = 0,
    },
    .fs = {
      .dirs = {
        "sandbox/block",
      }
    },
    .ops = {
      .write = {
        { "sandbox/block/blocked.txt", "content", SL_EXPECT_BLOCK, SL_EXPECT_PATH_MISSING },
      }
    }
  });
}

UTEST_F(stevelock, sandbox_write_multiple_allowed_dirs) {
  sl_test_run_sandbox_case(utest_result, &(sl_test_sandbox_case_t){
    .name = "sandbox_write_multiple_allowed_dirs",
    .state = {
      .write_dirs = {
        "sandbox/allow_a",
        "sandbox/allow_b",
      },
      .num_write_dirs = 2,
      .network = 0,
    },
    .fs = {
      .dirs = {
        "sandbox/allow_a",
        "sandbox/allow_b",
        "sandbox/block",
      }
    },
    .ops = {
      .write = {
        { "sandbox/allow_a/ok_a.txt", "a", SL_EXPECT_ALLOW, SL_EXPECT_PATH_EXISTS },
        { "sandbox/allow_b/ok_b.txt", "b", SL_EXPECT_ALLOW, SL_EXPECT_PATH_EXISTS },
        { "sandbox/block/blocked.txt", "x", SL_EXPECT_BLOCK, SL_EXPECT_PATH_MISSING },
      }
    }
  });
}

UTEST_F(stevelock, sandbox_write_existing_file_overwrite_allowed_blocked) {
  sl_test_run_sandbox_case(utest_result, &(sl_test_sandbox_case_t){
    .name = "sandbox_write_existing_file_overwrite_allowed_blocked",
    .state = {
      .write_dirs = { "sandbox/allow" },
      .num_write_dirs = 1,
      .network = 0,
    },
    .fs = {
      .dirs = {
        "sandbox/allow",
        "sandbox/block",
      },
      .files = {
        { "sandbox/allow/existing.txt", "old-allow" },
        { "sandbox/block/existing.txt", "old-block" },
      },
    },
    .ops = {
      .write = {
        {
          .path = "sandbox/allow/existing.txt",
          .content = "new-allow",
          .expect = SL_EXPECT_ALLOW,
          .path_expect = SL_EXPECT_PATH_EXISTS,
          .expected_content = "new-allow",
        },
        {
          .path = "sandbox/block/existing.txt",
          .content = "new-block",
          .expect = SL_EXPECT_BLOCK,
          .path_expect = SL_EXPECT_PATH_EXISTS,
          .expected_content = "old-block",
        },
      },
    },
  });
}

UTEST_F(stevelock, sandbox_write_nested_allowed) {
  sl_test_run_sandbox_case(utest_result, &(sl_test_sandbox_case_t){
    .name = "sandbox_write_nested_allowed",
    .state = {
      .write_dirs = { "sandbox/allow" },
      .num_write_dirs = 1,
      .network = 0,
    },
    .fs = {
      .dirs = {
        "sandbox/allow/a/b",
      },
    },
    .ops = {
      .write = {
        { "sandbox/allow/a/b/c.txt", "nested", SL_EXPECT_ALLOW, SL_EXPECT_PATH_EXISTS },
      },
    },
  });
}

UTEST_F(stevelock, sandbox_write_prefix_confusion) {
  sl_test_run_sandbox_case(utest_result, &(sl_test_sandbox_case_t){
    .name = "sandbox_write_prefix_confusion",
    .state = {
      .write_dirs = { "sandbox/allow" },
      .num_write_dirs = 1,
      .network = 0,
    },
    .fs = {
      .dirs = {
        "sandbox/allow",
        "sandbox/allow2",
      },
    },
    .ops = {
      .write = {
        { "sandbox/allow2/not-allowed.txt", "x", SL_EXPECT_BLOCK, SL_EXPECT_PATH_MISSING },
      },
    },
  });
}

UTEST_F(stevelock, sandbox_write_dotdot_escape) {
  sl_test_run_sandbox_case(utest_result, &(sl_test_sandbox_case_t){
    .name = "sandbox_write_dotdot_escape",
    .state = {
      .write_dirs = { "sandbox/allow" },
      .num_write_dirs = 1,
      .network = 0,
    },
    .fs = {
      .dirs = {
        "sandbox/allow",
        "sandbox/block",
      },
    },
    .ops = {
      .write = {
        { "sandbox/allow/../block/escape.txt", "x", SL_EXPECT_BLOCK, SL_EXPECT_PATH_MISSING },
      },
    },
  });
}

UTEST_F(stevelock, sandbox_write_symlink_escape) {
  sl_test_run_sandbox_case(utest_result, &(sl_test_sandbox_case_t){
    .name = "sandbox_write_symlink_escape",
    .state = {
      .write_dirs = { "sandbox/allow" },
      .num_write_dirs = 1,
      .network = 0,
    },
    .fs = {
      .dirs = {
        "sandbox/allow",
        "sandbox/block",
      },
      .symlinks = {
        { "sandbox/allow/link_out", "sandbox/block" },
      },
    },
    .ops = {
      .write = {
        { "sandbox/allow/link_out/escape.txt", "x", SL_EXPECT_BLOCK, SL_EXPECT_PATH_MISSING },
      },
    },
  });
}

UTEST_F(stevelock, sandbox_write_no_allowed_dirs_dev_null) {
  sl_test_run_sandbox_case(utest_result, &(sl_test_sandbox_case_t){
    .name = "sandbox_write_no_allowed_dirs_dev_null",
    .state = {
      .num_write_dirs = 0,
      .network = 0,
    },
    .ops = {
      .write = {
        { "/dev/null", "discard", SL_EXPECT_ALLOW, SL_EXPECT_PATH_ANY },
      },
    },
  });
}

UTEST_F(stevelock, sandbox_write_blocked_remove_file) {
  c8 root_template[256] = SL_ZERO;
  sp_str_t root = sl_test_make_case_root(utest_result, "sandbox_write_blocked_remove_file", root_template, SP_CARR_LEN(root_template));
  if (sp_str_empty(root)) {
    return;
  }

  sp_str_t allow_dir = sl_test_case_path(root, "sandbox/allow");
  sp_str_t block_dir = sl_test_case_path(root, "sandbox/block");
  sp_fs_create_dir(allow_dir);
  sp_fs_create_dir(block_dir);

  sp_str_t block_file = sl_test_case_path(root, "sandbox/block/remove.txt");
  sp_io_writer_t writer = sp_io_writer_from_file(block_file, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_cstr(&writer, "x");
  sp_io_writer_close(&writer);

  sp_str_t allow_dir_cstr = sp_str_null_terminate(allow_dir);
  const c8* write_dirs[] = {
    allow_dir_cstr.data,
  };

  sl_test_state_t state = {
    .write_dirs = write_dirs,
    .num_write_dirs = SP_CARR_LEN(write_dirs),
    .network = 0,
  };

  sp_str_t block_file_cstr = sp_str_null_terminate(block_file);
  const c8* args[] = {
    "remove-file",
    "--path",
    block_file_cstr.data,
  };

  sl_test_exec_result_t result = sl_test_run_box_exec(state, args, SP_CARR_LEN(args), SP_LIT(""));
  EXPECT_EQ(result.spawn, SL_OK);
  EXPECT_NE(result.wait, 0);
  EXPECT_TRUE(sp_fs_exists(block_file));

  sp_fs_remove_dir(root);
}

UTEST_F(stevelock, sandbox_write_blocked_remove_dir) {
  c8 root_template[256] = SL_ZERO;
  sp_str_t root = sl_test_make_case_root(utest_result, "sandbox_write_blocked_remove_dir", root_template, SP_CARR_LEN(root_template));
  if (sp_str_empty(root)) {
    return;
  }

  sp_str_t allow_dir = sl_test_case_path(root, "sandbox/allow");
  sp_str_t block_dir = sl_test_case_path(root, "sandbox/block/empty");
  sp_fs_create_dir(allow_dir);
  sp_fs_create_dir(block_dir);

  sp_str_t allow_dir_cstr = sp_str_null_terminate(allow_dir);
  const c8* write_dirs[] = {
    allow_dir_cstr.data,
  };

  sl_test_state_t state = {
    .write_dirs = write_dirs,
    .num_write_dirs = SP_CARR_LEN(write_dirs),
    .network = 0,
  };

  sp_str_t block_dir_cstr = sp_str_null_terminate(block_dir);
  const c8* args[] = {
    "remove-dir",
    "--path",
    block_dir_cstr.data,
  };

  sl_test_exec_result_t result = sl_test_run_box_exec(state, args, SP_CARR_LEN(args), SP_LIT(""));
  EXPECT_EQ(result.spawn, SL_OK);
  EXPECT_NE(result.wait, 0);
  EXPECT_TRUE(sp_fs_exists(block_dir));

  sp_fs_remove_dir(root);
}

UTEST_F(stevelock, sandbox_write_blocked_rename_out_of_allowed) {
  c8 root_template[256] = SL_ZERO;
  sp_str_t root = sl_test_make_case_root(utest_result, "sandbox_write_blocked_rename_out_of_allowed", root_template, SP_CARR_LEN(root_template));
  if (sp_str_empty(root)) {
    return;
  }

  sp_str_t allow_dir = sl_test_case_path(root, "sandbox/allow");
  sp_str_t block_dir = sl_test_case_path(root, "sandbox/block");
  sp_fs_create_dir(allow_dir);
  sp_fs_create_dir(block_dir);

  sp_str_t source = sl_test_case_path(root, "sandbox/allow/source.txt");
  sp_str_t target = sl_test_case_path(root, "sandbox/block/target.txt");

  sp_io_writer_t writer = sp_io_writer_from_file(source, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_cstr(&writer, "source");
  sp_io_writer_close(&writer);

  sp_str_t allow_dir_cstr = sp_str_null_terminate(allow_dir);
  const c8* write_dirs[] = {
    allow_dir_cstr.data,
  };

  sl_test_state_t state = {
    .write_dirs = write_dirs,
    .num_write_dirs = SP_CARR_LEN(write_dirs),
    .network = 0,
  };

  sp_str_t source_cstr = sp_str_null_terminate(source);
  sp_str_t target_cstr = sp_str_null_terminate(target);
  const c8* args[] = {
    "move-path",
    "--from",
    source_cstr.data,
    "--to",
    target_cstr.data,
  };

  sl_test_exec_result_t result = sl_test_run_box_exec(state, args, SP_CARR_LEN(args), SP_LIT(""));
  EXPECT_EQ(result.spawn, SL_OK);
  EXPECT_NE(result.wait, 0);
  EXPECT_TRUE(sp_fs_exists(source));
  EXPECT_FALSE(sp_fs_exists(target));

  sp_fs_remove_dir(root);
}

UTEST_F(stevelock, sandbox_write_blocked_rename_in_blocked) {
  c8 root_template[256] = SL_ZERO;
  sp_str_t root = sl_test_make_case_root(utest_result, "sandbox_write_blocked_rename_in_blocked", root_template, SP_CARR_LEN(root_template));
  if (sp_str_empty(root)) {
    return;
  }

  sp_str_t allow_dir = sl_test_case_path(root, "sandbox/allow");
  sp_str_t block_dir = sl_test_case_path(root, "sandbox/block");
  sp_fs_create_dir(allow_dir);
  sp_fs_create_dir(block_dir);

  sp_str_t source = sl_test_case_path(root, "sandbox/block/source.txt");
  sp_str_t target = sl_test_case_path(root, "sandbox/block/target.txt");

  sp_io_writer_t writer = sp_io_writer_from_file(source, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_cstr(&writer, "source");
  sp_io_writer_close(&writer);

  sp_str_t allow_dir_cstr = sp_str_null_terminate(allow_dir);
  const c8* write_dirs[] = {
    allow_dir_cstr.data,
  };

  sl_test_state_t state = {
    .write_dirs = write_dirs,
    .num_write_dirs = SP_CARR_LEN(write_dirs),
    .network = 0,
  };

  sp_str_t source_cstr = sp_str_null_terminate(source);
  sp_str_t target_cstr = sp_str_null_terminate(target);
  const c8* args[] = {
    "move-path",
    "--from",
    source_cstr.data,
    "--to",
    target_cstr.data,
  };

  sl_test_exec_result_t result = sl_test_run_box_exec(state, args, SP_CARR_LEN(args), SP_LIT(""));
  EXPECT_EQ(result.spawn, SL_OK);
  EXPECT_NE(result.wait, 0);
  EXPECT_TRUE(sp_fs_exists(source));
  EXPECT_FALSE(sp_fs_exists(target));

  sp_fs_remove_dir(root);
}

UTEST_F(stevelock, sandbox_write_blocked_hardlink_block_to_allow) {
  c8 root_template[256] = SL_ZERO;
  sp_str_t root = sl_test_make_case_root(utest_result, "sandbox_write_blocked_hardlink_block_to_allow", root_template, SP_CARR_LEN(root_template));
  if (sp_str_empty(root)) {
    return;
  }

  sp_str_t allow_dir = sl_test_case_path(root, "sandbox/allow");
  sp_str_t block_dir = sl_test_case_path(root, "sandbox/block");
  sp_fs_create_dir(allow_dir);
  sp_fs_create_dir(block_dir);

  sp_str_t source = sl_test_case_path(root, "sandbox/block/source.txt");
  sp_str_t link = sl_test_case_path(root, "sandbox/allow/link.txt");

  sp_io_writer_t writer = sp_io_writer_from_file(source, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_cstr(&writer, "source");
  sp_io_writer_close(&writer);

  sp_str_t allow_dir_cstr = sp_str_null_terminate(allow_dir);
  const c8* write_dirs[] = {
    allow_dir_cstr.data,
  };

  sl_test_state_t state = {
    .write_dirs = write_dirs,
    .num_write_dirs = SP_CARR_LEN(write_dirs),
    .network = 0,
  };

  sp_str_t source_cstr = sp_str_null_terminate(source);
  sp_str_t link_cstr = sp_str_null_terminate(link);
  const c8* args[] = {
    "hardlink",
    "--from",
    source_cstr.data,
    "--to",
    link_cstr.data,
  };

  sl_test_exec_result_t result = sl_test_run_box_exec(state, args, SP_CARR_LEN(args), SP_LIT(""));
  EXPECT_EQ(result.spawn, SL_OK);
  EXPECT_NE(result.wait, 0);
  EXPECT_TRUE(sp_fs_exists(source));
  EXPECT_FALSE(sp_fs_exists(link));

  sp_fs_remove_dir(root);
}

UTEST_F(stevelock, sandbox_read_global_allowed) {
  c8 root_template[256] = SL_ZERO;
  sp_str_t root = sl_test_make_case_root(utest_result, "sandbox_read_global_allowed", root_template, SP_CARR_LEN(root_template));
  if (sp_str_empty(root)) {
    return;
  }

  sp_str_t allow_dir = sl_test_case_path(root, "sandbox/allow");
  sp_str_t block_dir = sl_test_case_path(root, "sandbox/block");
  sp_fs_create_dir(allow_dir);
  sp_fs_create_dir(block_dir);

  sp_str_t read_path = sl_test_case_path(root, "sandbox/block/read.txt");
  sp_io_writer_t writer = sp_io_writer_from_file(read_path, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_cstr(&writer, "read-global");
  sp_io_writer_close(&writer);

  sp_str_t allow_dir_cstr = sp_str_null_terminate(allow_dir);
  const c8* write_dirs[] = {
    allow_dir_cstr.data,
  };

  sl_test_state_t state = {
    .write_dirs = write_dirs,
    .num_write_dirs = SP_CARR_LEN(write_dirs),
    .network = 0,
  };

  sp_str_t read_path_cstr = sp_str_null_terminate(read_path);
  const c8* args[] = {
    "read-file",
    "--path",
    read_path_cstr.data,
  };

  sl_test_exec_result_t result = sl_test_run_box_exec(state, args, SP_CARR_LEN(args), SP_LIT(""));
  EXPECT_EQ(result.spawn, SL_OK);
  EXPECT_EQ(result.wait, 0);
  EXPECT_TRUE(sp_str_equal_cstr(SP_CSTR(result.out_buffer), "read-global"));

  sp_fs_remove_dir(root);
}

UTEST_F(stevelock, sandbox_read_extra_dirs_dont_expand_write) {
  c8 root_template[256] = SL_ZERO;
  sp_str_t root = sl_test_make_case_root(utest_result, "sandbox_read_extra_dirs_dont_expand_write", root_template, SP_CARR_LEN(root_template));
  if (sp_str_empty(root)) {
    return;
  }

  sp_str_t allow_dir = sl_test_case_path(root, "sandbox/allow");
  sp_str_t block_dir = sl_test_case_path(root, "sandbox/block");
  sp_str_t read_dir = sl_test_case_path(root, "sandbox/read");
  sp_fs_create_dir(allow_dir);
  sp_fs_create_dir(block_dir);
  sp_fs_create_dir(read_dir);

  sp_str_t read_path = sl_test_case_path(root, "sandbox/read/value.txt");
  sp_io_writer_t writer = sp_io_writer_from_file(read_path, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_cstr(&writer, "read-extra");
  sp_io_writer_close(&writer);

  sp_str_t allow_dir_cstr = sp_str_null_terminate(allow_dir);
  sp_str_t read_dir_cstr = sp_str_null_terminate(read_dir);
  const c8* write_dirs[] = {
    allow_dir_cstr.data,
  };
  const c8* read_dirs[] = {
    read_dir_cstr.data,
  };

  sl_test_state_t state = {
    .read_dirs = read_dirs,
    .num_read_dirs = SP_CARR_LEN(read_dirs),
    .write_dirs = write_dirs,
    .num_write_dirs = SP_CARR_LEN(write_dirs),
    .network = 0,
  };

  sp_str_t read_path_cstr = sp_str_null_terminate(read_path);
  const c8* read_args[] = {
    "read-file",
    "--path",
    read_path_cstr.data,
  };

  sl_test_exec_result_t read_result = sl_test_run_box_exec(state, read_args, SP_CARR_LEN(read_args), SP_LIT(""));
  EXPECT_EQ(read_result.spawn, SL_OK);
  EXPECT_EQ(read_result.wait, 0);
  EXPECT_TRUE(sp_str_equal_cstr(SP_CSTR(read_result.out_buffer), "read-extra"));

  sp_str_t blocked_write = sl_test_case_path(root, "sandbox/block/blocked.txt");
  sp_str_t blocked_write_cstr = sp_str_null_terminate(blocked_write);
  const c8* write_args[] = {
    "write-file",
    "--path",
    blocked_write_cstr.data,
  };

  sl_test_exec_result_t write_result = sl_test_run_box_exec(state, write_args, SP_CARR_LEN(write_args), SP_LIT("x"));
  EXPECT_EQ(write_result.spawn, SL_OK);
  EXPECT_NE(write_result.wait, 0);
  EXPECT_FALSE(sp_fs_exists(blocked_write));

  sp_fs_remove_dir(root);
}

UTEST_F(stevelock, sandbox_spawn_nonexistent_write_dir) {
  sp_str_t suite_root = SP_LIT("/tmp/stevelock");
  sp_fs_create_dir(suite_root);

  c8 missing_write[256] = "/tmp/stevelock/missing-write-XXXXXX";
  c8* created = mkdtemp(missing_write);
  ASSERT_TRUE(created != SL_NULLPTR);
  sp_fs_remove_dir(SP_CSTR(created));

  const c8* write_dirs[] = {
    missing_write,
  };

  sb_opts_t opts = {
    .write = {
      .dirs = (c8**)write_dirs,
      .num_dirs = SP_CARR_LEN(write_dirs),
    },
  };

  sl_ctx_t* sb = sb_create(&opts);
  ASSERT_TRUE(sb != SL_NULLPTR);

  sp_str_t cmd = sl_test_testbox_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);
  const c8* args[] = {
    "status",
    "--code",
    "0",
  };

  sl_err_t err = sb_spawn(sb, cmd_cstr.data, args, SP_CARR_LEN(args), SL_NULLPTR);
  EXPECT_EQ(err, SL_ERROR_INVALID_SCOPE);
  sb_destroy(sb);
}

UTEST_F(stevelock, sandbox_spawn_nonexistent_read_dir) {
  sp_str_t suite_root = SP_LIT("/tmp/stevelock");
  sp_fs_create_dir(suite_root);

  c8 missing_read[256] = "/tmp/stevelock/missing-read-XXXXXX";
  c8* created = mkdtemp(missing_read);
  ASSERT_TRUE(created != SL_NULLPTR);
  sp_fs_remove_dir(SP_CSTR(created));

  const c8* read_dirs[] = {
    missing_read,
  };

  sb_opts_t opts = {
    .read = {
      .dirs = (c8**)read_dirs,
      .num_dirs = SP_CARR_LEN(read_dirs),
    },
  };

  sl_ctx_t* sb = sb_create(&opts);
  ASSERT_TRUE(sb != SL_NULLPTR);

  sp_str_t cmd = sl_test_testbox_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);
  const c8* args[] = {
    "status",
    "--code",
    "0",
  };

  sl_err_t err = sb_spawn(sb, cmd_cstr.data, args, SP_CARR_LEN(args), SL_NULLPTR);
  EXPECT_EQ(err, SL_ERROR_INVALID_SCOPE);
  sb_destroy(sb);
}

UTEST_F(stevelock, sandbox_network_denied_connect) {
  sp_str_t cmd = sl_test_net_probe_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);
  const c8* args[] = {
    "connect",
  };

  sl_test_exec_result_t result = sl_test_run_exec((sl_test_exec_t){
    .state = {
      .network = 0,
    },
    .process = {
      .cmd = cmd_cstr.data,
      .args = args,
      .num_args = SP_CARR_LEN(args),
    },
  });

  EXPECT_EQ(result.spawn, SL_OK);
  EXPECT_EQ(result.wait, 10);
}

UTEST_F(stevelock, sandbox_network_allowed_connect) {
  sp_str_t cmd = sl_test_net_probe_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);
  const c8* args[] = {
    "connect",
  };

  sl_test_exec_result_t result = sl_test_run_exec((sl_test_exec_t){
    .state = {
      .network = 1,
    },
    .process = {
      .cmd = cmd_cstr.data,
      .args = args,
      .num_args = SP_CARR_LEN(args),
    },
  });

  EXPECT_EQ(result.spawn, SL_OK);
  EXPECT_EQ(result.wait, 0);
}

UTEST_F(stevelock, sandbox_network_denied_bind) {
  sp_str_t cmd = sl_test_net_probe_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);
  const c8* args[] = {
    "bind",
  };

  sl_test_exec_result_t result = sl_test_run_exec((sl_test_exec_t){
    .state = {
      .network = 0,
    },
    .process = {
      .cmd = cmd_cstr.data,
      .args = args,
      .num_args = SP_CARR_LEN(args),
    },
  });

  EXPECT_EQ(result.spawn, SL_OK);
  EXPECT_EQ(result.wait, 10);
}

UTEST_F(stevelock, sandbox_network_allowed_bind) {
  sp_str_t cmd = sl_test_net_probe_path();
  sp_str_t cmd_cstr = sp_str_null_terminate(cmd);
  const c8* args[] = {
    "bind",
  };

  sl_test_exec_result_t result = sl_test_run_exec((sl_test_exec_t){
    .state = {
      .network = 1,
    },
    .process = {
      .cmd = cmd_cstr.data,
      .args = args,
      .num_args = SP_CARR_LEN(args),
    },
  });

  EXPECT_EQ(result.spawn, SL_OK);
  EXPECT_EQ(result.wait, 0);
}

UTEST_MAIN();
