#include <mutex>
#include <functional>
#include <thread>
#include <deque>
#include <random>
#include <atomic>
#include <condition_variable>
#include <iostream>

using Task = std::function<void()>;

class ThreadPool {
        const unsigned BAD_INDEX = UINT32_MAX;

        struct alignas(64) QueuePair {
            std::mutex m;
            std::deque<Task> q;
        };

        static std::atomic<bool> done;
        const unsigned n_threads;
        std::vector<std::thread> threads;

        QueuePair pool_queue;

        std::vector<QueuePair> queue;

        static thread_local unsigned ind;
        static ThreadPool* self;


        void worker() {
            while (true) {
                Task t;
                if (tryPopLocal(t) || tryPopPool(t) || trySteal(t)) {
                    try {
                        t();
                    }
                    catch(std::exception& e) {
                        std::cout << e.what();
                    }
                } 
                else if (done)
                    break;
                else {
                    std::this_thread::yield();
                }
            }
        }

        // return true if stealing was successful and this thread get a task, otherwise return false
        bool trySteal(Task& t) {
            if (n_threads < 2)
                return false;

            auto th1 = rand() % n_threads;
            auto th2 = rand() % n_threads;
            
            while (th1 == th2)
                th2 = rand() % n_threads;
            
            if (th1 > th2)
                std::swap(th1, th2);

            auto& m1 = queue[th1].m;
            auto& m2 = queue[th2].m;

            auto& q1 = queue[th1].q;
            auto& q2 = queue[th2].q;

            auto lg1 = std::lock_guard(m1);
            auto lg2 = std::lock_guard(m2);

            if (q1.size() > q2.size()) {
                auto stealed = q1.back();
                q1.pop_back();
                if (ind == th2) {
                    t = stealed;
                    return true;
                }
                q2.push_front(stealed);
                return false;
            } 
            
            if (q1.size() < q2.size()) {
                auto stealed = q2.back();
                q2.pop_back();
                if (th1 == ind) {
                    t = stealed;
                    return true;
                }
                q1.push_front(stealed);
                return false;
            }
            return false;
        }

        bool tryPopLocal(Task& t) {
            auto& m = queue[ind].m;
            auto& q = queue[ind].q;

            auto lg = std::lock_guard(m);

            if (q.empty())
                return false;
            
            t = q.back();
            q.pop_back();

            return true;
        }

        bool tryPopPool(Task& t) {
            auto lg = std::lock_guard(pool_queue.m);

            if (pool_queue.q.empty())
                return false;
            
            t = pool_queue.q.back();
            pool_queue.q.pop_back();

            return true;
        }

    public:

        /// Not DefaultConstructable
        ThreadPool() = delete; 

        /// Not CopyConstructable
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        /// Not MoveConstructable
        ThreadPool(ThreadPool &&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        /// @brief  Constructs a ThreadPool with work stealing implementation of `n_threads_` threads 
        /// @warning The behavior of constructor and any other class methods is undefined if `n_threads_` is 0
        explicit ThreadPool(const unsigned n_threads_ = std::thread::hardware_concurrency())
            : n_threads(n_threads_), queue(std::vector<QueuePair>{n_threads})
        {
            done = false, 
            self = this;
            ind = BAD_INDEX;
            try {
                for (unsigned i = 0; i < n_threads; i++) {
                    threads.push_back(std::thread([this, i](){
                        ind = i;
                        this->worker();
                    }));
                }
            }
            catch(...) {
                done = true;
                throw;
            }
        }
        ~ThreadPool() {
            done = true;
            for (auto& th : threads) {
                if (th.joinable())
                    th.join();
            }
            self = nullptr;
        }

        void submit(Task tsk) {
            if (ind != BAD_INDEX) {
                auto lg = std::lock_guard(queue[ind].m);
                queue[ind].q.push_front(std::move(tsk));
            }
            else {
                auto lg = std::lock_guard(pool_queue.m);
                pool_queue.q.push_front(std::move(tsk));
            }
        }

        // each thread can obtain a pointer to the ThreadPool for submiting tasks 
        static ThreadPool* current() {
            return self;
        }
};