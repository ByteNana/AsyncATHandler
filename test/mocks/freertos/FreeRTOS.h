#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>     // For memcpy
#include <functional>  // For std::function in TaskManager
#include <iostream>
#include <map>     // For TaskManager's task map
#include <memory>  // For std::unique_ptr in TaskManager
#include <mutex>   // For std::mutex, std::lock_guard, std::unique_lock, AND std::recursive_mutex
#include <queue>   // For std::queue (used internally by RawByteThreadSafeQueue)
#include <thread>  // For std::thread in TaskManager, and std::thread::id
#include <vector>  // For std::vector<char> in RawByteThreadSafeQueue

#include "queue.h"
#include "task.h"

// --- Standard FreeRTOS Type Definitions ---
// These are platform-independent type aliases for basic types.
typedef void* SemaphoreHandle_t;
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define pdTRUE ((BaseType_t)1)
#define pdFALSE ((BaseType_t)0)
#define pdPASS pdTRUE
#define pdFAIL pdFALSE
#define portMAX_DELAY 0xFFFFFFFFUL  // Ensure it's unsigned long for consistency

// --- Abstract base class for generic queue operations ---
// This interface allows FreeRTOS.cpp to manage queues polymorphically.

// --- Binary Semaphore implementation (unchanged, already type-agnostic) ---
class BinarySemaphore {
 private:
  std::mutex mutex;
  std::condition_variable cv;
  bool signaled = false;

 public:
  void give() {
    std::unique_lock<std::mutex> lock(mutex);
    signaled = true;
    cv.notify_one();
  }

  bool take(uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(mutex);
    if (timeoutMs == 0) {
      bool result = signaled;
      if (result) signaled = false;
      return result;
    }
    bool result =
        cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return signaled; });
    if (result) signaled = false;  // Consume the signal if successfully taken
    return result;
  }
};

// --- NEW: Recursive Mutex implementation for xSemaphoreCreateMutex ---
// This simulates a FreeRTOS mutex, which is recursive and owner-aware.
class RecursiveMutex {
 private:
  std::recursive_mutex internal_mutex;  // The actual C++ recursive mutex
  std::thread::id owner_thread_id;      // Tracks which thread owns it
  int lock_count;                       // How many times the owner has locked it

 public:
  RecursiveMutex()
      : lock_count(0), owner_thread_id(std::thread::id()) {}  // Initialize owner_thread_id

  // Simulates xSemaphoreTake for a recursive mutex
  bool take(uint32_t timeoutMs) {
    std::thread::id current_thread_id = std::this_thread::get_id();

    if (owner_thread_id == current_thread_id) {
      internal_mutex.lock();  // Acquire the underlying recursive_mutex
      lock_count++;
      return true;
    }

    if (timeoutMs == portMAX_DELAY) {
      internal_mutex.lock();  // Blocks indefinitely
      owner_thread_id = current_thread_id;
      lock_count = 1;
      return true;
    } else {
      // --- FIX: Replace try_lock_for with a loop using try_lock ---
      auto start_time = std::chrono::steady_clock::now();
      auto end_time = start_time + std::chrono::milliseconds(timeoutMs);

      while (std::chrono::steady_clock::now() < end_time) {
        if (internal_mutex.try_lock()) {
          owner_thread_id = current_thread_id;
          lock_count = 1;
          return true;
        }
        // Small sleep to avoid busy-waiting for non-blocking try_lock
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      // One last attempt after loop for exact timeoutMs
      if (internal_mutex.try_lock()) {
        owner_thread_id = current_thread_id;
        lock_count = 1;
        return true;
      }
      // -----------------------------------------------------------
      return false;  // Timeout occurred
    }
  }

  // Simulates xSemaphoreGive for a recursive mutex
  void give() {
    std::thread::id current_thread_id = std::this_thread::get_id();

    // CRITICAL: Only the owner can give it back (FreeRTOS mutex behavior)
    if (owner_thread_id != current_thread_id) {
      std::cerr << "FreeRTOS Mock WARNING: Mutex given by non-owner. Owner: " << owner_thread_id
                << ", Caller: " << current_thread_id << std::endl;
      // In a real RTOS, this might assert or cause a hard fault. For mock, log and return.
      return;
    }

    internal_mutex.unlock();  // Release the underlying recursive_mutex
    lock_count--;

    // If lock_count is zero, the mutex is fully released, clear owner
    if (lock_count == 0) { owner_thread_id = std::thread::id(); }
  }
  // Add a reset function for cleanup in TaskManager::reset() (optional, for safety)
  void reset() {
    // This attempts to unlock any held locks and clear ownership.
    // Be careful if a mutex is held by a thread that's being force-joined/reset,
    // unlocking it here might be unsafe if the owner thread is still holding it.
    // For testing, often relying on the destructor to eventually clean up is okay,
    // or ensuring all tests give back what they take.
    owner_thread_id = std::thread::id();  // Clear owner (if not already)
    // Forcing unlock is not safe or possible with std::recursive_mutex without owner.
  }
};
// -------------------------------------------------------------

extern "C" {
// Queue Functions
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait);
BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait);
void vQueueDelete(QueueHandle_t xQueue);
QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);

// Task Functions
BaseType_t xTaskCreatePinnedToCore(
    void (*taskFunction)(void*), const char* name, uint32_t stackSize, void* parameter,
    UBaseType_t priority, TaskHandle_t* handle, BaseType_t coreID);
void vTaskDelete(TaskHandle_t handle);
void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount();  // For mocking time progression

// Semaphore/Mutex Functions
SemaphoreHandle_t xSemaphoreCreateBinary();
SemaphoreHandle_t xSemaphoreCreateMutex();  // This will create a RecursiveMutex
void vSemaphoreDelete(SemaphoreHandle_t sem);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout);

// Utility/Time Functions
TickType_t pdMS_TO_TICKS(uint32_t ms);

}  // extern "C"
