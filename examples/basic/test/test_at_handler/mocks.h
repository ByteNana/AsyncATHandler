#pragma once

#include <Arduino.h>
#include <Stream.h>
#include <vector>

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
