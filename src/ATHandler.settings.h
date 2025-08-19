#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>

// Queue sizes
#ifndef AT_COMMAND_QUEUE_SIZE
#define AT_COMMAND_QUEUE_SIZE 10
#endif

#ifndef AT_RESPONSE_QUEUE_SIZE
#define AT_RESPONSE_QUEUE_SIZE 20
#endif

// Task configuration
#ifndef AT_TASK_STACK_SIZE
#define AT_TASK_STACK_SIZE configMINIMAL_STACK_SIZE * 4
#endif

#ifndef AT_TASK_PRIORITY
#define AT_TASK_PRIORITY tskIDLE_PRIORITY + 2
#endif

#ifndef AT_TASK_CORE
#define AT_TASK_CORE 1
#endif

// Timing
#ifndef AT_DEFAULT_TIMEOUT
#define AT_DEFAULT_TIMEOUT 1000
#endif

#ifndef AT_QUEUE_TIMEOUT
#define AT_QUEUE_TIMEOUT 100
#endif

// Buffer sizes
#ifndef AT_RESPONSE_BUFFER_SIZE
#define AT_RESPONSE_BUFFER_SIZE 1024
#endif

#ifndef AT_COMMAND_MAX_LENGTH
#define AT_COMMAND_MAX_LENGTH 512
#endif

#ifndef AT_EXPECTED_RESPONSE_MAX_LENGTH
#define AT_EXPECTED_RESPONSE_MAX_LENGTH 512
#endif

struct ATCommand {
  uint32_t id;
  char command[AT_COMMAND_MAX_LENGTH];
  char expectedResponse[AT_EXPECTED_RESPONSE_MAX_LENGTH];
  uint32_t timeout;
  bool waitForResponse;
  SemaphoreHandle_t responseSemaphore;
};

struct ATResponse {
  uint32_t commandId;
  char response[AT_RESPONSE_BUFFER_SIZE];
  bool success;
  unsigned long timestamp;
};
