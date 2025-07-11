#include "semaphore.h"

#include <iostream>

#define portMAX_DELAY 0xFFFFFFFFUL

BinarySemaphore::BinarySemaphore() : signaled(false) {}

void BinarySemaphore::give() {
  std::unique_lock<std::mutex> lock(mutex);
  signaled = true;
  cv.notify_one();
}

bool BinarySemaphore::take(uint32_t timeoutMs) {
  std::unique_lock<std::mutex> lock(mutex);
  if (timeoutMs == 0) {
    bool result = signaled;
    if (result) signaled = false;
    return result;
  }
  bool result =
      cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return signaled; });
  if (result) signaled = false;
  return result;
}

RecursiveMutex::RecursiveMutex() : lock_count(0), owner_thread_id(std::thread::id()) {}

bool RecursiveMutex::take(uint32_t timeoutMs) {
  std::thread::id current_thread_id = std::this_thread::get_id();

  if (owner_thread_id == current_thread_id) {
    internal_mutex.lock();
    lock_count++;
    return true;
  }

  if (timeoutMs == portMAX_DELAY) {
    internal_mutex.lock();
    owner_thread_id = current_thread_id;
    lock_count = 1;
    return true;
  } else {
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < end_time) {
      if (internal_mutex.try_lock()) {
        owner_thread_id = current_thread_id;
        lock_count = 1;
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (internal_mutex.try_lock()) {
      owner_thread_id = current_thread_id;
      lock_count = 1;
      return true;
    }
    return false;
  }
}

void RecursiveMutex::give() {
  std::thread::id current_thread_id = std::this_thread::get_id();

  if (owner_thread_id != current_thread_id) {
    std::cerr << "FreeRTOS Mock WARNING: Mutex given by non-owner. Owner: " << owner_thread_id
              << ", Caller: " << current_thread_id << std::endl;
    return;
  }

  internal_mutex.unlock();
  lock_count--;

  if (lock_count == 0) { owner_thread_id = std::thread::id(); }
}

void RecursiveMutex::reset() { owner_thread_id = std::thread::id(); }
