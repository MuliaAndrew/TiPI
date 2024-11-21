#include <atomic>
#include <thread>
#include <mutex>

class TASLock final {
    std::atomic<uint32_t> locked{0};
    std::mutex m;

  public:
    void lock() {
      uint32_t state = 0;
      while (!locked.compare_exchange_weak(state, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
        asm volatile ("pause");
        state = 0;
      }
    }
    
    void unlock() {
      locked.store(0);
    }
};