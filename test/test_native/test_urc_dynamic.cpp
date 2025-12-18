#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "AsyncATHandler.h"
#include "BoneBuilder.h"
#include "SerialCommunicator.h"  // Added this line
#include "Stream.h"
#include "common.h"
#include "esp_log.h"

using ::testing::NiceMock;

class AsyncATHandlerURCTest : public FreeRTOSTest {
 protected:
  SerialCommunicator* testStream = nullptr;
  AsyncATHandler* handler = nullptr;

  void SetUp() override {
    FreeRTOSTest::SetUp();
    testStream = new SerialCommunicator();
    handler = new AsyncATHandler();
  }

  void TearDown() override {
    if (handler) {
      bool success = CleanupATHandler(handler);
      if (!success) { log_w("Handler teardown may have failed"); }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      delete handler;
      handler = nullptr;
    }
    if (testStream) {
      delete testStream;
      testStream = nullptr;
    }
    FreeRTOSTest::TearDown();
  }
};

TEST_F(AsyncATHandlerURCTest, RegisterAndTriggerURC) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(handler->begin(*testStream));
        vTaskDelay(pdMS_TO_TICKS(50));

        std::atomic<bool> called{false};
        String captured;

        handler->urc.registerEvent("+CMTI:", [&](const String& urc) {
          called = true;
          captured = urc;
        });

        // Inject a URC line that matches the registered prefix
        testStream->mockResponseWithDelay("+CMTI: \"SM\",1\r\n", 50);

        // Wait for it to be processed
        vTaskDelay(pdMS_TO_TICKS(300));

        ASSERT_TRUE(called.load());
        ASSERT_TRUE(captured.startsWith("+CMTI:"));
      },
      "URCRegisterTrigger", configMINIMAL_STACK_SIZE * 6, 2, 5000);

  EXPECT_TRUE(ok);
}

TEST_F(AsyncATHandlerURCTest, UnregisterStopsCallback) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(handler->begin(*testStream));
        vTaskDelay(pdMS_TO_TICKS(50));

        std::atomic<int> count{0};
        handler->urc.registerEvent("RING", [&](const String&) { count.fetch_add(1); });
        testStream->mockResponseWithDelay("RING\r\n", 20);
        vTaskDelay(pdMS_TO_TICKS(200));
        ASSERT_EQ(count.load(), 1);

        handler->urc.unregisterEvent("RING");
        testStream->mockResponseWithDelay("RING\r\n", 20);
        vTaskDelay(pdMS_TO_TICKS(200));
        ASSERT_EQ(count.load(), 1) << "Unregistered handler should not fire";
      },
      "URCUnregister", configMINIMAL_STACK_SIZE * 6, 2, 5000);

  EXPECT_TRUE(ok);
}

TEST_F(AsyncATHandlerURCTest, MultipleHandlersIndependent) {
  bool ok = runInFreeRTOSTask(
      [this]() {
        ASSERT_TRUE(handler->begin(*testStream));
        vTaskDelay(pdMS_TO_TICKS(50));

        std::atomic<int> ringCount{0};
        std::atomic<int> clipCount{0};

        handler->urc.registerEvent("RING", [&](const String&) { ringCount.fetch_add(1); });
        handler->urc.registerEvent("+CLIP:", [&](const String&) { clipCount.fetch_add(1); });

        // Trigger only RING
        testStream->mockResponseWithDelay("RING\r\n", 30);
        vTaskDelay(pdMS_TO_TICKS(150));
        ASSERT_EQ(ringCount.load(), 1);
        ASSERT_EQ(clipCount.load(), 0);

        // Trigger only +CLIP:
        testStream->mockResponseWithDelay("+CLIP: \"+123\",129\r\n", 30);
        vTaskDelay(pdMS_TO_TICKS(150));
        ASSERT_EQ(ringCount.load(), 1);
        ASSERT_EQ(clipCount.load(), 1);
      },
      "URCMultiple", configMINIMAL_STACK_SIZE * 6, 2, 5000);

  EXPECT_TRUE(ok);
}

ENV_BONES