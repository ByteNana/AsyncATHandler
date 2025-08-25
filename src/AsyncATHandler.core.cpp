#include "AsyncATHandler.h"
#include <esp_log.h>

AsyncATHandler::AsyncATHandler() {}

AsyncATHandler::~AsyncATHandler() {
  end();
}

bool AsyncATHandler::begin(Stream& s) {
  if (readerTask) {
    return false;
  }
  stream = &s;

  mutex = xSemaphoreCreateMutex();
  if (!mutex) {
    stream = nullptr;
    return false;
  }

  BaseType_t result =
      xTaskCreatePinnedToCore(readerTaskFunction, "AT_Reader", 4096, this, 2, &readerTask, 1);

  if (result != pdPASS) {
    if (mutex) {
      vSemaphoreDelete(mutex);
      mutex = nullptr;
    }
    stream = nullptr;
    return false;
  }
  return true;
}

void AsyncATHandler::end() {
  if (mutex) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(200))) {
      pendingPromises.clear();
      xSemaphoreGive(mutex);
    } else {
      log_e("Failed to acquire mutex for promise cleanup on end()");
    }
  }

  if (readerTask) {
    TaskHandle_t taskToDelete = readerTask;
    readerTask = nullptr;
    vTaskDelete(taskToDelete);
  }

  if (mutex) {
    SemaphoreHandle_t mutexToDelete = mutex;
    mutex = nullptr;
    vSemaphoreDelete(mutexToDelete);
  }

  stream = nullptr;
}
