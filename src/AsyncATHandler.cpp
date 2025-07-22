#include "AsyncATHandler.h"

#include <string.h>  // For strncpy, strlen, strncmp, strstr

#include "ATHandler.settings.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

// Helper function to convert char array to String (for public API return)
static String charArrayToString(const char* arr) { return String(arr); }

AsyncATHandler::AsyncATHandler()
    : _stream(nullptr),
      readerTask(nullptr),
      commandQueue(nullptr),
      responseQueue(nullptr),
      mutex(nullptr),
      nextCommandId(1),
      unsolicitedCallback(nullptr),
      running(false),
      pendingSyncCommand() {
  // Initialize char array buffer
  memset(responseBuffer, 0, sizeof(responseBuffer));
  responseBufferPos = 0;
}

AsyncATHandler::~AsyncATHandler() {
  end();  // Ensure task is stopped and resources are cleaned up

  // It's safer to delete resources only if they were successfully created
  // and are not being used by a running task.
  // The 'end()' method should handle stopping the task.
  if (readerTask) {
    // If for some reason the task is still running, try to delete it.
    // This should ideally be handled by 'end()'.
    vTaskDelete(readerTask);
    readerTask = nullptr;
  }

  if (mutex) {
    vSemaphoreDelete(mutex);
    log_d("Mutex deleted.");
    mutex = nullptr;
  }
  if (commandQueue) {
    vQueueDelete(commandQueue);
    log_d("Command queue deleted.");
    commandQueue = nullptr;
  }
  if (responseQueue) {
    vQueueDelete(responseQueue);
    log_d("Response queue deleted.");
    responseQueue = nullptr;
  }
  log_d("Cleanup complete.");
}

bool AsyncATHandler::begin(Stream& stream) {
  if (readerTask || running || mutex || commandQueue || responseQueue) {
    log_e("Handler already initialized or resources exist.");
    return false;
  }

  log_d("Attempting to create FreeRTOS resources.");
  mutex = xSemaphoreCreateMutex();
  commandQueue = xQueueCreate(AT_COMMAND_QUEUE_SIZE, sizeof(ATCommand));
  responseQueue = xQueueCreate(AT_RESPONSE_QUEUE_SIZE, sizeof(ATResponse));

  if (!mutex || !commandQueue || !responseQueue) {
    log_e("Failed to create FreeRTOS resources.");
    if (mutex) {
      vSemaphoreDelete(mutex);
      mutex = nullptr;
    }
    if (commandQueue) {
      vQueueDelete(commandQueue);
      commandQueue = nullptr;
    }
    if (responseQueue) {
      vQueueDelete(responseQueue);
      responseQueue = nullptr;
    }
    return false;
  }
  log_d("FreeRTOS resources created.");

  _stream = &stream;
  running = true;
  nextCommandId = 1;
  memset(responseBuffer, 0, sizeof(responseBuffer));  // Clear buffer
  responseBufferPos = 0;
  flushResponseQueue();

  pendingSyncCommand = PendingCommandInfo();  // Reset pending command info

  log_d("Creating reader task...");
  BaseType_t result = xTaskCreatePinnedToCore(
      readerTaskFunction, "AT_Reader", AT_TASK_STACK_SIZE, this, AT_TASK_PRIORITY, &readerTask,
      AT_TASK_CORE);

  if (result == pdPASS) {
    log_d("Handler started, reader task created.");
    return true;
  } else {
    log_e("Failed to create reader task.");
    running = false;
    _stream = nullptr;
    readerTask = nullptr;  // Ensure task handle is null if creation failed

    // Clean up resources if task creation failed
    if (mutex) {
      vSemaphoreDelete(mutex);
      mutex = nullptr;
    }
    if (commandQueue) {
      vQueueDelete(commandQueue);
      commandQueue = nullptr;
    }
    if (responseQueue) {
      vQueueDelete(responseQueue);
      responseQueue = nullptr;
    }
    return false;
  }
}

void AsyncATHandler::end() {
  if (!running && !readerTask) {
    log_d("Handler not running or task not created.");
    return;
  }
  log_d("Stopping handler...");
  running = false;  // Signal the task to exit its loop

  // Give the reader task a moment to self-terminate
  // It's generally better to use a notification or a flag for task termination
  // and then vTaskDelete(readerTask) from the calling context if necessary,
  // but a short delay can work if the task checks 'running' frequently.
  log_d("Delaying for task to exit: %lu ms", pdMS_TO_TICKS(100));
  vTaskDelay(pdMS_TO_TICKS(100));

  if (readerTask) {
    // If the task is still running after the delay, forcefully delete it.
    // This should be done carefully as it can leave resources uncleaned if not managed.
    // In a real-world scenario, prefer cooperative task termination.
    vTaskDelete(readerTask);
    readerTask = nullptr;
    log_d("Reader task deleted.");
  }

  _stream = nullptr;
  if (responseQueue) { flushResponseQueue(); }
  pendingSyncCommand = PendingCommandInfo();  // Reset pending command info

  log_d("Handler stopped.");
}

bool AsyncATHandler::sendCommandAsync(const String& command) {
  if (!_stream || !running || !mutex || !commandQueue) {
    log_w("Handler not fully initialized or running.");
    return false;
  }

  ATCommand cmd;
  xSemaphoreTake(mutex, portMAX_DELAY);
  cmd.id = nextCommandId++;
  xSemaphoreGive(mutex);

  // Copy command string to char array, ensuring null termination
  strncpy(cmd.command, command.c_str(), AT_COMMAND_MAX_LENGTH - 1);
  cmd.command[AT_COMMAND_MAX_LENGTH - 1] = '\0';  // Ensure null termination

  cmd.waitForResponse = false;
  cmd.responseSemaphore = nullptr;  // Should be null for async commands

  BaseType_t queueSuccess = xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(AT_QUEUE_TIMEOUT));

  if (queueSuccess != pdTRUE) {
    log_e("Failed to send async command to command queue.");
    return false;
  }
  return true;
}

bool AsyncATHandler::sendCommand(
    const String& command, String& response, const String& expectedResponse, uint32_t timeout) {
  // Check if core FreeRTOS resources are initialized
  if (!_stream || !running || !mutex || !commandQueue || !responseQueue) {
    log_e("Handler not fully initialized or running.");
    return false;
  }

  ATCommand cmd;
  xSemaphoreTake(mutex, portMAX_DELAY);
  cmd.id = nextCommandId++;
  xSemaphoreGive(mutex);

  // Copy command string to char array
  strncpy(cmd.command, command.c_str(), AT_COMMAND_MAX_LENGTH - 1);
  cmd.command[AT_COMMAND_MAX_LENGTH - 1] = '\0';

  // Copy expected response string to char array
  strncpy(cmd.expectedResponse, expectedResponse.c_str(), AT_EXPECTED_RESPONSE_MAX_LENGTH - 1);
  cmd.expectedResponse[AT_EXPECTED_RESPONSE_MAX_LENGTH - 1] = '\0';

  cmd.timeout = timeout;
  cmd.waitForResponse = true;
  cmd.responseSemaphore = xSemaphoreCreateBinary();

  if (!cmd.responseSemaphore) {
    log_e("Failed to create response semaphore for command ID: %lu", cmd.id);
    return false;
  }

  log_d(
      "Queuing (sync) cmd ID: %lu, Cmd: '%s', Expected: '%s'", cmd.id, cmd.command,
      cmd.expectedResponse);

  // Send command to commandQueue
  BaseType_t queueSuccess = xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(AT_QUEUE_TIMEOUT));

  if (queueSuccess != pdTRUE) {
    log_e("Failed to send sync command to command queue for ID: %lu", cmd.id);
    vSemaphoreDelete(cmd.responseSemaphore);  // Clean up semaphore if queue send fails
    return false;
  }

  log_d("Waiting on semaphore for cmd ID: %lu (Timeout: %lu ms)", cmd.id, timeout);
  BaseType_t semaphoreTaken = xSemaphoreTake(cmd.responseSemaphore, pdMS_TO_TICKS(timeout));

  vSemaphoreDelete(cmd.responseSemaphore);  // Always delete the semaphore after use
  cmd.responseSemaphore = nullptr;

  if (semaphoreTaken == pdTRUE) {
    String fullCollectedResponse = "";
    bool finalResponseReceived = false;
    bool overallSuccess = false;

    // Acquire mutex to safely access responseQueue
    xSemaphoreTake(mutex, pdMS_TO_TICKS(AT_QUEUE_TIMEOUT));

    // Iterate through responses in the queue that belong to this command ID
    // We might need to receive more than initialQueueSize if the reader task
    // puts more responses in while we are processing.
    // The '+ 5' is a heuristic, better to check for finalResponseReceived.
    size_t initialQueueSize = uxQueueMessagesWaiting(responseQueue);
    for (size_t i = 0; i < initialQueueSize + AT_RESPONSE_QUEUE_SIZE; ++i) {
      ATResponse receivedResp;
      // Try to receive a response without blocking
      if (xQueueReceive(responseQueue, &receivedResp, 0) == pdTRUE) {
        if (receivedResp.commandId == cmd.id) {
          fullCollectedResponse += charArrayToString(receivedResp.response);

          // Determine if this is the final response based on content
          if (strncmp(receivedResp.response, "OK\r\n", 4) == 0) {
            overallSuccess = true;
            finalResponseReceived = true;
          } else if (strncmp(receivedResp.response, "ERROR\r\n", 7) == 0) {
            overallSuccess = false;
            finalResponseReceived = true;
          }
        } else {
          // Not our response, put it back.
          // This is still a tricky part. If the queue is full, it won't go back.
          // A better design might involve separate queues per command or a more
          // robust filtering mechanism in the reader task.
          if (xQueueSend(responseQueue, &receivedResp, 0) != pdTRUE) {
            log_w("Could not re-queue unmatched response for ID: %lu", receivedResp.commandId);
          }
        }
      } else {
        // No more items currently in queue.
        if (!finalResponseReceived) {
          log_d("No more items in queue for ID: %lu. Breaking collection.", cmd.id);
        }
        break;  // Exit loop if no more responses are immediately available
      }
      if (finalResponseReceived) {
        log_d("Final response received for ID: %lu. Breaking collection.", cmd.id);
        break;  // Exit loop once final response is found
      }
    }
    xSemaphoreGive(mutex);  // Release mutex

    response = fullCollectedResponse;  // Assign collected response to output parameter

    // Additional check for specific expected response if it's not "OK"
    if (overallSuccess && strlen(cmd.expectedResponse) > 0 &&
        strncmp(cmd.expectedResponse, "OK", 2) != 0 &&
        strstr(fullCollectedResponse.c_str(), cmd.expectedResponse) == nullptr) {
      overallSuccess = false;  // Specific expected response not found
      log_w(
          "Specific expected response '%s' not found in '%s' for cmd ID: %lu", cmd.expectedResponse,
          fullCollectedResponse.c_str(), cmd.id);
    }

    if (!finalResponseReceived) {
      overallSuccess = false;
      log_e(
          "Command ID %lu ended without final OK/ERROR response. Collected: '%s'", cmd.id,
          fullCollectedResponse.c_str());
    }

    return overallSuccess;
  } else {
    log_e("Timeout waiting for semaphore for cmd ID: %lu", cmd.id);
    return false;
  }
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

bool AsyncATHandler::hasResponse() {
  // Check if responseQueue and mutex exist before using them
  if (!responseQueue || !mutex) { return false; }
  BaseType_t result = pdFALSE;
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    result = uxQueueMessagesWaiting(responseQueue) > 0;
    xSemaphoreGive(mutex);
  } else {
    log_w("Failed to acquire mutex.");
  }
  return result == pdTRUE;
}

ATResponse AsyncATHandler::getResponse() {
  ATResponse resp;
  resp.commandId = 0;
  memset(resp.response, 0, sizeof(resp.response));  // Clear char array
  resp.success = false;
  resp.timestamp = 0;

  // Check if responseQueue and mutex exist before using them
  if (!responseQueue || !mutex) {
    log_e("Resources not initialized.");
    return resp;
  }

  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (xQueueReceive(responseQueue, &resp, 0) == pdTRUE) {
      log_d(
          "Retrieved response: cmdID=%lu, response='%s', success=%s", resp.commandId, resp.response,
          (resp.success ? "TRUE" : "FALSE"));
    } else {
      log_e("No response in queue or failed to receive.");
    }
    xSemaphoreGive(mutex);
  } else {
    log_w("Failed to acquire mutex.");
  }
  return resp;
}

void AsyncATHandler::flushResponseQueue() {
  if (!responseQueue || !mutex) { return; }
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    ATResponse resp;
    while (uxQueueMessagesWaiting(responseQueue) > 0) {
      if (xQueueReceive(responseQueue, &resp, 0) != pdTRUE) { break; }
    }
    xSemaphoreGive(mutex);
  } else {
    log_w("Failed to acquire mutex.");
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

void AsyncATHandler::readerTaskFunction(void* parameter) {
  AsyncATHandler* handler = static_cast<AsyncATHandler*>(parameter);
  if (!handler) {
    log_e("Invalid handler parameter. Exiting task.");
    vTaskDelete(NULL);
    return;
  }

  while (handler->running) {
    ATCommand cmd;
    // Try to receive a command from the queue without blocking indefinitely
    if (xSemaphoreTake(handler->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (xQueueReceive(handler->commandQueue, &cmd, pdMS_TO_TICKS(0)) == pdTRUE) {
        // Send the command to the actual stream
        handler->_stream->println(cmd.command);  // println takes const char*
        handler->_stream->flush();

        if (cmd.waitForResponse) {
          // Store this command's details as the currently pending synchronous command
          handler->pendingSyncCommand.id = cmd.id;
          handler->pendingSyncCommand.responseSemaphore = cmd.responseSemaphore;
          strncpy(
              handler->pendingSyncCommand.expectedResponse, cmd.expectedResponse,
              AT_EXPECTED_RESPONSE_MAX_LENGTH - 1);
          handler->pendingSyncCommand.expectedResponse[AT_EXPECTED_RESPONSE_MAX_LENGTH - 1] = '\0';
          handler->pendingSyncCommand.active = true;
        } else {
          // For fire-and-forget async commands, clean up semaphore if it exists
          // (should be null for async commands, but good practice to check)
          if (cmd.responseSemaphore) { vSemaphoreDelete(cmd.responseSemaphore); }
        }
      }
      xSemaphoreGive(handler->mutex);
    }

    handler->processIncomingData();

    vTaskDelay(pdMS_TO_TICKS(10));  // Small delay to yield to other tasks
  }

  log_d("Reader task exiting.");
  vTaskDelete(NULL);  // Self-delete the task
}

void AsyncATHandler::processIncomingData() {
  // Acquire mutex to safely access _stream and responseBuffer
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) { return; }

  // Don't consume data if there is no pending command and no unsolicited
  // callback. This avoids reading responses that belong to the next command
  // before it is queued, which can happen in tests where responses for
  // multiple commands are preloaded.
  if (!pendingSyncCommand.active && !unsolicitedCallback) {
    xSemaphoreGive(mutex);
    return;
  }

  while (_stream->available()) {
    char c = static_cast<char>(_stream->read());

    // Check for buffer overflow before adding character
    if (responseBufferPos < AT_RESPONSE_BUFFER_SIZE - 1) {
      responseBuffer[responseBufferPos++] = c;
      responseBuffer[responseBufferPos] = '\0';  // Always null-terminate
    } else {
      log_w("responseBuffer overflow. Clearing.");
      memset(responseBuffer, 0, sizeof(responseBuffer));
      responseBufferPos = 0;
      // If overflow, discard current character and continue
      continue;
    }

    // Check for line termination (\r\n)
    if (responseBufferPos >= 2 && responseBuffer[responseBufferPos - 2] == '\r' &&
        responseBuffer[responseBufferPos - 1] == '\n') {
      // A full line is received. Process it.
      handleResponse(responseBuffer);

      // Clear the buffer for the next line
      memset(responseBuffer, 0, sizeof(responseBuffer));
      responseBufferPos = 0;

      // If handling this line cleared the pending command, stop reading
      // further data to avoid consuming responses that belong to the next
      // command before it is queued.
      if (!pendingSyncCommand.active) {
        break;
      }
    }
  }
  xSemaphoreGive(mutex);  // Release mutex
}

void AsyncATHandler::handleResponse(const char* response) {
  // Trim trailing/leading whitespace to handle modems that send extra spaces or
  // CR characters. Using String here is fine since response lines are short
  // (\r\n-terminated).
  String line(response);
  line.trim();
  if (line.length() == 0) {
    // Ignore blank lines entirely
    return;
  }

  const char* trimmed = line.c_str();

  if (isUnsolicitedResponse(trimmed)) {
    if (unsolicitedCallback) {
      unsolicitedCallback(trimmed);  // Pass const char* to callback
    }
    return;
  }

  // Check if there's a pending synchronous command
  if (pendingSyncCommand.active) {
    bool isFinalResponseForCommand = false;
    bool lineRepresentsSuccess = false;

    // Logic to determine if this 'response' line completes the pending command.
    if (strcmp(trimmed, "OK") == 0) {
      lineRepresentsSuccess = true;
      isFinalResponseForCommand = true;
    } else if (strcmp(trimmed, "ERROR") == 0) {
      lineRepresentsSuccess = false;
      isFinalResponseForCommand = true;
    } else if (
        strlen(pendingSyncCommand.expectedResponse) > 0 &&
        strstr(trimmed, pendingSyncCommand.expectedResponse) != nullptr) {
      // This line contains the specific expected response (e.g., "+CGMI: SIMCOM\r\n")
      // This is a data line. It's successful as data.
      lineRepresentsSuccess = true;
      isFinalResponseForCommand = false;  // It's a data line, usually not the final OK/ERROR
    } else {
      // For any other lines that don't start with OK/ERROR and don't contain specific expected
      // response. These might be echoes or unexpected lines. We'll mark their success as false.
      lineRepresentsSuccess = false;
      isFinalResponseForCommand = false;
    }

    ATResponse resp;
    resp.commandId = pendingSyncCommand.id;
    strncpy(resp.response, response, AT_RESPONSE_BUFFER_SIZE - 1);
    resp.response[AT_RESPONSE_BUFFER_SIZE - 1] = '\0';  // Ensure null termination
    resp.success = lineRepresentsSuccess;
    resp.timestamp = millis();

    // Send the response to the responseQueue
    if (xQueueSend(responseQueue, &resp, pdMS_TO_TICKS(10)) != pdTRUE) {
      log_e("Failed to send response to responseQueue for cmd ID: %lu", resp.commandId);
    }

    // If this is the final response for the command, signal the semaphore
    if (isFinalResponseForCommand) {
      if (pendingSyncCommand.responseSemaphore) {
        xSemaphoreGive(pendingSyncCommand.responseSemaphore);
        log_d("Signaled semaphore for cmd ID: %lu", pendingSyncCommand.id);
      } else {
        log_w("No semaphore to signal for pending command ID: %lu", pendingSyncCommand.id);
      }
      // Clear the pending command state after handling final response
      pendingSyncCommand.active = false;
      pendingSyncCommand.responseSemaphore = nullptr;  // Clear semaphore handle
      memset(
          pendingSyncCommand.expectedResponse, 0,
          sizeof(pendingSyncCommand.expectedResponse));  // Clear expected response
    }
  } else {
    log_d("Unmatched response (no pending sync command): '%s'", trimmed);
  }
}

// --- isUnsolicitedResponse ---
bool AsyncATHandler::isUnsolicitedResponse(const char* response) {
  // Use strncmp for prefix matching
  return strncmp(response, "+CMT:", 5) == 0 || strncmp(response, "+CMTI:", 6) == 0 ||
         strncmp(response, "+CLIP:", 6) == 0 || strncmp(response, "+CREG:", 6) == 0 ||
         strncmp(response, "+CPIN:", 6) == 0 || strncmp(response, "RING", 4) == 0;
}
