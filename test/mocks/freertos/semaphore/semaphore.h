#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

class BinarySemaphore {
 private:
  std::mutex mutex;
  std::condition_variable cv;
  bool signaled;

 public:
  BinarySemaphore();
  void give();
  bool take(uint32_t timeoutMs);
};

class RecursiveMutex {
 private:
  std::recursive_mutex internal_mutex;
  std::thread::id owner_thread_id;
  int lock_count;

 public:
  RecursiveMutex();
  bool take(uint32_t timeoutMs);
  void give();
  void reset();
};
