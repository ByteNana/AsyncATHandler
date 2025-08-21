#include "AsyncATHandler.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

void AsyncATHandler::handleResponse(const char* response) {
  String line = String(response);
  if (line.length() == 0) { return; }

  // Check if this is a final response (OK, ERROR, +CME ERROR)
  if (line.indexOf("OK") != -1 || line.indexOf("ERROR") != -1 || line.indexOf("+CME ERROR") != -1) {
    hasCompleteResponse = true;
  }
}

int AsyncATHandler::readData(uint8_t* buf, size_t size) {
  if (size == 0) return 0;

  // First try pending data buffer
  if (pendingDataPos > 0) {
    int bytesToRead = min((int)size, (int)pendingDataPos);
    memcpy(buf, pendingDataBuffer, bytesToRead);

    memmove(pendingDataBuffer, pendingDataBuffer + bytesToRead, pendingDataPos - bytesToRead);
    pendingDataPos -= bytesToRead;
    pendingDataBuffer[pendingDataPos] = '\0';

    return bytesToRead;
  }

  // Try to extract complete response
  if (isResponseComplete()) {
    extractCompleteResponse();
    if (pendingDataPos > 0) {
      return readData(buf, size);  // Recursive call
    }
  }

  return 0;
}

int AsyncATHandler::readFromBuffer() {
  if (responseBufferPos > 0) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100))) {
      char c = responseBuffer[0];
      memmove(responseBuffer, responseBuffer + 1, responseBufferPos);
      responseBufferPos--;
      responseBuffer[responseBufferPos] = '\0';
      xSemaphoreGive(mutex);
      return c;
    }
  }
  return -1;
}

int AsyncATHandler::read() {
  // First check if we have data in pending buffer
  if (pendingDataPos > 0) {
    char c = pendingDataBuffer[0];
    memmove(pendingDataBuffer, pendingDataBuffer + 1, pendingDataPos - 1);
    pendingDataPos--;
    pendingDataBuffer[pendingDataPos] = '\0';
    return c;
  }

  // Try to extract a complete response if available
  if (isResponseComplete()) {
    extractCompleteResponse();
    if (pendingDataPos > 0) {
      return read();  // Recursive call to get first char
    }
  }

  return -1;  // No data available
}

int8_t AsyncATHandler::waitResponseMultiple(
    uint32_t timeout, const char* expectedResponses[], size_t count) {
  unsigned long startTime = millis();

  if (count == 0 || expectedResponses == nullptr) {
    expectedResponses[0] = "OK";
    count = 1;
  }

  log_d("Waiting for any of %zu expected responses, Timeout: %lu ms", count, timeout);
  while (millis() - startTime < timeout) {
    if (strstr(responseBuffer, "ERROR")) {
      log_e("Received ERROR response");
      return 0;
    }
    for (size_t i = 0; i < count; i++) {
      if (expectedResponses[i] && strstr(responseBuffer, expectedResponses[i])) {
        log_d("Received expected response: %s", expectedResponses[i]);
        return i + 1;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  log_w("Timeout waiting for any expected response");
  return 0;
}

int8_t AsyncATHandler::waitResponse(uint32_t timeout) {
  const char* expectedResponses[] = {"OK"};
  return waitResponseMultiple(timeout, expectedResponses, 1);
}

bool AsyncATHandler::sendCommand(
    const String& command, const String& expectedResponse, uint32_t timeout) {
  String response;
  return sendCommand(command, response, expectedResponse, timeout);
}

bool AsyncATHandler::sendCommand(
    const String& command, String& response, const String& expectedResponse, uint32_t timeout) {
  sendAT(command);
  int8_t result = waitResponse(timeout, expectedResponse.c_str());

  if (result > 0) {
    response = sanitizeResponseBuffer(expectedResponse);
    log_d("Command sent: %s, Response: %s", command.c_str(), response.c_str());
    return true;
  } else {
    response = sanitizeResponseBuffer(expectedResponse);
    return false;
  }
}

bool AsyncATHandler::isResponseComplete() {
  // Look for complete AT response pattern: <CR><LF>OK<CR><LF> or similar
  if (responseBufferPos < 6) return false;  // Minimum size for "\r\nOK\r\n"

  // Check for final response indicators
  const char* finalResponses[] = {"\r\nOK\r\n", "\r\nERROR\r\n", "\r\n+CME ERROR"};

  for (const char* finalResp : finalResponses) {
    if (strstr(responseBuffer, finalResp)) { return true; }
  }
  return false;
}

void AsyncATHandler::extractCompleteResponse() {
  if (!isResponseComplete()) return;

  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100))) {
    // Find the end of the complete response
    const char* finalResponses[] = {"\r\nOK\r\n", "\r\nERROR\r\n"};
    char* endPos = nullptr;

    for (const char* finalResp : finalResponses) {
      char* found = strstr(responseBuffer, finalResp);
      if (found) {
        endPos = found + strlen(finalResp);
        break;
      }
    }

    if (endPos) {
      // Calculate response length
      size_t responseLen = endPos - responseBuffer;

      // Copy complete response to pending data buffer
      memcpy(pendingDataBuffer, responseBuffer, responseLen);
      pendingDataPos = responseLen;
      pendingDataBuffer[pendingDataPos] = '\0';

      // Remove processed response from main buffer
      size_t remainingLen = responseBufferPos - responseLen;
      if (remainingLen > 0) {
        memmove(responseBuffer, endPos, remainingLen);
        responseBufferPos = remainingLen;
      } else {
        responseBufferPos = 0;
      }
      responseBuffer[responseBufferPos] = '\0';

      hasCompleteResponse = false;
    }

    xSemaphoreGive(mutex);
  }
}

int AsyncATHandler::available() {
  if (pendingDataPos > 0) { return pendingDataPos; }

  // Check if we have a complete response ready to extract
  if (isResponseComplete()) {
    extractCompleteResponse();
    return pendingDataPos;
  }

  return 0;
}
