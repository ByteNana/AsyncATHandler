#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>

typedef void* QueueHandle_t;

class ThreadSafeQueueBase {
 public:
  virtual ~ThreadSafeQueueBase() = default;
  virtual bool sendGeneric(const void* item, uint32_t timeoutMs) = 0;
  virtual bool receiveGeneric(void* item, uint32_t timeoutMs) = 0;
  virtual size_t messagesWaiting() const = 0;
  virtual size_t getItemSize() const = 0;
};

class RawByteThreadSafeQueue : public ThreadSafeQueueBase {
 private:
  std::deque<std::vector<char>> queue_data;
  mutable std::mutex mutex;
  std::condition_variable cv_send;
  std::condition_variable cv_receive;
  size_t maxSize;
  size_t actualItemSize;

 public:
  explicit RawByteThreadSafeQueue(size_t length, size_t item_size);

  bool sendGeneric(const void* item, uint32_t timeoutMs) override;
  bool receiveGeneric(void* item, uint32_t timeoutMs) override;
  size_t messagesWaiting() const override;
  size_t getItemSize() const override;
};
