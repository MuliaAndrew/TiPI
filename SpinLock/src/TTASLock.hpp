#include <atomic>
#include <thread>

class TTASLockYield {
    std::atomic<bool> locked;

  public:
    void lock() {
      bool state = 0;
      do {
        bool loaded = locked.load(std::memory_order_relaxed);
        while (loaded == 1) {
          loaded = locked.load(std::memory_order_relaxed);
          std::this_thread::yield();
        }
      }
      while (locked.compare_exchange_strong(state, 1, std::memory_order_acquire, std::memory_order_relaxed));
    }

    void unlock() {
      locked.store(0, std::memory_order_release);
    }
};

class TTASLockConstBackoff {
    std::atomic<bool> locked;

  public:
    void lock() {
      using namespace std::chrono_literals;
      bool state = 0;
      do {
        bool loaded = locked.load(std::memory_order_relaxed);
        while (loaded == 1) {
          loaded = locked.load(std::memory_order_relaxed);
          std::this_thread::sleep_for(200ns);
        }
      }
      while (locked.compare_exchange_weak(state, 1, std::memory_order_acquire, std::memory_order_relaxed));
    }

    void unlock() {
      locked.store(0, std::memory_order_release);
    }
};

class TTASLockExpBackoff {
    std::atomic<bool> locked;

  public:
    void lock() {
      using namespace std::chrono_literals;
      bool state = 0;
      do {
        bool loaded = locked.load(std::memory_order_relaxed);
        auto backoff = 10ns;
        while (loaded == 1) {
          loaded = locked.load(std::memory_order_relaxed);
          std::this_thread::sleep_for(backoff);
          if (backoff < 200us) 
            backoff *= 2;
        }
      }
      while (locked.compare_exchange_weak(state, 1, std::memory_order_acquire, std::memory_order_relaxed));
    }

    void unlock() {
      locked.store(0, std::memory_order_release);
    }
};
