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
#define AT_TASK_STACK_SIZE 4096
#endif

#ifndef AT_TASK_PRIORITY
#define AT_TASK_PRIORITY 2
#endif

#ifndef AT_TASK_CORE
#define AT_TASK_CORE 1
#endif

// Timing
#ifndef AT_DEFAULT_TIMEOUT
#define AT_DEFAULT_TIMEOUT 5000
#endif

#ifndef AT_QUEUE_TIMEOUT
#define AT_QUEUE_TIMEOUT 100
#endif

// Buffer sizes
#ifndef AT_RESPONSE_BUFFER_SIZE
#define AT_RESPONSE_BUFFER_SIZE 256
#endif

struct ATCommand {
  uint32_t id;
  String command;
  String expectedResponse;
  uint32_t timeout;
  bool waitForResponse;
  SemaphoreHandle_t responseSemaphore;
};

struct ATResponse {
  uint32_t commandId;
  String response;
  bool success;
  unsigned long timestamp;
};

struct PendingCommandInfo {
  uint32_t id;
  SemaphoreHandle_t responseSemaphore;
  String expectedResponse;
  bool active;

  PendingCommandInfo() : id(0), responseSemaphore(nullptr), expectedResponse(""), active(false) {}
};
