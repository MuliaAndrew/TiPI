#pragma once

#include <functional>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include "thread_pool.hpp"

#define BUFF_SIZE 1024

#define TASKMSG_SIZE 32

#define SPCL_SYMB 16

// represent a Task as a string, used to create tsks for test samples, to convert 
// char request[] to Task and in reverse
class TaskString {
    char s[TASKMSG_SIZE];
    public:
        TaskString() = default;
        TaskString(int dest, std::string msg);
        std::string taskToMsg();
        static TaskString taskFromMsg(std::string msg);
        operator Task();
};

void SubmitTasksFromFile(int fd, ThreadPool& pool);
