#pragma once

#include <stdint.h>

#include "projdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QueueControlBlock* QueueHandle_t;

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
void vQueueDelete(QueueHandle_t xQueue);

BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait);

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue);

#ifdef __cplusplus
}
#endif

