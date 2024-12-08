#include "lock_free_stack.hpp"
#include <thread>
#include <vector>

int main() {
  LockFreeStack<int> stk;
  std::vector<std::thread> threads;

  std::atomic<uint64_t> count_readed;

  auto writer = [&](int base) {
    for (int val = base; val < 10000; val += 16)
      stk.push(val);
  };

  auto reader = [&](){
    while (!stk.isEmpty()) {
      stk.pop();
      count_readed.fetch_add(1, std::memory_order_relaxed);
    }
  };

  for (int i = 0; i < 16; i++) {
    threads.push_back(std::thread(writer, i));
  }

  for (int i = 0; i < 15; i++) {
    threads.push_back(std::thread(reader));
  }

  for (auto& thr : threads)
    thr.join();
}