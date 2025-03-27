#include "common.h"
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <libgen.h>

/*
  LOG MSG TEMPLATE: -- prefix
    <YYYY.MM.DDThh:mm:ss>(blue) '['<srv_addr_ipv4>' ; '<task_id>']:' <@MSG@>'\n' 
  
  <@MSG@>: -- msg
    <operation(params)>(blue)': "reason msg" 
  
    
*/

#define PREFIX_FORMAT "\033[34m%s\033[39m [addr = { \033[34m%s:%d\033[39m } : task_id = %d]: %s"
#define MSG_FORMAT_GREEN "\033[34m%s()\033[39m: (\033[32mOK\033[39m): %s\n"
#define MSG_FORMAT_RED  "\033[34m%s()\033[39m: (\033[31mERR\033[39m): %s\n"
#define MSG_FORMAT_YELLOW "\033[34m%s()\033[39m: (\033[33mWARN\033[39m): %s\n"

enum MsgColor {
  MSG_COLOR_RED,
  MSG_COLOR_GREEN,
  MSG_COLOR_YELLOW
};

static char* log_path = NULL;
static int log_d = -1;

static char* self_addr = "unspecified";
static uint16_t self_port = 0;

#define TASK_FMT "task = {.id \033[36m%d\033[39;49m, .interval = {%.6lf, %.6lf}"
static const char* const TASK_STAT_SRV_FORMAT_MAP[] = {
  "accepted from #cl.addr = %s and added to queue",
  "is in process",
  "is done",
  "is successfully delivered to #cl.addr = %s, foggeting it...",
};

static const char* const TASK_STAT_CL_FORMAT_MAP[] = {
  "created",
  "assigned to the #srv.addr = %s",
  "got result from #srv.addr = %s, forgetting it...",
};

static const char* const ERROR_FORMAT_MAP[] = {
  "unexpectedly closed connection on #cl.addr = %s: ",
  "queue overflow",
  "unable to parse msg: %s", // actual msg here
  "no answer with complete msg: ",

  ""
};

static void log_flush();
static char* get_datetime();
static char* log_make_prefix(uint32_t task_id, const char* msg);
static char* log_make_msg(const char* func_name, const char* reason, enum MsgColor cl);

void log_init(const char* path_to_log_file) {
  log_path = strdup(path_to_log_file);

  int d = open(log_path, O_CREAT | O_TRUNC | O_WRONLY, 644);
  if (d == -1) {
    if (errno == ENOENT) {
      if (mkdir(dirname(strdup(path_to_log_file)), 755) == -1) {
        printf("%s[#srv.addr=%s] failed to start up logger on file \"%s\": %d (%s). terminating...\n", 
          get_datetime(), self_addr, path_to_log_file, errno, strerror(errno)
        );
        exit(1);
      }
    }
    else {
      printf("%s[#srv.addr=%s] failed to start up logger on file \"%s\": %d (%s). terminating...\n", 
        get_datetime(), self_addr, path_to_log_file, errno, strerror(errno)
      );
      exit(1);
    }
  }
#ifdef LOG_TO_STDOUT
  log_d = 1;
#else
  log_d = d;
#endif
  char buf[1024] = {};

  int buf_len = sprintf(buf, "\033[34m%s\033[39m [STARTING UP \033[34m%s\033[39m]\n", get_datetime(), self_addr);
  if (-1 == write(log_d, buf, buf_len)) {
    printf("%s[#srv.addr=%s] failed to write log file with error: %d (%s). terminating...\n",
      get_datetime(), self_addr, errno, strerror(errno)
    );
    exit(1);
  }
}

void log_set_host_addr(const char* addr) {
  if (!addr) {
    printf("%s[#srv.addr=%s] failed to set host addr: log_set_host_addr() got nullptr args\n");
    exit(1);
  }
  self_addr = addr;
}

void log_set_host_port(uint16_t port) {
  self_port = port;
}

void log_fini(void) {
  free(self_addr);

  log_flush();
#ifndef LOG_TO_STDOUT
  close(log_d);
#endif
}

static void log_flush() {
  if (fsync(log_d) == -1) {
    printf("%s[#srv.addr=%s] failed to flush log file with error: %d (%s). terminating...\n",
      get_datetime(), self_addr, errno, strerror(errno)
    );
    exit(1);
  }
}

const char* log_strerror(int errnum) {
  if (errnum >= 1500 && errnum < 1600)
    return TASK_STAT_SRV_FORMAT_MAP[errnum - 1500];
  
  if (errnum >= 1600 && errnum < 1700)
    return TASK_STAT_CL_FORMAT_MAP[errnum - 1600];
  
  if (errnum >= 1700 && errnum < 1800)
    return ERROR_FORMAT_MAP[errnum - 1700];
  
  return strerror(errnum);
}

static char* get_datetime(void) {
  time_t tmp = time(NULL);
  char* time_str = ctime(&tmp);
  time_str[strlen(time_str) - 2] = '\0';
  return time_str;
}

static char* log_make_prefix(uint32_t task_id, const char* msg) {
  char buf[512] = {};
  sprintf(buf, PREFIX_FORMAT, get_datetime(), self_addr, self_port, task_id, msg);
  return strdup(buf);
}

static char* log_make_msg(const char* func_name, const char* reason, enum MsgColor cl) {
  char buf[512] = {};
  char* fmt;
  
  switch (cl) {
    case MSG_COLOR_GREEN:   { fmt = MSG_FORMAT_GREEN; break; }
    case MSG_COLOR_YELLOW:  { fmt = MSG_FORMAT_YELLOW; break; }
    case MSG_COLOR_RED:     { fmt = MSG_FORMAT_RED; break; }
  }
  
  sprintf(buf, fmt, func_name, reason);

  return strdup(buf);
}

void log_msg(const char* func_name, uint32_t task_id, char* format, ...) {
  char buf[512] = {};
  if (!func_name)
    printf("func_name SIGSEGV\n");
  if (!format)
    printf("format sigsegv\n");
  fflush(stdout);

  va_list va;
  va_start(va, format);
  vsprintf(buf, format, va);
  va_end(va);

  char* msg = log_make_msg(func_name, buf, MSG_COLOR_GREEN);
  char* full_msg = log_make_prefix(task_id, msg);
  write(log_d, full_msg, strlen(full_msg));

  free(full_msg);
  free(msg);
}

void log_warn(const char* func_name, uint32_t task_id, char* format, ...) {
  char buf[512] = {};

  va_list va;
  va_start(va, format);
  vsprintf(buf, format, va);
  va_end(va);

  char* msg = log_make_msg(func_name, buf, MSG_COLOR_YELLOW);
  char* full_msg = log_make_prefix(task_id, msg);
  write(log_d, full_msg, strlen(full_msg));

  free(full_msg);
  free(msg);
}

void log_error(const char* func_name, uint32_t task_id, char* format, ...) {
  char buf[512] = {};

  va_list va;
  va_start(va, format);
  vsprintf(buf, format, va);
  va_end(va);

  char* msg = log_make_msg(func_name, buf, MSG_COLOR_RED);
  char* full_msg = log_make_prefix(task_id, msg);
  write(log_d, full_msg, strlen(full_msg));

  free(full_msg);
  free(msg);

  exit(errno);
}