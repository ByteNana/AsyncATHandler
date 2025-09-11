#include <gtest/gtest.h>

#include <memory>

#include "AsyncATHandler.h"
#include "Stream.h"
#include "common.h"
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
};

TEST_F(AsyncATHandlerHTTPTest, HttpSocketOpenWithLongTimeout) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }
        vTaskDelay(pdMS_TO_TICKS(100));
        InjectDataWithDelay(
            mockStream,
            "AT+QIOPEN=0,0,\"TCP\",\"220.180.239.212\",8062,0,1\r\n"
            "OK\r\n",
            100);
        String openResponse;
        bool openResult = handler->sendSync(
            "AT+QIOPEN=0,0,\"TCP\",\"220.180.239.212\",8062,0,1", openResponse, 5000);
        if (!openResult) { throw std::runtime_error("QIOPEN command failed"); }
        if (openResponse.indexOf("OK") == -1) {
          throw std::runtime_error("QIOPEN should return OK");
        }
        std::atomic<bool> urcReceived{false};
        String urcData;
        handler->onURC([&](const String& urc) {
          if (urc.indexOf("+QIOPEN:") != -1) {
            urcReceived = true;
            urcData = urc;
          }
        });
        InjectDataWithDelay(mockStream, "+QIOPEN: 0,0\r\n", 2000);
        for (int i = 0; i < 100 && !urcReceived.load(); i++) { vTaskDelay(pdMS_TO_TICKS(100)); }
        if (!urcReceived.load()) { throw std::runtime_error("Should have received +QIOPEN URC"); }
        if (urcData.indexOf("0,0") == -1) {
          throw std::runtime_error("Connection should have succeeded (0,0)");
        }
        InjectDataWithDelay(
            mockStream,
            "AT+QISTATE=1,0\r\n"
            "+QISTATE: 0,\"TCP\",\"220.180.239.212\",8062,0,2,0,1\r\n"
            "OK\r\n",
            100);
        String stateResponse;
        bool stateResult = handler->sendSync("AT+QISTATE=1,0", stateResponse, 3000);
        if (!stateResult) { throw std::runtime_error("QISTATE command failed"); }
        if (stateResponse.indexOf("220.180.239.212") == -1) {
          throw std::runtime_error("Status should show connected IP");
        }
      },
      "HttpSocketLongTimeoutTest", configMINIMAL_STACK_SIZE * 6, 2, 15000);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerHTTPTest, SendHttpDataOverTcp) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }
        vTaskDelay(pdMS_TO_TICKS(100));

        String httpRequest =
            "GET /api/test HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Connection: close\r\n"
            "\r\n";
        size_t dataLength = httpRequest.length();
        String qisendCommand = "AT+QISEND=0," + String(dataLength);

        // Step 1: Send AT+QISEND command and wait for the '>' prompt.
        auto promptResponderTask = [](void* p) {
          auto* stream = static_cast<NiceMock<MockStream>*>(p);
          vTaskDelay(pdMS_TO_TICKS(100));
          stream->InjectRxData(">\r\n");
          vTaskDelete(nullptr);
        };
        TaskHandle_t promptTaskHandle = nullptr;
        xTaskCreate(
            promptResponderTask, "PromptResponder", configMINIMAL_STACK_SIZE * 2, mockStream, 1,
            &promptTaskHandle);

        // This promise should only wait for the '>'
        ATPromise* promptPromise = handler->sendCommand(qisendCommand);
        if (!promptPromise) throw std::runtime_error("Failed to create prompt promise");
        if (!promptPromise->expect(">")->wait()) {
          throw std::runtime_error("Did not receive prompt '>'");
        }

        // Step 2: Send raw data and wait for 'OK' and 'SEND OK'
        auto dataResponderTask = [](void* p) {
          auto* stream = static_cast<NiceMock<MockStream>*>(p);
          vTaskDelay(pdMS_TO_TICKS(100));
          stream->InjectRxData("OK\r\n");
          stream->InjectRxData("SEND OK\r\n");
          vTaskDelete(nullptr);
        };
        TaskHandle_t dataTaskHandle = nullptr;
        xTaskCreate(
            dataResponderTask, "DataResponder", configMINIMAL_STACK_SIZE * 2, mockStream, 1,
            &dataTaskHandle);

        Stream* stream = handler->getStream();
        stream->write(reinterpret_cast<const uint8_t*>(httpRequest.c_str()), dataLength);
        stream->flush();

        // A new promise is needed to wait for the final response
        ATPromise* dataPromise = handler->sendCommand("");
        if (!dataPromise) throw std::runtime_error("Failed to create data promise");
        dataPromise->expect("OK")->expect("SEND OK");
        if (!dataPromise->wait()) { throw std::runtime_error("Did not receive SEND OK"); }
      },
      "SendHttpDataTest", configMINIMAL_STACK_SIZE * 6);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerHTTPTest, SendHttpDataChunked) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }
        vTaskDelay(pdMS_TO_TICKS(100));

        String jsonPayload =
            "{\"sensor_id\":\"ESP32_001\","
            "\"temperature\":23.5,"
            "\"humidity\":65.2,"
            "\"status\":\"active\"}";
        String httpRequest =
            "POST /api/sensors/data HTTP/1.1\r\n"
            "Host: api.iot-server.com\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " +
            String(jsonPayload.length()) +
            "\r\n"
            "Connection: close\r\n"
            "\r\n" +
            jsonPayload;
        size_t totalLength = httpRequest.length();
        static const size_t chunkSize = totalLength / 2;
        const char* data = httpRequest.c_str();

        // Step 1: Send first chunk
        auto chunk1Responder = [](void* p) {
          auto* stream = static_cast<NiceMock<MockStream>*>(p);
          vTaskDelay(pdMS_TO_TICKS(100));
          stream->InjectRxData(">\r\n");
          vTaskDelete(nullptr);
        };
        TaskHandle_t c1ResponderHandle = nullptr;
        xTaskCreate(
            chunk1Responder, "C1Responder", configMINIMAL_STACK_SIZE * 2, mockStream, 1,
            &c1ResponderHandle);
        ATPromise* promise1 = handler->sendCommand("AT+QISEND=0," + String(chunkSize));
        if (!promise1) throw std::runtime_error("Failed to create chunk 1 promise");
        promise1->expect(">");
        if (!promise1->wait()) { throw std::runtime_error("Chunk 1 prompt failed"); }

        auto chunk1DataResponder = [](void* p) {
          auto* stream = static_cast<NiceMock<MockStream>*>(p);
          vTaskDelay(pdMS_TO_TICKS(100));
          stream->InjectRxData("OK\r\nSEND OK\r\n");
          vTaskDelete(nullptr);
        };
        TaskHandle_t c1DataHandle = nullptr;
        xTaskCreate(
            chunk1DataResponder, "C1Data", configMINIMAL_STACK_SIZE * 2, mockStream, 1,
            &c1DataHandle);
        handler->getStream()->write(reinterpret_cast<const uint8_t*>(data), chunkSize);
        handler->getStream()->flush();
        ATPromise* confirmPromise = handler->sendCommand("");
        if (!confirmPromise)
          throw std::runtime_error("Failed to create chunk 1 confirmation promise");
        confirmPromise->expect("OK")->expect("SEND OK");
        if (!confirmPromise->wait()) {
          throw std::runtime_error("Chunk 1 send confirmation failed");
        }

        // Step 2: Send second chunk
        static const size_t chunk2Size = totalLength - chunkSize;
        auto chunk2Responder = [](void* p) {
          auto* stream = static_cast<NiceMock<MockStream>*>(p);
          vTaskDelay(pdMS_TO_TICKS(100));
          stream->InjectRxData(">\r\n");
          vTaskDelete(nullptr);
        };
        TaskHandle_t c2ResponderHandle = nullptr;
        xTaskCreate(
            chunk2Responder, "C2Responder", configMINIMAL_STACK_SIZE * 2, mockStream, 1,
            &c2ResponderHandle);
        ATPromise* promise2 = handler->sendCommand("AT+QISEND=0," + String(chunk2Size));
        if (!promise2) throw std::runtime_error("Failed to create chunk 2 promise");
        promise2->expect(">");
        if (!promise2->wait()) { throw std::runtime_error("Chunk 2 prompt failed"); }

        auto chunk2DataResponder = [](void* p) {
          auto* stream = static_cast<NiceMock<MockStream>*>(p);
          vTaskDelay(pdMS_TO_TICKS(100));
          stream->InjectRxData("OK\r\n");
          stream->InjectRxData("SEND OK\r\n");
          vTaskDelete(nullptr);
        };
        TaskHandle_t c2DataHandle = nullptr;
        xTaskCreate(
            chunk2DataResponder, "C2Data", configMINIMAL_STACK_SIZE * 2, mockStream, 1,
            &c2DataHandle);
        handler->getStream()->write(reinterpret_cast<const uint8_t*>(data + chunkSize), chunk2Size);
        handler->getStream()->flush();
        ATPromise* confirmPromise2 = handler->sendCommand("");
        if (!confirmPromise2)
          throw std::runtime_error("Failed to create chunk 2 confirmation promise");
        confirmPromise2->expect("OK")->expect("SEND OK");
        if (!confirmPromise2->wait()) {
          throw std::runtime_error("Chunk 2 send confirmation failed");
        }
      },
      "SendHttpDataChunkedTest", configMINIMAL_STACK_SIZE * 6);

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
