#include <atomic>
#include <thread>

class TicketLock final {
    std::atomic<uint64_t> next_ticket{0};
    std::atomic<uint64_t> current{0};

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
      auto ticket = next_ticket.fetch_add(1, std::memory_order_relaxed);
      uint64_t next = current.load(std::memory_order_acquire);
      while (ticket != next) {
        while (next == current.load(std::memory_order_relaxed)) {
          std::this_thread::yield();
        }
        next = current.load(std::memory_order_acquire);
      }
    }

    void unlock() {
      current.fetch_add(1, std::memory_order_release);
    }
};