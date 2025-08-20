#include "AsyncATHandler.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

void AsyncATHandler::handleResponse(const char* response) {
  String line = String(response);

  if (line.length() == 0) { return; }
}

int8_t AsyncATHandler::waitResponseMultiple(
    uint32_t timeout, const char* expectedResponses[], size_t count) {
  unsigned long startTime = millis();

  if (count == 0 || expectedResponses == nullptr) {
    expectedResponses[0] = "OK";  // Default to waiting for "OK" if no responses provided
    count = 1;                    // Set count to 1 since we are now waiting for
  }

  log_d("Waiting for any of %zu expected responses, Timeout: %lu ms", count, timeout);
  while (millis() - startTime < timeout) {
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
