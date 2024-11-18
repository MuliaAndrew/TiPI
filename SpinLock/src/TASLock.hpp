#include <atomic>

class TASLock {
    std::atomic<bool> lock;

  public:
    void lock() {
      bool state = 0;
      while (!lock.compare_exchange_weak(state, 1, std::memory_order_acquire, std::memory_order_relaxed));
    }

    void unlock() {
      lock.store(0, std::memory_order_release);
    }
};