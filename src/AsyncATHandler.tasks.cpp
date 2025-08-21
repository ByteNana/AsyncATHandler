#include "AsyncATHandler.h"

#include <esp_log.h>

bool AsyncATHandler::isCompleteLineInBuffer() {
  return responseBufferPos >= 2 && responseBuffer[responseBufferPos - 2] == '\r' &&
         responseBuffer[responseBufferPos - 1] == '\n';
}

bool AsyncATHandler::addCharToBuffer(char c) {
  if (responseBufferPos < AT_RESPONSE_BUFFER_SIZE - 1) {
    responseBuffer[responseBufferPos++] = c;
    responseBuffer[responseBufferPos] = '\0';
    return true;
  }
  return false;
}

void AsyncATHandler::processIncomingData() {
  if (!xSemaphoreTake(mutex, pdMS_TO_TICKS(100))) { return; }

  if (!_stream) {
    xSemaphoreGive(mutex);
    return;
  }

  while (_stream->available()) {
    char c = static_cast<char>(_stream->read());

    if (!addCharToBuffer(c)) {
      log_w("responseBuffer overflow. Clearing.");
      flushResponseBuffer();
      continue;
    }

    if (isCompleteLineInBuffer()) {
      // Check if we have a complete AT response
      if (isResponseComplete()) {
        // Process the complete response
        handleResponse(responseBuffer);
      }
    }
  }

  xSemaphoreGive(mutex);
}

void AsyncATHandler::readerTaskFunction(void* parameter) {
  AsyncATHandler* handler = static_cast<AsyncATHandler*>(parameter);

  while (true) {
    handler->processIncomingData();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
