#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <error.h>

#define PORT 40404

//------------------ GLOBAL PROPERTIES -------------------

#define F(x) exp(-(x)*sin(x))

#define SRV_MAX_RECONN_TIMEOUT 1000000 // 1s
#define CL_MAX_RECONN_TIMEOUT 1000000 // 1s

enum TaskStatusSrv {
  TSK_ACCEPTED = 1500,  // server accepted msg from client (got task id, integral interval) and put it to the queue
  TSK_PROCCESS,         // server is currently calculating interval
  TSK_DONE,             // server successfully calculated interval 
  TSK_SERVED,           // server successfully deliver msg back to the client (dont storing it, only for logger)
};

enum TaskStatusCl {
  TSK_CREATED = 1600,   // client created a task but did not assign it to the server yet
  TSK_ASSIGNED,         // client successfully assigned task to the server
  TSK_ACCEPTED          // client recieved calculated task from server (dont storing it, only for logger)
};

enum Error {
  SRV_ECONN = 1700,     // unexpectedly closed conneection
  SRV_EBUFF,            // queue overflow
  SRV_EREQ,             // unable to parse request
  SRV_NOANS,            // client didnt response with acceptance msg
  
  CL_ECONN,             // unexpectedly closed connection
  CL_NOANS,             // srv didnt response with acceptance msg
};

//------------------ SIMPLE LOGGER -----------------------

#define SRV_LOG_PATH "/var/log/DI/srv.log"
#define CL_LOG_PATH "/var/log/DI/cl.log"

void log_init(const char* log_path, struct sockaddr self_peer_addr);
void log_msg(const char* format, ...);
void log_error(const char* format, ...);
void log_fini();
const char* log_strerror(int errnum); // errno + TaskStatus strerror

//------------------ TASK_QUEUE_SRV --------------------------

/*
  task queue is a fixed size buffer which store newcome tasks and ready tasks which was delivered yet:
  
  ---------------------------------------------------------------------------------------------
  | .id = 0 | .id = 1 | .id = 6 | .id = 8  | .id = 9  | .id = 10  | .id = 20 | .id = 21 | ... |
  | SERVED  |   DONE  |   DONE  | PROCCESS | PROCCESS | PRECCESS  | ACCEPTED | ACCEPTED | ... |
  ---------------------------------------------------------------------------------------------

  served tasks are automaticaly removed after got an acceptance answer from client (task_accepted_req/task_accepted_resp) 
*/





//------------------ TASK_QUEUE_CL -----------------------------

/*
  client task queue is more complex

            --------------------------------------------------------------------------------------------
  ID        | .id = 0  | .id = 1  | .id = 2  | .id = 3  | .id = 4  | .id = 5 | .id = 6 | .id = 7 | ... |
  STATUS    | ASSIGNED | ASSIGNED | ASSIGNED | ASSIGNED | ASSIGNED | CREATED | CREATED | CREATED | ... |  
  SRV_ADDR  |  #addr1  |  #addr1  |  #addr1  |  #addr2  |  #addr2  | 0.0.0.0 | 0.0.0.0 | 0.0.0.0 | ... |
            --------------------------------------------------------------------------------------------
  
  additionaly storing right border of interval to correctly create new when tasks accepted and removed from queue
  
  when server reqsponse with task id which is not in the queue we accept it without modifying result 
*/





//------------------ CLIENT ALGORITHM -----------------------------

/*
  
*/