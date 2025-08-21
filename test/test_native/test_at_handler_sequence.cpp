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
  bool testResult = runInFreeRTOSTask([this]() {
    std::string responses =
        "AT+QIDEACT=1\r\nOK\r\n"
        "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"\r\nOK\r\n"
        "AT+QIACT=1\r\nOK\r\n"
        "AT+CGATT=1\r\nOK\r\n";

    mockStream->InjectRxData(responses.c_str());

    bool ok;
    String resp;
    bool allPassed = true;

    ok = handler.sendCommand("AT+QIDEACT=1", resp, "OK", 1000);
    allPassed &= ok;

    ok = handler.sendCommand("AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"", resp, "OK", 1000);
    allPassed &= ok;

    ok = handler.sendCommand("AT+QIACT=1", resp, "OK", 1000);
    allPassed &= ok;

    ok = handler.sendCommand("AT+CGATT=1", resp, "OK", 1000);
    allPassed &= ok;

    if (!allPassed) {
      throw std::runtime_error("One or more AT commands failed");
    }
  }, "GPRSSequenceTest", 4096, 2, 10000);

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
