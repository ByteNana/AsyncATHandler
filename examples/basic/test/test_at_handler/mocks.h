#pragma once

#include <Arduino.h>
#include <Stream.h>

class HardwareMockStream : public Stream {
 private:
  String _rxBuffer;  // Holds the data we want the device to "receive"
  String _txBuffer;  // Captures the data our code "sends"
  int _rxReadIndex = 0;

 public:
  HardwareMockStream() {}
  void mockResponse(const String& response) {
    _rxBuffer = response;
    _rxReadIndex = 0;
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
    return 1;
  }

  virtual size_t write(const uint8_t* buffer, size_t size) override {
    for (size_t i = 0; i < size; i++) { _txBuffer += (char)buffer[i]; }
    return size;
  }

  virtual void flush() override {
    // Not needed for this mock
  }
};
