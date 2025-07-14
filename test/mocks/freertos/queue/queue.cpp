#include "queue.h"

RawByteThreadSafeQueue::RawByteThreadSafeQueue(size_t length, size_t item_size)
    : maxSize(length), actualItemSize(item_size) {}

bool RawByteThreadSafeQueue::sendGeneric(const void* item, uint32_t timeoutMs) {
  if (!item) return false;

  std::unique_lock<std::mutex> lock(mutex);

  if (queue_data.size() >= maxSize) {
    if (timeoutMs == 0) return false;
    if (!cv_send.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] {
          return queue_data.size() < maxSize;
        })) {
      return false;
    }
  }

  std::vector<char> item_data(actualItemSize);
  std::memcpy(item_data.data(), item, actualItemSize);
  queue_data.push_back(std::move(item_data));

  cv_receive.notify_one();
  return true;
}

bool RawByteThreadSafeQueue::receiveGeneric(void* item, uint32_t timeoutMs) {
  if (!item) return false;

  std::unique_lock<std::mutex> lock(mutex);

  if (queue_data.empty()) {
    if (timeoutMs == 0) return false;
    if (!cv_receive.wait_for(
            lock, std::chrono::milliseconds(timeoutMs), [this] { return !queue_data.empty(); })) {
      return false;
    }
  }

  if (!queue_data.empty()) {
    const std::vector<char>& front_item_data = queue_data.front();
    if (front_item_data.size() != actualItemSize) { return false; }

    std::memcpy(item, front_item_data.data(), actualItemSize);
    queue_data.pop_front();
    cv_send.notify_one();
    return true;
  }

  return false;
}

size_t RawByteThreadSafeQueue::messagesWaiting() const {
  std::lock_guard<std::mutex> lock(mutex);
  return queue_data.size();
}

size_t RawByteThreadSafeQueue::getItemSize() const { return actualItemSize; }
