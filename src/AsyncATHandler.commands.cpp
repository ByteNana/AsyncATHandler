#include "AsyncATHandler.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

// Helper function to convert char array to String (for public API return)
static String charArrayToString(const char* arr) { return String(arr); }

bool AsyncATHandler::sendCommandAsync(const String& command) {
  if (!_stream || !mutex || !commandQueue) {
    log_w("Handler not fully initialized or running.");
    return false;
  }

  ATCommand cmd;
  xSemaphoreTake(mutex, portMAX_DELAY);
  cmd.id = nextCommandId++;
  xSemaphoreGive(mutex);

  strncpy(cmd.command, command.c_str(), AT_COMMAND_MAX_LENGTH - 1);
  cmd.command[AT_COMMAND_MAX_LENGTH - 1] = '\0';

  cmd.waitForResponse = false;
  cmd.responseSemaphore = nullptr;

  BaseType_t queueSuccess = xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(AT_QUEUE_TIMEOUT));

  if (queueSuccess != pdTRUE) {
    log_e("Failed to send async command to command queue.");
    return false;
  }
  return true;
}

bool AsyncATHandler::sendCommand(
    const String& command, String& response, const String& expectedResponse, uint32_t timeout) {
  if (!_stream || !mutex || !commandQueue || !responseQueue) {
    log_e("Handler not fully initialized or running.");
    return false;
  }

  // Clear response string
  response = "";

  // Flush any old responses to ensure clean state
  flushResponseQueue();

  // Send the command as async (no pending command state needed)
  if (!sendCommandAsync(command)) {
    log_e("Failed to send command: %s", command.c_str());
    return false;
  }

  log_d("Sent command: '%s', waiting for response...", command.c_str());

  // Wait for responses and collect them
  uint32_t startTime = millis();
  bool foundCompletion = false;

  while (millis() - startTime < timeout) {
    ATResponse resp;
    if (xQueueReceive(responseQueue, &resp, pdMS_TO_TICKS(100)) == pdTRUE) {
      String line(resp.response);
      line.trim();

      // Collect all response lines
      if (line.length() > 0) { response += String(resp.response); }

      // Check if this line completes the command
      if (lineCompletesCommand(line, expectedResponse.c_str())) {
        foundCompletion = true;
        break;
      }
    }
  }

  log_d(
      "Command completed. Found completion: %s, Response: '%s'", foundCompletion ? "YES" : "NO",
      response.c_str());

  if (!foundCompletion) {
    log_e("Timeout waiting for command completion. Collected response: '%s'", response.c_str());
    return false;
  }

  // Determine success based on response content
  bool isSuccessResponse =
      (response.indexOf("OK") != -1) ||
      (expectedResponse.length() > 0 && response.indexOf(expectedResponse) != -1);

  return isSuccessResponse;
}

bool AsyncATHandler::sendCommand(
    const String& command, const String& expectedResponse, uint32_t timeout) {
  String dummy;
  return sendCommand(command, dummy, expectedResponse, timeout);
}

bool AsyncATHandler::sendCommandBatch(
    const String commands[], size_t count, String responses[], uint32_t timeout) {
  bool allSuccess = true;
  for (size_t i = 0; i < count; ++i) {
    String currentResponse;
    bool success = sendCommand(commands[i], currentResponse, "OK", timeout);

    if (responses) { responses[i] = currentResponse; }

    if (!success) { allSuccess = false; }
  }
  return allSuccess;
}
