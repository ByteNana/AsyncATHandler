#pragma once

#include <stddef.h>
#include <stdint.h>

#include "projdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*TaskFunction_t)(void*);

typedef struct TaskControlBlock* TaskHandle_t;

// Priorities
#ifndef tskIDLE_PRIORITY
#define tskIDLE_PRIORITY 0
#endif

// API
BaseType_t xTaskCreate(
    TaskFunction_t pxTaskCode, const char* const pcName, const uint32_t usStackDepth,
    void* const pvParameters, UBaseType_t uxPriority, TaskHandle_t* const pxCreatedTask);

void vTaskDelete(TaskHandle_t xTask);

void vTaskDelay(const TickType_t xTicksToDelay);

void vTaskStartScheduler(void);
void vTaskEndScheduler(void);

TickType_t xTaskGetTickCount(void);

// Optional helpers used by hooks/tests
size_t xPortGetFreeHeapSize(void);

#ifdef __cplusplus
}
#endif
