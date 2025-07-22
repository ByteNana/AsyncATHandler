#include <Arduino.h>
#include <AsyncATHandler.h>
#include <unity.h>

#include "mocks.h"

AsyncATHandler handler;
HardwareMockStream mockSerial;

void setUp(void) { mockSerial.clearSentData(); }

void tearDown(void) {}

// Test case for a successful "AT" command.
// 1. Mock a successful "OK" response.
// 2. Send the command and expect a successful result.
// 3. Assert that the command was sent and the response contains "OK".
void test_at_command_success() {
  mockSerial.mockResponse("AT\r\nOK\r\n");

  String response;
  bool ok = handler.sendCommand("AT", response, "OK", 1000);

  TEST_ASSERT_EQUAL_STRING("AT\r\n", mockSerial.getSentData().c_str());
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_TRUE(response.indexOf("OK") != -1);
}

// Test case for a command that times out.
// 1. Mock no response at all.
// 2. Send the command and expect it to fail due to timeout.
// 3. Assert that the command was sent and the handler reported failure.
void test_at_command_timeout() {
  mockSerial.mockResponse("");

  String response;
  bool ok = handler.sendCommand("AT", response, "OK", 500);

  TEST_ASSERT_EQUAL_STRING("AT\r\n", mockSerial.getSentData().c_str());
  TEST_ASSERT_FALSE(ok);
}

// Test case for a command that returns an ERROR.
// 1. Mock an "ERROR" response.
// 2. Send the command and expect it to fail.
// 3. Assert that the command was sent and the response contains "ERROR".
void test_at_command_error() {
  mockSerial.mockResponse("AT\r\nERROR\r\n");

  String response;
  bool ok = handler.sendCommand("AT", response, "OK", 1000);

  TEST_ASSERT_EQUAL_STRING("AT\r\n", mockSerial.getSentData().c_str());
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_TRUE(response.indexOf("ERROR") != -1);
}

void test_gprs_connect_sequence() {
  mockSerial.mockResponse(
    "AT+QIDEACT=1\r\nOK\r\n"
    "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"\r\nOK\r\n"
    "AT+QIACT=1\r\nOK\r\n"
    "AT+CGATT=1\r\nOK\r\n"
  );

  String response;
  bool ok = true;

  ok &= handler.sendCommand("AT+QIDEACT=1", response, "OK", 2000);
  ok &= handler.sendCommand(response, "OK", 5000, "AT+QICSGP=1,1,\"", "internet", "\",\"user\",\"pass\"");
  ok &= handler.sendCommand("AT+QIACT=1", response, "OK", 150000);
  ok &= handler.sendCommand("AT+CGATT=1", response, "OK", 60000);

  String expected =
    "AT+QIDEACT=1\r\n"
    "AT+QICSGP=1,1,\"internet\",\"user\",\"pass\"\r\n"
    "AT+QIACT=1\r\n"
    "AT+CGATT=1\r\n";

  TEST_ASSERT_EQUAL_STRING(expected.c_str(), mockSerial.getSentData().c_str());
  TEST_ASSERT_TRUE(ok);
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  UNITY_BEGIN();

  TEST_ASSERT_TRUE(handler.begin(mockSerial));

  RUN_TEST(test_at_command_success);
  RUN_TEST(test_at_command_timeout);
  RUN_TEST(test_at_command_error);
  RUN_TEST(test_gprs_connect_sequence);

  UNITY_END();
}

void loop() {
  // The loop is not used for unit testing
}
