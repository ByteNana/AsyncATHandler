#include "semphore.h"

#include <condition_variable>
#include <mutex>

struct SemaphoreControlBlock {
  std::mutex mtx;
  std::condition_variable cv;
  bool available{false};
};

extern "C" {

SemaphoreHandle_t xSemaphoreCreateBinary(void) { return new SemaphoreControlBlock(); }

SemaphoreHandle_t xSemaphoreCreateMutex(void) {
  auto* s = new SemaphoreControlBlock();
  s->available = true;  // mutex starts unlocked
  return s;
}

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) {
  if (!xSemaphore) return;
  delete xSemaphore;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
  auto* s = xSemaphore;
  if (!s) return pdFALSE;
  std::lock_guard<std::mutex> lk(s->mtx);
  if (s->available) return pdTRUE;  // already available
  s->available = true;
  s->cv.notify_one();
  return pdTRUE;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) {
  auto* s = xSemaphore;
  if (!s) return pdFALSE;
  std::unique_lock<std::mutex> lk(s->mtx);
  if (!s->available) {
    if (xTicksToWait == 0) return pdFALSE;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(xTicksToWait);
    if (s->cv.wait_until(lk, deadline) == std::cv_status::timeout && !s->available) {
      return pdFALSE;
    }
  }
  // Consume
  s->available = false;
  return pdTRUE;
}

}  // extern "C"
