#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "AsyncATHandler.h"
#include "FreeRTOSConfig.h"
#include "Stream.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

class GlobalSchedulerEnvironment : public ::testing::Environment {
 private:
  static std::thread globalSchedulerThread;
  static std::atomic<bool> globalSchedulerStarted;

 public:
  void SetUp() override {
    globalSchedulerThread = std::thread([]() {
      globalSchedulerStarted = true;
      vTaskStartScheduler();
    });
    while (!globalSchedulerStarted.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  void TearDown() override {
    if (globalSchedulerThread.joinable()) {
      vTaskEndScheduler();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      globalSchedulerThread.join();
    }
  }
};

inline std::thread GlobalSchedulerEnvironment::globalSchedulerThread;
inline std::atomic<bool> GlobalSchedulerEnvironment::globalSchedulerStarted{false};

inline bool runInFreeRTOSTask(
    std::function<void()> func, const char* taskName = "HelperTask", uint32_t stackSize = 2048,
    UBaseType_t priority = 2, uint32_t timeoutMs = 5000) {
  std::atomic<bool> taskComplete{false};
  std::atomic<bool> taskResult{true};

  auto taskWrapper = [](void* pvParameters) {
    struct TaskData {
      std::function<void()>* function;
      std::atomic<bool>* complete;
      std::atomic<bool>* result;
    };
    auto* data = static_cast<TaskData*>(pvParameters);

    try {
      (*data->function)();
    } catch (const std::exception& e) {
      log_e("[FreeRTOS Task] Exception caught: %s", e.what());
      data->result->store(false);
    } catch (...) {
      log_e("[FreeRTOS Task] Unknown exception caught");
      data->result->store(false);
    }

    data->complete->store(true);
    vTaskDelete(nullptr);
  };

  struct TaskData {
    std::function<void()>* function;
    std::atomic<bool>* complete;
    std::atomic<bool>* result;
  } taskData = {&func, &taskComplete, &taskResult};

  TaskHandle_t taskHandle = nullptr;
  BaseType_t result =
      xTaskCreate(taskWrapper, taskName, stackSize, &taskData, priority, &taskHandle);

  if (result != pdPASS) { return false; }

  uint32_t waitTime = 0;
  while (!taskComplete.load() && waitTime < timeoutMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    waitTime += 10;
  }

  return taskComplete.load() && taskResult.load();
}

#define FREERTOS_TEST_MAIN()                                             \
  int main(int argc, char** argv) {                                      \
    ::testing::InitGoogleTest(&argc, argv);                              \
    ::testing::AddGlobalTestEnvironment(new GlobalSchedulerEnvironment); \
    return RUN_ALL_TESTS();                                              \
  }

class FreeRTOSTest : public ::testing::Test {
 protected:
  void TearDown() override { std::this_thread::sleep_for(std::chrono::milliseconds(50)); }
};

// Common responder task pattern used across multiple AT handler tests
inline void InjectDataWithDelay(
    class MockStream* mockStream, const std::string& data, uint32_t delayMs = 50) {
  struct InjectorData {
    MockStream* stream;
    std::string data;
    uint32_t delay;
  };

  auto* injectorData = new InjectorData{mockStream, data, delayMs};

  auto injectorTask = [](void* pvParameters) {
    auto* data = static_cast<InjectorData*>(pvParameters);
    vTaskDelay(pdMS_TO_TICKS(data->delay));
    data->stream->InjectRxData(data->data);
    delete data;
    vTaskDelete(nullptr);
  };

  TaskHandle_t injectorHandle = nullptr;
  xTaskCreate(
      injectorTask, "InjectorTask", configMINIMAL_STACK_SIZE * 2, injectorData, 1, &injectorHandle);
}

// Common teardown pattern for AT handler tests
inline bool CleanupATHandler(class AsyncATHandler* handler) {
  return runInFreeRTOSTask([handler]() { handler->end(); }, "TeardownTask");
}

// Test message structure used in queue tests
struct TestMessage {
  int id;
  const char* name;
  float value;

  bool operator==(const TestMessage& other) const {
    return id == other.id && strcmp(name, other.name) == 0 && value == other.value;
  }
};
