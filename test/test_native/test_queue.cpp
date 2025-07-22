#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "common.h"

class QueueTest : public FreeRTOSTest {};

TEST_F(QueueTest, CreateDelete) {
  auto q1 = xQueueCreate(10, sizeof(TestMessage));
  auto q2 = xQueueCreate(5, sizeof(int));
  ASSERT_NE(q1, nullptr);
  ASSERT_NE(q2, nullptr);
  EXPECT_EQ(uxQueueMessagesWaiting(q1), 0u);
  EXPECT_EQ(uxQueueMessagesWaiting(q2), 0u);
  vQueueDelete(q1);
  vQueueDelete(q2);
}

TEST_F(QueueTest, IntNonBlocking) {
  auto q = xQueueCreate(2, sizeof(int));
  ASSERT_NE(q, nullptr);
  int a = 10, b = 20, c = 30, out{};

  EXPECT_EQ(xQueueSend(q, &a, 0), pdTRUE);
  EXPECT_EQ(xQueueSend(q, &b, 0), pdTRUE);
  EXPECT_EQ(xQueueSend(q, &c, 0), pdFALSE);
  EXPECT_EQ(uxQueueMessagesWaiting(q), 2u);

  EXPECT_EQ(xQueueReceive(q, &out, 0), pdTRUE);
  EXPECT_EQ(out, 10);
  EXPECT_EQ(xQueueReceive(q, &out, 0), pdTRUE);
  EXPECT_EQ(out, 20);
  EXPECT_EQ(xQueueReceive(q, &out, 0), pdFALSE);

  vQueueDelete(q);
}

TEST_F(QueueTest, StructNonBlocking) {
  auto q = xQueueCreate(2, sizeof(TestMessage));
  ASSERT_NE(q, nullptr);

  TestMessage m1{1, "One", 1.1f};
  TestMessage m2{2, "Two", 2.2f};
  TestMessage m3{3, "Three", 3.3f};
  TestMessage out{};

  EXPECT_EQ(xQueueSend(q, &m1, 0), pdTRUE);
  EXPECT_EQ(xQueueSend(q, &m2, 0), pdTRUE);
  EXPECT_EQ(xQueueSend(q, &m3, 0), pdFALSE);

  EXPECT_EQ(xQueueReceive(q, &out, 0), pdTRUE);
  EXPECT_TRUE(out == m1);
  EXPECT_EQ(xQueueReceive(q, &out, 0), pdTRUE);
  EXPECT_TRUE(out == m2);
  EXPECT_EQ(xQueueReceive(q, &out, 0), pdFALSE);

  vQueueDelete(q);
}

TEST_F(QueueTest, IntBlockingWithTimeout) {
  struct BlockingTestData {
    QueueHandle_t queue;
    int* valueToSend;
    std::atomic<bool> attempted{false};
    std::atomic<bool> succeeded{false};
  };

  auto blockingSendTask = [](void* pvParameters) {
    auto* data = static_cast<BlockingTestData*>(pvParameters);
    data->attempted = true;
    data->succeeded = (xQueueSend(data->queue, data->valueToSend, pdMS_TO_TICKS(50)) == pdTRUE);
    vTaskDelete(nullptr);
  };

  auto q = xQueueCreate(1, sizeof(int));
  ASSERT_NE(q, nullptr);

  int a = 100, b = 200, out{};

  EXPECT_EQ(xQueueSend(q, &a, 0), pdTRUE);

  BlockingTestData testData;
  testData.queue = q;
  testData.valueToSend = &b;

  TaskHandle_t taskHandle = nullptr;
  xTaskCreate(
      blockingSendTask, "BlockingSend", configMINIMAL_STACK_SIZE, &testData, 1, &taskHandle);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(testData.attempted);
  EXPECT_FALSE(testData.succeeded);

  EXPECT_EQ(xQueueReceive(q, &out, 0), pdTRUE);
  EXPECT_EQ(out, 100);

  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(testData.succeeded);
  EXPECT_EQ(uxQueueMessagesWaiting(q), 1u);

  EXPECT_EQ(xQueueReceive(q, &out, 0), pdTRUE);
  EXPECT_EQ(out, 200);

  vQueueDelete(q);
}

FREERTOS_TEST_MAIN()
