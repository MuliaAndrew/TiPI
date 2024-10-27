#include "thread_pool.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>
#include "data_samples.hpp"


int main(int argc, char** argv) {
    // making fds
    const int M = 100;

    int fd[M][2] = {0};
    
    epoll_event events[M];

    for (int i = 0; i < M; i++) {
        if (pipe2(fd[i], O_NONBLOCK)) {
            std::cout << "Cannot create pipe\n";
            return 1;
        }
    }

    std::vector<TaskString> MSGS;
    for (int i = 0; i < 10000; i++) {
        std::string fname = "tests/" + std::to_string(i) + ".txt"; 
        auto fd = open(fname.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
        MSGS.push_back(TaskString(fd, std::to_string(i)));
    }

    // the child process will be echo server
    // the parent process will represent M clients, writting msgs to echo server
    auto pid = fork();

    if (pid == -1) {
        std::cout << "Cannot fork\n";
        return 1;
    }

    if (pid) {
        // writing to the random of M desriptors the request with appropriate task
        for (int p = 0; p < MSGS.size(); p++) {
            std::string msg = MSGS[p].taskToMsg();
            int rand_fd_ind = rand() % M;
            int count = 0;
            while(true) {
                auto written = write(fd[rand_fd_ind][1], msg.c_str() + count, msg.size() - count);
                count += written;
                if (errno != EAGAIN || count == msg.size())
                    break;
            }
        }
    }
    // child process
    else {
        auto fd_epoll = epoll_create1(0);
        if (fd_epoll == -1) {
            std::cout << "Bad epoll creation\n";
            return 1;
        }

        // setting interest list up 
        for (int i = 0; i < M; i++) {
            events[i].events = EPOLLIN;
            events[i].data.fd = fd[i][0];
            if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd[i][0], &(events[i])) == -1) {
                std::cout << "epoll_ctl error\n";
                return 1;
            }
        }

        ThreadPool pool{15};

        // listen for M descriptors
        while(true) {
            auto nfds = epoll_wait(fd_epoll, events, M, 1000);
            if (nfds == 0 || nfds == -1) {
                break;
            }
            std::cout << nfds << "\n";
            for (int i = 0; i < nfds; i++) {
                auto fd = events[i].data.fd;
                
                SubmitTasksFromFile(fd, pool);
            }
        }
        close(fd_epoll);
    }
}