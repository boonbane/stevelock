#include <errno.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static int net_probe_connect(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return 11;
  }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(9),
  };
  if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
    close(fd);
    return 11;
  }

  int rc = connect(fd, (const struct sockaddr*)&addr, sizeof(addr));
  if (rc == 0) {
    close(fd);
    return 0;
  }

  int err = errno;
  close(fd);
  if (err == EACCES || err == EPERM) {
    return 10;
  }
  if (err == ECONNREFUSED || err == ETIMEDOUT || err == ENETUNREACH || err == EHOSTUNREACH) {
    return 0;
  }
  return 11;
}

static int net_probe_bind(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return 11;
  }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(0),
  };
  if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
    close(fd);
    return 11;
  }

  int rc = bind(fd, (const struct sockaddr*)&addr, sizeof(addr));
  if (rc == 0) {
    close(fd);
    return 0;
  }

  int err = errno;
  close(fd);
  if (err == EACCES || err == EPERM) {
    return 10;
  }
  return 11;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    return 12;
  }

  if (strcmp(argv[1], "connect") == 0) {
    return net_probe_connect();
  }
  if (strcmp(argv[1], "bind") == 0) {
    return net_probe_bind();
  }

  return 12;
}
