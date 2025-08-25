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

TEST_F(AsyncATHandlerSyncTest, SendSyncCommandWithOKResponse) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay(mockStream, "AT\r\nOK\r\n", 50);

        String response;
        bool sendResult = handler->sendSync("AT", response, 1000);

        if (!sendResult) { throw std::runtime_error("Test failed: command should have succeeded"); }

        if (response.indexOf("OK") == -1) {
          throw std::runtime_error("Test failed: response missing OK");
        }
      },
      "SyncOKTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerSyncTest, SendSyncCommandWithTimeout) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        String response;
        bool sendResult = handler->sendSync("AT+TIMEOUT", response, 100);

        if (sendResult) { throw std::runtime_error("Should have timed out"); }

        if (!response.empty()) {
          throw std::runtime_error("Response should be empty on timeout");
        }
      },
      "TimeoutTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerSyncTest, SendSyncCommandWithErrorResponse) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay(mockStream, "AT+FAIL\r\nERROR\r\n", 50);

        String response;
        bool sendResult = handler->sendSync("AT+FAIL", response, 1000);

        if (sendResult) { throw std::runtime_error("Test failed: command should have failed"); }

        if (response.indexOf("ERROR") == -1) {
          throw std::runtime_error("Test failed: response missing ERROR");
        }
      },
      "SyncErrorTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerSyncTest, SendCommandWithoutResponseParameter) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay(mockStream, "AT\r\nOK\r\n", 50);

        bool sendResult = handler->sendSync("AT", 1000);

        if (!sendResult) { throw std::runtime_error("Test failed: command should have succeeded"); }
      },
      "SyncNoResponseTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerSyncTest, ResponseContainsAllLines) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay(
            mockStream,
            "+CGMI: SIMCOM\r\nManufacturer: SIMCOM INCORPORATED\r\nModel: SIM7600E\r\nOK\r\n", 50);

        String response;
        bool sendResult = handler->sendSync("AT+CGMI", response, 1000);

        if (!sendResult) throw std::runtime_error("Command should have succeeded");

        if (response.indexOf("+CGMI: SIMCOM") == -1 ||
            response.indexOf("Manufacturer: SIMCOM") == -1 ||
            response.indexOf("Model: SIM7600E") == -1) {
          throw std::runtime_error("Response missing intermediate lines");
        }
      },
      "MultiLineTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerSyncTest, TimeoutStillReturnsCollectedResponse) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        InjectDataWithDelay(
            mockStream,
            "+QISTATE: 0,\"TCP\",\"192.168.1.1\",8080,5000,2,1\r\n"
            "+QISTATE: 1,\"UDP\",\"10.0.0.1\",53,0,0,0\r\n",
            50);

        String response;
        bool sendResult = handler->sendSync("AT+QISTATE", response, 200);

        if (sendResult) throw std::runtime_error("Command should have timed out");

        if (!response.empty()) {
          log_e("Response on timeout: '%s'", response.c_str());
          throw std::runtime_error("Response should be empty on timeout");
        }
      },
      "TimeoutResponseTest");

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
