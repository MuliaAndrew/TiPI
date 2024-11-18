#include <atomic>

class TicketLock {
    std::atomic<uint64_t> next_ticket;
    char padding[64 - sizeof(std::atomic<uint64_t>)];
    std::atomic<uint64_t> current;
  
  public:
    void lock() {
      auto ticket = next_ticket.fetch_add(1, std::memory_order_relaxed);
      uint64_t next = current.load(std::memory_order_acquire);
      while (ticket != next) {
        while (next == current.load(std::memory_order_relaxed));
        next = current.load(std::memory_order_acquire);
      }
    }

    void unlock() {
      current.fetch_add(1, std::memory_order_release);
    }
};