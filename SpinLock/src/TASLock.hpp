#include <atomic>
#include <thread>

class TASLockYield {
    std::atomic<bool> locked;

  public:
    void lock() {
      bool state = 0;
      while (!locked.compare_exchange_weak(state, 1, std::memory_order_acquire, std::memory_order_relaxed))
        std::this_thread::yield();
    }
    
    void unlock() {
      locked.store(0, std::memory_order_release);
    }
};

class TASLockConstBackoff {
    std::atomic<bool> locked;

  public:
    void lock() {
      using namespace std::chrono_literals;
      bool state = 0;
      while (!locked.compare_exchange_strong(state, 1, std::memory_order_acquire, std::memory_order_relaxed))
        std::this_thread::sleep_for(200ns);
    }

    void unlock() {
      locked.store(0, std::memory_order_release);
    }
};

class TASLockExpBackoff {
    std::atomic<bool> locked;

  public:
    void lock() {
      using namespace std::chrono_literals;
      auto backoff = 10ns;
      bool state = 0;
      while (!locked.compare_exchange_weak(state, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
        std::this_thread::sleep_for(backoff);
        if (backoff < 200us)
          backoff *= 2;
      }
    }

    void unlock() {
      locked.store(0, std::memory_order_release);
    }
};
