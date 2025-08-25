#include "ATPromise.h"

#include <esp_log.h>

ATPromise::ATPromise(uint32_t id, uint32_t timeout)
    : commandId(id), response(nullptr), timeoutMs(timeout) {
  completionSemaphore = xSemaphoreCreateBinary();
  if (!completionSemaphore) { log_e("Failed to create completion semaphore"); }
  response = new ATResponse(id);
}

ATPromise::~ATPromise() {
  if (completionSemaphore) { vSemaphoreDelete(completionSemaphore); }
  delete response;
}

ATPromise* ATPromise::expect(const String& expectedResponse) {
  log_d("Promise [%u] adding expected response: %s", commandId, expectedResponse.c_str());
  expectedResponses.push_back(expectedResponse);
  hasExpected = true;
  return this;
}

ATPromise* ATPromise::timeout(uint32_t ms) {
  log_d("Promise [%u] setting timeout to %u ms", commandId, ms);
  timeoutMs = ms;
  return this;
}

bool ATPromise::wait() {
  log_d("Promise [%u] waiting for completion with timeout %u ms", commandId, timeoutMs);
  if (!completionSemaphore) return false;
  return xSemaphoreTake(completionSemaphore, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void ATPromise::addResponseLine(const ResponseLine& line) {
  if (isCompleted()) return;

  if (response == nullptr) {
    log_e("Promise [%u] has no response object to add line to", commandId);
    return;
  }

  response->addLine(line);

  // Check if the current line matches the NEXT expected response
  if (!expectedResponses.empty() && line.content.indexOf(expectedResponses.front()) != -1) {
    log_d(
        "Promise [%u] matched expected response: %s", commandId, expectedResponses.front().c_str());
    expectedResponses.pop_front();
  }

  if (line.isFinalResponse()) {
    log_i("Promise [%u] completed", commandId);
    log_d("Full response:\n%s", response->getFullResponse().c_str());
    if (completionSemaphore) { xSemaphoreGive(completionSemaphore); }
  }

  if (!hasExpected) { return; }
  if (expectedResponses.empty()) {
    log_i("Promise [%u] completed (no more expectations)", commandId);
    log_d("Full response:\n%s", response->getFullResponse().c_str());
    if (completionSemaphore) { xSemaphoreGive(completionSemaphore); }
  }
}

bool ATPromise::matchesExpected(const String& line) const {
  if (expectedResponses.empty()) return false;
  return line.indexOf(expectedResponses.front()) != -1;
}

bool ATPromise::isCompleted() const {
  if (response) { return response->isCompleted(); }
  return false;
}
