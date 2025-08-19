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

using ::testing::_;
using ::testing::AtLeast;
using ::testing::NiceMock;

class AsyncATHandlerSyncTest : public FreeRTOSTest {
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

TEST_F(AsyncATHandlerSyncTest, SendSyncCommandWithOKResponse) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

    // Start a separate task to inject the response after a delay
    struct ResponderData {
      AsyncATHandlerSyncTest* test;
      std::atomic<bool> complete{false};
    } responderData = {this, {false}};

    auto responderTask = [](void* pvParameters) {
      auto* data = static_cast<ResponderData*>(pvParameters);
      vTaskDelay(pdMS_TO_TICKS(50));
      log_i("[Responder Task] Injecting OK with whitespace");
      data->test->mockStream->InjectRxData("OK \r\n");
      data->complete = true;
      vTaskDelete(nullptr);
    };

    TaskHandle_t responderHandle = nullptr;
    xTaskCreate(responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1, &responderHandle);

    // Execute the main test logic in this same task
    String response;
    bool sendResult = handler->sendCommand("AT", response);
    log_i("[Main Test Task] sendCommand returned: %s", sendResult ? "TRUE" : "FALSE");
    log_i("[Main Test Task] Received response: '%s'", response.c_str());

    // Wait for responder to complete
    while (!responderData.complete.load()) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (!sendResult || response != "OK \r\n") {
      throw std::runtime_error("Test failed: incorrect response");
    }
  }, "SyncOKTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerSyncTest, SendSyncCommandWithTimeout) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        String response;
        bool sendResult = handler->sendCommand("AT+TIMEOUT", response, "OK", 100);
        log_i("[Test Task] sendCommand (timeout) returned: %s", sendResult ? "TRUE" : "FALSE");
        log_i("[Test Task] Received response (timeout): '%s'", response.c_str());

        if (sendResult) throw std::runtime_error("Should have timed out");
        if (!response.empty()) throw std::runtime_error("Response should be empty on timeout");
      },
      "TimeoutTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerSyncTest, SendSyncCommandWithErrorResponse) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

    // Start a separate task to inject the error response
    struct ResponderData {
      AsyncATHandlerSyncTest* test;
      std::atomic<bool> complete{false};
    } responderData = {this, {false}};

    auto responderTask = [](void* pvParameters) {
      auto* data = static_cast<ResponderData*>(pvParameters);
      vTaskDelay(pdMS_TO_TICKS(50));
      log_i("[Responder Task] Injecting ERROR");
      data->test->mockStream->InjectRxData("ERROR\r\n");
      data->complete = true;
      vTaskDelete(nullptr);
    };

    TaskHandle_t responderHandle = nullptr;
    xTaskCreate(responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1, &responderHandle);

    // Execute the main test logic
    String response;
    bool sendResult = handler->sendCommand("AT+FAIL", response);
    log_i("[Test Task] sendCommand (error) returned: %s", sendResult ? "TRUE" : "FALSE");
    log_i("[Test Task] Received response (error): '%s'", response.c_str());

    // Wait for responder to complete
    while (!responderData.complete.load()) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (sendResult || response != "ERROR\r\n") {
      throw std::runtime_error("Test failed: should return false with ERROR response");
    }
  }, "SyncErrorTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerSyncTest, SendCommandWithoutResponseParameter) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

    // Start a separate task to inject the response
    struct ResponderData {
      AsyncATHandlerSyncTest* test;
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
    xTaskCreate(responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1, &responderHandle);

    // Clear TX data and send command without response parameter
    mockStream->ClearTxData();
    bool sendResult = handler->sendCommand("AT", "OK", 1000);

    vTaskDelay(pdMS_TO_TICKS(100));
    std::string sentData = mockStream->GetTxData();

    // Wait for responder to complete
    while (!responderData.complete.load()) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (!sendResult || sentData != "AT\r\n") {
      throw std::runtime_error("Test failed: command not sent correctly");
    }
  }, "SyncNoResponseTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
