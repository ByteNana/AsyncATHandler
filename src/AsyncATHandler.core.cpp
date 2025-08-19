#include "AsyncATHandler.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

AsyncATHandler::AsyncATHandler() { flushResponseBuffer(); }
AsyncATHandler::~AsyncATHandler() { end(); }

bool AsyncATHandler::begin(Stream &stream) {
  if (readerTask || mutex) {
    log_e("Reader task already started");
    return false;
  }
  mutex = xSemaphoreCreateMutex();

  if (!mutex) {
    log_e("Failed to create FreeRTOS resources.");
    end();
    return false;
  }

  _stream = &stream;
  flushResponseBuffer();
  BaseType_t result = xTaskCreatePinnedToCore(
      readerTaskFunction, "AT_Reader", AT_TASK_STACK_SIZE, this, AT_TASK_PRIORITY, &readerTask,
      AT_TASK_CORE);

  if (result == pdPASS) {
    log_d("Reader task started successfully");
    return true;
  }

  end();
  return false;
}

void AsyncATHandler::end() {
  if (readerTask) {
    vTaskDelete(readerTask);
    readerTask = nullptr;
  }
  if (mutex) {
    vSemaphoreDelete(mutex);
    mutex = nullptr;
  }
  _stream = nullptr;
  flushResponseBuffer();
}
