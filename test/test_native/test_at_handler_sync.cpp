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
  bool testResult = runInFreeRTOSTask(
      [this]() {
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
        xTaskCreate(
            responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1,
            &responderHandle);

        // Execute the main test logic in this same task
        String response;
        bool sendResult = handler->sendCommand("AT", response);
        log_i("[Main Test Task] sendCommand returned: %s", sendResult ? "TRUE" : "FALSE");
        log_i("[Main Test Task] Received response: '%s'", response.c_str());

        // Wait for responder to complete
        while (!responderData.complete.load()) { vTaskDelay(pdMS_TO_TICKS(10)); }

        if (!sendResult || response != "OK \r\n") {
          throw std::runtime_error("Test failed: incorrect response");
        }
      },
      "SyncOKTest", configMINIMAL_STACK_SIZE * 4);

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
  bool testResult = runInFreeRTOSTask(
      [this]() {
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
        xTaskCreate(
            responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1,
            &responderHandle);

        // Execute the main test logic
        String response;
        bool sendResult = handler->sendCommand("AT+FAIL", response);
        log_i("[Test Task] sendCommand (error) returned: %s", sendResult ? "TRUE" : "FALSE");
        log_i("[Test Task] Received response (error): '%s'", response.c_str());

        // Wait for responder to complete
        while (!responderData.complete.load()) { vTaskDelay(pdMS_TO_TICKS(10)); }

        if (sendResult || response != "ERROR\r\n") {
          throw std::runtime_error("Test failed: should return false with ERROR response");
        }
      },
      "SyncErrorTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerSyncTest, SendCommandWithoutResponseParameter) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
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
        xTaskCreate(
            responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1,
            &responderHandle);

        // Clear TX data and send command without response parameter
        mockStream->ClearTxData();
        bool sendResult = handler->sendCommand("AT", "OK", 1000);

        vTaskDelay(pdMS_TO_TICKS(100));
        std::string sentData = mockStream->GetTxData();

        // Wait for responder to complete
        while (!responderData.complete.load()) { vTaskDelay(pdMS_TO_TICKS(10)); }

        if (!sendResult || sentData != "AT\r\n") {
          throw std::runtime_error("Test failed: command not sent correctly");
        }
      },
      "SyncNoResponseTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}
// Add these two new tests to test_at_handler_sync.cpp

TEST_F(AsyncATHandlerSyncTest, ResponseContainsAllLines) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        // Start a separate task to inject multi-line response
        struct ResponderData {
          AsyncATHandlerSyncTest* test;
          std::atomic<bool> complete{false};
        } responderData = {this, {false}};

        auto responderTask = [](void* pvParameters) {
          auto* data = static_cast<ResponderData*>(pvParameters);
          vTaskDelay(pdMS_TO_TICKS(50));
          log_i("[Responder Task] Injecting multi-line response");
          // Inject multiple lines before the expected response
          data->test->mockStream->InjectRxData("+CGMI: SIMCOM\r\n");
          data->test->mockStream->InjectRxData("Manufacturer: SIMCOM INCORPORATED\r\n");
          data->test->mockStream->InjectRxData("Model: SIM7600E\r\n");
          data->test->mockStream->InjectRxData("OK\r\n");
          data->complete = true;
          vTaskDelete(nullptr);
        };

        TaskHandle_t responderHandle = nullptr;
        xTaskCreate(
            responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1,
            &responderHandle);

        // Send command expecting OK
        String response;
        bool sendResult = handler->sendCommand("AT+CGMI", response, "OK", 1000);
        log_i("[Test Task] sendCommand returned: %s", sendResult ? "TRUE" : "FALSE");
        log_i("[Test Task] Full response: '%s'", response.c_str());

        // Wait for responder to complete
        while (!responderData.complete.load()) { vTaskDelay(pdMS_TO_TICKS(10)); }

        // Verify we got ALL lines, not just the expected "OK"
        if (!sendResult) { throw std::runtime_error("Command should have succeeded"); }

        if (response.indexOf("+CGMI: SIMCOM") == -1) {
          throw std::runtime_error("Response missing +CGMI line");
        }

        if (response.indexOf("Manufacturer: SIMCOM") == -1) {
          throw std::runtime_error("Response missing Manufacturer line");
        }

        if (response.indexOf("Model: SIM7600E") == -1) {
          throw std::runtime_error("Response missing Model line");
        }

        if (response.indexOf("OK") == -1) { throw std::runtime_error("Response missing OK line"); }

        log_i("[Test Task] All response lines collected successfully");
      },
      "MultiLineTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerSyncTest, TimeoutStillReturnsCollectedResponse) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        // Start a separate task to inject partial response (no completion)
        struct ResponderData {
          AsyncATHandlerSyncTest* test;
          std::atomic<bool> complete{false};
        } responderData = {this, {false}};

        auto responderTask = [](void* pvParameters) {
          auto* data = static_cast<ResponderData*>(pvParameters);
          vTaskDelay(pdMS_TO_TICKS(50));
          log_i("[Responder Task] Injecting partial response (no OK/ERROR)");
          // Inject data but NO completion response (OK/ERROR)
          data->test->mockStream->InjectRxData(
              "+QISTATE: 0,\"TCP\",\"192.168.1.1\",8080,5000,2,1\r\n");
          data->test->mockStream->InjectRxData("+QISTATE: 1,\"UDP\",\"10.0.0.1\",53,0,0,0\r\n");
          // Note: NO "OK" or "ERROR" - this will cause timeout
          data->complete = true;
          vTaskDelete(nullptr);
        };

        TaskHandle_t responderHandle = nullptr;
        xTaskCreate(
            responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1,
            &responderHandle);

        // Send command with short timeout - will timeout but should collect responses
        String response;
        bool sendResult = handler->sendCommand("AT+QISTATE", response, "OK", 200);  // Short timeout
        log_i("[Test Task] sendCommand returned: %s", sendResult ? "TRUE" : "FALSE");
        log_i("[Test Task] Response despite timeout: '%s'", response.c_str());

        // Wait for responder to complete
        while (!responderData.complete.load()) { vTaskDelay(pdMS_TO_TICKS(10)); }

        // Command should fail due to timeout
        if (sendResult) { throw std::runtime_error("Command should have timed out"); }

        // BUT response should contain the partial data we collected
        if (response.indexOf("+QISTATE") == -1) {
          throw std::runtime_error("Response should contain partial data despite timeout");
        }

        if (response.indexOf("TCP") == -1) {
          throw std::runtime_error("Response should contain TCP data");
        }

        if (response.indexOf("UDP") == -1) {
          throw std::runtime_error("Response should contain UDP data");
        }

        log_i("[Test Task] Timeout correctly returned partial response data");
      },
      "TimeoutResponseTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
