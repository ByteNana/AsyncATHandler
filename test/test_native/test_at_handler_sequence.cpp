#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "AsyncATHandler.h"
#include "Stream.h"
#include "common.h"
#include "esp_log.h"

using ::testing::NiceMock;

class SequenceTest : public FreeRTOSTest {
 protected:
  AsyncATHandler handler;
  NiceMock<MockStream>* mockStream;

  void SetUp() override {
    FreeRTOSTest::SetUp();
    mockStream = new NiceMock<MockStream>();
    mockStream->SetupDefaults();
    handler.begin(*mockStream);
  }

  void TearDown() override {
    bool success = runInFreeRTOSTask([this]() { handler.end(); }, "TeardownTask");

    if (!success) { log_e("Handler teardown may have failed"); }

    delete mockStream;
    FreeRTOSTest::TearDown();
  }
};

TEST_F(SequenceTest, GPRSConnectSequence) {
  struct TestData {
    AsyncATHandler* handler;
    NiceMock<MockStream>* mockStream;
    std::atomic<bool> testComplete{false};
    std::atomic<bool> allTestsPassed{false};
  };

  auto testTask = [](void* pvParameters) {
    auto* data = static_cast<TestData*>(pvParameters);

    std::string responses =
        "AT+QIDEACT=1\r\nOK\r\n"
        "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"\r\nOK\r\n"
        "AT+QIACT=1\r\nOK\r\n"
        "AT+CGATT=1\r\nOK\r\n";

    data->mockStream->InjectRxData(responses.c_str());

    bool ok;
    String resp;
    bool allPassed = true;

    ok = data->handler->sendCommand("AT+QIDEACT=1", resp, "OK", 1000);
    allPassed &= ok;

    ok = data->handler->sendCommand(
        "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"", resp, "OK", 1000);
    allPassed &= ok;

    ok = data->handler->sendCommand("AT+QIACT=1", resp, "OK", 1000);
    allPassed &= ok;

    ok = data->handler->sendCommand("AT+CGATT=1", resp, "OK", 1000);
    allPassed &= ok;

    data->allTestsPassed = allPassed;
    data->testComplete = true;
    vTaskDelete(nullptr);
  };

  TestData testData;
  testData.handler = &handler;
  testData.mockStream = mockStream;

  TaskHandle_t testTaskHandle = nullptr;
  BaseType_t result = xTaskCreate(testTask, "TestTask", 2048, &testData, 2, &testTaskHandle);
  ASSERT_EQ(result, pdPASS) << "Failed to create test task";

  // Wait for test to complete (with timeout)
  int waitCount = 0;
  while (!testData.testComplete.load() && waitCount < 1000) {  // 10 second timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    waitCount++;
  }

  ASSERT_TRUE(testData.testComplete.load()) << "Test task did not complete in time";
  EXPECT_TRUE(testData.allTestsPassed.load()) << "One or more AT commands failed";
}

FREERTOS_TEST_MAIN()
