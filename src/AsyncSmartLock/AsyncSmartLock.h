#pragma once

#include <Arduino.h>

#include <memory>

#include "freertos/FreeRTOS.h"

class AsyncSmartLock {
 private:
  using Ptr = std::shared_ptr<void>;
  SemaphoreHandle_t mutex = nullptr;

 public:
  AsyncSmartLock() {
    this->mutex = xSemaphoreCreateMutex();
    configASSERT(this->mutex);
  }

  ~AsyncSmartLock() {
    configASSERT(mutex);
    vSemaphoreDelete(mutex);
  }

  void lock() {
    configASSERT(mutex);
    xSemaphoreTake(mutex, portMAX_DELAY);
  }

  void unlock() {
    configASSERT(mutex);
    xSemaphoreGive(mutex);
  }

  // `Ptr` automatically releases via destructor when refcount == 0
  Ptr guard() {
    lock();
    return Ptr(this, [this](void*) { this->unlock(); });
  }
};
