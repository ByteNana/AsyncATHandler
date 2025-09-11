#pragma once
#include <Arduino.h>

#include <deque>
#include <vector>

#include "../ATResponse/ATResponse.h"
#include "freertos/FreeRTOS.h"

class ATPromise {
 private:
  bool hasExpected = false;
  uint32_t commandId;
  ATResponse* response;
  SemaphoreHandle_t completionSemaphore;
  std::deque<String> expectedResponses;
  uint32_t timeoutMs;

 public:
  ATPromise(uint32_t id, uint32_t timeout = 5000);
  ~ATPromise();
  ATPromise* expect(const String& expectedResponse);
  ATPromise* timeout(uint32_t ms);
  bool wait();
  void addResponseLine(const ResponseLine& line);
  bool matchesExpected(const String& line) const;
  bool isCompleted() const;

  ATResponse* getResponse() { return response; }
  uint32_t getId() const { return commandId; }
};
