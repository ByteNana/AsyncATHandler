#pragma once

#include <stdint.h>

typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
typedef short portSHORT;
typedef uint32_t portSTACK_TYPE;

#ifndef pdTRUE
#define pdTRUE 1
#endif
#ifndef pdFALSE
#define pdFALSE 0
#endif
#ifndef pdPASS
#define pdPASS 1
#endif

#ifndef portTICK_PERIOD_MS
#define portTICK_PERIOD_MS 1
#endif

#define pdMS_TO_TICKS(x) ((TickType_t)((x) / portTICK_PERIOD_MS))

#ifndef portMAX_DELAY
#define portMAX_DELAY ((TickType_t)0xffffffffUL)
#endif
