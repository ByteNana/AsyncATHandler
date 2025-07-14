#pragma once

typedef void* QueueHandle_t;

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
