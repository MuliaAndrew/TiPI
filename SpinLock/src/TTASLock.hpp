#include <atomic>
#include <thread>

class TTASLock final {
    std::atomic<bool> locked{0};

  public:
    void lock() {
      do {
        bool loaded = locked.load(std::memory_order_relaxed);
        while (loaded == 1) {
          loaded = locked.load(std::memory_order_relaxed);
          asm volatile ("pause");
        }
      }
      while (locked.exchange(1, std::memory_order_acquire) == 1);
    }

    void unlock() {
      locked.store(0, std::memory_order_release);
    }
};
