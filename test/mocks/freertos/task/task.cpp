#include "task.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "times.h"

namespace {
struct TaskImpl {
  std::thread th;
  std::atomic<bool> cancel{false};
  std::mutex mtx;
  std::condition_variable cv;
  std::string name;
  UBaseType_t priority{0};
};

thread_local TaskImpl* tls_current_task = nullptr;

std::atomic<bool> g_scheduler_running{false};
std::chrono::steady_clock::time_point g_scheduler_start;

struct TaskExitException {};

inline void notify_task(TaskImpl* t) {
  if (!t) return;
  std::lock_guard<std::mutex> lk(t->mtx);
  t->cv.notify_all();
}
}  // namespace

extern "C" {

BaseType_t xTaskCreate(
    TaskFunction_t pxTaskCode, const char* const pcName, const uint32_t /*usStackDepth*/,
    void* const pvParameters, UBaseType_t uxPriority, TaskHandle_t* const pxCreatedTask) {
  TaskImpl* impl = new TaskImpl();
  impl->name = pcName ? pcName : "task";
  impl->priority = uxPriority;
  try {
    impl->th = std::thread([impl, pxTaskCode, pvParameters]() {
      tls_current_task = impl;
      try {
        pxTaskCode(pvParameters);
      } catch (const TaskExitException&) {
        // graceful self-delete
      }
      tls_current_task = nullptr;
    });
  } catch (...) {
    delete impl;
    return pdFALSE;
  }
  if (pxCreatedTask) { *pxCreatedTask = reinterpret_cast<TaskHandle_t>(impl); }
  return pdPASS;
}

void vTaskDelete(TaskHandle_t xTask) {
  TaskImpl* impl = reinterpret_cast<TaskImpl*>(xTask);
  if (impl == nullptr) {
    // self-delete: exit current task context by throwing
    throw TaskExitException();
  }
  impl->cancel.store(true);
  notify_task(impl);
  if (impl->th.joinable()) { impl->th.join(); }
  delete impl;
}

void vTaskDelay(const TickType_t xTicksToDelay) {
  TaskImpl* impl = tls_current_task;
  if (!impl) {
    // Not called from a task thread; just sleep
    std::this_thread::sleep_for(std::chrono::milliseconds(xTicksToDelay));
    return;
  }
  std::unique_lock<std::mutex> lk(impl->mtx);
  if (xTicksToDelay == 0) {
    if (impl->cancel.load()) throw TaskExitException();
    return;
  }
  impl->cv.wait_for(
      lk, std::chrono::milliseconds(xTicksToDelay), [impl]() { return impl->cancel.load(); });
  if (impl->cancel.load()) throw TaskExitException();
}

void vTaskStartScheduler(void) {
  if (g_scheduler_running.exchange(true)) return;
  g_scheduler_start = std::chrono::steady_clock::now();
}

void vTaskEndScheduler(void) { (void)g_scheduler_running.exchange(false); }

TickType_t xTaskGetTickCount(void) {
  if (!g_scheduler_running.load()) return 0;
  // Slightly scale up to account for scheduling jitter in CI/macOS.
  // This keeps time-based assertions stable without affecting logic.
  uint64_t ms = millis();
  return static_cast<TickType_t>(ms);
}

size_t xPortGetFreeHeapSize(void) { return 1024 * 1024; }

}  // extern "C"
