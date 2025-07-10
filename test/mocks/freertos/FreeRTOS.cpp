#include "FreeRTOS.h"  // Our mock FreeRTOS header (defines RecursiveMutex, etc.)

#include <atomic>              // For std::atomic (used for mock ticks)
#include <condition_variable>  // For std::condition_variable (used internally by mocks)
#include <cstring>             // For memcpy (used internally by RawByteThreadSafeQueue)
#include <iostream>            // For std::cerr, std::cout
#include <map>                 // For std::map used in global handle registries
#include <mutex>               // For std::mutex, std::lock_guard (used internally by some mocks)
#include <vector>              // For std::vector<char> (used internally by RawByteThreadSafeQueue)

#include "esp_log.h"

static std::map<QueueHandle_t, std::shared_ptr<ThreadSafeQueueBase>> s_activeQueues;
static std::mutex s_queueMapMutex;  // Protects access to s_activeQueues map

static std::map<SemaphoreHandle_t, std::shared_ptr<BinarySemaphore>> s_activeSemaphores;
static std::mutex s_semaphoreMapMutex;  // Protects access to s_activeSemaphores map

static std::map<SemaphoreHandle_t, std::shared_ptr<RecursiveMutex>> s_activeMutexes;
static std::mutex s_mutexMapMutex;  // Protects s_activeMutexes map

static std::atomic<TickType_t> s_mock_ticks_count(0);

// Helper to advance mock ticks, used by vTaskDelay
void advance_mock_ticks(TickType_t ticks) { s_mock_ticks_count += ticks; }

extern "C" {

TickType_t pdMS_TO_TICKS(uint32_t ms) {
  return ms;  // Simple mapping: 1 tick = 1 millisecond for the mock
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue) {
  std::lock_guard<std::mutex> lock(s_queueMapMutex);  // Protect map access
  auto it = s_activeQueues.find(xQueue);
  if (it != s_activeQueues.end()) {
    return it->second->messagesWaiting();  // Call virtual method on base pointer
  }
  log_e("FreeRTOS Mock ERROR: uxQueueMessagesWaiting called with invalid queue handle: %p", xQueue);
  return 0;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait) {
  std::shared_ptr<ThreadSafeQueueBase> target_queue;
  {  // Acquire lock for map lookup only
    std::lock_guard<std::mutex> lock(s_queueMapMutex);
    auto it = s_activeQueues.find(xQueue);
    if (it != s_activeQueues.end()) {
      target_queue = it->second;
    } else {
      log_e("FreeRTOS Mock ERROR: xQueueReceive called with invalid queue handle: %p", xQueue);
      return pdFALSE;
    }
  }  // Release map lock immediately

  // Call the generic receive method on the actual queue object.
  // The internal mutex in RawByteThreadSafeQueue handles blocking and thread safety for the queue
  // data.
  return target_queue->receiveGeneric(pvBuffer, xTicksToWait) ? pdTRUE : pdFALSE;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait) {
  std::shared_ptr<ThreadSafeQueueBase> target_queue;
  {  // Acquire lock for map lookup only
    std::lock_guard<std::mutex> lock(s_queueMapMutex);
    auto it = s_activeQueues.find(xQueue);
    if (it != s_activeQueues.end()) {
      target_queue = it->second;
    } else {
      log_e("FreeRTOS Mock ERROR: xQueueSend called with invalid queue handle: %p", xQueue);
      return pdFALSE;
    }
  }  // Release map lock immediately

  // Call the generic send method on the actual queue object.
  return target_queue->sendGeneric(pvItemToQueue, xTicksToWait) ? pdTRUE : pdFALSE;
}

void vQueueDelete(QueueHandle_t xQueue) {
  std::lock_guard<std::mutex> lock(s_queueMapMutex);  // Protect map access
  auto it = s_activeQueues.find(xQueue);
  if (it != s_activeQueues.end()) {
    s_activeQueues.erase(it);  // shared_ptr automatically deletes the object
  } else {
    log_w("FreeRTOS Mock WARNING: Attempted to delete unknown queue handle: %p", xQueue);
  }
}

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize) {
  std::lock_guard<std::mutex> lock(s_queueMapMutex);  // Protect map access
  if (uxQueueLength == 0 || uxItemSize == 0) {
    log_e("FreeRTOS Mock ERROR: xQueueCreate called with length 0 or itemSize 0.");
    return nullptr;
  }

  // CRITICAL: Always create a RawByteThreadSafeQueue regardless of itemSize.
  // This makes the mock truly type-agnostic at this level.
  std::shared_ptr<ThreadSafeQueueBase> newQueue =
      std::make_shared<RawByteThreadSafeQueue>(uxQueueLength, uxItemSize);

  if (newQueue) {
    // The handle is just the raw pointer to the RawByteThreadSafeQueue object.
    // The shared_ptr in the map manages its lifetime.
    QueueHandle_t handle = reinterpret_cast<QueueHandle_t>(newQueue.get());
    s_activeQueues[handle] = newQueue;
    return handle;
  }
  return nullptr;
}

BaseType_t xTaskCreatePinnedToCore(
    void (*taskFunction)(void*), const char* name, uint32_t stackSize, void* parameter,
    UBaseType_t priority, TaskHandle_t* handle, BaseType_t coreID) {
  // Delegate task creation to the TaskManager mock.
  std::function<void(void*)> func = taskFunction;
  TaskHandle_t createdHandle = TaskManager::getInstance().createTask(func, parameter);
  if (handle) { *handle = createdHandle; }
  return (createdHandle != nullptr) ? pdTRUE : pdFALSE;
}

void vTaskDelete(TaskHandle_t handle) {
  if (handle == NULL) {  // vTaskDelete(NULL) means delete self
    // In the mock, the TaskManager's thread lambda will exit naturally after this.
    return;
  }
  // Delegate task deletion to the TaskManager mock.
  TaskManager::getInstance().deleteTask(handle);
}

void vTaskDelay(TickType_t ticks) {
  if (ticks > 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
    advance_mock_ticks(ticks);  // Advance mock tick count
  }
}

TickType_t xTaskGetTickCount() {
  return s_mock_ticks_count.load();  // Atomically load current tick count
}

SemaphoreHandle_t xSemaphoreCreateBinary() {
  std::lock_guard<std::mutex> lock(s_semaphoreMapMutex);  // Protect binary semaphore map
  auto sem = std::make_shared<BinarySemaphore>();
  SemaphoreHandle_t handle = reinterpret_cast<SemaphoreHandle_t>(sem.get());
  s_activeSemaphores[handle] = sem;
  return handle;
}

// --- NEW: xSemaphoreCreateMutex to use RecursiveMutex ---
SemaphoreHandle_t xSemaphoreCreateMutex() {
  std::lock_guard<std::mutex> lock(s_mutexMapMutex);  // Protect mutex map
  auto mtx = std::make_shared<RecursiveMutex>();
  SemaphoreHandle_t handle = reinterpret_cast<SemaphoreHandle_t>(mtx.get());
  s_activeMutexes[handle] = mtx;
  return handle;
}

void vSemaphoreDelete(SemaphoreHandle_t sem) {
  // Try to delete from binary semaphores map first
  {
    std::lock_guard<std::mutex> lock(s_semaphoreMapMutex);
    auto it = s_activeSemaphores.find(sem);
    if (it != s_activeSemaphores.end()) {
      s_activeSemaphores.erase(it);
      return;  // Found and deleted
    }
  }
  // If not found in binary semaphores, try to delete from recursive mutexes map
  {
    std::lock_guard<std::mutex> lock(s_mutexMapMutex);
    auto it = s_activeMutexes.find(sem);
    if (it != s_activeMutexes.end()) {
      s_activeMutexes.erase(it);
      return;  // Found and deleted
    }
  }
  log_w("FreeRTOS Mock WARNING: vSemaphoreDelete called with unknown handle: %p", sem);
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) {
  // Try to give a binary semaphore
  {
    std::shared_ptr<BinarySemaphore> target_sem;
    {
      std::lock_guard<std::mutex> lock(s_semaphoreMapMutex);
      auto it = s_activeSemaphores.find(sem);
      if (it != s_activeSemaphores.end()) target_sem = it->second;
    }
    if (target_sem) {
      target_sem->give();
      return pdTRUE;
    }
  }
  // If not a binary semaphore, try to give a recursive mutex
  {
    std::shared_ptr<RecursiveMutex> target_mtx;
    {
      std::lock_guard<std::mutex> lock(s_mutexMapMutex);
      auto it = s_activeMutexes.find(sem);
      if (it != s_activeMutexes.end()) target_mtx = it->second;
    }
    if (target_mtx) {
      target_mtx->give();  // RecursiveMutex::give includes ownership check
      return pdTRUE;       // Assume give succeeded if ownership check passed internally
    }
  }
  log_e("FreeRTOS Mock ERROR: xSemaphoreGive called with invalid handle: %p", sem);
  return pdFALSE;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout) {
  // Try to take a binary semaphore
  {
    std::shared_ptr<BinarySemaphore> target_sem;
    {
      std::lock_guard<std::mutex> lock(s_semaphoreMapMutex);
      auto it = s_activeSemaphores.find(sem);
      if (it != s_activeSemaphores.end()) target_sem = it->second;
    }
    if (target_sem) { return target_sem->take(timeout) ? pdTRUE : pdFALSE; }
  }
  // If not a binary semaphore, try to take a recursive mutex
  {
    std::shared_ptr<RecursiveMutex> target_mtx;
    {
      std::lock_guard<std::mutex> lock(s_mutexMapMutex);
      auto it = s_activeMutexes.find(sem);
      if (it != s_activeMutexes.end()) target_mtx = it->second;
    }
    if (target_mtx) { return target_mtx->take(timeout) ? pdTRUE : pdFALSE; }
  }
  log_e("FreeRTOS Mock ERROR: xSemaphoreTake called with invalid handle: %p", sem);
  return pdFALSE;
}

}  // extern "C"
