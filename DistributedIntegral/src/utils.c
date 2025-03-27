#include "common.h"

//------------------------ REQUESTS AND RESPONSES ------------------------------

readn(int fd, void *vptr, size_t n)

{
  size_t nleft;
  ssize_t nread;
  char *ptr;
  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nread = read(fd, ptr, nleft)) < 0) {
      if (errno == EINTR)
        nread = 0; /* и вызывает снова функцию read() */
      else
        return (-1);
    }
    else if (nread == 0)
      break; /* EOF */
    nleft -= nread;
    ptr += nread;
  }
  return (n - nleft); /* возвращает значение >= 0 */
}

size_t writen(int fd, const void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;
  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ((nwritten = write(fd, ptr, nleft)) <= 0) {
      if (errno == EINTR)
        nwritten = 0; /* и снова вызывает функцию write() */
      else
        return (-1); /* ошибка */
    }
    nleft -= nwritten;
    ptr += nwritten;
  }
  return (n);
}

//------ assing resp -------

struct assign_resp {
  uint32_t task_id;
  int error; // either errno or common.h::Error
};

// msg layout:
// | FN_ID | task_id | error | .000. |
// 0       32        32      64     256

int handle_assign_resp(int sockfd, struct assign_resp* resp) {
  int ret;
  char msg[256] = {0};

  readn(sockfd, (void*)msg, sizeof(msg));

  
  
  return ret;
}

int serve_assign_resp(int sockfd, struct assign_resp* resp) {
  int ret;
  char msg[256] = {0};

  *((uint32_t*)(msg + 0)) = FN_ASSIGN;
  *((uint32_t*)(msg + 32)) = resp->task_id;
  *((uint32_t*)(msg + 64)) = resp->error;

  writen(sockfd, (void*)msg, sizeof(msg));

  return ret;
}

//------ assign req -------

struct assign_req {
  uint32_t task_id;
  struct {
    double l;
    double r;
  } range;
  int error;
};

int serve_assign_req(int sockfd, struct assign_req* req) {
  int ret;
  // TODO
  return ret;
}

//------ result req ------

struct result_req {
  uint32_t task_id;
  double result; // calculated interval
};

int handle_result_req(int sockfd, struct result_req* req) {
  int ret;
  // TODO
  return ret;
}

//------ result resp ------
// notify server that we got his result so he may not to store this result 

int serve_result_resp(int sockfd, struct resilt_resp* resp) {
  
}