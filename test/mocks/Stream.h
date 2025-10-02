#pragma once

#include <gmock/gmock.h>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>

#include "WString.h"
#include "times.h"

class Stream {
 protected:
  unsigned long _startMillis;
  unsigned long _timeout;

 public:
  Stream(unsigned long timeout = 1000) : _startMillis(0), _timeout(timeout) {}
  virtual ~Stream() = default;
  virtual int available() = 0;
  virtual int read() = 0;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* buffer, size_t size) = 0;
  virtual void flush() = 0;
  virtual int peek() = 0;
  virtual size_t print(int value) { return print(String(value)); }
  virtual size_t print(uint16_t value) { return print(String(value)); }

  virtual size_t println() { return println(""); }  // Empty line
  virtual size_t println(int value) { return println(String(value)); }

  // Add timedRead for HttpClient
  int timedRead() {
    int c;
    _startMillis = millis();
    do {
      c = read();
      if (c >= 0) { return c; }
    } while (millis() - _startMillis < _timeout);
    return -1;  // -1 indicates timeout
  }

  // These `print` methods are fine, they delegate to `write`.
  size_t print(const String& str) {
    return write(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
  }

  size_t println(const String& str) {
    size_t n = print(str);
    n += write('\r');
    n += write('\n');
    return n;
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

class HardwareSerial : public MockStream {
 public:
  HardwareSerial(int indexA) {}
  void begin(unsigned long baud) {}
  void println(const String& str) {}
  void print(const String& str) {}
  size_t write(uint8_t c) { return 1; }
  size_t write(const uint8_t* buffer, size_t size) { return size; }
  int available() { return 0; }
  int read() { return -1; }
  int peek() { return -1; }
};

extern HardwareSerial Serial;
extern HardwareSerial Serial2;
extern HardwareSerial Serial3;
