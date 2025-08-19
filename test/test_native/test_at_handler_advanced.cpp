#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "AsyncATHandler.h"
#include "Stream.h"
#include "common.h"
#include "esp_log.h"

using ::testing::NiceMock;

class AsyncATHandlerAdvancedTest : public FreeRTOSTest {
 protected:
  void SetUp() override {
    FreeRTOSTest::SetUp();
    mockStream = new NiceMock<MockStream>();
    mockStream->SetupDefaults();
    handler = new AsyncATHandler();
  }

  void TearDown() override {
    if (handler) {
      bool success = runInFreeRTOSTask([this]() { handler->end(); }, "TeardownTask");

      if (!success) { log_w("Handler teardown may have failed"); }

      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      delete handler;
      handler = nullptr;
    }
    if (mockStream) {
      log_w("Cleaning up mockStream");
      delete mockStream;
      mockStream = nullptr;
    }
    FreeRTOSTest::TearDown();
  }

  void WaitFor(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

 public:
  NiceMock<MockStream>* mockStream;
  AsyncATHandler* handler;
};

TEST_F(AsyncATHandlerAdvancedTest, VariadicSendCommandHelper) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        // Start a separate task to inject the response
        struct ResponderData {
          AsyncATHandlerAdvancedTest* test;
          std::atomic<bool> complete{false};
        } responderData = {this, {false}};

        auto responderTask = [](void* pvParameters) {
          auto* data = static_cast<ResponderData*>(pvParameters);
          vTaskDelay(pdMS_TO_TICKS(50));
          data->test->mockStream->InjectRxData("OK\r\n");
          data->complete = true;
          vTaskDelete(nullptr);
        };

        TaskHandle_t responderHandle = nullptr;
        xTaskCreate(
            responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1,
            &responderHandle);

        // Test variadic command building
        mockStream->ClearTxData();
        String response;
        bool sendResult = handler->sendCommand(response, "OK", 1000, "AT+", "VAR");

        vTaskDelay(pdMS_TO_TICKS(100));
        std::string sentData = mockStream->GetTxData();

        // Wait for responder to complete
        while (!responderData.complete.load()) { vTaskDelay(pdMS_TO_TICKS(10)); }

        if (!sendResult || sentData != "AT+VAR\r\n" || response != "OK\r\n") {
          throw std::runtime_error("Test failed: variadic command building or response incorrect");
        }
      },
      "VariadicTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerAdvancedTest, UnsolicitedResponseHandling) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        std::atomic<bool> callbackCalled{false};
        String unsolicitedData;

        handler->setUnsolicitedCallback([&](const String& response) {
          callbackCalled = true;
          unsolicitedData = response;
          log_i("[Callback] Unsolicited response received: '%s'", response.c_str());
        });

        log_i("[Test Task] Injecting unsolicited data...");
        mockStream->InjectRxData("+CMT: \"+1234567890\",\"\",\"24/01/15,10:30:00\"\r\n");
        mockStream->InjectRxData("Hello World\r\n");
        vTaskDelay(pdMS_TO_TICKS(200));

        if (!callbackCalled) throw std::runtime_error("Callback not called");
        if (!unsolicitedData.startsWith("+CMT:"))
          throw std::runtime_error("Incorrect unsolicited data");
      },
      "UnsolicitedTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerAdvancedTest, LongResponseNotTruncated) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        // Generate a long response (600 chars + OK)
        std::string longLine(600, 'A');

        // Start a separate task to inject the long response
        struct ResponderData {
          AsyncATHandlerAdvancedTest* test;
          std::string longLine;
          std::atomic<bool> complete{false};
        } responderData = {this, longLine, {false}};

        auto responderTask = [](void* pvParameters) {
          auto* data = static_cast<ResponderData*>(pvParameters);
          vTaskDelay(pdMS_TO_TICKS(50));
          std::string injected = data->longLine + "\r\nOK\r\n";
          data->test->mockStream->InjectRxData(injected);
          data->complete = true;
          vTaskDelete(nullptr);
        };

        TaskHandle_t responderHandle = nullptr;
        xTaskCreate(
            responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 3, &responderData, 1,
            &responderHandle);

        // Send command and expect long response
        String response;
        bool result = handler->sendCommand("AT+LONG", response);

        // Wait for responder to complete
        while (!responderData.complete.load()) { vTaskDelay(pdMS_TO_TICKS(10)); }

        String expectedResponse = String(longLine.c_str()) + "\r\nOK\r\n";

        if (!result || response != expectedResponse) {
          log_e(
              "Expected length: %d, Actual length: %d", expectedResponse.length(),
              response.length());
          throw std::runtime_error("Test failed: long response was truncated or incorrect");
        }
      },
      "LongResponseTest", configMINIMAL_STACK_SIZE * 6);  // Extra large stack for long string

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
