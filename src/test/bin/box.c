#define ARGPARSE_IMPLEMENTATION
#include "argparse.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int testbox_status(int argc, const char** argv) {
  int code = 0;

  struct argparse_option options[] = {
    OPT_INTEGER(0, "code", &code, "exit code", NULL, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, 0);
  argparse_parse(&argparse, argc, argv);
  return code;
}

static int testbox_echo(int argc, const char** argv) {
  const char* sep = "";

  struct argparse_option options[] = {
    OPT_STRING(0, "sep", &sep, "separator", NULL, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, ARGPARSE_STOP_AT_NON_OPTION);
  int used = argparse_parse(&argparse, argc, argv);

  for (int i = 0; i < used; i++) {
    if (i > 0) {
      fwrite(sep, 1, strlen(sep), stdout);
    }
    fwrite(argparse.out[i], 1, strlen(argparse.out[i]), stdout);
  }

  return 0;
}

static int testbox_cat(void) {
  char buffer[4096] = {0};
  for (;;) {
    ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (n == 0) {
      return 0;
    }
    if (n < 0) {
      return 2;
    }

    const char* it = buffer;
    ssize_t rem = n;
    while (rem > 0) {
      ssize_t w = write(STDOUT_FILENO, it, (size_t)rem);
      if (w <= 0) {
        return 2;
      }
      it += w;
      rem -= w;
    }
  }
}

static int testbox_write_file(int argc, const char** argv) {
  const char* file_path = NULL;

  struct argparse_option options[] = {
    OPT_STRING('p', "path", &file_path, "file path", NULL, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, 0);
  argparse_parse(&argparse, argc, argv);

  if (!file_path) {
    return 3;
  }

  int fd = open(file_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (fd < 0) {
    return 4;
  }

  char buffer[4096] = {0};
  for (;;) {
    ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (n == 0) {
      close(fd);
      return 0;
    }
    if (n < 0) {
      close(fd);
      return 5;
    }

    const char* it = buffer;
    ssize_t rem = n;
    while (rem > 0) {
      ssize_t w = write(fd, it, (size_t)rem);
      if (w <= 0) {
        close(fd);
        return 6;
      }
      it += w;
      rem -= w;
    }
  }
}

static int testbox_print_env(int argc, const char** argv) {
  const char* key = NULL;

  struct argparse_option options[] = {
    OPT_STRING('k', "key", &key, "env key", NULL, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, 0);
  argparse_parse(&argparse, argc, argv);

  if (!key) {
    return 7;
  }

  const char* value = getenv(key);
  if (!value) {
    return 8;
  }

  fwrite(key, 1, strlen(key), stdout);
  fwrite("=", 1, 1, stdout);
  fwrite(value, 1, strlen(value), stdout);
  return 0;
}

static int testbox_read_file(int argc, const char** argv) {
  const char* file_path = NULL;

  struct argparse_option options[] = {
    OPT_STRING('p', "path", &file_path, "file path", NULL, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, 0);
  argparse_parse(&argparse, argc, argv);

  if (!file_path) {
    return 9;
  }

  int fd = open(file_path, O_RDONLY);
  if (fd < 0) {
    return 10;
  }

  char buffer[4096] = {0};
  for (;;) {
    ssize_t n = read(fd, buffer, sizeof(buffer));
    if (n == 0) {
      close(fd);
      return 0;
    }
    if (n < 0) {
      close(fd);
      return 11;
    }

    const char* it = buffer;
    ssize_t rem = n;
    while (rem > 0) {
      ssize_t w = write(STDOUT_FILENO, it, (size_t)rem);
      if (w <= 0) {
        close(fd);
        return 12;
      }
      it += w;
      rem -= w;
    }
  }
}

static int testbox_remove_file(int argc, const char** argv) {
  const char* file_path = NULL;

  struct argparse_option options[] = {
    OPT_STRING('p', "path", &file_path, "file path", NULL, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, 0);
  argparse_parse(&argparse, argc, argv);

  if (!file_path) {
    return 13;
  }
  if (unlink(file_path) == 0) {
    return 0;
  }
  return 14;
}

static int testbox_remove_dir(int argc, const char** argv) {
  const char* dir_path = NULL;

  struct argparse_option options[] = {
    OPT_STRING('p', "path", &dir_path, "dir path", NULL, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, 0);
  argparse_parse(&argparse, argc, argv);

  if (!dir_path) {
    return 15;
  }
  if (rmdir(dir_path) == 0) {
    return 0;
  }
  return 16;
}

static int testbox_move_path(int argc, const char** argv) {
  const char* from = NULL;
  const char* to = NULL;

  struct argparse_option options[] = {
    OPT_STRING('f', "from", &from, "from path", NULL, 0, 0),
    OPT_STRING('t', "to", &to, "to path", NULL, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, 0);
  argparse_parse(&argparse, argc, argv);

  if (!from || !to) {
    return 17;
  }
  if (rename(from, to) == 0) {
    return 0;
  }
  return 18;
}

static int testbox_hardlink(int argc, const char** argv) {
  const char* from = NULL;
  const char* to = NULL;

  struct argparse_option options[] = {
    OPT_STRING('f', "from", &from, "from path", NULL, 0, 0),
    OPT_STRING('t', "to", &to, "to path", NULL, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, 0);
  argparse_parse(&argparse, argc, argv);

  if (!from || !to) {
    return 19;
  }
  if (link(from, to) == 0) {
    return 0;
  }
  return 20;
}

static int testbox_sleep(int argc, const char** argv) {
  int ms = 0;

  struct argparse_option options[] = {
    OPT_INTEGER(0, "ms", &ms, "milliseconds", NULL, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, 0);
  argparse_parse(&argparse, argc, argv);

  if (ms <= 0) {
    for (;;) {
      sleep(1);
    }
  }

  usleep((useconds_t)(ms * 1000));
  return 0;
}

static int testbox_emit(int argc, const char** argv) {
  const char* out = NULL;
  const char* err = NULL;

  struct argparse_option options[] = {
    OPT_STRING(0, "stdout", &out, "stdout text", NULL, 0, 0),
    OPT_STRING(0, "stderr", &err, "stderr text", NULL, 0, 0),
    OPT_END(),
  };

  struct argparse argparse;
  argparse_init(&argparse, options, NULL, 0);
  argparse_parse(&argparse, argc, argv);

  if (out) {
    fwrite(out, 1, strlen(out), stdout);
  }
  if (err) {
    fwrite(err, 1, strlen(err), stderr);
  }
  return 0;
}

int main(int argc, const char** argv) {
  if (argc < 2) {
    return 1;
  }

  const char* cmd = argv[1];

  if (strcmp(cmd, "status") == 0) {
    return testbox_status(argc - 1, argv + 1);
  }
  if (strcmp(cmd, "echo") == 0) {
    return testbox_echo(argc - 1, argv + 1);
  }
  if (strcmp(cmd, "cat") == 0) {
    return testbox_cat();
  }
  if (strcmp(cmd, "write-file") == 0) {
    return testbox_write_file(argc - 1, argv + 1);
  }
  if (strcmp(cmd, "print-env") == 0) {
    return testbox_print_env(argc - 1, argv + 1);
  }
  if (strcmp(cmd, "read-file") == 0) {
    return testbox_read_file(argc - 1, argv + 1);
  }
  if (strcmp(cmd, "remove-file") == 0) {
    return testbox_remove_file(argc - 1, argv + 1);
  }
  if (strcmp(cmd, "remove-dir") == 0) {
    return testbox_remove_dir(argc - 1, argv + 1);
  }
  if (strcmp(cmd, "move-path") == 0) {
    return testbox_move_path(argc - 1, argv + 1);
  }
  if (strcmp(cmd, "hardlink") == 0) {
    return testbox_hardlink(argc - 1, argv + 1);
  }
  if (strcmp(cmd, "sleep") == 0) {
    return testbox_sleep(argc - 1, argv + 1);
  }
  if (strcmp(cmd, "emit") == 0) {
    return testbox_emit(argc - 1, argv + 1);
  }

  return 1;
}
