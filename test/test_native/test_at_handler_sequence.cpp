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

class SequenceTest : public FreeRTOSTest {
 public:
  NiceMock<MockStream>* mockStream;

 protected:
  AsyncATHandler* handler;

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

  void InjectCommandResponse(
      const std::string& command, const std::string& response, uint32_t delayMs = 50) {
    struct ResponderData {
      SequenceTest* test;
      std::string command;
      std::string response;
      uint32_t delay;
    };

    auto* responderData = new ResponderData{this, command, response, delayMs};

    auto responderTask = [](void* pvParameters) {
      auto* data = static_cast<ResponderData*>(pvParameters);
      vTaskDelay(pdMS_TO_TICKS(data->delay));
      data->test->mockStream->InjectRxData(data->command + "\r\n");
      data->test->mockStream->InjectRxData(data->response + "\r\n");
      delete data;
      vTaskDelete(nullptr);
    };

    TaskHandle_t responderHandle = nullptr;
    xTaskCreate(
        responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, responderData, 1,
        &responderHandle);
  }
};

TEST_F(SequenceTest, GPRSConnectSequence) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

        // Step 1: Deactivate context
        InjectCommandResponse("AT+QIDEACT=1", "OK", 100);
        ATPromise* promise1 = handler->sendCommand("AT+QIDEACT=1");
        if (!promise1) { throw std::runtime_error("Step 1 failed: Promise creation failed"); }
        if (!promise1->wait()) {
          throw std::runtime_error("Step 1 failed: Timeout waiting for OK");
        }
        if (!promise1->getResponse()->isSuccess()) {
          throw std::runtime_error("Step 1 failed: Command should have succeeded");
        }

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 2: Configure APN
        InjectCommandResponse("AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"", "OK", 100);
        ATPromise* promise2 = handler->sendCommand("AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"");
        if (!promise2) { throw std::runtime_error("Step 2 failed: Promise creation failed"); }
        if (!promise2->wait()) {
          throw std::runtime_error("Step 2 failed: Timeout waiting for OK");
        }
        if (!promise2->getResponse()->isSuccess()) {
          throw std::runtime_error("Step 2 failed: Command should have succeeded");
        }

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 3: Activate context
        InjectCommandResponse("AT+QIACT=1", "OK", 100);
        ATPromise* promise3 = handler->sendCommand("AT+QIACT=1");
        if (!promise3) { throw std::runtime_error("Step 3 failed: Promise creation failed"); }
        if (!promise3->wait()) {
          throw std::runtime_error("Step 3 failed: Timeout waiting for OK");
        }
        if (!promise3->getResponse()->isSuccess()) {
          throw std::runtime_error("Step 3 failed: Command should have succeeded");
        }

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 4: Attach to GPRS
        InjectCommandResponse("AT+CGATT=1", "OK", 100);
        ATPromise* promise4 = handler->sendCommand("AT+CGATT=1");
        if (!promise4) { throw std::runtime_error("Step 4 failed: Promise creation failed"); }
        if (!promise4->wait()) {
          throw std::runtime_error("Step 4 failed: Timeout waiting for OK");
        }
        if (!promise4->getResponse()->isSuccess()) {
          throw std::runtime_error("Step 4 failed: Command should have succeeded");
        }
      },
      "GPRSSequenceTest", configMINIMAL_STACK_SIZE * 8, 2, 15000);

  EXPECT_TRUE(testResult);
}

TEST_F(SequenceTest, GPRSConnectSequenceWithErrors) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

        // Step 1: Deactivate context - success
        InjectCommandResponse("AT+QIDEACT=1", "OK", 100);
        ATPromise* promise1 = handler->sendCommand("AT+QIDEACT=1");
        if (!promise1) { throw std::runtime_error("Step 1 failed: Promise creation failed"); }
        if (!promise1->wait()) {
          throw std::runtime_error("Step 1 failed: Timeout waiting for OK");
        }
        if (!promise1->getResponse()->isSuccess()) {
          throw std::runtime_error("Step 1 failed: Command should have succeeded");
        }

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 2: Configure APN - simulate error
        InjectCommandResponse("AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"", "ERROR", 100);
        ATPromise* promise2 = handler->sendCommand("AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"");
        if (!promise2) { throw std::runtime_error("Step 2 failed: Promise creation failed"); }

        bool waitResult = promise2->wait();
        if (!waitResult) {
          throw std::runtime_error("Step 2 failed: Timeout waiting for ERROR");
        }

        if (promise2->getResponse()->isSuccess()) {
          throw std::runtime_error("Step 2 failed: Command reported unexpected success");
        }

        if (!promise2->getResponse()->containsResponse("ERROR")) {
          throw std::runtime_error("Step 2 failed: ERROR not found in response");
        }
      },
      "GPRSSequenceErrorTest", configMINIMAL_STACK_SIZE * 8, 2, 15000);

  EXPECT_TRUE(testResult);
}

TEST_F(SequenceTest, GPRSConnectSequenceWithTimeout) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

        // Step 1: Deactivate context - success
        InjectCommandResponse("AT+QIDEACT=1", "OK", 100);
        ATPromise* promise1 = handler->sendCommand("AT+QIDEACT=1");
        if (!promise1) { throw std::runtime_error("Step 1 failed: Promise creation failed"); }
        if (!promise1->wait()) {
          throw std::runtime_error("Step 1 failed: Timeout waiting for OK");
        }

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 2: Configure APN - no response (timeout)
        ATPromise* promise2 = handler->sendCommand("AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"");
        if (!promise2) { throw std::runtime_error("Step 2 failed: Promise creation failed"); }
        promise2->timeout(500);  // Set a short timeout
        if (promise2->wait()) {
          throw std::runtime_error("Step 2 should have timed out");
        }
      },
      "GPRSSequenceTimeoutTest", configMINIMAL_STACK_SIZE * 8, 2, 15000);

  EXPECT_TRUE(testResult);
}

TEST_F(SequenceTest, ComplexATSequenceWithURC) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

        // Step 1: Query network registration
        std::atomic<bool> urcReceived{false};
        String urcData;
        handler->onURC([&](const String& urc) {
          log_i("[URC] Received: '%s'", urc.c_str());
          if (urc.indexOf("+CREG: 2") != -1) {
            urcReceived = true;
            urcData = urc;
          }
        });

        // Use a precise responder task to ensure correct line order
        auto complexResponderTask = [](void* pvParameters) {
          auto* stream = static_cast<NiceMock<MockStream>*>(pvParameters);
          vTaskDelay(pdMS_TO_TICKS(50));
          stream->InjectRxData("AT+CREG?\r\n");  // Command Echo
          vTaskDelay(pdMS_TO_TICKS(20));
          stream->InjectRxData("+CREG: 2\r\n");  // URC (unsolicited)
          vTaskDelay(pdMS_TO_TICKS(20));
          stream->InjectRxData("+CREG: 0,1\r\n");  // Expected Response
          stream->InjectRxData("OK\r\n");          // Final Response
          vTaskDelete(nullptr);
        };

        TaskHandle_t complexResponderHandle = nullptr;
        xTaskCreate(
            complexResponderTask, "ComplexResponder", configMINIMAL_STACK_SIZE * 3, mockStream, 1,
            &complexResponderHandle);

        ATPromise* promise1 = handler->sendCommand("AT+CREG?");
        if (!promise1) { throw std::runtime_error("Step 1 failed: Promise creation failed"); }

        promise1->timeout(2000);             // Set a reasonable timeout for the whole transaction
        bool waitResult = promise1->wait();  // Simply wait for the transaction to complete

        // Now, validate the results after the wait is complete
        if (!waitResult) {
          throw std::runtime_error("Step 1 failed: Command timed out or failed to complete");
        }

        // Assertions after the wait
        if (!urcReceived.load()) {
          throw std::runtime_error("Step 1 failed: URC callback was not called");
        }
        ATResponse* response1 = promise1->getResponse();
        if (response1->containsResponse("+CREG: 2")) {
          throw std::runtime_error("Step 1 failed: URC should not be in command response");
        }
        if (response1->containsResponse("+CREG: 0,1")) {
          throw std::runtime_error("Step 1 failed: Expected +CREG: 0,1 response not found");
        }
        if (!response1->containsResponse("OK")) {
          throw std::runtime_error("Step 1 failed: OK not found in response");
        }

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 2: Query signal quality
        InjectCommandResponse("AT+CSQ", "+CSQ: 15,99\r\nOK", 100);
        ATPromise* promise2 = handler->sendCommand("AT+CSQ");
        if (!promise2) { throw std::runtime_error("Step 2 failed: Promise creation failed"); }
        if (!promise2->wait()) {
          throw std::runtime_error("Step 2 failed: AT+CSQ did not receive OK");
        }
        ATResponse* response2 = promise2->getResponse();
        if (!response2->containsResponse("+CSQ: 15,99")) {
          throw std::runtime_error("Step 2 failed: Expected +CSQ response not found");
        }
      },
      "ComplexSequenceTest", configMINIMAL_STACK_SIZE * 8, 2, 15000);

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
