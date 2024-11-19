#include <atomic>
#include <thread>

class TASLock {
    std::atomic<bool> locked{0};

    void nop() {
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
    }

  public:
    void lock() {
      using namespace std::chrono_literals;
      bool state = 0;
      auto backoff = 40ns;
      uint64_t iter = 0;
      while (!locked.compare_exchange_strong(state, 1, std::memory_order_acquire)) {
        if (iter < 0x10) {
          for (int i = 0; i < iter; i++)
            nop();
        }
        else if (iter < 0x20) 
          std::this_thread::yield();
        else {
          std::this_thread::sleep_for(backoff);
          backoff *= (backoff < 10ms) ? 2 : 1;
        }
        iter++;
      }
    }
    
    void unlock() {
      locked.store(0, std::memory_order_release);
    }
};