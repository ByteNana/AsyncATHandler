#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "AsyncATHandler.h"
#include "Stream.h"
#include "common.h"
#include "esp_log.h"

using ::testing::NiceMock;

class SequenceTest : public FreeRTOSTest {
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
      bool success = runInFreeRTOSTask([this]() { handler->end(); }, "TeardownTask");
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

  // Helper to inject response for a specific command with proper timing
  void InjectCommandResponse(
      const std::string& command, const std::string& response, uint32_t delayMs = 50) {
    struct ResponderData {
      SequenceTest* test;
      std::string command;
      std::string response;
      uint32_t delay;
      std::atomic<bool> complete{false};
    };

    auto* responderData = new ResponderData{this, command, response, delayMs, {false}};

    auto responderTask = [](void* pvParameters) {
      auto* data = static_cast<ResponderData*>(pvParameters);
      vTaskDelay(pdMS_TO_TICKS(data->delay));

      // Inject command echo first (like real modem)
      data->test->mockStream->InjectRxData(data->command + "\r\n");
      // Then inject the response
      data->test->mockStream->InjectRxData(data->response + "\r\n");

      data->complete = true;
      delete data;
      vTaskDelete(nullptr);
    };

    TaskHandle_t responderHandle = nullptr;
    xTaskCreate(
        responderTask, "ResponderTask", configMINIMAL_STACK_SIZE * 2, responderData, 1,
        &responderHandle);
  }

 public:
  NiceMock<MockStream>* mockStream;
};

TEST_F(SequenceTest, GPRSConnectSequence) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

        log_i("[Test] Starting GPRS connection sequence...");

        // Step 1: Deactivate context
        log_i("[Test] Step 1: Deactivating context");
        InjectCommandResponse("AT+QIDEACT=1", "OK", 100);

        String response1;
        bool result1 = handler->sendCommand("AT+QIDEACT=1", response1, "OK", 2000);
        if (!result1) {
          throw std::runtime_error("Step 1 failed: AT+QIDEACT=1 did not receive OK");
        }
        if (response1.indexOf("OK") == -1) {
          throw std::runtime_error("Step 1 failed: OK not found in response");
        }
        log_i("[Test] Step 1 completed: %s", response1.c_str());

        // Small delay between commands like real usage
        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 2: Configure APN
        log_i("[Test] Step 2: Configuring APN");
        InjectCommandResponse("AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"", "OK", 100);

        String response2;
        bool result2 = handler->sendCommand(
            "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"", response2, "OK", 2000);
        if (!result2) { throw std::runtime_error("Step 2 failed: AT+QICSGP did not receive OK"); }
        if (response2.indexOf("OK") == -1) {
          throw std::runtime_error("Step 2 failed: OK not found in response");
        }
        log_i("[Test] Step 2 completed: %s", response2.c_str());

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 3: Activate context
        log_i("[Test] Step 3: Activating context");
        InjectCommandResponse("AT+QIACT=1", "OK", 100);

        String response3;
        bool result3 = handler->sendCommand("AT+QIACT=1", response3, "OK", 2000);
        if (!result3) { throw std::runtime_error("Step 3 failed: AT+QIACT=1 did not receive OK"); }
        if (response3.indexOf("OK") == -1) {
          throw std::runtime_error("Step 3 failed: OK not found in response");
        }
        log_i("[Test] Step 3 completed: %s", response3.c_str());

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 4: Attach to GPRS
        log_i("[Test] Step 4: Attaching to GPRS");
        InjectCommandResponse("AT+CGATT=1", "OK", 100);

        String response4;
        bool result4 = handler->sendCommand("AT+CGATT=1", response4, "OK", 2000);
        if (!result4) { throw std::runtime_error("Step 4 failed: AT+CGATT=1 did not receive OK"); }
        if (response4.indexOf("OK") == -1) {
          throw std::runtime_error("Step 4 failed: OK not found in response");
        }
        log_i("[Test] Step 4 completed: %s", response4.c_str());

        log_i("[Test] GPRS connection sequence completed successfully");
      },
      "GPRSSequenceTest", configMINIMAL_STACK_SIZE * 8, 2, 15000);  // Larger stack and timeout

  EXPECT_TRUE(testResult);
}

TEST_F(SequenceTest, GPRSConnectSequenceWithErrors) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

        log_i("[Test] Starting GPRS sequence with simulated error...");

        // Step 1: Deactivate context - success
        log_i("[Test] Step 1: Deactivating context");
        InjectCommandResponse("AT+QIDEACT=1", "OK", 100);

        String response1;
        bool result1 = handler->sendCommand("AT+QIDEACT=1", response1, "OK", 2000);
        if (!result1) { throw std::runtime_error("Step 1 should have succeeded"); }
        log_i("[Test] Step 1 completed successfully");

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 2: Configure APN - simulate error
        log_i("[Test] Step 2: Configuring APN (will fail)");
        InjectCommandResponse("AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"", "ERROR", 100);

        String response2;
        bool result2 = handler->sendCommand(
            "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"", response2, "OK", 2000);
        if (result2) { throw std::runtime_error("Step 2 should have failed with ERROR"); }
        if (response2.indexOf("ERROR") == -1) {
          throw std::runtime_error("Step 2 should contain ERROR response");
        }
        log_i("[Test] Step 2 failed as expected: %s", response2.c_str());

        log_i("[Test] GPRS sequence correctly handled error condition");
      },
      "GPRSSequenceErrorTest", configMINIMAL_STACK_SIZE * 8, 2, 15000);

  EXPECT_TRUE(testResult);
}

TEST_F(SequenceTest, GPRSConnectSequenceWithTimeout) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

        log_i("[Test] Starting GPRS sequence with timeout...");

        // Step 1: Deactivate context - success
        InjectCommandResponse("AT+QIDEACT=1", "OK", 100);

        String response1;
        bool result1 = handler->sendCommand("AT+QIDEACT=1", response1, "OK", 2000);
        if (!result1) { throw std::runtime_error("Step 1 should have succeeded"); }

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 2: Configure APN - no response (timeout)
        log_i("[Test] Step 2: Configuring APN (will timeout)");
        // Don't inject any response - will timeout

        String response2;
        bool result2 = handler->sendCommand(
            "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"", response2, "OK", 500);  // Short timeout
        if (result2) { throw std::runtime_error("Step 2 should have timed out"); }
        log_i("[Test] Step 2 timed out as expected");

        log_i("[Test] GPRS sequence correctly handled timeout condition");
      },
      "GPRSSequenceTimeoutTest", configMINIMAL_STACK_SIZE * 8, 2, 15000);

  EXPECT_TRUE(testResult);
}

TEST_F(SequenceTest, ComplexATSequenceWithURC) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

        log_i("[Test] Starting complex sequence with URC...");

        // Step 1: Query network registration
        log_i("[Test] Step 1: Query network registration");

        // Create complex responder that sends URC first, then command response
        struct ComplexResponderData {
          SequenceTest* test;
          std::atomic<bool> complete{false};
        };

        auto* responderData = new ComplexResponderData{this, {false}};

        auto complexResponderTask = [](void* pvParameters) {
          auto* data = static_cast<ComplexResponderData*>(pvParameters);

          vTaskDelay(pdMS_TO_TICKS(50));

          // First inject some URC (unsolicited response)
          data->test->mockStream->InjectRxData("+CREG: 2\r\n");

          vTaskDelay(pdMS_TO_TICKS(20));

          // Then inject command echo and response
          data->test->mockStream->InjectRxData("AT+CREG?\r\n");
          data->test->mockStream->InjectRxData("+CREG: 0,1\r\n");
          data->test->mockStream->InjectRxData("OK\r\n");

          data->complete = true;
          delete data;
          vTaskDelete(nullptr);
        };

        TaskHandle_t complexResponderHandle = nullptr;
        xTaskCreate(
            complexResponderTask, "ComplexResponder", configMINIMAL_STACK_SIZE * 3, responderData,
            1, &complexResponderHandle);

        String response1;
        bool result1 = handler->sendCommand("AT+CREG?", response1, "OK", 3000);
        if (!result1) { throw std::runtime_error("Step 1 failed: AT+CREG? did not receive OK"); }

        // Response should contain the command response, not the URC
        if (response1.indexOf("+CREG: 0,1") == -1) {
          throw std::runtime_error("Step 1 failed: Expected +CREG: 0,1 not found");
        }
        if (response1.indexOf("OK") == -1) {
          throw std::runtime_error("Step 1 failed: OK not found in response");
        }

        log_i("[Test] Step 1 completed with URC handling: %s", response1.c_str());

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 2: Query signal quality
        log_i("[Test] Step 2: Query signal quality");
        InjectCommandResponse("AT+CSQ", "+CSQ: 15,99\r\nOK", 100);

        String response2;
        bool result2 = handler->sendCommand("AT+CSQ", response2, "OK", 2000);
        if (!result2) { throw std::runtime_error("Step 2 failed: AT+CSQ did not receive OK"); }
        if (response2.indexOf("+CSQ: 15,99") == -1) {
          throw std::runtime_error("Step 2 failed: Expected +CSQ response not found");
        }

        log_i("[Test] Step 2 completed: %s", response2.c_str());

        log_i("[Test] Complex sequence with URC completed successfully");
      },
      "ComplexSequenceTest", configMINIMAL_STACK_SIZE * 8, 2, 15000);

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
