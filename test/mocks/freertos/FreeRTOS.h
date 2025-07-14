#pragma once
#include <iostream>

#include "queue/queue.h"
#include "semaphore/semaphore.h"
#include "task/task.h"

typedef void* SemaphoreHandle_t;
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define pdTRUE ((BaseType_t)1)
#define pdFALSE ((BaseType_t)0)
#define pdPASS pdTRUE
#define pdFAIL pdFALSE
#define portMAX_DELAY 0xFFFFFFFFUL  // Ensure it's unsigned long for consistency

extern "C" {
// Queue Functions
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait);
BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait);
void vQueueDelete(QueueHandle_t xQueue);
QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);

// Task Functions
BaseType_t xTaskCreatePinnedToCore(
    void (*taskFunction)(void*), const char* name, uint32_t stackSize, void* parameter,
    UBaseType_t priority, TaskHandle_t* handle, BaseType_t coreID);
void vTaskDelete(TaskHandle_t handle);
void vTaskDelay(TickType_t ticks);
TickType_t xTaskGetTickCount();  // For mocking time progression

// Semaphore/Mutex Functions
SemaphoreHandle_t xSemaphoreCreateBinary();
SemaphoreHandle_t xSemaphoreCreateMutex();  // This will create a RecursiveMutex
void vSemaphoreDelete(SemaphoreHandle_t sem);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout);

// Utility/Time Functions
TickType_t pdMS_TO_TICKS(uint32_t ms);

}  // extern "C"
