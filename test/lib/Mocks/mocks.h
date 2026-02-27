#pragma once

#include <Arduino.h>
#include <Stream.h>
#include <gmock/gmock.h>

#include <condition_variable>
#include <mutex>
#include <queue>

// Mock Stream for testing
class MockStream : public Stream {
 private:
  std::queue<uint8_t> rxBuffer;
  std::queue<uint8_t> txBuffer;
  mutable std::mutex rxMutex;
  mutable std::mutex txMutex;
  std::condition_variable dataAvailable;  // For signaling RX data

 public:
  MOCK_METHOD(int, available, (), (override));
  MOCK_METHOD(int, read, (), (override));
  MOCK_METHOD(size_t, write, (uint8_t), (override));
  MOCK_METHOD(size_t, write, (const uint8_t*, size_t), (override));
  MOCK_METHOD(void, flush, (), (override));
  MOCK_METHOD(int, peek, (), (override));

  void SetupDefaults() {
    ON_CALL(*this, available()).WillByDefault([this]() {
      std::lock_guard<std::mutex> lock(rxMutex);
      return static_cast<int>(rxBuffer.size());
    });

    ON_CALL(*this, read()).WillByDefault([this]() {
      std::lock_guard<std::mutex> lock(rxMutex);
      if (rxBuffer.empty()) return -1;
      int c = rxBuffer.front();
      rxBuffer.pop();
      return c;
    });

    ON_CALL(*this, write(testing::_)).WillByDefault([this](uint8_t c) {
      std::lock_guard<std::mutex> lock(txMutex);
      txBuffer.push(c);
      return 1;
    });

    ON_CALL(*this, write(testing::_, testing::_))
        .WillByDefault([this](const uint8_t* buffer, size_t size) {
          std::lock_guard<std::mutex> lock(txMutex);
          for (size_t i = 0; i < size; i++) { txBuffer.push(buffer[i]); }
          return size;
        });

    // Add default action for flush() if not already present
    ON_CALL(*this, flush()).WillByDefault([]() { /* No-op for mock */ });
  }

  void InjectRxData(const std::string& data) {
    std::lock_guard<std::mutex> lock(rxMutex);
    for (char c : data) { rxBuffer.push(static_cast<uint8_t>(c)); }
    dataAvailable.notify_all();  // Notify any waiting readers
  }

  // Retrieve data from the TX buffer to verify outgoing data
  std::string GetTxData() {
    std::lock_guard<std::mutex> lock(txMutex);
    std::string result;
    while (!txBuffer.empty()) {
      result += static_cast<char>(txBuffer.front());
      txBuffer.pop();
    }
    return result;
  }

  // Clear the TX buffer
  void ClearTxData() {
    std::lock_guard<std::mutex> lock(txMutex);
    std::queue<uint8_t> emptyQueue;
    std::swap(txBuffer, emptyQueue);
  }
  void ClearRxData() {
    std::lock_guard<std::mutex> lock(rxMutex);
    std::queue<uint8_t> emptyQueue;
    std::swap(rxBuffer, emptyQueue);
  }
  operator bool() const { return true; }
};
