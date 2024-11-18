#include <atomic>
#include <thread>

class TicketLockYield {
    std::atomic<uint64_t> next_ticket;
    char padding[64 - sizeof(std::atomic<uint64_t>)];
    std::atomic<uint64_t> current;
  
  public:
    void lock() {
      auto ticket = next_ticket.fetch_add(1, std::memory_order_relaxed);
      uint64_t next = current.load(std::memory_order_acquire);
      while (ticket != next) {
        while (next == current.load(std::memory_order_relaxed))
          std::this_thread::yield();
        next = current.load(std::memory_order_acquire);
      }
    }

    void unlock() {
      current.fetch_add(1, std::memory_order_release);
    }
};

class TicketLockConstBackoff {
    std::atomic<uint64_t> next_ticket;
    char padding[64 - sizeof(std::atomic<uint64_t>)];
    std::atomic<uint64_t> current;
  
  public:
    void lock() {
      using namespace std::chrono_literals;
      auto ticket = next_ticket.fetch_add(1, std::memory_order_relaxed);
      uint64_t next = current.load(std::memory_order_acquire);
      while (ticket != next) {
        while (next == current.load(std::memory_order_relaxed))
          std::this_thread::sleep_for(200ns);
        next = current.load(std::memory_order_acquire);
      }
    }

    void unlock() {
      current.fetch_add(1, std::memory_order_release);
    }
};

class TicketLockExpBackoff {
    std::atomic<uint64_t> next_ticket;
    char padding[64 - sizeof(std::atomic<uint64_t>)];
    std::atomic<uint64_t> current;
  
  public:
    void lock() {
      using namespace std::chrono_literals;
      auto ticket = next_ticket.fetch_add(1, std::memory_order_relaxed);
      uint64_t next = current.load(std::memory_order_acquire);
      while (ticket != next) {
        auto backoff = 10ns;
        while (next == current.load(std::memory_order_relaxed)) {
          std::this_thread::sleep_for(backoff);
          if (backoff < 200us)
            backoff *= 2;
        }
        next = current.load(std::memory_order_acquire);
      }
    }

    void unlock() {
      current.fetch_add(1, std::memory_order_release);
    }
};