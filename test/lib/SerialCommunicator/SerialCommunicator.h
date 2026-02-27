#pragma once

#ifdef ESP32
#include <Arduino.h>
#endif  // ESP32

#include "common.h"
class SerialCommunicator : public Stream {
 private:
#ifdef ESP32
  HardwareSerial *hardwareStream = nullptr;
#else
  Stream *hardwareStream = nullptr;  // For native, it's a generic Stream
#endif
  MockStream *mockStream = nullptr;
  Stream *activeStream = nullptr;  // points to hardwareStream or mockStream

 public:
  SerialCommunicator();
  ~SerialCommunicator();

  Stream *getActiveStream() { return activeStream; }

  void mockResponse(const std::string &data);
  void mockResponseWithDelay(const std::string &data, uint32_t delayMs = 50);
  void ClearSentData();

  // Stream interface
  int available() override;
  int read() override;
  int peek() override;
  void flush() override;
  size_t write(uint8_t) override;
  size_t write(const uint8_t *buffer, size_t size) override;
};

inline SerialCommunicator::SerialCommunicator() {
  mockStream = new ::testing::NiceMock<MockStream>();
  mockStream->SetupDefaults();

#ifdef ESP32
  Serial2.setTxBufferSize(2048);
  Serial2.setRxBufferSize(2048);
  Serial2.begin(115200);
  activeStream = &Serial2;
#else
  activeStream = mockStream;  // For native, use mock stream by default
#endif  // ESP32
}

inline SerialCommunicator::~SerialCommunicator() {
#ifdef ESP32
  Serial2.end();
#endif  // ESP32

  delete mockStream;
  mockStream = nullptr;
}

inline void SerialCommunicator::mockResponse(const std::string &data) {
  mockStream->InjectRxData(data);
}

inline void SerialCommunicator::ClearSentData() {
  if (mockStream) { mockStream->ClearTxData(); }
}

inline int SerialCommunicator::available() { return activeStream->available(); }

inline int SerialCommunicator::read() { return activeStream->read(); }

inline int SerialCommunicator::peek() { return activeStream->peek(); }

inline void SerialCommunicator::flush() { activeStream->flush(); }

inline void SerialCommunicator::mockResponseWithDelay(const std::string &data, uint32_t delayMs) {
  InjectDataWithDelay(mockStream, data, delayMs);
}

inline size_t SerialCommunicator::write(uint8_t c) {
  size_t result = activeStream->write(c);
  if (activeStream != mockStream) { mockStream->write(c); }
  return result;
}

inline size_t SerialCommunicator::write(const uint8_t *buffer, size_t size) {
  size_t result = activeStream->write(buffer, size);
  if (activeStream != mockStream) { mockStream->write(buffer, size); }
  return result;
}
