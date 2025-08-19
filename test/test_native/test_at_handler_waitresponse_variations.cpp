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

class AsyncATHandlerWaitResponseComprehensiveTest : public FreeRTOSTest {
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
      delete mockStream;
      mockStream = nullptr;
    }
    FreeRTOSTest::TearDown();
  }

  // Helper to inject data with delay in separate task
  void InjectDataWithDelay(const std::string& data, uint32_t delayMs = 50) {
    struct InjectorData {
      AsyncATHandlerWaitResponseComprehensiveTest* test;
      std::string data;
      uint32_t delay;
      std::atomic<bool> complete{false};
    };

    auto* injectorData = new InjectorData{this, data, delayMs, {false}};

    auto injectorTask = [](void* pvParameters) {
      auto* data = static_cast<InjectorData*>(pvParameters);
      vTaskDelay(pdMS_TO_TICKS(data->delay));
      data->test->mockStream->InjectRxData(data->data);
      data->complete = true;
      delete data;
      vTaskDelete(nullptr);
    };

    TaskHandle_t injectorHandle = nullptr;
    xTaskCreate(
        injectorTask, "InjectorTask", configMINIMAL_STACK_SIZE * 2, injectorData, 1,
        &injectorHandle);
  }

 public:
  NiceMock<MockStream>* mockStream;
  AsyncATHandler* handler;
};

TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, WaitResponse_DefaultTimeout_AnyResponse) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay("OK\r\n", 100);

        if (!handler->waitResponse()) {
          throw std::runtime_error("Should have found response with default timeout");
        }

        log_i("[Test] waitResponse() with default timeout works");
      },
      "DefaultTimeoutTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, WaitResponse_CustomTimeout_AnyResponse) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay("+QISTATE: 0,\"TCP\"\r\nOK\r\n", 100);

        int8_t result = handler->waitResponse(2000);  // 2 second timeout

        if (result != 1) {
          throw std::runtime_error("Should have found response with custom timeout");
        }

        log_i("[Test] waitResponse(timeout) works");
      },
      "CustomTimeoutTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, WaitResponse_SingleExpected_DefaultTimeout) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay("+QISTATE: 0,\"TCP\",\"connected\"\r\n", 100);

        int8_t result = handler->waitResponse("+QISTATE:");

        if (result != 1) { throw std::runtime_error("Should have found +QISTATE: response"); }

        log_i("[Test] waitResponse(expectedResponse) works");
      },
      "SingleExpectedTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, WaitResponse_MultipleExpected_DefaultTimeout) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay("+QIRD: 1024,data_here\r\n", 100);

        int8_t result = handler->waitResponse("+QIRD:", "OK", "ERROR");

        if (result != 1) {
          throw std::runtime_error("Should have found +QIRD: as first match (index 1)");
        }

        log_i("[Test] waitResponse(multiple expected) works - found index %d", result);
      },
      "MultipleExpectedTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, WaitResponse_MultipleExpected_CustomTimeout) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay("ERROR\r\n", 150);

        int8_t result = handler->waitResponse(1000, "+QIRD:", "OK", "ERROR");

        if (result != 3) {
          throw std::runtime_error("Should have found ERROR as third match (index 3)");
        }

        log_i("[Test] waitResponse(timeout, multiple expected) works - found index %d", result);
      },
      "MultipleExpectedCustomTimeoutTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, WaitResponse_StringObjects) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay("OK\r\n", 100);

        String expected1 = "+QIRD:";
        String expected2 = "OK";
        String expected3 = "ERROR";

        int8_t result = handler->waitResponse(1000, expected1, expected2, expected3);

        if (result != 2) {
          throw std::runtime_error("Should have found OK as second match (index 2)");
        }

        log_i("[Test] waitResponse with String objects works - found index %d", result);
      },
      "StringObjectsTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, WaitResponse_MixedTypes) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay("+CONNECT\r\n", 100);

        String expectedString = "+CONNECT";
        const char* expectedCStr = "OK";

        int8_t result = handler->waitResponse(1000, expectedString, expectedCStr, "ERROR");

        if (result != 1) {
          throw std::runtime_error("Should have found +CONNECT as first match (index 1)");
        }

        log_i("[Test] waitResponse with mixed types works - found index %d", result);
      },
      "MixedTypesTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, WaitResponse_TimeoutScenarios) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        // Test 1: No data injected - should timeout
        int8_t result1 = handler->waitResponse(200);  // Short timeout
        if (result1 != 0) { throw std::runtime_error("Should have timed out with no data"); }

        // Test 2: Data that doesn't match - should timeout
        InjectDataWithDelay("DIFFERENT_RESPONSE\r\n", 50);
        int8_t result2 = handler->waitResponse(300, "EXPECTED", "ALSO_EXPECTED");
        if (result2 != 0) {
          throw std::runtime_error("Should have timed out with non-matching data");
        }

        // Test 3: Data arrives after timeout - should timeout
        InjectDataWithDelay("OK\r\n", 400);  // Arrives after timeout
        int8_t result3 = handler->waitResponse(200, "OK");
        if (result3 != 0) {
          throw std::runtime_error("Should have timed out when data arrives late");
        }

        log_i("[Test] All timeout scenarios work correctly");
      },
      "TimeoutScenariosTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, WaitResponse_ManyExpectedResponses) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay("+RESPONSE7\r\n", 100);

        int8_t result = handler->waitResponse(
            1000, "+RESPONSE1", "+RESPONSE2", "+RESPONSE3", "+RESPONSE4", "+RESPONSE5",
            "+RESPONSE6", "+RESPONSE7", "+RESPONSE8", "OK", "ERROR");

        if (result != 7) {
          throw std::runtime_error("Should have found +RESPONSE7 as 7th match (index 7)");
        }

        log_i("[Test] waitResponse with many expected responses works - found index %d", result);
      },
      "ManyExpectedTest", configMINIMAL_STACK_SIZE * 8);  // Extra stack for many strings

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, WaitResponse_PartialMatches) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay(
            "+QIRD: 1024,some_very_long_data_payload_here_with_lots_of_info\r\n", 100);

        // Should match because "+QIRD:" is contained in the response
        int8_t result = handler->waitResponse(1000, "+QIRD:", "NOMATCH", "ALSONOMATCH");

        if (result != 1) {
          throw std::runtime_error("Should have found +QIRD: using partial matching");
        }

        log_i("[Test] waitResponse partial matching works");
      },
      "PartialMatchTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, WaitResponse_OldSignature) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay("+CGMI: SIMCOM\r\nOK\r\n", 100);

        int result = handler->waitResponse(1000, "OK");
        if (result != 1) { throw std::runtime_error("Should have found OK response"); }
        String collectedResponse = handler->getResponse("OK");

        if (collectedResponse.indexOf("+CGMI") == -1) {
          throw std::runtime_error("Should have collected the full response");
        }

        log_i("[Test] waitResponse old signature works. Response: '%s'", collectedResponse.c_str());
      },
      "OldSignatureTest");

  EXPECT_TRUE(testResult);
}

// // =============================================================================
// // Test 12: Ensure unsolicited responses work with callback
// // =============================================================================
// TEST_F(AsyncATHandlerWaitResponseComprehensiveTest, UnsolicitedResponsesWithCallback) {
//   bool testResult = runInFreeRTOSTask(
//       [this]() {
//         if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");
//
//         std::atomic<int> unsolicitedCount{0};
//         std::string lastUnsolicited;
//
//         handler->setUnsolicitedCallback([&](const char* response) {
//           unsolicitedCount++;
//           lastUnsolicited = response;
//           log_i("Unsolicited callback: '%s'", response);
//         });
//
//         // Inject unsolicited responses
//         InjectDataWithDelay("+CREG: 0,1\r\n+CSQ: 15,99\r\n", 100);
//
//         // Wait for processing
//         vTaskDelay(pdMS_TO_TICKS(200));
//
//         if (unsolicitedCount.load() != 2) {
//           throw std::runtime_error("Should have received 2 unsolicited responses");
//         }
//
//         // Now inject non-unsolicited response
//         InjectDataWithDelay("OK\r\n", 100);
//         int8_t result = handler->waitResponse(1000, "OK");
//
//         if (result != 1) {
//           throw std::runtime_error("Should have found OK response after unsolicited");
//         }
//
//         log_i("[Test] Unsolicited responses and waitResponse coexist properly");
//       },
//       "UnsolicitedTest");
//
//   EXPECT_TRUE(testResult);
// }

FREERTOS_TEST_MAIN()
