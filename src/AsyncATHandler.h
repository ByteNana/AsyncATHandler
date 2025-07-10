#pragma once

// For actual Arduino/ESP32
#include <Arduino.h>
#include <Stream.h>

#include <functional>

#include "ATHandler.settings.h"
#include "freertos/FreeRTOS.h"

typedef std::function<void(const String &response)> UnsolicitedCallback;

class AsyncATHandler {
 private:
  Stream *_stream;
  TaskHandle_t readerTask;
  QueueHandle_t commandQueue;
  QueueHandle_t responseQueue;
  SemaphoreHandle_t mutex;
  uint32_t nextCommandId;
  String responseBuffer;
  UnsolicitedCallback unsolicitedCallback;
  volatile bool running;

  PendingCommandInfo pendingSyncCommand;

  // Task function
  static void readerTaskFunction(void *parameter);
  void processIncomingData();
  void handleResponse(const String &response);
  bool isUnsolicitedResponse(const String &response);

 public:
  AsyncATHandler();
  ~AsyncATHandler();

  bool begin(Stream &stream);
  void end();

  bool sendCommandAsync(const String &command);

  bool sendCommand(
      const String &command, String &response, const String &expectedResponse = "OK",
      uint32_t timeout = AT_DEFAULT_TIMEOUT);

  bool sendCommandBatch(
      const String commands[], size_t count, String responses[] = nullptr,
      uint32_t timeout = AT_DEFAULT_TIMEOUT);

  void setUnsolicitedCallback(UnsolicitedCallback callback);

  bool hasResponse();
  ATResponse getResponse();

  void flushResponseQueue();
  size_t getQueuedCommandCount();
  size_t getQueuedResponseCount();
  bool isRunning() const { return running; }
};
