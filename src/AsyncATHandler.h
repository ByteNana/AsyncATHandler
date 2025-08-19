#pragma once
#include <Arduino.h>
#include <Stream.h>

#include <functional>

#include "ATHandler.settings.h"
#include "freertos/FreeRTOS.h"

typedef std::function<void(const char *response)> UnsolicitedCallback;

class AsyncATHandler {
 private:
  TaskHandle_t readerTask = nullptr;
  SemaphoreHandle_t mutex = nullptr;

  char responseBuffer[AT_RESPONSE_BUFFER_SIZE];
  size_t responseBufferPos;

  static void readerTaskFunction(void *parameter);
  void flushResponseBuffer();
  void processIncomingData();
  bool isCompleteLineInBuffer();
  bool addCharToBuffer(char c);
  void handleResponse(const char *response);
  String sanitizeResponseBuffer(const String &expectedResponse);

 public:
  AsyncATHandler();
  ~AsyncATHandler();

  Stream *_stream;
  bool begin(Stream &stream);
  void end();
  template <typename... Args>
  void sendAT(Args... cmd);
  int8_t waitResponseMultiple(uint32_t timeout, const char *expectedResponses[], size_t count);
  int8_t waitResponse(uint32_t timeout = 5000);

  template <typename FirstArg, typename... RestArgs>
  int8_t waitResponse(FirstArg &&first, RestArgs &&...rest);

  bool sendCommand(
      const String &command, String &response, const String &expectedResponse = "OK",
      uint32_t timeout = AT_DEFAULT_TIMEOUT);
  bool sendCommand(
      const String &command, const String &expectedResponse = "OK",
      uint32_t timeout = AT_DEFAULT_TIMEOUT);
  template <typename... Args>
  bool sendCommand(
      String &response, const String &expectedResponse = "OK",
      uint32_t timeout = AT_DEFAULT_TIMEOUT, Args &&...parts);

  String getResponse(const String &expectedResponse);
};

#include "AsyncATHandler.tpp"
