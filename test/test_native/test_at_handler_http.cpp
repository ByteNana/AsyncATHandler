#include <gtest/gtest.h>
#include "common.h"
#include "AsyncATHandler.h"
#include "Stream.h"
#include "esp_log.h"

using ::testing::NiceMock;

class AsyncATHandlerHTTPTest : public FreeRTOSTest {
protected:
  AsyncATHandler* handler;
  NiceMock<MockStream>* mockStream;

  void SetUp() override {
    handler = new AsyncATHandler();
    mockStream = new NiceMock<MockStream>();
    mockStream->SetupDefaults();
  }

  void TearDown() override {
    if (handler) {
      handler->end();
      delete handler;
      handler = nullptr;
    }
    if (mockStream) {
      delete mockStream;
      mockStream = nullptr;
    }
    FreeRTOSTest::TearDown();
  }
};

TEST_F(AsyncATHandlerHTTPTest, HttpSocketOpenWithLongTimeout) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) {
      throw std::runtime_error("Handler begin failed");
    }

    handler->sendAT("AT+QIOPEN=0,0,\"TCP\",\"220.180.239.212\",8062,0,1");

    mockStream->InjectRxData("AT+QIOPEN=0,0,\"TCP\",\"220.180.239.212\",8062,0,1\r\n");
    mockStream->InjectRxData("OK\r\n");

    vTaskDelay(pdMS_TO_TICKS(50));

    int8_t result = handler->waitResponse(1000, "OK");
    if (result <= 0) {
      throw std::runtime_error("Should have received OK response");
    }

    String okResponse = handler->getResponse("OK");
    log_i("[Test] OK response: %s", okResponse.c_str());

    log_i("[Test] Waiting for +QIOPEN URC (up to 60 seconds)...");

    vTaskDelay(pdMS_TO_TICKS(2000));
    mockStream->InjectRxData("+QIOPEN: 0,0\r\n");

    result = handler->waitResponse(60000, "+QIOPEN:");
    if (result <= 0) {
      throw std::runtime_error("Should have received +QIOPEN URC within 60 seconds");
    }

    String urcResponse = handler->getResponse("+QIOPEN:");
    if (urcResponse.indexOf("+QIOPEN: 0,0") == -1) {
      throw std::runtime_error("URC content incorrect");
    }

    log_i("[Test] URC received: %s", urcResponse.c_str());

    handler->sendAT("AT+QISTATE=1,0");

    mockStream->InjectRxData("AT+QISTATE=1,0\r\n");
    mockStream->InjectRxData("+QISTATE: 0,\"TCP\",\"220.180.239.212\",8062,0,2,0,1\r\n");
    mockStream->InjectRxData("OK\r\n");

    vTaskDelay(pdMS_TO_TICKS(50));

    result = handler->waitResponse(1000, "OK");
    if (result <= 0) {
      throw std::runtime_error("Should have received QISTATE OK response");
    }

    String qistateResponse = handler->getResponse("OK");
    if (qistateResponse.indexOf("+QISTATE: 0,\"TCP\"") == -1) {
      throw std::runtime_error("QISTATE response missing");
    }

    log_i("[Test] QISTATE response: %s", qistateResponse.c_str());

  }, "HttpSocketLongTimeoutTest", 2048, 2, 70000);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerHTTPTest, CompleteHttpConnectionFlow) {
  bool testResult = runInFreeRTOSTask([this]() {
    if (!handler->begin(*mockStream)) {
      throw std::runtime_error("Handler begin failed");
    }

    String response = "";

    log_i("[Test] Opening HTTP socket...");

    handler->sendAT("AT+QIOPEN=0,0,\"TCP\",\"220.180.239.212\",8062,0,1");

    mockStream->InjectRxData("AT+QIOPEN=0,0,\"TCP\",\"220.180.239.212\",8062,0,1\r\n");
    mockStream->InjectRxData("OK\r\n");

    vTaskDelay(pdMS_TO_TICKS(50));

    if (handler->waitResponse(1000, "OK") <= 0) {
      throw std::runtime_error("QIOPEN OK not received");
    }
    handler->getResponse("OK");

    vTaskDelay(pdMS_TO_TICKS(1000));
    mockStream->InjectRxData("+QIOPEN: 0,0\r\n");

    log_i("[Test] Waiting for connection URC...");
    if (handler->waitResponse(60000, "+QIOPEN:") <= 0) {
      throw std::runtime_error("Connection URC not received within 60 seconds");
    }

    String connectionResult = handler->getResponse("+QIOPEN:");
    if (connectionResult.indexOf("0,0") == -1) {
      throw std::runtime_error("Connection failed");
    }

    log_i("[Test] Socket connected successfully: %s", connectionResult.c_str());

    log_i("[Test] Querying connection status...");

    handler->sendAT("AT+QISTATE=1,0");
    mockStream->InjectRxData("AT+QISTATE=1,0\r\n");
    mockStream->InjectRxData("+QISTATE: 0,\"TCP\",\"220.180.239.212\",8062,0,2,0,1\r\n");
    mockStream->InjectRxData("OK\r\n");

    vTaskDelay(pdMS_TO_TICKS(50));

    if (handler->waitResponse(1000, "OK") <= 0) {
      throw std::runtime_error("QISTATE OK not received");
    }

    String statusResponse = handler->getResponse("OK");
    if (statusResponse.indexOf("220.180.239.212") == -1) {
      throw std::runtime_error("Status query failed");
    }

    log_i("[Test] Connection status confirmed: %s", statusResponse.c_str());

  }, "CompleteHttpFlowTest", 2048, 2, 70000);

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
