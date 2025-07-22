#pragma once

#include <FreeRTOS.h>

#include "FreeRTOSConfig.h"
#include "projdefs.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

inline BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t pxTaskCode, const char* const pcName, const portSTACK_TYPE usStackDepth,
    void* const pvParameters, UBaseType_t uxPriority, TaskHandle_t* const pxCreatedTask,
    BaseType_t coreID) {
  return xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
}
