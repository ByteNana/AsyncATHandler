#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "AsyncATHandler.h"
#include "Stream.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

using ::testing::NiceMock;

class SequenceTest : public ::testing::Test {
protected:
  AsyncATHandler handler;
  NiceMock<MockStream> *mockStream;

  void SetUp() override {
    mockStream = new NiceMock<MockStream>();
    mockStream->SetupDefaults();
    handler.begin(*mockStream);
  }
  void TearDown() override {
    handler.end();
    delete mockStream;
  }
};

TEST_F(SequenceTest, GPRSConnectSequence) {
  std::string responses =
    "AT+QIDEACT=1\r\nOK\r\n"
    "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"\r\nOK\r\n"
    "AT+QIACT=1\r\nOK\r\n"
    "AT+CGATT=1\r\nOK\r\n";
  mockStream->InjectRxData(responses.c_str());

  bool ok;
  String resp;

  ok = handler.sendCommand("AT+QIDEACT=1", resp, "OK", 1000);
  EXPECT_TRUE(ok);
  ok = handler.sendCommand("AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"", resp, "OK", 1000);
  EXPECT_TRUE(ok);
  ok = handler.sendCommand("AT+QIACT=1", resp, "OK", 1000);
  EXPECT_TRUE(ok);
  ok = handler.sendCommand("AT+CGATT=1", resp, "OK", 1000);
  EXPECT_TRUE(ok);
}

