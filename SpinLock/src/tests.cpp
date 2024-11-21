#include <gtest/gtest.h>
#include <list>

#include "TASLock.hpp"
#include "TTASLock.hpp"
#include "TicketLock.hpp"

TASLock tas;
TTASLock ttas;
TicketLock tick;

TEST(MutualExclusion, TASLock) {
  unsigned int num1 = 0;

  std::vector<std::thread> threads1;

  for (int i = 0; i < 2; i++) {
    threads1.push_back(std::thread([&](){
      for (int i = 0; i < 10000; i++) {
        tas.lock();
        num1++;
        tas.unlock();
      }
    }));
  }
  for (auto &th : threads1)
    th.join();

  EXPECT_EQ(num1, 20000);

  unsigned int num2 = 0;

  std::vector<std::thread> threads2;

  for (int i = 0; i < 20; i++) {
    threads2.push_back(std::thread([&](){
      for (int i = 0; i < 10000; i++) {
        tas.lock();
        num2++;
        tas.unlock();
      }
    }));
  }
  for (auto &th : threads2)
    th.join();
  

  EXPECT_EQ(num2, 200000);
}

TEST(MutualExclusion, TTASLock) {unsigned int num1 = 0;

  std::vector<std::thread> threads1;

  for (int i = 0; i < 2; i++) {
    threads1.push_back(std::thread([&](){
      for (int i = 0; i < 10000; i++) {
        ttas.lock();
        num1++;
        ttas.unlock();
      }
    }));
  }
  for (auto &th : threads1)
    th.join();
  EXPECT_EQ(num1, 20000);

  unsigned int num2 = 0;

  std::vector<std::thread> threads2;

  for (int i = 0; i < 20; i++) {
    threads2.push_back(std::thread([&](){
      for (int i = 0; i < 10000; i++) {
        ttas.lock();
        num2++;
        ttas.unlock();
      }
    }));
  }
  for (auto &th : threads2)
    th.join();
  EXPECT_EQ(num2, 200000);
}

TEST(MutualExclusion, TicketLock) {unsigned int num1 = 0;

  std::vector<std::thread> threads1;

  for (int i = 0; i < 2; i++) {
    threads1.push_back(std::thread([&](){
      for (int i = 0; i < 10000; i++) {
        tick.lock();
        num1++;
        tick.unlock();
      }
    }));
  }
  for (auto &th : threads1)
    th.join();
  EXPECT_EQ(num1, 20000);

  unsigned int num2 = 0;

  std::vector<std::thread> threads2;

  for (int i = 0; i < 20; i++) {
    threads2.push_back(std::thread([&](){
      for (int i = 0; i < 10000; i++) {
        tick.lock();
        num2++;
        tick.unlock();
      }
    }));
  }
  for (auto &th : threads2)
    th.join();
  EXPECT_EQ(num2, 200000);
}