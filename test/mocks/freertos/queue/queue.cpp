#include "queue.h"

#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

struct QueueControlBlock {
  std::mutex mtx;
  std::condition_variable cv_not_full;
  std::condition_variable cv_not_empty;
  UBaseType_t capacity{0};
  UBaseType_t item_size{0};
  std::deque<std::vector<uint8_t>> q;
};

extern "C" {

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize) {
  if (uxQueueLength == 0 || uxItemSize == 0) return nullptr;
  auto* q = new QueueControlBlock();
  q->capacity = uxQueueLength;
  q->item_size = uxItemSize;
  return q;
}

void vQueueDelete(QueueHandle_t xQueue) {
  auto* q = xQueue;
  if (!q) return;
  delete q;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait) {
  auto* q = xQueue;
  if (!q || !pvItemToQueue) return pdFALSE;
  std::unique_lock<std::mutex> lk(q->mtx);
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(xTicksToWait);
  while (q->q.size() >= q->capacity) {
    if (xTicksToWait == 0) return pdFALSE;
    if (q->cv_not_full.wait_until(lk, deadline) == std::cv_status::timeout) {
      if (q->q.size() >= q->capacity) return pdFALSE;
      break;
    }
  }
  std::vector<uint8_t> item(q->item_size);
  std::memcpy(item.data(), pvItemToQueue, q->item_size);
  q->q.push_back(std::move(item));
  q->cv_not_empty.notify_one();
  return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait) {
  auto* q = xQueue;
  if (!q || !pvBuffer) return pdFALSE;
  std::unique_lock<std::mutex> lk(q->mtx);
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(xTicksToWait);
  while (q->q.empty()) {
    if (xTicksToWait == 0) return pdFALSE;
    if (q->cv_not_empty.wait_until(lk, deadline) == std::cv_status::timeout) {
      if (q->q.empty()) return pdFALSE;
      break;
    }
  }
  auto item = std::move(q->q.front());
  q->q.pop_front();
  std::memcpy(pvBuffer, item.data(), q->item_size);
  q->cv_not_full.notify_one();
  return pdTRUE;
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue) {
  auto* q = xQueue;
  if (!q) return 0;
  std::lock_guard<std::mutex> lk(q->mtx);
  return static_cast<UBaseType_t>(q->q.size());
}

}  // extern "C"
