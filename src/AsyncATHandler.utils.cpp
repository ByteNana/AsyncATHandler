#include "AsyncATHandler.h"

#include <string.h>  // For strncmp

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

void AsyncATHandler::flushResponseBuffer() {
  memset(responseBuffer, 0, sizeof(responseBuffer));
  responseBufferPos = 0;
}

String AsyncATHandler::sanitizeResponseBuffer(const String& expectedResponse) {
  String result = "";

  if (responseBufferPos == 0) { return result; }

  // Find the position of the expected response
  char* foundPos = strstr(responseBuffer, expectedResponse.c_str());

  // If the expected response is not found, return the entire buffer
  if (foundPos == nullptr) {
    result = String(responseBuffer);
    flushResponseBuffer();
    return result;
  }

  if (foundPos != nullptr) {
    // Find the end of the line containing the expected response
    char* lineEnd = strchr(foundPos, '\n');
    if (lineEnd == nullptr) {
      // If no newline found, use end of buffer
      lineEnd = responseBuffer + responseBufferPos;
    } else {
      lineEnd++;  // Include the newline
    }

    // Extract response from beginning to end of found line
    size_t responseLength = lineEnd - responseBuffer;
    result = String(responseBuffer).substring(0, responseLength);

    // Remove the extracted portion from buffer
    size_t remainingLength = responseBufferPos - responseLength;

    // Buffer is empty after extraction
    if (remainingLength <= 0) {
      flushResponseBuffer();
      return result;
    }

    memmove(responseBuffer, lineEnd, remainingLength);
    responseBufferPos = remainingLength;
    responseBuffer[responseBufferPos] = '\0';
  }

  return result;
}

String AsyncATHandler::getResponse(const String& expectedResponse) {
  return sanitizeResponseBuffer(expectedResponse);
}
