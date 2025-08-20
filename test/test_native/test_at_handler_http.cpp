#include <gtest/gtest.h>

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
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

        handler->sendAT("AT+QIOPEN=0,0,\"TCP\",\"220.180.239.212\",8062,0,1");

        mockStream->InjectRxData("AT+QIOPEN=0,0,\"TCP\",\"220.180.239.212\",8062,0,1\r\n");
        mockStream->InjectRxData("OK\r\n");

        vTaskDelay(pdMS_TO_TICKS(50));

        int8_t result = handler->waitResponse(1000, "OK");
        if (result <= 0) { throw std::runtime_error("Should have received OK response"); }

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
        if (result <= 0) { throw std::runtime_error("Should have received QISTATE OK response"); }

        String qistateResponse = handler->getResponse("OK");
        if (qistateResponse.indexOf("+QISTATE: 0,\"TCP\"") == -1) {
          throw std::runtime_error("QISTATE response missing");
        }

        log_i("[Test] QISTATE response: %s", qistateResponse.c_str());
      },
      "HttpSocketLongTimeoutTest", 2048, 2, 70000);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerHTTPTest, CompleteHttpConnectionFlow) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

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
      },
      "CompleteHttpFlowTest", 2048, 2, 70000);

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerHTTPTest, SendHttpDataOverTcp) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

        // Step 1: Prepare HTTP request data
        String httpRequest =
            "GET /api/test HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Connection: close\r\n"
            "\r\n";

        size_t dataLength = httpRequest.length();
        log_i("[Test] HTTP request length: %zu bytes", dataLength);
        log_i("[Test] HTTP request: %s", httpRequest.c_str());

        // Step 2: Send QISEND command with data length
        String qisendCommand = "AT+QISEND=0," + String(dataLength);
        handler->sendAT(qisendCommand);

        // Inject command echo and ">" prompt
        mockStream->InjectRxData(qisendCommand + "\r\n");
        mockStream->InjectRxData(">\r\n");

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 3: Wait for ">" prompt using sendCommand to clean buffer
        String response;
        bool promptReceived = handler->sendCommand(qisendCommand, response, ">", 1000);
        if (!promptReceived) { throw std::runtime_error("Should have received > prompt"); }

        if (response.indexOf(">") == -1) {
          throw std::runtime_error("> prompt not found in response");
        }

        log_i("[Test] Received prompt: %s", response.c_str());

        // Step 4: Clear TX buffer and send HTTP data directly to stream
        mockStream->ClearTxData();

        // Write HTTP request data to stream (like TinyGSM does)
        handler->_stream->write(reinterpret_cast<const uint8_t*>(httpRequest.c_str()), dataLength);
        handler->_stream->flush();

        // Step 5: Inject response for data transmission
        vTaskDelay(pdMS_TO_TICKS(50));
        mockStream->InjectRxData("OK\r\n");
        mockStream->InjectRxData("SEND OK\r\n");

        vTaskDelay(pdMS_TO_TICKS(50));

        // Step 6: Wait for SEND OK confirmation
        int8_t result = handler->waitResponse(1000, "SEND OK");
        if (result <= 0) { throw std::runtime_error("Should have received SEND OK"); }

        String sendConfirmation = handler->getResponse("SEND OK");
        if (sendConfirmation.indexOf("SEND OK") == -1) {
          throw std::runtime_error("SEND OK not found in response");
        }

        log_i("[Test] Send confirmation: %s", sendConfirmation.c_str());

        // Step 7: Validate that all HTTP data was sent to the stream
        std::string sentData = mockStream->GetTxData();

        // Extract only the HTTP data (skip AT commands)
        // TxData should contain the HTTP request we wrote to the stream
        if (sentData.find("GET /api/test HTTP/1.1") == std::string::npos) {
          throw std::runtime_error("HTTP GET request not found in sent data");
        }

        if (sentData.find("Host: example.com") == std::string::npos) {
          throw std::runtime_error("HTTP Host header not found in sent data");
        }

        if (sentData.find("Connection: close") == std::string::npos) {
          throw std::runtime_error("HTTP Connection header not found in sent data");
        }

        // Verify exact length was sent (excluding AT commands)
        size_t httpDataStart = sentData.find("GET /api/test");
        if (httpDataStart == std::string::npos) {
          throw std::runtime_error("Could not find HTTP data start in sent data");
        }

        std::string actualHttpData = sentData.substr(httpDataStart);
        if (actualHttpData.length() < dataLength) {
          throw std::runtime_error(
              "Incomplete HTTP data sent. Expected: " + String(dataLength) +
              ", Got: " + String(actualHttpData.length()));
        }

        log_i("[Test] HTTP data sent successfully over TCP");
        log_i("[Test] Sent data length: %zu bytes", actualHttpData.length());
        log_i("[Test] Expected length: %zu bytes", dataLength);
      },
      "SendHttpDataTest");

  EXPECT_TRUE(testResult);
}

TEST_F(AsyncATHandlerHTTPTest, SendLargeHttpDataOverTcp) {
  bool testResult = runInFreeRTOSTask(
      [this]() {
        if (!handler->begin(*mockStream)) { throw std::runtime_error("Handler begin failed"); }

        // Step 1: Prepare larger HTTP POST request with JSON data
        String jsonPayload =
            "{\"sensor_id\":\"ESP32_001\","
            "\"temperature\":23.5,"
            "\"humidity\":65.2,"
            "\"timestamp\":\"2024-08-19T10:30:00Z\","
            "\"status\":\"active\","
            "\"metadata\":{\"version\":\"1.0\",\"location\":\"office\"}}";

        String httpRequest =
            "POST /api/sensors/data HTTP/1.1\r\n"
            "Host: api.iot-server.com\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " +
            String(jsonPayload.length()) +
            "\r\n"
            "Authorization: Bearer abc123def456\r\n"
            "Connection: close\r\n"
            "\r\n" +
            jsonPayload;

        size_t totalDataLength = httpRequest.length();
        log_i("[Test] Large HTTP request length: %zu bytes", totalDataLength);

        // Step 2: Send data in chunks
        const size_t chunkSize = 64;
        size_t sent = 0;
        const char* data = httpRequest.c_str();
        int chunkNumber = 1;

        mockStream->ClearTxData();  // Clear before starting

        while (sent < totalDataLength) {
          size_t currentChunkSize = min(chunkSize, totalDataLength - sent);

          log_i(
              "[Test] Sending chunk %d: %zu bytes (offset: %zu)", chunkNumber, currentChunkSize,
              sent);

          // Step 2a: Send QISEND command for this chunk
          String qisendCommand = "AT+QISEND=0," + String(currentChunkSize);

          // Inject responses for this chunk
          mockStream->InjectRxData(qisendCommand + "\r\n");
          mockStream->InjectRxData(">\r\n");

          vTaskDelay(pdMS_TO_TICKS(50));

          // Step 2b: Wait for ">" prompt for this chunk
          String response;
          bool success = handler->sendCommand(qisendCommand, response, ">", 1000);
          if (!success) {
            throw std::runtime_error("Failed to get > prompt for chunk " + String(chunkNumber));
          }

          log_i("[Test] Got prompt for chunk %d: %s", chunkNumber, response.c_str());

          // Step 2c: Write this chunk to stream
          handler->_stream->write(reinterpret_cast<const uint8_t*>(data + sent), currentChunkSize);
          handler->_stream->flush();

          // Step 2d: Inject completion responses for this chunk
          vTaskDelay(pdMS_TO_TICKS(50));
          mockStream->InjectRxData("OK\r\n");
          mockStream->InjectRxData("SEND OK\r\n");

          vTaskDelay(pdMS_TO_TICKS(50));

          // Step 2e: Wait for SEND OK for this chunk
          if (handler->waitResponse(1000, "SEND OK") <= 0) {
            throw std::runtime_error("Chunk " + String(chunkNumber) + " send not confirmed");
          }

          String sendConfirmation = handler->getResponse("SEND OK");
          log_i("[Test] Chunk %d confirmed: %s", chunkNumber, sendConfirmation.c_str());

          sent += currentChunkSize;
          chunkNumber++;
        }

        log_i(
            "[Test] All chunks sent successfully. Total: %zu bytes in %d chunks", sent,
            chunkNumber - 1);

        // Step 3: Filter out AT command patterns to get only HTTP data
        std::string sentData = mockStream->GetTxData();

        log_i("[Test] Total sent data length: %zu bytes", sentData.length());

        // Remove all AT+QISEND command patterns using simple string replacement
        std::string httpDataOnly = sentData;
        size_t pos = 0;
        while ((pos = httpDataOnly.find("AT+QISEND=", pos)) != std::string::npos) {
          // Find the end of this AT command (until newline or end of string)
          size_t endPos = httpDataOnly.find('\n', pos);
          if (endPos == std::string::npos) {
            endPos = httpDataOnly.length();
          } else {
            endPos++;  // Include the newline
          }

          // Remove this AT command
          httpDataOnly.erase(pos, endPos - pos);
        }

        log_i("[Test] Filtered HTTP data length: %zu bytes", httpDataOnly.length());
        log_i("[Test] HTTP data preview: \n%s", httpDataOnly.c_str());

        // Step 4: Validate the filtered HTTP data
        if (httpDataOnly.find("POST /api/sensors/data") == std::string::npos) {
          throw std::runtime_error("HTTP POST not found in filtered data");
        }

        if (httpDataOnly.find(jsonPayload.c_str()) == std::string::npos) {
          throw std::runtime_error("JSON payload not found in filtered data");
        }

        if (httpDataOnly.find("Authorization: Bearer") == std::string::npos) {
          throw std::runtime_error("Authorization header not found in filtered data");
        }

        // Step 5: Verify the filtered data matches our original request
        if (httpDataOnly != httpRequest.c_str()) {
          log_e("[Test] Content mismatch between original and transmitted data");
          log_e("[Test] Original length: %zu", httpRequest.length());
          log_e("[Test] Filtered length: %zu", httpDataOnly.length());

          // Show a detailed comparison for debugging
          log_e("[Test] Original: %.100s...", httpRequest.c_str());
          log_e("[Test] Filtered: %.100s...", httpDataOnly.c_str());

          throw std::runtime_error("Filtered HTTP data doesn't match original request");
        }

        log_i("[Test] Chunked HTTP transmission validated successfully");
        log_i(
            "[Test] All %zu bytes transmitted correctly in %d chunks", totalDataLength,
            chunkNumber - 1);
      },
      "SendLargeHttpDataTest");

  EXPECT_TRUE(testResult);
}

FREERTOS_TEST_MAIN()
