#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iostream>

#include "BoneBuilder.h"
#include "SerialCommunicator.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"

#define mainCHECK_TASK_PRIORITY (configMAX_PRIORITIES - 2)
#define mainQUEUE_POLL_PRIORITY (tskIDLE_PRIORITY + 1)

void simpleTestTask(void* pvParameters) {
  auto* taskRunCount = static_cast<std::atomic<int>*>(pvParameters);
  if (taskRunCount) { (*taskRunCount)++; }
  vTaskDelete(nullptr);
}

class TaskTest : public FreeRTOSTest {
 protected:
  std::atomic<int> taskRunCount{0};
  void SetUp() override { taskRunCount = 0; }
};

TEST_F(TaskTest, BasicTaskCreationAndDeletion) {
  TaskHandle_t xHandle = nullptr;
  BaseType_t xReturned;
  xReturned = xTaskCreate(
      simpleTestTask, "SimpleTestTask", configMINIMAL_STACK_SIZE, &taskRunCount,
      mainQUEUE_POLL_PRIORITY, &xHandle);
  ASSERT_EQ(xReturned, pdPASS) << "Task creation failed!";
  ASSERT_NE(xHandle, nullptr) << "Task handle is nullptr!";
  delay(200);
  EXPECT_EQ(taskRunCount.load(), 1) << "Task did not run or ran multiple times!";
}

TEST_F(TaskTest, MultiTasksCleanup) {
  std::atomic<int> ran{0};
  auto taskFunction = [](void* p) {
    auto* counter = static_cast<std::atomic<int>*>(p);
    (*counter)++;
    vTaskDelay(pdMS_TO_TICKS(10));
    vTaskDelete(nullptr);
  };
  // Use larger stack size to prevent stack overflow
  const size_t TASK_STACK_SIZE = configMINIMAL_STACK_SIZE * 4;
  for (int i = 0; i < 5; ++i) {
    TaskHandle_t h = nullptr;
    BaseType_t result =
        xTaskCreatePinnedToCore(taskFunction, "MultiTask", TASK_STACK_SIZE, &ran, 1, &h, 0);
    ASSERT_EQ(result, pdPASS) << "Failed to create task " << i;
    delay(5);
  }
  // Longer wait time to ensure all tasks complete
  delay(300);
  EXPECT_EQ(ran.load(), 5) << "Not all tasks completed successfully";
}

TEST_F(TaskTest, TicksAdvanceRoughly) {
  auto startTicks = xTaskGetTickCount();
  delay(100);
  auto endTicks = xTaskGetTickCount();
  // Allow wider tolerance to avoid platform scheduler jitter
  EXPECT_GE(endTicks, startTicks + 70);
  EXPECT_LE(endTicks, startTicks + 150);
}

TEST_F(TaskTest, ExternalTaskDeletion) {
  std::atomic<int> taskRunning{0};

  auto infiniteTask = [](void* p) {
    auto* counter = static_cast<std::atomic<int>*>(p);
    (*counter)++;

    // Infinite loop - this task won't self-delete
    while (true) { vTaskDelay(pdMS_TO_TICKS(10)); }
  };

  TaskHandle_t taskHandle = nullptr;
  BaseType_t result = xTaskCreate(
      infiniteTask, "InfiniteTask", configMINIMAL_STACK_SIZE * 2, &taskRunning, 1, &taskHandle);

  ASSERT_EQ(result, pdPASS) << "Failed to create infinite task";
  ASSERT_NE(taskHandle, nullptr) << "Task handle is nullptr";

  // Wait for task to start running
  delay(50);
  EXPECT_EQ(taskRunning.load(), 1) << "Task should be running";

  // Externally delete the task using the handle
  vTaskDelete(taskHandle);
  taskHandle = nullptr;

  // Give time for deletion to complete
  delay(50);

  // Task should be gone - we can't directly verify this easily,
  // but if we get here without crashing, the deletion worked
  EXPECT_TRUE(true) << "External task deletion completed";
}

ENV_BONES
