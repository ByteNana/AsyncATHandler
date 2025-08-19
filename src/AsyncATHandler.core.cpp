#include "AsyncATHandler.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

AsyncATHandler::AsyncATHandler()
    : _stream(nullptr),
      readerTask(nullptr),
      commandQueue(nullptr),
      responseQueue(nullptr),
      mutex(nullptr),
      nextCommandId(1),
      unsolicitedCallback(nullptr) {
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
  if (readerTask || mutex || commandQueue || responseQueue) {
    log_e("Handler already initialized or resources exist.");
    return false;
  }

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
  nextCommandId = 1;
  memset(responseBuffer, 0, sizeof(responseBuffer));
  responseBufferPos = 0;
  flushResponseQueue();

  log_d("Creating reader task...");
  BaseType_t result = xTaskCreatePinnedToCore(
      readerTaskFunction, "AT_Reader", AT_TASK_STACK_SIZE, this, AT_TASK_PRIORITY, &readerTask,
      AT_TASK_CORE);

  if (result == pdPASS) {
    log_d("Handler started, reader task created.");
    return true;
  } else {
    log_e("Failed to create reader task.");
    _stream = nullptr;
    readerTask = nullptr;

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
  if (!readerTask) {
    log_d("Handler not running or task not created.");
    return;
  }
  log_d("Stopping handler...");

  vTaskDelay(pdMS_TO_TICKS(100));

  if (readerTask) {
    vTaskDelete(readerTask);
    readerTask = nullptr;
    log_d("Reader task deleted.");
  }

  _stream = nullptr;
  if (responseQueue) { flushResponseQueue(); }

  log_d("Handler stopped.");
}
