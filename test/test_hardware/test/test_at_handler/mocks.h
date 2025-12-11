#pragma once

#include <Arduino.h>
#include <Stream.h>

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <gmock/gmock.h>

class HardwareMockStream : public Stream {
 private:
  String _rxBuffer;  // Holds the data we want the device to "receive"
  String _txBuffer;  // Captures the data our code "sends"
  int _rxReadIndex = 0;
  std::vector<String> _responses;  // Pre-parsed response chunks
  size_t _nextResponse = 0;

 public:
  HardwareMockStream() {}
  void mockResponse(const String& response) {
    _rxBuffer = "";
    _rxReadIndex = 0;
    _responses.clear();
    _nextResponse = 0;

    if (response.length() == 0) { return; }

    String chunk = "";
    size_t pos = 0;
    while (pos < response.length()) {
      int end = response.indexOf("\r\n", pos);
      if (end == -1) {
        chunk += response.substring(pos);
        break;
      }
      String line = response.substring(pos, end + 2);
      chunk += line;

      String trimmed = line;
      trimmed.trim();
      if (trimmed == "OK" || trimmed == "ERROR") {
        _responses.push_back(chunk);
        chunk = "";
      }
      pos = end + 2;
    }
    if (chunk.length() > 0) { _responses.push_back(chunk); }
  }
  String getSentData() { return _txBuffer; }
  void clearSentData() { _txBuffer = ""; }

  virtual int available() override { return _rxBuffer.length() - _rxReadIndex; }

  virtual int read() override {
    if (_rxReadIndex < _rxBuffer.length()) { return _rxBuffer.charAt(_rxReadIndex++); }
    return -1;
  }

  virtual int peek() override {
    if (_rxReadIndex < _rxBuffer.length()) { return _rxBuffer.charAt(_rxReadIndex); }
    return -1;
  }

  virtual size_t write(uint8_t c) override {
    _txBuffer += (char)c;
    if (c == '\n' && _nextResponse < _responses.size()) {
      _rxBuffer += _responses[_nextResponse++];
    }
    return 1;
  }

  virtual size_t write(const uint8_t* buffer, size_t size) override {
    for (size_t i = 0; i < size; i++) { write(buffer[i]); }
    return size;
  }

  virtual void flush() override {
    // Not needed for this mock
  }
};

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
