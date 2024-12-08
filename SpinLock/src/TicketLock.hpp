#include <atomic>
#include <thread>

class TicketLock final {
    std::atomic<uint64_t> next_ticket{0};
    std::atomic<uint64_t> current{0};
  
  public:
    void lock() {
      using namespace std::chrono_literals;
      const auto ticket = next_ticket.fetch_add(1, std::memory_order_relaxed);
      uint64_t next = current.load(std::memory_order_relaxed);
      while (ticket != next) {
        next = current.load(std::memory_order_relaxed);
        std::this_thread::yield();
      }
      current.load(std::memory_order_acquire);
    }

    void unlock() {
      current.fetch_add(1, std::memory_order_release);
    }
};