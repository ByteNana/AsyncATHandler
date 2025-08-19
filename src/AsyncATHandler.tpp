#pragma once

// Template implementation for multiple expected responses (default timeout)

template <typename... Args>
int8_t AsyncATHandler::waitResponse(Args&&... expectedResponses) {
  // Convert all arguments to String array
  String responses[] = {String(expectedResponses)...};
  size_t responseCount = sizeof...(expectedResponses);

  uint32_t startMillis = millis();

  while (millis() - startMillis < AT_DEFAULT_TIMEOUT) {
    ATResponse resp;
    if (xQueueReceive(responseQueue, &resp, pdMS_TO_TICKS(100)) == pdTRUE) {
      String line(resp.response);
      line.trim();

      if (line.length() > 0) {
        for (size_t i = 0; i < responseCount; i++) {
          if (line.indexOf(responses[i]) >= 0) {
            return i + 1;  // Return 1-based index
          }
        }
      }
    }
  }

  return -1;  // Timeout
}

// Template implementation for multiple expected responses (custom timeout)
template <typename... Args>
int8_t AsyncATHandler::waitResponse(uint32_t timeout, Args&&... expectedResponses) {
  // Convert all arguments to String array
  String responses[] = {String(expectedResponses)...};
  size_t responseCount = sizeof...(expectedResponses);

  uint32_t startMillis = millis();

  while (millis() - startMillis < timeout) {
    ATResponse resp;
    if (xQueueReceive(responseQueue, &resp, pdMS_TO_TICKS(100)) == pdTRUE) {
      String line(resp.response);
      line.trim();

      if (line.length() > 0) {
        for (size_t i = 0; i < responseCount; i++) {
          if (line.indexOf(responses[i]) >= 0) {
            return i + 1;  // Return 1-based index
          }
        }
      }
    }
  }

  return -1;  // Timeout
}
