#pragma once

// For actual Arduino/ESP32
#include <Arduino.h>
#include <Stream.h>

#include <functional>

#include "ATHandler.settings.h"  // Includes the updated struct definitions
#include "freertos/FreeRTOS.h"

// Define the UnsolicitedCallback type to accept const char*
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
  volatile bool running;

  PendingCommandInfo pendingSyncCommand;

  // Task function
  static void readerTaskFunction(void *parameter);
  void processIncomingData();
  void handleResponse(const char *response);
  bool isUnsolicitedResponse(const char *response);

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
  ;

  void setUnsolicitedCallback(UnsolicitedCallback callback);

  bool hasResponse();
  ATResponse getResponse();

  void flushResponseQueue();
  size_t getQueuedCommandCount();
  size_t getQueuedResponseCount();
  bool isRunning() const { return running; }
};
