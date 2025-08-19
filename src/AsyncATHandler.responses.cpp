#include "AsyncATHandler.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

// AsyncATHandler.responses.cpp - Simplified without pending command logic

void AsyncATHandler::processIncomingData() {
  if (!xSemaphoreTake(mutex, pdMS_TO_TICKS(100))) { return; }

  // Always process data if we have streams and queues
  if (!_stream || !responseQueue) {
    xSemaphoreGive(mutex);
    return;
  }

  while (_stream->available()) {
    char c = static_cast<char>(_stream->read());

    if (!addCharToBuffer(c)) {
      handleBufferOverflow();
      continue;
    }

    if (isCompleteLineInBuffer()) {
      handleResponse(responseBuffer);
      clearResponseBuffer();
    }
  }

  xSemaphoreGive(mutex);
}

void AsyncATHandler::handleResponse(const char* response) {
  String line = trimAndValidateResponse(response);

  // Ignore empty lines
  if (line.length() == 0) { return; }

  // Handle unsolicited responses via callback
  if (isUnsolicitedResponse(line.c_str())) {
    if (unsolicitedCallback) {
      unsolicitedCallback(line.c_str());
    }
    return;
  }

  // All other responses go to the response queue
  ATResponse resp;
  resp.commandId = 0; // No specific command tracking needed
  strncpy(resp.response, response, AT_RESPONSE_BUFFER_SIZE - 1);
  resp.response[AT_RESPONSE_BUFFER_SIZE - 1] = '\0';
  resp.success = true;
  resp.timestamp = millis();

  if (xQueueSend(responseQueue, &resp, pdMS_TO_TICKS(10)) != pdTRUE) {
    log_w("Failed to enqueue response: '%s'", line.c_str());
  } else {
    log_d("Enqueued response: '%s'", line.c_str());
  }
}

// Keep the existing helper functions but remove pending command specific ones
bool AsyncATHandler::lineCompletesCommand(const String& line, const char* expectedResponse) {
  // Always check for standard completion responses first
  if (line == "OK" || line == "ERROR") { return true; }

  // Then check for custom expected response
  if (strlen(expectedResponse) > 0 && strstr(line.c_str(), expectedResponse) != nullptr) {
    return true;
  }

  return false;
}

void AsyncATHandler::flushResponseQueue() {
  if (!responseQueue || !mutex) { return; }

  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100))) {
    ATResponse resp;
    while (uxQueueMessagesWaiting(responseQueue) > 0) {
      if (xQueueReceive(responseQueue, &resp, 0) != pdTRUE) { break; }
    }
    xSemaphoreGive(mutex);
  } else {
    log_w("Failed to acquire mutex for flush operation.");
  }
}

int AsyncATHandler::waitResponse(
    const String& expectedResponse, String& response, uint32_t timeout) {
  uint32_t startMillis = millis();
  String collectedResponse = "";

  while (millis() - startMillis < timeout) {
    ATResponse resp;
    if (xQueueReceive(responseQueue, &resp, pdMS_TO_TICKS(100)) == pdTRUE) {
      String line(resp.response);
      line.trim();
      collectedResponse += line + "\n";

      if (line.indexOf(expectedResponse) != -1) {
        response = collectedResponse;
        return 1;
      }
    }
  }

  response = collectedResponse;
  return 0;
}

int8_t AsyncATHandler::waitResponse(uint32_t timeout) {
  uint32_t startMillis = millis();
  log_d("waitResponse(timeout=%lu) started", timeout);

  while (millis() - startMillis < timeout) {
    ATResponse resp;
    BaseType_t queueResult = xQueueReceive(responseQueue, &resp, pdMS_TO_TICKS(100));

    if (queueResult == pdTRUE) {
      String line(resp.response);
      line.trim();
      log_d("waitResponse got response: '%s', length: %d", line.c_str(), line.length());

      if (line.length() > 0) {
        log_d("waitResponse returning 1 (found response)");
        return 1;  // Found any response
      } else {
        log_d("waitResponse ignoring empty response");
      }
    } else {
      log_d("waitResponse: no queue data available");
    }
  }

  log_d("waitResponse timeout after %lu ms", timeout);
  return -1;  // Timeout
}

int8_t AsyncATHandler::checkLineForExpectedResponses(
    const String& line, const String* responses, size_t count) {
  log_d("checkLineForExpectedResponses: checking '%s' against %zu responses", line.c_str(), count);

  for (size_t i = 0; i < count; i++) {
    log_d("  Checking against[%zu]: '%s'", i, responses[i].c_str());
    if (line.indexOf(responses[i]) >= 0) {
      log_d("  MATCH found at index %zu", i);
      return i + 1;  // Return 1-based index
    }
  }
  log_d("  No matches found");
  return -1;  // Not found
}
