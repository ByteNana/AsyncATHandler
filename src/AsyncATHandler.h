#pragma once
#include <Arduino.h>
#include <Stream.h>
#include <functional>
#include "ATHandler.settings.h"
#include "freertos/FreeRTOS.h"

typedef std::function<void(const char *response)> UnsolicitedCallback;

class AsyncATHandler {
 private:
  TaskHandle_t readerTask;
  QueueHandle_t commandQueue;
  QueueHandle_t responseQueue;
  SemaphoreHandle_t mutex;
  uint32_t nextCommandId;
  char responseBuffer[AT_RESPONSE_BUFFER_SIZE];
  size_t responseBufferPos;
  UnsolicitedCallback unsolicitedCallback;

  // Task function
  static void readerTaskFunction(void *parameter);

  // Response processing functions
  void processIncomingData();
  void handleResponse(const char *response);

  // Helper functions
  bool lineCompletesCommand(const String &line, const char *expectedResponse);
  int8_t checkLineForExpectedResponses(const String &line, const String *responses, size_t count);

  // Utility functions
  bool isUnsolicitedResponse(const char *response);
  void handleBufferOverflow();
  void clearResponseBuffer();
  bool addCharToBuffer(char c);
  bool isCompleteLineInBuffer();
  String trimAndValidateResponse(const char *response);

 public:
  Stream *_stream;
  AsyncATHandler();
  ~AsyncATHandler();
  bool begin(Stream &stream);
  void end();
  bool sendCommandAsync(const String &command);
  bool sendCommand(
      const String &command, String &response, const String &expectedResponse = "OK",
      uint32_t timeout = AT_DEFAULT_TIMEOUT);
  bool sendCommand(
      const String &command, const String &expectedResponse = "OK",
      uint32_t timeout = AT_DEFAULT_TIMEOUT);
  template <typename... Args>
  bool sendCommand(
      String &response, const String &expectedResponse = "OK",
      uint32_t timeout = AT_DEFAULT_TIMEOUT, Args &&...parts) {
    String cmd;
    size_t reserveSize = 0;
    (void)std::initializer_list<int>{(reserveSize += String(parts).length(), 0)...};
    cmd.reserve(reserveSize);
    (void)std::initializer_list<int>{(cmd += String(parts), 0)...};
    return sendCommand(cmd, response, expectedResponse, timeout);
  }
  bool sendCommandBatch(
      const String commands[], size_t count, String responses[] = nullptr,
      uint32_t timeout = AT_DEFAULT_TIMEOUT);
  int waitResponse(
      const String &expectedResponse, String &response, uint32_t timeout = AT_DEFAULT_TIMEOUT);

  // Enhanced waitResponse methods
  int8_t waitResponse(uint32_t timeout = AT_DEFAULT_TIMEOUT);

  // Variadic template for multiple expected responses
  template <typename... Args>
  int8_t waitResponse(Args&&... expectedResponses);

  template <typename... Args>
  int8_t waitResponse(uint32_t timeout, Args&&...expectedResponses);

  void setUnsolicitedCallback(UnsolicitedCallback callback);
  bool hasResponse();
  ATResponse getResponse();
  void flushResponseQueue();
  size_t getQueuedCommandCount();
  size_t getQueuedResponseCount();
};

#include "AsyncATHandler.tpp"
