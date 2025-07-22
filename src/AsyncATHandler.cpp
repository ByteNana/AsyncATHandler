#include "AsyncATHandler.h"
#include <esp_log.h>
#include <algorithm>

ATPromise* AsyncATHandler::sendCommand(const String& command) {
  if (!stream || !mutex) {
    return nullptr;
  }

  uint32_t id = nextCommandId++;
  auto promise = std::make_unique<ATPromise>(id);
  ATPromise* rawPromise = promise.get();

  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100))) {
    pendingPromises.push_back(std::move(promise));
    xSemaphoreGive(mutex);
    log_i("Sending command [%u]: %s", id, command.c_str());
    stream->print(command);
    stream->print("\r\n");
    stream->flush();
    return rawPromise;
  }
  log_e("Failed to acquire mutex for sendCommand");
  return nullptr;
}

bool AsyncATHandler::sendSync(const String& command, String& response, uint32_t timeout) {
  ATPromise* promise = sendCommand(command);
  if (!promise) {
    return false;
  }

  promise->timeout(timeout);
  log_i("Waiting for promise [%u] with timeout %u ms", promise->getId(), timeout);
  bool success = promise->wait();
  log_i("Promise [%u] wait finished. Success: %s", promise->getId(), success ? "TRUE" : "FALSE");

  if (success && promise->getResponse()) {
    response = promise->getResponse()->getFullResponse();
    success = promise->getResponse()->isSuccess();
  } else {
    response = "";
    success = false;
  }

  auto completedPromise = popCompletedPromise(promise->getId());
  if (!completedPromise) {
    log_w("Failed to pop completed promise [%u] from list", promise->getId());
  }
  return success;
}

bool AsyncATHandler::sendSync(const String& command, uint32_t timeout) {
  String response;
  return sendSync(command, response, timeout);
}

std::unique_ptr<ATPromise> AsyncATHandler::popCompletedPromise(uint32_t commandId) {
  std::unique_ptr<ATPromise> promise = nullptr;
  if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100))) {
    auto it = std::find_if(pendingPromises.begin(), pendingPromises.end(),
                           [commandId](const std::unique_ptr<ATPromise>& p) {
                             return p && p->getId() == commandId;
                           });
    if (it != pendingPromises.end()) {
      promise = std::move(*it);
      pendingPromises.erase(it);
      log_d("Popped promise with ID: %u", commandId);
    }
    xSemaphoreGive(mutex);
    log_d("Promise list size after pop: %zu", pendingPromises.size());
  }
  return promise;
}
