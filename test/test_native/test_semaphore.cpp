#include <gtest/gtest.h>

#include <atomic>
#include <chrono>

#include "FreeRTOSConfig.h"
#include "common.h"
#include "freertos/FreeRTOS.h"

class SemaphoreTest : public FreeRTOSTest {};

TEST_F(SemaphoreTest, BinaryBasic) {
  auto s = xSemaphoreCreateBinary();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(xSemaphoreTake(s, 0), pdFALSE);
  EXPECT_EQ(xSemaphoreGive(s), pdTRUE);
  EXPECT_EQ(xSemaphoreTake(s, 0), pdTRUE);
  EXPECT_EQ(xSemaphoreTake(s, 0), pdFALSE);
  vSemaphoreDelete(s);
}

TEST_F(SemaphoreTest, BlockingTake) {
  struct BlockingTestData {
    SemaphoreHandle_t semaphore;
    std::atomic<bool> attempted{false};
    std::atomic<bool> took{false};
  };
  auto blockingTakeTask = [](void* pvParameters) {
    auto* data = static_cast<BlockingTestData*>(pvParameters);
    data->attempted = true;
    data->took = (xSemaphoreTake(data->semaphore, pdMS_TO_TICKS(100)) == pdTRUE);
    vTaskDelete(nullptr);
  };
  auto s = xSemaphoreCreateBinary();
  ASSERT_NE(s, nullptr);
  BlockingTestData testData;
  testData.semaphore = s;
  TaskHandle_t taskHandle = nullptr;
  xTaskCreate(
      blockingTakeTask, "BlockingTask", configMINIMAL_STACK_SIZE, &testData, 1, &taskHandle);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(testData.attempted);
  EXPECT_FALSE(testData.took);
  EXPECT_EQ(xSemaphoreGive(s), pdTRUE);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(testData.took);
  EXPECT_EQ(xSemaphoreTake(s, 0), pdFALSE);
  vSemaphoreDelete(s);
}

FREERTOS_TEST_MAIN()
