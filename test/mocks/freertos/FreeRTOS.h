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
// Removed: #include <recursive_mutex> // <<< THIS LINE IS REMOVED! It's part of <mutex>

// --- Standard FreeRTOS Type Definitions ---
// These are platform-independent type aliases for basic types.
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
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
class ThreadSafeQueueBase {
 public:
  virtual ~ThreadSafeQueueBase() = default;
  // These methods operate on raw bytes (void*) and delegate to memcpy internally.
  virtual bool sendGeneric(const void* item, uint32_t timeoutMs) = 0;
  virtual bool receiveGeneric(void* item, uint32_t timeoutMs) = 0;
  virtual size_t messagesWaiting() const = 0;
  virtual size_t getItemSize() const = 0;  // Returns the itemSize this queue was created with
};

// --- Concrete, Non-Templated Raw Byte Thread-Safe Queue Implementation ---
// This class stores and retrieves data as raw bytes using std::vector<char>.
class RawByteThreadSafeQueue : public ThreadSafeQueueBase {
 private:
  std::deque<std::vector<char>> queue_data;  // Stores raw byte data for each item
  mutable std::mutex mutex;
  std::condition_variable cv_send;     // For blocking send when full
  std::condition_variable cv_receive;  // For blocking receive when empty
  size_t maxSize;
  size_t actualItemSize;  // The size of each item (in bytes) this specific queue holds

 public:
  // Constructor takes length and item_size, just like xQueueCreate
  explicit RawByteThreadSafeQueue(size_t length, size_t item_size)
      : maxSize(length), actualItemSize(item_size) {}

  // Implement virtual methods from ThreadSafeQueueBase using memcpy
  bool sendGeneric(const void* item, uint32_t timeoutMs) override {
    if (!item) return false;

    std::unique_lock<std::mutex> lock(mutex);

    // Wait if the queue is full
    if (queue_data.size() >= maxSize) {
      if (timeoutMs == 0) return false;  // Non-blocking send
      if (!cv_send.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] {
            return queue_data.size() < maxSize;
          })) {
        return false;  // Timeout occurred
      }
    }

    // Copy item bytes into a new vector<char> and push to queue
    std::vector<char> item_data(actualItemSize);
    std::memcpy(item_data.data(), item, actualItemSize);
    queue_data.push_back(std::move(item_data));  // Use std::move for efficiency

    // Notify any waiting receivers that an item is available
    cv_receive.notify_one();
    return true;
  }

  bool receiveGeneric(void* item, uint32_t timeoutMs) override {
    if (!item) return false;

    std::unique_lock<std::mutex> lock(mutex);

    // Wait if the queue is empty
    if (queue_data.empty()) {
      if (timeoutMs == 0) return false;  // Non-blocking receive
      if (!cv_receive.wait_for(
              lock, std::chrono::milliseconds(timeoutMs), [this] { return !queue_data.empty(); })) {
        return false;
      }
    }

    // Retrieve data and copy to destination
    if (!queue_data.empty()) {  // Check again in case of spurious wakeup or after waiting
      const std::vector<char>& front_item_data = queue_data.front();
      // Sanity check for item size consistency
      if (front_item_data.size() != actualItemSize) {
        // This indicates a severe logic error if items of wrong size are somehow queued.
        return false;
      }
      std::memcpy(item, front_item_data.data(), actualItemSize);
      queue_data.pop_front();

      // Notify any waiting senders that space is available
      cv_send.notify_one();
      return true;
    }
    return false;  // Should not be reached if wait succeeded and queue not empty
  }

  size_t messagesWaiting() const override {
    std::lock_guard<std::mutex> lock(mutex);  // Lock for thread safety on read
    return queue_data.size();
  }

  size_t getItemSize() const override { return actualItemSize; }
};

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

// --- Task management (unchanged from your existing mock) ---
class TaskManager {
 private:
  struct Task {
    std::thread thread;
    std::atomic<bool> running{true};
    std::function<void(void*)> function;
    void* parameter;
  };

  std::map<TaskHandle_t, std::unique_ptr<Task>> tasks;
  std::mutex mutex;  // Protects access to the tasks map

 public:
  static TaskManager& getInstance() {
    static TaskManager instance;
    return instance;
  }

  TaskHandle_t createTask(std::function<void(void*)> func, void* param) {
    std::lock_guard<std::mutex> lock(mutex);

    auto task = std::make_unique<Task>();
    task->function = func;
    task->parameter = param;

    TaskHandle_t handle = reinterpret_cast<TaskHandle_t>(task.get());

    task->thread = std::thread([this, handle, task = task.get()]() {
      while (task->running) {
        task->function(task->parameter);
        break;
      }
      // garbage collection: after the task finishes, we can delete it
      std::thread([this, handle]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        this->deleteTask(handle);
      }).detach();
    });

    tasks[handle] = std::move(task);
    return handle;
  }

  void deleteTask(TaskHandle_t handle) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = tasks.find(handle);
    if (it != tasks.end()) {
      it->second->running = false;  // Signal task thread to stop
      if (it->second->thread.joinable()) {
        it->second->thread.join();  // Wait for the thread to finish execution
      }
      tasks.erase(it);  // Remove from map
    }
  }
};

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
