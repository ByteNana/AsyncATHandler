#pragma once
#include <Arduino.h>
#include <Stream.h>

#include <functional>
#include <memory>
#include <vector>

#include "ATPromise/ATPromise.h"
#include "ATResponse/ATResponse.h"
#include "freertos/FreeRTOS.h"

class AsyncATHandler {
 private:
  Stream* stream = nullptr;
  TaskHandle_t readerTask = nullptr;
  SemaphoreHandle_t mutex = nullptr;

  String lineBuffer = "";
  std::vector<std::unique_ptr<ATPromise>> pendingPromises;
  URCCallback urcCallback = nullptr;

  uint32_t nextCommandId = 1;

  static void readerTaskFunction(void* parameter);
  void processIncomingData();
  void processCompleteLine(const String& line);

  ResponseType classifyLine(const String& line);
  ATPromise* findPromiseForResponse(const String& line);
  void handleUnsolicitedResponse(const String& line);

  bool isLineComplete(String& buffer);
  void cleanupCompletedPromises();

 public:
  AsyncATHandler();
  ~AsyncATHandler();

  bool begin(Stream& stream);
  void end();

  ATPromise* sendCommand(const String& command);

  template <typename... Args>
  ATPromise* sendCommand(Args... parts) {
    String command = "";
    ((command += String(parts)), ...);
    return sendCommand(command);
  }

  bool sendSync(const String& command, String& response, uint32_t timeout = 5000);
  bool sendSync(const String& command, uint32_t timeout = 5000);

  std::unique_ptr<ATPromise> popCompletedPromise(uint32_t commandId);

  void onURC(URCCallback callback) { urcCallback = callback; }

  Stream* getStream() { return stream; }
};
