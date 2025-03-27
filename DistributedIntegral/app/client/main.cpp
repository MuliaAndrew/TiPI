#include "../../src/common.h"
#include <sys/epoll.h>
#include <map>
#include <set>
#include <string>
#include <vector>

struct Task {
  uint32_t id;
  
  struct {
    double l;
    double r;
  } range;

  TaskStatusCl status;

  int connfd;
};

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, 
      "Usage :\n"
      "client <nr_servers> [<server_addr>](nr_servers)"
    );
    return 1;
  }

  int nr_servers = std::atoi(argv[1]);
  std::vector<std::string> srv_table((size_t)nr_servers);
  for (int i = 0; i < nr_servers; i++)
    srv_table[i] = strdup(argv[i + 2]);

  // in_addr_t srv_addr = inet_addr(argv[1]);  
  log_init("/var/log/DI/client.log");
  log_set_host_addr("0.0.0.0");

  // struct sockaddr_in srv = {
  //   .sin_family = AF_INET,
  //   .sin_port = htons(PORT),
  //   .sin_addr = { .s_addr = srv_addr },
  // };

  int srv_d = socket(AF_INET, SOCK_STREAM, 0);
  if (srv_d == -1) {
    log_error(__func__, -1, "socket() error: %d (%s)", errno, log_strerror(errno));
  }
  log_msg(__func__, -1, "ok socket()");

  // if (connect(srv_d, (SA*)&srv, sizeof(srv)) == -1) {
  //   log_error(__func__, -1, "connect() error: %d (%s)", errno, log_strerror(errno));
  // }
  // struct sockaddr_in cl;
  // socklen_t cl_len = sizeof(cl);
  // if (getsockname(srv_d, (SA*)&cl, &cl_len) == -1) {
  //   log_error(__func__, -1, "getsockname() error: %d (%s)", errno, log_strerror(errno));
  // }
  // log_set_host_addr(inet_ntoa(cl.sin_addr));
  // log_set_host_port(ntohs(cl.sin_port));
  // log_msg(__func__, -1, "ok connect() to %s:%d", argv[1], PORT);

  const double left = 0;
  const double right = 100;
  double result = 0;

  std::multimap<sockaddr_in, Task> q_assigned;
  std::multimap<sockaddr_in, Task> q_created;
  std::map<int, sockaddr_in> connected;

  while (true) {
    int r = 0;
    for (int i = 0; i < nr_servers; i++) {
      in_addr_t srv_addr = inet_addr(srv_table[i].c_str());
      struct sockaddr_in srv = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr = { .s_addr = srv_addr },
      };
      if ((connect(srv_d, (SA*)&srv_addr)))
    }
  }
}