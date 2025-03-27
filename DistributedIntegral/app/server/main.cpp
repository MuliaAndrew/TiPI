#include "../../src/common.h"

double calculate_range(double l, double r) {
#define N_STEPS 100000
  double range = l - r;
  double step = range / N_STEPS;
  double res = 0;
  res += F(l) + F(r);
  for (unsigned i = 1; i < N_STEPS - 1; i++) {
    res += F(l + step * i);
  }
  res = res / (2 * range);
  return res;
#undef N_STEPS
}

//-----------------------------------

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "wrong number of aruments: expected 1, given %d", argc - 1);
    return 1;
  }

  int addr = inet_addr(argv[1]);
  log_set_host_addr(argv[1]);
  log_set_host_port(40404);
  log_init("/var/log/DI/client.log");

  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (listen_fd == -1) {
    log_e("socket() error: %d (%s)", errno, log_strerror(errno));
  }
  log_msg(__func__, -1, "ok socket()");
  
  struct sockaddr_in srvaddr = { 
    .sin_family = AF_INET,
    .sin_port = htons(PORT),
    .sin_addr = { .s_addr = addr }
  };

  if (bind(listen_fd, (SA*)&srvaddr, sizeof(srvaddr))) {
    log_error(__func__, -1, "bind() error: %d (%s)", errno, log_strerror(errno));
  }
  log_msg(__func__, -1, "ok bind() on port: 40404");

  if (listen(listen_fd, 10)) {
    log_error(__func__, -1, "listen() error: %d (%s)", errno, log_strerror(errno));
  }
  log_msg(__func__, -1, "ok listen()");

  char buf[1024];

  while (1) {

  }

  return 0;
}