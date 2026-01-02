#include <gtest/gtest.h>

#include <memory>

#include "AsyncATHandler.h"
#include "BoneBuilder.h"
#include "SerialCommunicator.h"
#include "Stream.h"
#include "common.h"
#include "esp_log.h"

using ::testing::NiceMock;

class AsyncATHandlerBasicTest : public FreeRTOSTest {
 public:
  SerialCommunicator* testStream = nullptr;
  AsyncATHandler* handler = nullptr;

 protected:
  void SetUp() override {
    FreeRTOSTest::SetUp();

    testStream = new SerialCommunicator();
    log_d("Stream created: %p", testStream);

    handler = new AsyncATHandler();
    log_d("AsyncATHandler created: %p", handler);
  }

  void TearDown() override {
    if (handler) {
      while (true) {
        auto promise = handler->popCompletedPromise(0);
        if (!promise) {
          break;  // No more promises to clean up
        }
      }
      bool success = CleanupATHandler(handler);
      if (!success) { log_w("Handler teardown may have failed"); }
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      delete handler;
      handler = nullptr;
    }

    if (testStream) {
      log_d("Deleting testStream: %p", testStream);
      delete testStream;
      testStream = nullptr;
    }

    FreeRTOSTest::TearDown();
  }
};

TEST_F(AsyncATHandlerBasicTest, InitializationTest) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        // Test initial state - handler should not be connected
        if (handler->getStream() != nullptr) {
          throw std::runtime_error("Handler should not have stream initially");
        }

        // Test successful initialization
        if (!handler->begin(*testStream)) { throw std::runtime_error("Handler begin failed"); }

        // Verify stream is set
        if (handler->getStream() != testStream) {
          throw std::runtime_error("Stream not properly set");
        }

        // Test that we cannot initialize twice
        if (handler->begin(*testStream)) {
          throw std::runtime_error("Should not initialize twice");
        }
      },
      "InitTest", configMINIMAL_STACK_SIZE * 4);

  log_d("Task result: %s", testResult ? "SUCCESS" : "FAILURE");
  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerBasicTest, SendSyncBasicCommand) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*testStream)) { throw std::runtime_error("Handler begin failed"); }

        // Give handler time to fully initialize
        vTaskDelay(pdMS_TO_TICKS(100));

        // Start responder task to simulate modem response
        struct ResponderData {
          AsyncATHandlerBasicTest* test;
          std::atomic<bool> complete{false};
        } responderData = {this, {false}};

        auto responderTask = [](void* pvParameters) {
          auto* data = static_cast<ResponderData*>(pvParameters);
          vTaskDelay(pdMS_TO_TICKS(100));

          data->test->testStream->mockResponse("AT\r\n");
          data->test->testStream->mockResponse("OK\r\n");

          data->complete = true;
          vTaskDelete(nullptr);
        };

        TaskHandle_t responderHandle = nullptr;
        xTaskCreate(
            responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1,
            &responderHandle);

        // Clear TX buffer before test
        testStream->ClearSentData();

        // Send sync command
        String response;
        bool success = handler->sendSync("AT", response, 3000);

        // Wait for responder to complete
        while (!responderData.complete.load()) { vTaskDelay(pdMS_TO_TICKS(10)); }

        // Give extra time for response processing
        vTaskDelay(pdMS_TO_TICKS(100));

        // Verify response
        if (!success) { throw std::runtime_error("Command should have succeeded"); }

        if (response.indexOf("OK") == -1) {
          throw std::runtime_error("Response should contain OK: " + response);
        }
      },
      "SyncBasicTest", configMINIMAL_STACK_SIZE * 6, 2, 5000);

  log_d("SendSync task result: %s", testResult ? "SUCCESS" : "FAILURE");
  EXPECT_TRUE(testResult);
}

// Simplified test to isolate the issue
TEST_F(AsyncATHandlerBasicTest, MinimalTest) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*testStream)) { throw std::runtime_error("Handler begin failed"); }

        // Just wait a bit
        vTaskDelay(pdMS_TO_TICKS(100));

        // Don't call end() here - let teardown handle it
      },
      "MinimalTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

ENV_BONES();
