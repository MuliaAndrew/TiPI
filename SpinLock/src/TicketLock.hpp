#include <atomic>
#include <thread>

class TicketLock {
    std::atomic<uint64_t> next_ticket;
    std::atomic<uint64_t> current;

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
        auto backoff = 40ns;
        uint64_t iter = 0;
        while (next == current.load(std::memory_order_relaxed)) {
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
        next = current.load(std::memory_order_acquire);
      }
    }

    void unlock() {
      current.fetch_add(1, std::memory_order_release);
    }
};