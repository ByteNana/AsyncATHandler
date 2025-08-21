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

class AsyncATHandlerWaitResponseTest : public FreeRTOSTest {
 protected:
  void SetUp() override {
    FreeRTOSTest::SetUp();
    mockStream = new NiceMock<MockStream>();
    mockStream->SetupDefaults();
    handler = new AsyncATHandler();
  }

  void TearDown() override {
    if (handler) {
      bool success = CleanupATHandler(handler);
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

 public:
  NiceMock<MockStream>* mockStream;
  AsyncATHandler* handler;
};

TEST_F(AsyncATHandlerWaitResponseTest, WaitResponseWithTimeoutOnly) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        // Step 1: Inject unsolicited data (simulating incoming responses)
        mockStream->InjectRxData("+CNEG: 0,1\r\n");
        mockStream->InjectRxData("+CNQ: 15,99\r\n");

        // Give reader task time to process the injected data
        vTaskDelay(pdMS_TO_TICKS(100));

        // Step 2: Wait for any response with timeout
        int8_t result = handler->waitResponse(1000, "+CNEG:", "+CNQ:");
        if (result <= 0) { throw std::runtime_error("Should have found some response"); }

        if (result != 1) {
          throw std::runtime_error(
              "Expected to find +CNEG: as first match, but got result: " + std::to_string(result));
        }

        // Step 3: Sanitize and check the response
        String foundResponse = "";
        foundResponse = handler->getResponse("+CNEG:");
        if (foundResponse.indexOf("+CNEG: 0,1") == -1) {
          throw std::runtime_error("Expected +CNEG response not found in sanitized buffer");
        }
        log_i("[Test] Found response %d as expected: %s", result, foundResponse.c_str());
        foundResponse = handler->getResponse("+CNQ:");
        if (foundResponse.indexOf("+CNQ: 15,99") == -1) {
          throw std::runtime_error("Expected +CNQ response not found in sanitized buffer");
        }

        log_i("[Test] Found response %d as expected: %s", result, foundResponse.c_str());
      },
      "TimeoutOnlyTest");
  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseTest, WaitResponseWithDefaultTimeout) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        // Inject response after a short delay
        vTaskDelay(pdMS_TO_TICKS(100));
        mockStream->InjectRxData("+QISTATE: 0,\"TCP\"");
        mockStream->InjectRxData("\r\nOK\r\n");

        if (!handler->waitResponse()) {
          throw std::runtime_error("Should have found response with default timeout");
        }

        log_i("[Test] Found response with default timeout");
      },
      "DefaultTimeoutTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseTest, WaitResponseTimeout) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        // Don't inject any data - should timeout
        if (handler->waitResponse(200)) { throw std::runtime_error("Should have timed out"); }

        log_i("[Test] Correctly timed out as expected");
      },
      "TimeoutTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseTest, WaitResponseMultipleExpected) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

    // Inject various responses
    mockStream->InjectRxData("+CREG: 0,1\r\n");
    mockStream->InjectRxData("+QIRD: 1024,data_here\r\n");
    mockStream->InjectRxData("OK\r\n");

    // Wait for specific responses - should find +QIRD: first
    int8_t result = handler->waitResponse("+QIRD:", "OK", "ERROR");

    if (result != 1) {
      throw std::runtime_error("Should have found +QIRD: as first match (index 1)");
    }

    log_i("[Test] Found +QIRD: as expected (result: %d)", result);
  }, "MultipleExpectedTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseTest, WaitResponseMultipleExpectedSecondMatch) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

    // Inject responses that will match the second expected response
    mockStream->InjectRxData("+CREG: 0,1\r\n");
    mockStream->InjectRxData("OK\r\n");  // This should match second position

    // Wait for responses - should find OK as second match
    int8_t result = handler->waitResponse("+QIRD:", "OK", "ERROR");

    if (result != 2) {
      throw std::runtime_error("Should have found OK as second match (index 2)");
    }

    log_i("[Test] Found OK as expected (result: %d)", result);
  }, "SecondMatchTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseTest, WaitResponseMultipleExpectedError) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

    // Inject responses that will match the third expected response
    mockStream->InjectRxData("+CREG: 0,1\r\n");
    mockStream->InjectRxData("ERROR\r\n");  // This should match third position

    // Wait for responses - should find ERROR as third match
    int8_t result = handler->waitResponse("+QIRD:", "OK", "ERROR");

    if (result != 3) {
      throw std::runtime_error("Should have found ERROR as third match (index 3)");
    }

    log_i("[Test] Found ERROR as expected (result: %d)", result);
  }, "ThirdMatchTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseTest, WaitResponseCustomTimeout) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

    // Start a separate task to inject the response after delay
    struct ResponderData {
      AsyncATHandlerWaitResponseTest* test;
      std::atomic<bool> complete{false};
    } responderData = {this, {false}};

    auto responderTask = [](void* pvParameters) {
      auto* data = static_cast<ResponderData*>(pvParameters);
      vTaskDelay(pdMS_TO_TICKS(150));  // Delay before injection
      log_i("[Responder Task] Injecting +QISTATE response");
      data->test->mockStream->InjectRxData("+QISTATE: 0,\"TCP\",\"192.168.1.1\"\r\n");
      data->complete = true;
      vTaskDelete(nullptr);
    };

    TaskHandle_t responderHandle = nullptr;
    xTaskCreate(
        responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, &responderData, 1,
        &responderHandle);

    // Wait with custom timeout - this runs concurrently with responder task
    int8_t result = handler->waitResponse(500, "+QISTATE:", "OK", "ERROR");

    // Wait for responder to complete
    while (!responderData.complete.load()) { vTaskDelay(pdMS_TO_TICKS(10)); }

    if (result != 1) {
      throw std::runtime_error("Should have found +QISTATE: with custom timeout");
    }

    log_i("[Test] Found +QISTATE: with custom timeout");
  }, "CustomTimeoutTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseTest, WaitResponseContainsMatching) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

    // Inject a response that CONTAINS the expected string
    mockStream->InjectRxData("+QIRD: 1024,some_long_data_payload_here\r\n");

    // Wait for partial match - should find it using contains logic
    int8_t result = handler->waitResponse("+QIRD:");

    if (result != 1) {
      throw std::runtime_error("Should have found +QIRD: using contains matching");
    }

    log_i("[Test] Found +QIRD: using contains logic");
  }, "ContainsMatchTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseTest, WaitResponseSingleExpectedUsage) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

    // Inject the expected response
    mockStream->InjectRxData("+QISTATE: 0,\"TCP\",\"connected\"\r\n");

    // Test the specific pattern: if (waitResponse("+QISTATE:") != 1)
    if (handler->waitResponse("+QISTATE:") != 1) {
      throw std::runtime_error("waitResponse should return 1 when +QISTATE: found");
    }

    log_i("[Test] Single expected response usage works correctly");
  }, "SingleExpectedTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseTest, WaitResponseMultipleTimeout) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

    // Inject responses that don't match any expected
    mockStream->InjectRxData("+CNEG: 0,1\r\n");
    mockStream->InjectRxData("+CNQ: 15,99\r\n");

    // Wait for responses that won't be found
    int8_t result = handler->waitResponse(300, "+QIRD:", "CONNECT", "+CME ERROR:");

    if (result != 0) {
      throw std::runtime_error("Should have timed out when no expected responses found");
    }

    log_i("[Test] Multiple expected timeout works correctly");
  }, "MultipleTimeoutTest");

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
