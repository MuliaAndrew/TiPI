#include "thread_pool.hpp"
#include <gtest/gtest.h>
#include <sstream>
TEST(ThreadPool, Unit1) {
    ThreadPool pool{2};

    for (int i = 0; i < 2; i++) {
        pool.submit([](){
            std::cout << "[" << gettid() << "] ok task" << std::endl;
        });
    }
}

TEST(ThreadPool, Unit2) {
    ThreadPool pool{16};
    std::mutex cout_lock;
    for (int i = 0; i < 100; i++) {
        pool.submit([i, &cout_lock](){
            auto pool = ThreadPool::current();
            cout_lock.lock();
            std::stringstream s;
            s << "[" << gettid() << "] I am task number " << i << "!" << std::endl;
            std::cout << s.rdbuf();
            cout_lock.unlock();
            for (int k = 0; k < 2; k ++) {
                pool->submit([=, &cout_lock]() {
                    auto lg = std::lock_guard(cout_lock);
                    std::cout << "[" << gettid() << "] I am subtask number " << i * 2 + k + 100 << "!" << std::endl;
                });
            }
        });
    }
}