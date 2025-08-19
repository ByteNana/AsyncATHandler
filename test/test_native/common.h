#pragma once

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>
#include "FreeRTOSConfig.h"
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

// Define static members
inline std::thread GlobalSchedulerEnvironment::globalSchedulerThread;
inline std::atomic<bool> GlobalSchedulerEnvironment::globalSchedulerStarted{false};

// Helper function to run code in a FreeRTOS task context
inline bool runInFreeRTOSTask(std::function<void()> func, const char* taskName = "HelperTask", uint32_t stackSize = 2048, UBaseType_t priority = 2, uint32_t timeoutMs = 5000) {
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
        } catch (...) {
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
    BaseType_t result = xTaskCreate(taskWrapper, taskName, stackSize, &taskData, priority, &taskHandle);

    if (result != pdPASS) {
        return false;
    }

    // Wait for completion with timeout
    uint32_t waitTime = 0;
    while (!taskComplete.load() && waitTime < timeoutMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        waitTime += 10;
    }

    return taskComplete.load() && taskResult.load();
}

// Helper macro to set up the environment in main
#define FREERTOS_TEST_MAIN()                                             \
  int main(int argc, char** argv) {                                      \
    ::testing::InitGoogleTest(&argc, argv);                              \
    ::testing::AddGlobalTestEnvironment(new GlobalSchedulerEnvironment); \
    return RUN_ALL_TESTS();                                              \
  }

// Base test class with common teardown
class FreeRTOSTest : public ::testing::Test {
 protected:
  void TearDown() override { std::this_thread::sleep_for(std::chrono::milliseconds(50)); }
};
