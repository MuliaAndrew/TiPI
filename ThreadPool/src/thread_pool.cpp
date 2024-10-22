#include "thread_pool.hpp"

ThreadPool* ThreadPool::self;
thread_local unsigned ThreadPool::ind;
std::atomic_bool ThreadPool::done;