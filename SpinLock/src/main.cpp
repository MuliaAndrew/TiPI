#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>

#include "TASLock.hpp"
#include "TTASLock.hpp"
#include "TicketLock.hpp"

TASLock tas;
TTASLock ttas;
TicketLock tick;

const int ITER_NUM = 50000;

void printStat(
    const std::string lock_name, 
    const std::vector<int64_t>& average, // index of array equal to number of threads in test
    const std::vector<int64_t>& maximum  // index of array equal to number of threads in test
) {
  std::cout << lock_name << "\n";
  for (auto& dur : average) {
    std::cout << dur << " ";
  }
  std::cout << "\n";
  for (auto& dur : maximum) {
    std::cout << dur << " ";
  }
  std::cout << "\n\n";
}

#define DECLARE_TEST(lock_prefix) \
void lock_prefix##Test() { \
  std::vector<std::thread> threads; \
  std::vector<int64_t> average((size_t)32, 0); \
  std::vector<int64_t> maximum((size_t)32, 0); \
  auto worker = [](uint64_t* counter){ \
    for (int i = 0; i < ITER_NUM; i++) { \
      lock_prefix.lock(); \
      (*counter)++; \
      lock_prefix.unlock(); \
    } \
  }; \
  for (int n = 0; n < 32; n++) { \
    uint64_t counter; \
    std::vector<int64_t> diffs((size_t)ITER_NUM, 0); \
    int64_t max_; \
    std::chrono::system_clock clk; \
    for (int i = 0; i < n; i++) { \
      threads.push_back(std::thread(worker, &counter)); \
    } \
    for (int i = 0; i < ITER_NUM; i++) { \
      auto st = clk.now(); \
      lock_prefix.lock(); \
      auto end = clk.now(); \
      counter++; \
      lock_prefix.unlock(); \
      diffs[i] = std::chrono::duration_cast<std::chrono::nanoseconds>((end - st)).count(); \
    } \
    for (auto& thr : threads) \
      thr.join(); \
    threads.clear(); \
    average[n] = std::accumulate(diffs.begin(), diffs.end(), 0) / ITER_NUM; \
    for (int i = 0; i < ITER_NUM; i++) \
      maximum[n] = (maximum[n] >= diffs[i]) ? maximum[n] : diffs[i]; \
  } \
  printStat(#lock_prefix, average, maximum); \
} 

DECLARE_TEST(tas)
DECLARE_TEST(ttas)
DECLARE_TEST(tick)

int main() {
  tasTest();
  ttasTest();
  tickTest();

  return 0;
}