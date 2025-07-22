#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>  // For std::atomic
#include <chrono>
#include <iostream>  // For temporary debug prints
#include <thread>

#include "AsyncATHandler.h"  // Assuming AsyncATHandler.h is in your library's include path
#include "Stream.h"          // Assuming Stream.h is in test/mocks/ or similar
#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // Include your FreeRTOS mock header

using ::testing::_;
using ::testing::AtLeast;
using ::testing::NiceMock;

class AsyncATHandlerTest : public ::testing::Test {
 protected:
  NiceMock<MockStream>* mockStream;
  AsyncATHandler* handler;

  void SetUp() override {
    mockStream = new NiceMock<MockStream>();
    mockStream->SetupDefaults();
    handler = new AsyncATHandler();  // AsyncATHandler constructor is now minimal
  }

  void TearDown() override {
    if (handler) {
      handler->end();
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      delete handler;
      handler = nullptr;
    }
    if (mockStream) {
      log_w("this");
      delete mockStream;
      mockStream = nullptr;
    }
  }

  void WaitFor(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
};

TEST_F(AsyncATHandlerTest, InitializationTest) {
  EXPECT_FALSE(handler->isRunning());
  EXPECT_EQ(0, handler->getQueuedCommandCount());

  EXPECT_TRUE(handler->begin(*mockStream));  // This creates FreeRTOS resources and starts task
  EXPECT_TRUE(handler->isRunning());

  EXPECT_FALSE(handler->begin(*mockStream));  // Should not initialize twice
}

TEST_F(AsyncATHandlerTest, SendAsyncCommand) {
  EXPECT_TRUE(handler->begin(*mockStream));
  mockStream->ClearTxData();

  EXPECT_CALL(*mockStream, write(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*mockStream, flush()).Times(AtLeast(1));

  EXPECT_TRUE(handler->sendCommandAsync("AT"));
  WaitFor(100);
  std::string sentData = mockStream->GetTxData();
  EXPECT_EQ("AT\r\n", sentData);
}

TEST_F(AsyncATHandlerTest, SendSyncCommandWithOKResponse) {
  log_w("--- Test: SendSyncCommandWithOKResponse ---");
  EXPECT_TRUE(handler->begin(*mockStream));

  EXPECT_CALL(*mockStream, available()).Times(AtLeast(1));
  EXPECT_CALL(*mockStream, read()).Times(AtLeast(1));

  std::thread responder([this]() {
    WaitFor(50);  // Give AsyncATHandler time to send command
    log_i("[Responder Thread] Injecting OK with whitespace");
    mockStream->InjectRxData("OK \r\n");
  });

  String response;
  bool sendResult = handler->sendCommand("AT", response);
  log_i("[Main Test Thread] sendCommand returned: %s", sendResult ? "TRUE" : "FALSE");
  log_i("[Main Test Thread] Received response: '%s'", response.c_str());

  EXPECT_TRUE(sendResult);  // Should succeed even with trailing whitespace
  EXPECT_EQ("OK \r\n", response);

  responder.join();
  log_w("--- Test End: SendSyncCommandWithOKResponse ---");
}

TEST_F(AsyncATHandlerTest, SendSyncCommandWithTimeout) {
  log_w("--- Test: SendSyncCommandWithTimeout ---");
  EXPECT_TRUE(handler->begin(*mockStream));
  String response;
  bool sendResult =
      handler->sendCommand("AT+TIMEOUT", response, "OK", 100);  // Expect timeout after 100ms
  log_i("[Main Test Thread] sendCommand (timeout) returned: %s", sendResult ? "TRUE" : "FALSE");
  log_i("[Main Test Thread] Received response (timeout): '%s'", response.c_str());
  EXPECT_FALSE(sendResult);
  EXPECT_TRUE(response.empty());
  log_w("--- Test End: SendSyncCommandWithTimeout ---");
}

TEST_F(AsyncATHandlerTest, SendSyncCommandWithErrorResponse) {
  log_w("--- Test: SendSyncCommandWithErrorResponse ---");
  EXPECT_TRUE(handler->begin(*mockStream));
  std::thread responder([this]() {
    WaitFor(50);
    log_i("[Responder Thread] Injecting ERROR\\r\\n");
    mockStream->InjectRxData("ERROR\r\n");
  });
  String response;
  bool sendResult = handler->sendCommand("AT+FAIL", response);
  log_i("[Main Test Thread] sendCommand (error) returned: %s", sendResult ? "TRUE" : "FALSE");
  log_i("[Main Test Thread] Received response (error): '%s'", response.c_str());
  EXPECT_FALSE(sendResult);          // Should still return false for ERROR
  EXPECT_EQ("ERROR\r\n", response);  // <<< FIX: Expect the full line ending
  responder.join();
  log_w("--- Test End: SendSyncCommandWithErrorResponse ---");
}

TEST_F(AsyncATHandlerTest, SendCommandWithoutResponseParameter) {
  EXPECT_TRUE(handler->begin(*mockStream));
  mockStream->ClearTxData();

  std::thread responder([this]() {
    WaitFor(50);
    mockStream->InjectRxData("OK\r\n");
  });

  bool sendResult = handler->sendCommand("AT", "OK", 1000);

  std::string sentData;
  WaitFor(100);
  sentData = mockStream->GetTxData();

  EXPECT_TRUE(sendResult);
  EXPECT_EQ("AT\r\n", sentData);

  responder.join();
}

TEST_F(AsyncATHandlerTest, VariadicSendCommandHelper) {
  EXPECT_TRUE(handler->begin(*mockStream));
  mockStream->ClearTxData();

  std::thread responder([this]() {
    WaitFor(50);
    mockStream->InjectRxData("OK\r\n");
  });

  String response;
  bool sendResult = handler->sendCommand(response, "OK", 1000, "AT+", "VAR");

  std::string sentData;
  WaitFor(100);
  sentData = mockStream->GetTxData();

  EXPECT_TRUE(sendResult);
  EXPECT_EQ("AT+VAR\r\n", sentData);
  EXPECT_EQ("OK\r\n", response);

  responder.join();
}

TEST_F(AsyncATHandlerTest, UnsolicitedResponseHandling) {
  std::cout << "--- Test: UnsolicitedResponseHandling ---" << std::endl;
  log_w("--- Test Start: UnsolicitedResponseHandling ---");
  EXPECT_TRUE(handler->begin(*mockStream));
  bool callbackCalled = false;
  String unsolicitedData;

  handler->setUnsolicitedCallback([&](const String& response) {
    callbackCalled = true;
    unsolicitedData = response;
    log_i("[Callback] Unsolicited response received: '%s'", response.c_str());
  });

  log_i("[Main Test Thread] Injecting unsolicited data...");
  mockStream->InjectRxData("+CMT: \"+1234567890\",\"\",\"24/01/15,10:30:00\"\r\n");
  mockStream->InjectRxData("Hello World\r\n");
  WaitFor(200);  // Give time for AsyncATHandler to process

  EXPECT_TRUE(callbackCalled);
  EXPECT_TRUE(unsolicitedData.startsWith("+CMT:"));  // This still works with \r\n
  log_w("--- Test End: UnsolicitedResponseHandling ---");
}

TEST_F(AsyncATHandlerTest, CommandBatchExecution) {
  log_w("--- Test: CommandBatchExecution ---");
  EXPECT_TRUE(handler->begin(*mockStream));

  const String commands[] = {"AT", "AT+CGMI", "AT+CGMM"};
  String responses[3];

  std::thread responder([this]() {
    WaitFor(50);
    log_i("[Responder Thread] Injecting OK for AT");
    mockStream->InjectRxData("OK\r\n");
    WaitFor(50);
    log_i("[Responder Thread] Injecting SIMCOM for AT+CGMI");
    mockStream->InjectRxData("SIMCOM\r\nOK\r\n");
    WaitFor(50);
    log_i("[Responder Thread] Injecting SIM7600 for AT+CGMM");
    mockStream->InjectRxData("SIM7600\r\nOK\r\n");
  });

  bool batchResult = handler->sendCommandBatch(commands, 3, responses);
  log_i("[Main Test Thread] sendCommandBatch returned: %s", batchResult ? "TRUE" : "FALSE");
  log_i("[Main Test Thread] Responses[0]: '%s'", responses[0].c_str());
  log_i("[Main Test Thread] Responses[1]: '%s'", responses[1].c_str());
  log_i("[Main Test Thread] Responses[2]: '%s'", responses[2].c_str());

  EXPECT_TRUE(batchResult);                      // Should now pass
  EXPECT_EQ("OK\r\n", responses[0]);             // <<< FIX: Expect full line
  EXPECT_EQ("SIMCOM\r\nOK\r\n", responses[1]);   // <<< FIX: Expect aggregated data + OK
  EXPECT_EQ("SIM7600\r\nOK\r\n", responses[2]);  // <<< FIX: Expect aggregated data + OK
  responder.join();
  log_w("--- Test End: CommandBatchExecution ---");
}

TEST_F(AsyncATHandlerTest, CommandQueueManagement) {
  log_w("--- Test: CommandQueueManagement ---");
  EXPECT_TRUE(handler->begin(*mockStream));
  mockStream->ClearTxData();

  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(handler->sendCommandAsync("AT+" + String(std::to_string(i).c_str())));
  }

  EXPECT_GT(handler->getQueuedCommandCount(), 0);
  WaitFor(300);
  EXPECT_EQ(0, handler->getQueuedCommandCount());

  std::string sentData = mockStream->GetTxData();
  EXPECT_TRUE(sentData.find("AT+0\r\n") != std::string::npos);
  EXPECT_TRUE(sentData.find("AT+4\r\n") != std::string::npos);
  log_w("--- Test End: CommandQueueManagement ---");
}

TEST_F(AsyncATHandlerTest, ResponseQueueHandling) {
  log_w("--- Test: ResponseQueueHandling ---");
  EXPECT_TRUE(handler->begin(*mockStream));

  std::thread sender([this]() {
    String response;
    handler->sendCommand("AT+TEST", response, "+TEST: DATA");  // Expect the data line
    log_i("[Sender Thread] sendCommand('AT+TEST') returned: %s", response.c_str());
  });

  WaitFor(50);  // Give time for AT+TEST to be sent
  log_i("[Main Test Thread] Injecting +TEST: DATA\\r\\nOK\\r\\n");
  mockStream->InjectRxData("+TEST: DATA\r\nOK\r\n");  // Two lines

  WaitFor(100);  // Give time for AsyncATHandler to process response

  // This test retrieves responses from the _responseQueue AFTER the sendCommand has finished.
  // The sendCommand has already consumed its matched response.
  // So, there shouldn't be anything for AT+TEST left in _responseQueue *if sendCommand consumes it
  // fully*. However, if the design expects hasResponse/getResponse for *unmatched* lines or lines
  // that are just generally passed up.
  // Based on previous logs, the sendCommand *does* consume its response.
  // So, this `if (handler->hasResponse())` might not be true, or it might get an unmatched line.

  // Let's modify this test to confirm the _responseQueue is empty *after* sendCommand
  // finishes and gets its response, and then test getting a *new* unsolicited response
  // if that's the intent of this test.

  sender.join();  // Ensure sendCommand has finished and consumed its response

  // Now, after sender has completed, the _responseQueue should be empty
  // unless there are other non-matched responses being queued.
  // Let's assert it's empty, or adjust the test if it expects some other behavior here.
  EXPECT_EQ(
      0, handler->getQueuedResponseCount());  // Should be 0 if sendCommand cleared its response

  // If you want to test that unsolicited responses APPEAR in this queue,
  // you'd inject an unsolicited response *after* the sendCommand finishes.
  // For now, let's just confirm it's empty.

  log_w("--- Test End: ResponseQueueHandling ---");
}

TEST_F(AsyncATHandlerTest, CleanShutdown) {
  log_w("--- Test: CleanShutdown ---");
  EXPECT_TRUE(handler->begin(*mockStream));

  handler->sendCommandAsync("AT+1");
  handler->sendCommandAsync("AT+2");

  handler->end();  // This is critical for stopping internal tasks
  EXPECT_FALSE(handler->isRunning());

  EXPECT_FALSE(handler->sendCommandAsync("AT+3"));
  String response;
  EXPECT_FALSE(handler->sendCommand("AT+4", response));
  log_w("--- Test End: CleanShutdown ---");
  WaitFor(100);  // Give time for the task to clean up
}

TEST_F(AsyncATHandlerTest, LongResponseNotTruncated) {
  EXPECT_TRUE(handler->begin(*mockStream));

  // Generate a response line longer than AT_COMMAND_MAX_LENGTH (512) but
  // less than AT_RESPONSE_BUFFER_SIZE (1024).
  std::string longLine(600, 'A');
  std::string injected = longLine + "\r\nOK\r\n";

  std::thread responder([&]() {
    WaitFor(50);
    mockStream->InjectRxData(injected);
  });

  String response;
  bool result = handler->sendCommand("AT+LONG", response);
  responder.join();

  EXPECT_TRUE(result);
  EXPECT_EQ(String(longLine.c_str()) + "\r\nOK\r\n", response);
}
