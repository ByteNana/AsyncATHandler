#include "AsyncATHandler.h"

#include "ATHandler.settings.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

AsyncATHandler::AsyncATHandler()
    : _stream(nullptr),
      readerTask(nullptr),
      commandQueue(nullptr),
      responseQueue(nullptr),
      mutex(nullptr),
      nextCommandId(1),
      responseBuffer(""),
      unsolicitedCallback(nullptr),
      running(false),
      pendingSyncCommand() {}

AsyncATHandler::~AsyncATHandler() {
  end();

  if (mutex) {
    vSemaphoreDelete(mutex);
    log_d("Mutex deleted.");
  }
  if (commandQueue) {
    vQueueDelete(commandQueue);
    log_d("Command queue deleted.");
  }
  if (responseQueue) {
    vQueueDelete(responseQueue);
    log_d("Response queue deleted.");
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
  log_d("FreeRTOS resources created (mock).");

  _stream = &stream;
  running = true;
  nextCommandId = 1;
  responseBuffer = "";
  flushResponseQueue();

  pendingSyncCommand = PendingCommandInfo();

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
    readerTask = nullptr;

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
  running = false;

  log_d("Delaying for task to exit: %lu ms", pdMS_TO_TICKS(100));
  vTaskDelay(pdMS_TO_TICKS(100));

  if (readerTask) { readerTask = nullptr; }

  _stream = nullptr;
  if (responseQueue) { flushResponseQueue(); }
  pendingSyncCommand = PendingCommandInfo();

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

  cmd.command = command;
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
  // Check if core FreeRTOS resources are initialized
  if (!_stream || !running || !mutex || !commandQueue || !responseQueue) {
    log_e("Handler not fully initialized or running.");
    return false;
  }

  ATCommand cmd;
  xSemaphoreTake(mutex, portMAX_DELAY);
  cmd.id = nextCommandId++;
  xSemaphoreGive(mutex);

  cmd.command = command;
  cmd.expectedResponse = expectedResponse;
  cmd.timeout = timeout;
  cmd.waitForResponse = true;
  cmd.responseSemaphore = xSemaphoreCreateBinary();

  if (!cmd.responseSemaphore) {
    log_e("Failed to create response semaphore for command ID: %lu", cmd.id);
    return false;
  }

  log_d(
      "Queuing (sync) cmd ID: %lu, Cmd: '%s', Expected: '%s'", cmd.id, cmd.command.c_str(),
      cmd.expectedResponse.c_str());

  // Send command tocommandQueue
  BaseType_t queueSuccess = xQueueSend(commandQueue, &cmd, pdMS_TO_TICKS(AT_QUEUE_TIMEOUT));

  if (queueSuccess != pdTRUE) {
    log_e("Failed to send sync command to command queue for ID: %lu", cmd.id);
    vSemaphoreDelete(cmd.responseSemaphore);
    return false;
  }

  log_d("Waiting on semaphore for cmd ID: %lu (Timeout: %lu ms)", cmd.id, timeout);
  BaseType_t semaphoreTaken = xSemaphoreTake(cmd.responseSemaphore, pdMS_TO_TICKS(timeout));

  vSemaphoreDelete(cmd.responseSemaphore);
  cmd.responseSemaphore = nullptr;

  if (semaphoreTaken == pdTRUE) {
    String fullCollectedResponse = "";
    bool finalResponseReceived = false;
    bool overallSuccess = false;

    xSemaphoreTake(mutex, pdMS_TO_TICKS(AT_QUEUE_TIMEOUT));

    size_t initialQueueSize = uxQueueMessagesWaiting(responseQueue);

    for (size_t i = 0; i < initialQueueSize + 5; ++i) {
      ATResponse receivedResp;
      if (xQueueReceive(responseQueue, &receivedResp, 0) == pdTRUE) {
        if (receivedResp.commandId == cmd.id) {
          fullCollectedResponse += receivedResp.response;

          // Determine if this is the final response based on content
          if (receivedResp.response.startsWith("OK\r\n")) {
            overallSuccess = true;
            finalResponseReceived = true;
          } else if (receivedResp.response.startsWith("ERROR\r\n")) {
            overallSuccess = false;
            finalResponseReceived = true;
          }
        } else {
          // Not our response, put it back. (This is the risky part in your original logic)
          if (xQueueSend(responseQueue, &receivedResp, 0) != pdTRUE) {
            log_w("Could not re-queue unmatched response for ID: %lu", receivedResp.commandId);
          }
        }
      } else {
        // No more items currently in queue. If we haven't received final response, break.
        if (!finalResponseReceived) {
          log_d("No more items in queue for ID: %lu. Breaking collection.", cmd.id);
        }
        break;
      }
      if (finalResponseReceived) {
        log_d("Final response received for ID: %lu. Breaking collection.", cmd.id);
        break;
      }
    }
    xSemaphoreGive(mutex);

    response = fullCollectedResponse;

    if (overallSuccess && cmd.expectedResponse.length() > 0 &&
        !cmd.expectedResponse.startsWith("OK") &&
        fullCollectedResponse.indexOf(cmd.expectedResponse) == -1) {
      overallSuccess = false;  // Specific expected response not found
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
  // Check ifmutex exists before taking it
  if (mutex && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
    unsolicitedCallback = callback;
    xSemaphoreGive(mutex);
    log_d("Callback set.");
  } else {
    log_e("Failed to acquire mutex to set callback (mutex: %s).", (mutex ? "exists" : "null"));
  }
}

bool AsyncATHandler::hasResponse() {
  // Check ifresponseQueue and _mutex exist before using them
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
  resp.response = "";
  resp.success = false;
  resp.timestamp = 0;

  // Check ifresponseQueue and _mutex exist before using them
  if (!responseQueue || !mutex) {
    log_e("Resources not initialized.");
    return resp;
  }

  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (xQueueReceive(responseQueue, &resp, 0) == pdTRUE) {
      log_d(
          "Retrieved response: cmdID=%lu, response='%s', success=%s", resp.commandId,
          resp.response.c_str(), (resp.success ? "TRUE" : "FALSE"));
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
    if (xSemaphoreTake(handler->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (xQueueReceive(handler->commandQueue, &cmd, pdMS_TO_TICKS(0)) == pdTRUE) {
        // Send the command to the actual stream
        handler->_stream->println(cmd.command);
        handler->_stream->flush();

        if (cmd.waitForResponse) {
          // Store this command's details as the currently pending synchronous command for THIS
          // handler instance
          handler->pendingSyncCommand.id = cmd.id;
          handler->pendingSyncCommand.responseSemaphore = cmd.responseSemaphore;
          handler->pendingSyncCommand.expectedResponse = cmd.expectedResponse;
          handler->pendingSyncCommand.active = true;
        } else {
          // For fire-and-forget async commands, clean up semaphore if it exists (should be null for
          // async)
          if (cmd.responseSemaphore) { vSemaphoreDelete(cmd.responseSemaphore); }
        }
      }
      xSemaphoreGive(handler->mutex);
    }

    handler->processIncomingData();

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  vTaskDelete(NULL);
}

void AsyncATHandler::processIncomingData() {
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) != pdTRUE) { return; }

  while (_stream->available()) {
    char c = static_cast<char>(_stream->read());
    responseBuffer += c;

    if (responseBuffer.length() >= 2 && responseBuffer.endsWith("\r\n")) {
      String fullLine = responseBuffer;
      responseBuffer = "";

      handleResponse(fullLine);
    }

    if (responseBuffer.length() > AT_RESPONSE_BUFFER_SIZE) {
      log_w("_responseBuffer overflow. Clearing.");
      responseBuffer = "";
    }
  }
  xSemaphoreGive(mutex);
}

void AsyncATHandler::handleResponse(const String& response) {
  if (isUnsolicitedResponse(response)) {
    if (unsolicitedCallback) { unsolicitedCallback(response); }
    return;
  }

  if (pendingSyncCommand.active) {
    bool isFinalResponseForCommand = false;
    bool lineRepresentsSuccess = false;

    // Logic to determine if this 'response' line completes the pending command.
    if (response.startsWith("OK\r\n")) {
      lineRepresentsSuccess = true;
      isFinalResponseForCommand = true;
    } else if (response.startsWith("ERROR\r\n")) {
      lineRepresentsSuccess = false;
      isFinalResponseForCommand = true;
    } else if (
        pendingSyncCommand.expectedResponse.length() > 0 &&
        response.indexOf(pendingSyncCommand.expectedResponse) != -1) {
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
    resp.response = response;
    resp.success = lineRepresentsSuccess;
    resp.timestamp = millis();

    if (xQueueSend(responseQueue, &resp, pdMS_TO_TICKS(10)) != pdTRUE) {
      log_e("Failed to send response toresponseQueue for cmd ID: %lu", resp.commandId);
    }

    if (isFinalResponseForCommand) {
      if (pendingSyncCommand.responseSemaphore) {
        xSemaphoreGive(pendingSyncCommand.responseSemaphore);
        log_d("Signaled semaphore for cmd ID: %lu", pendingSyncCommand.id);
      } else {
        log_w("No semaphore to signal for pending command ID: %lu", pendingSyncCommand.id);
      }
      // Clear the pending command state after handling final response
      pendingSyncCommand.active = false;
      pendingSyncCommand.responseSemaphore = nullptr;
    }
  } else {
    log_d("Unmatched response (no pending sync command): '%s'", response.c_str());
  }
}

// --- isUnsolicitedResponse ---
bool AsyncATHandler::isUnsolicitedResponse(const String& response) {
  return response.startsWith("+CMT:") || response.startsWith("+CMTI:") ||
         response.startsWith("+CLIP:") || response.startsWith("+CREG:") ||
         response.startsWith("+CPIN:") || response.startsWith("RING");
}
