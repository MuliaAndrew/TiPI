#include "common.h"

/*
  LOG MSG TEMPLATE:
    <YYYY.MM.DDThh:mm:ss>(blue) '['<srv_addr_ipv4>' ; '<task_id>']:' <@MSG@>'\n'
  
  <@MSG@>:
    <operation(params)>(blue)': ('<STATUS>(ERRs[]:(red, orange, yellow), SUCCESS:green)'):' <@MSG@> | "reason msg"
*/

#define MSG_FORMAT

static char* log_path = NULL;
static int log_d = -1;

static char* self_addr = NULL;
static const char* const TASK_STAT_FORMAT_MAP[] = {
  "task = {.id \e[36m%d\e[39;49m, .interval = {%.6lf, %.6lf} accepted and added to queue",
  "task = {.id \e[36m%d\e[39;49m, .interval = {%.6lf, %.6lf} is in process",
  "task = {.id \e[36m%d\e[39;49m, .interval = {%.6lf, %.6lf} is done",
  "task = {.id \e[36m%d\e[39;49m, .interval = {%.6lf, %.6lf} is successfully delivered, foggeting it...",
  "TSK_"
}

static void log_flush();
static const char* get_datetime();

static const char* log_prefix();

void log_init(const char* log_path, struct sockaddr self_peer_addr) {

}
