#include "AsyncATHandler.h"

#include <string.h>  // For strncmp

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

String AsyncATHandler::trimAndValidateResponse(const char* response) {
  String line(response);
  line.trim();
  return line;
}

void AsyncATHandler::handleBufferOverflow() {
  log_w("responseBuffer overflow. Clearing.");
  clearResponseBuffer();
}

void AsyncATHandler::clearResponseBuffer() {
  memset(responseBuffer, 0, sizeof(responseBuffer));
  responseBufferPos = 0;
}

bool AsyncATHandler::addCharToBuffer(char c) {
  if (responseBufferPos < AT_RESPONSE_BUFFER_SIZE - 1) {
    responseBuffer[responseBufferPos++] = c;
    responseBuffer[responseBufferPos] = '\0';
    return true;
  }
  return false;
}

bool AsyncATHandler::isCompleteLineInBuffer() {
  return responseBufferPos >= 2 && responseBuffer[responseBufferPos - 2] == '\r' &&
         responseBuffer[responseBufferPos - 1] == '\n';
}
void AsyncATHandler::setUnsolicitedCallback(UnsolicitedCallback callback) {
  // Check if mutex exists before taking it
  if (mutex && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    unsolicitedCallback = callback;
    xSemaphoreGive(mutex);
    log_d("Callback set.");
  } else {
    log_e("Failed to acquire mutex to set callback (mutex: %s).", (mutex ? "exists" : "null"));
  }
}

size_t AsyncATHandler::getQueuedCommandCount() {
  if (!commandQueue || !mutex) { return 0; }
  size_t count = 0;
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    count = uxQueueMessagesWaiting(commandQueue);
    xSemaphoreGive(mutex);
  } else {
    log_w("Failed to acquire mutex.");
  }
  return count;
}

size_t AsyncATHandler::getQueuedResponseCount() {
  if (!responseQueue || !mutex) { return 0; }
  size_t count = 0;
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    count = uxQueueMessagesWaiting(responseQueue);
    xSemaphoreGive(mutex);
  } else {
    log_w("Failed to acquire mutex.");
  }
  return count;
}

bool AsyncATHandler::isUnsolicitedResponse(const char* response) {
  // Use strncmp for prefix matching
  return strncmp(response, "+CMT:", 5) == 0 ||        // SMS received
         strncmp(response, "+CMTI:", 6) == 0 ||       // SMS indication
         strncmp(response, "+CLIP:", 6) == 0 ||       // Calling line identification
         strncmp(response, "+CREG:", 6) == 0 ||       // Network registration
         strncmp(response, "+CPIN:", 6) == 0 ||       // PIN status
         strncmp(response, "+CSQ:", 5) == 0 ||        // Signal quality
         strncmp(response, "+CGEV:", 6) == 0 ||       // GPRS event
         strncmp(response, "+CUSD:", 6) == 0 ||       // USSD response
         strncmp(response, "RING", 4) == 0 ||         // Incoming call
         strncmp(response, "NO CARRIER", 10) == 0 ||  // Call ended
         strncmp(response, "BUSY", 4) == 0 ||         // Line busy
         strncmp(response, "NO ANSWER", 9) == 0;      // No answer
}
