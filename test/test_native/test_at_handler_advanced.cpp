#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <memory> // FIX: Add memory header

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
    if (mockStream) {
      delete mockStream;
      mockStream = nullptr;
    }
    FreeRTOSTest::TearDown();
  }

 public:
  NiceMock<MockStream>* mockStream = nullptr;
  AsyncATHandler* handler = nullptr;
};

TEST_F(AsyncATHandlerAdvancedTest, SimpleSyncCommand) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        vTaskDelay(pdMS_TO_TICKS(100));

        InjectDataWithDelay(mockStream, "AT+TEST\r\nOK\r\n", 100);

        String response;
        bool success = handler->sendSync("AT+TEST", response, 2000);

        if (!success) { throw std::runtime_error("Sync command failed"); }

        if (response.indexOf("OK") == -1) {
          throw std::runtime_error("Response should contain OK");
        }

        log_i("[Test] Simple sync command test passed");
      },
      "SimpleSyncTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerAdvancedTest, VariadicSendCommandHelper) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        vTaskDelay(pdMS_TO_TICKS(100));

        struct ResponderData {
          AsyncATHandlerAdvancedTest* test;
          std::atomic<bool> complete{false};
        } responderData = {this, {false}};

        auto responderTask = [](void* pvParameters) {
          auto* data = static_cast<ResponderData*>(pvParameters);
          vTaskDelay(pdMS_TO_TICKS(100));

          data->test->mockStream->InjectRxData("AT+VAR\r\n");
          data->test->mockStream->InjectRxData("OK\r\n");

          data->complete = true;
          vTaskDelete(nullptr);
        };

        TaskHandle_t responderHandle = nullptr;
        xTaskCreate(
            responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1,
            &responderHandle);

        mockStream->ClearTxData();

        log_i("[Test] Testing variadic template: sendCommand(\"AT+\", \"VAR\")");

        ATPromise* promise = handler->sendCommand("AT+", "VAR");
        if (!promise) {
          throw std::runtime_error("Failed to create promise from variadic template");
        }

        bool waitResult = promise->wait();

        while (!responderData.complete.load()) { vTaskDelay(pdMS_TO_TICKS(10)); }
        vTaskDelay(pdMS_TO_TICKS(100));

        std::string sentData = mockStream->GetTxData();
        log_i("[Response] Sent data: '%s'", sentData.c_str());

        if (!waitResult) {
          throw std::runtime_error("Promise timed out");
        }

        ATResponse* response_obj = promise->getResponse();
        if (!response_obj->isSuccess()) {
          throw std::runtime_error("Command should have succeeded");
        }

        String response = response_obj->getFullResponse();
        log_i("[Response] Response: '%s'", response.c_str());

        if (sentData != "AT+VAR\r\n") {
          throw std::runtime_error("Command not sent correctly: " + sentData);
        }

        if (response.indexOf("OK") == -1) {
          throw std::runtime_error("Response should contain OK: " + response);
        }

        // FIX: Safely pop the promise
        auto p = handler->popCompletedPromise(promise->getId());
        if (!p) {
            throw std::runtime_error("Failed to pop completed promise");
        }
        log_i("[Test] Variadic template test passed: sendCommand(\"AT+\", \"VAR\")");
      },
      "VariadicTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

static std::atomic<bool> g_callbackCalled{false};
static String g_unsolicitedData = "";

TEST_F(AsyncATHandlerAdvancedTest, UnsolicitedResponseHandling) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        vTaskDelay(pdMS_TO_TICKS(100));

        g_callbackCalled = false;
        g_unsolicitedData = "";

        handler->onURC([](const String& response) {
          g_callbackCalled = true;
          g_unsolicitedData = response;
          log_i("[Callback] URC received: '%s'", response.c_str());
        });

        log_i("[Test] URC callback set, injecting URC data...");

        vTaskDelay(pdMS_TO_TICKS(100));

        mockStream->InjectRxData("+CMT: \"+1234567890\",\"\",\"24/01/15,10:30:00\"\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));

        log_i("[Test] Checking if callback was called...");
        if (!g_callbackCalled.load()) { throw std::runtime_error("URC callback not called"); }

        if (!g_unsolicitedData.startsWith("+CMT:")) {
          throw std::runtime_error("Incorrect URC data: " + g_unsolicitedData);
        }

        log_i("[Test] URC handling successful: '%s'", g_unsolicitedData.c_str());
      },
      "UnsolicitedTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
