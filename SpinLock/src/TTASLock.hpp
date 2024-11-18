#include <atomic>

class TTASLock {
    std::atomic<bool> lock;

  public:
    void lock() {
      bool state = 0;
      do {
        bool loaded = lock.load(std::memory_order_relaxed);
        while (loaded == 1)
          loaded = lock.load(std::memory_order_relaxed);
      }
      while (lock.compare_exchange_weak(state, 1, std::memory_order_acquire, std::memory_order_relaxed));
    }

    void unlock() {
      lock.store(0, std::memory_order_release);
    }
};