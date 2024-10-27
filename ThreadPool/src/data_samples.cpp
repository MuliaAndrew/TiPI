#include "data_samples.hpp"
#include <errno.h>
#include <stdlib.h>
#include <exception>
#include <string.h>

static char buff[BUFF_SIZE];
static int nbytes = 0;

void SubmitTasksFromFile(int fd, ThreadPool& pool) {
    nbytes = read(fd, buff, BUFF_SIZE);
    std::cout << "OK" << nbytes << "\n"; 
    if (errno) {
        if (errno == EAGAIN) {
            return;
        }
        else {
            perror("WHY");
            std::system_error err(std::error_code(), "Bad fd read");
            throw std::system_error(err);
        }
    }
    auto buff_local_ptr = buff;
    while (buff_local_ptr[0] != SPCL_SYMB) {
        buff_local_ptr = buff_local_ptr + 1;
    }
    for (int i = 0; i < nbytes/TASKMSG_SIZE; i++) {
        auto tsk_str = TaskString::taskFromMsg(std::string(buff_local_ptr, TASKMSG_SIZE));
        buff_local_ptr = buff_local_ptr + TASKMSG_SIZE;
        pool.submit(tsk_str);
    }
    return;
}

TaskString::TaskString(int dest, std::string msg) {
    s[0] = SPCL_SYMB;
    *(int*)(s + 1) = dest;
    strncpy(s + sizeof(int) + 1, msg.c_str(), TASKMSG_SIZE - sizeof(int) - 1);
}

TaskString::operator Task() {
    return [=]() {
        write(*(int*)(s + 1), s + 2 * sizeof(int), TASKMSG_SIZE - 2 * sizeof(int));
    };
}

std::string TaskString::taskToMsg() {
    return s;
}

TaskString TaskString::taskFromMsg(std::string msg) {
    TaskString ts;
    strncpy(ts.s, msg.c_str(), TASKMSG_SIZE);
    return std::move(ts);
}

