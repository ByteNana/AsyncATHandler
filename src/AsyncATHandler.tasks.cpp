#include "AsyncATHandler.h"
#include <esp_log.h>

void AsyncATHandler::readerTaskFunction(void* parameter) {
  AsyncATHandler* handler = static_cast<AsyncATHandler*>(parameter);
  log_i("Reader task started.");
  while (true) {
    handler->processIncomingData();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void AsyncATHandler::processIncomingData() {
  if (!stream || !stream->available()) {
    return;
  }

  while (stream->available()) {
    char c = stream->read();
    lineBuffer += c;

    if (isLineComplete(lineBuffer)) {
      log_d("Processing line: '%s'", lineBuffer.c_str());
      processCompleteLine(lineBuffer);
      lineBuffer = "";
    }

    if (lineBuffer.length() > 512) {
      log_w("Line buffer overflow, clearing.");
      lineBuffer = "";
    }
  }
}

void AsyncATHandler::processCompleteLine(const String& line) {
  if (line.length() < 2) {
    return;
  }

  ResponseType type = classifyLine(line);

  ResponseLine responseLine;
  responseLine.content = line;
  responseLine.type = type;
  responseLine.timestamp = millis();
  responseLine.commandId = 0;

  if (type == ResponseType::UNSOLICITED) {
    handleUnsolicitedResponse(line);
    return;
  }

  ATPromise* promise = findPromiseForResponse(line);

  if (promise && mutex) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10))) {
      responseLine.commandId = promise->getId();
      promise->addResponseLine(responseLine);
      xSemaphoreGive(mutex);
    } else {
      log_e("Failed to acquire mutex for adding response");
    }
  }
}

void AsyncATHandler::handleUnsolicitedResponse(const String& line) {
  if (urcCallback) {
    urcCallback(line);
  }
}
