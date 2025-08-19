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

// using ::testing::_;
// using ::testing::AtLeast;
using ::testing::NiceMock;

class AsyncATHandlerBasicTest : public FreeRTOSTest {
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

TEST_F(AsyncATHandlerBasicTest, InitializationTest) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        // if (handler->getQueuedCommandCount() != 0)
        //   throw std::runtime_error("Queue should be empty initially");

        if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");

        if (handler->begin(*mockStream)) throw std::runtime_error("Should not initialize twice");
      },
      "InitTest");

  EXPECT_TRUE(testResult);
}

// TEST_F(AsyncATHandlerBasicTest, SendAsyncCommand) {
//   bool testResult = runInFreeRTOSTask(
//       [this]() {
//         if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");
//         mockStream->ClearTxData();
//
//         EXPECT_CALL(*mockStream, write(_, _)).Times(AtLeast(1));
//         EXPECT_CALL(*mockStream, flush()).Times(AtLeast(1));
//
//         if (!handler->sendCommandAsync("AT")) throw std::runtime_error("Send async command failed");
//         vTaskDelay(pdMS_TO_TICKS(100));
//
//         std::string sentData = mockStream->GetTxData();
//         if (sentData != "AT\r\n") throw std::runtime_error("Incorrect data sent");
//       },
//       "AsyncCmdTest");
//
//   EXPECT_TRUE(testResult);
// }

// TEST_F(AsyncATHandlerBasicTest, CleanShutdown) {
//   bool testResult = runInFreeRTOSTask(
//       [this]() {
//         if (!handler->begin(*mockStream)) throw std::runtime_error("Handler begin failed");
//
//         handler->sendCommandAsync("AT+1");
//         handler->sendCommandAsync("AT+2");
//
//         handler->end();
//
//         if (handler->sendCommandAsync("AT+3"))
//           throw std::runtime_error("Should not accept commands after end");
//
//         String response;
//         if (handler->sendCommand("AT+4", response))
//           throw std::runtime_error("Should not accept sync commands after end");
//
//         vTaskDelay(pdMS_TO_TICKS(100));
//       },
//       "ShutdownTest");
//
//   EXPECT_TRUE(testResult);
// }

FREERTOS_TEST_MAIN()
