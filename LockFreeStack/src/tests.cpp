#include "lock_free_stack.hpp"
#include <thread>
#include <vector>
#include <chrono>
#include <assert.h>

int main() {
  using namespace std::chrono_literals;

  LockFreeStack<int> stk;
  // stk.push(1);

  std::vector<std::thread> threads;

  std::atomic<uint64_t> count_readed = 0;
  std::atomic<uint64_t> count_written = 0;

  auto writer = [&](int base) {
    for (int val = base + 1, i = 0; i < 10000; i++, val += 16) {
      stk.push(val);
      count_written.fetch_add(1, std::memory_order_relaxed);
    }
  };

  auto reader = [&](){
    while (!stk.isEmpty()) {
      if (stk.pop())
        count_readed.fetch_add(1, std::memory_order_relaxed);
    }
  };

  for (int i = 0; i < 16; i++) {
    threads.push_back(std::thread(writer, i));
  }

  while(count_written != 16 * 10000) {
    std::this_thread::sleep_for(10us);
  }

  for (int i = 0; i < 15; i++) {
    threads.push_back(std::thread(reader));
  }

  for (auto& thr : threads)
    thr.join();

  std::cout << "readed: " << count_readed << ", written: " << count_written << "\n";
  assert(count_readed == count_written);
    
  return 0;
}