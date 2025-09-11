#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "AsyncATHandler.h"
#include "Stream.h"
#include "common.h"
#include "esp_log.h"

using ::testing::NiceMock;

class AsyncATHandlerPromiseTest : public FreeRTOSTest {
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

// TEST 1: Basic - Just Promise Creation/Deletion
TEST_F(AsyncATHandlerPromiseTest, PromiseCreationOnly) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }
        vTaskDelay(pdMS_TO_TICKS(100));

        ATPromise* promise = handler->sendCommand("AT+TEST");
        if (!promise) { throw std::runtime_error("Failed to create promise"); }

        // The handler owns the promise. Pop it to take ownership and clean up.
        auto p = handler->popCompletedPromise(promise->getId());
        if (!p) { throw std::runtime_error("Failed to pop promise after creation"); }
      },
      "CreationTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

// TEST 2: Promise Wait (No Response)
TEST_F(AsyncATHandlerPromiseTest, PromiseWaitTimeout) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }
        vTaskDelay(pdMS_TO_TICKS(100));

        ATPromise* promise = handler->sendCommand("AT+TIMEOUT");
        if (!promise) { throw std::runtime_error("Failed to create promise"); }

        promise->timeout(200);
        bool waitResult = promise->wait();
        if (waitResult) { throw std::runtime_error("Promise should have timed out"); }

        auto p = handler->popCompletedPromise(promise->getId());
        if (!p) { throw std::runtime_error("Failed to pop timed-out promise"); }
      },
      "TimeoutTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

// TEST 3: Promise with Actual Response
TEST_F(AsyncATHandlerPromiseTest, PromiseWithResponse) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }
        vTaskDelay(pdMS_TO_TICKS(100));

        InjectDataWithDelay(mockStream, "AT+TEST\r\nOK\r\n", 150);
        ATPromise* promise = handler->sendCommand("AT+TEST");
        if (!promise) { throw std::runtime_error("Failed to create promise"); }

        bool waitResult = promise->wait();
        if (!waitResult) { throw std::runtime_error("Promise timed out"); }

        ATResponse* response = promise->getResponse();
        if (!response) { throw std::runtime_error("No response object"); }

        if (!response->isSuccess()) { throw std::runtime_error("Command should have succeeded"); }

        String fullResponse = response->getFullResponse();
        if (fullResponse.indexOf("OK") == -1) {
          throw std::runtime_error("Response should contain OK");
        }

        auto p = handler->popCompletedPromise(promise->getId());
        if (!p) { throw std::runtime_error("Failed to pop completed promise"); }
      },
      "ResponseTest", configMINIMAL_STACK_SIZE * 6);

  EXPECT_TRUE(testResult);
}

// TEST 4: Promise Chaining Methods
TEST_F(AsyncATHandlerPromiseTest, PromiseChaining) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }
        vTaskDelay(pdMS_TO_TICKS(100));

        ATPromise* promise = handler->sendCommand("AT+CSQ")->expect("+CSQ:")->timeout(2000);

        if (!promise) { throw std::runtime_error("Failed to create chained promise"); }

        // This test only verifies the chaining and does not wait for a response
        // so the promise needs to be cleaned up manually.
        auto p = handler->popCompletedPromise(promise->getId());
        if (!p) { throw std::runtime_error("Failed to pop chained promise"); }
      },
      "ChainingTest", configMINIMAL_STACK_SIZE * 4);

  EXPECT_TRUE(testResult);
}

// TEST 5: Multiple Promises Management
TEST_F(AsyncATHandlerPromiseTest, MultiplePromises) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }
        vTaskDelay(pdMS_TO_TICKS(100));

        ATPromise* promise1 = handler->sendCommand("AT+TEST1");
        if (!promise1) throw std::runtime_error("Failed to create promise1");

        ATPromise* promise2 = handler->sendCommand("AT+TEST2");
        if (!promise2) { throw std::runtime_error("Failed to create promise2"); }

        ATPromise* promise3 = handler->sendCommand("AT+TEST3");
        if (!promise3) { throw std::runtime_error("Failed to create promise3"); }

        auto p3 = handler->popCompletedPromise(promise3->getId());
        if (!p3) throw std::runtime_error("Failed to pop promise3");
        auto p2 = handler->popCompletedPromise(promise2->getId());
        if (!p2) throw std::runtime_error("Failed to pop promise2");
        auto p1 = handler->popCompletedPromise(promise1->getId());
        if (!p1) throw std::runtime_error("Failed to pop promise1");
      },
      "MultipleTest", configMINIMAL_STACK_SIZE * 6);

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
