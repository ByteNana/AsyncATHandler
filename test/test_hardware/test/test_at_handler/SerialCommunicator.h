#define ESP32 1
#pragma once

#ifdef ESP32
#include <Arduino.h>
#include <Peripherals.h>
#endif  // ESP32

#include "common.h" // Assuming common.h brings in MockStream or defines it.
#include "Stream.h" // For the Stream class. It is already present in common.h but lets keep it explicit for now.

class SerialCommunicator : public Stream {
 private:
#ifdef ESP32
  HardwareSerial *hardwareStream = nullptr;
#else
  Stream *hardwareStream = nullptr; // For native, it's a generic Stream
#endif
  MockStream *mockStream = nullptr;
  Stream *activeStream = nullptr;  // points to hardwareStream or mockStream

 public:
  SerialCommunicator();
  ~SerialCommunicator();

  Stream *getActiveStream() { return activeStream; }

  void mockResponse(const std::string &data);
  void ClearSentData();
  std::string GetSentData();

  // Stream interface
  int available() override;
  int read() override;
  int peek() override;
  void flush() override;
  size_t write(uint8_t) override;
  size_t write(const uint8_t *buffer, size_t size) override;
};

SerialCommunicator::SerialCommunicator() {
  mockStream = new ::testing::NiceMock<MockStream>();
  mockStream->SetupDefaults();

#ifdef ESP32
  startPeripherals();
  SERIAL_PORT_UART_MODEM.setTxBufferSize(2048);
  SERIAL_PORT_UART_MODEM.setRxBufferSize(2048);
  SERIAL_PORT_UART_MODEM.begin(115200, SERIAL_8N1, UART_RX_MOD_PIN, UART_TX_MOD_PIN);

  expander1.digitalWrite(MCP1_gsm_pwrkey, HIGH);
  delay(1000);
  expander1.digitalWrite(MCP1_gsm_pwrkey, LOW);
  delay(5000);

  activeStream = &SERIAL_PORT_UART_MODEM;
#else
  activeStream = mockStream; // For native, use mock stream by default
#endif  // ESP32
}

SerialCommunicator::~SerialCommunicator() {
  // hardwareStream is only allocated for ESP32
  // and is not owned by this class in that case (it's a global SERIAL_PORT_UART_MODEM)
  // so no need to delete it.

  delete mockStream;
  mockStream = nullptr;
}

void SerialCommunicator::mockResponse(const std::string &data) { mockStream->InjectRxData(data); }

void SerialCommunicator::ClearSentData() {
  if (mockStream) { mockStream->ClearTxData(); }
}

std::string SerialCommunicator::GetSentData() { return mockStream->GetTxData(); }

int SerialCommunicator::available() { return activeStream->available(); }

int SerialCommunicator::read() { return activeStream->read(); }

int SerialCommunicator::peek() { return activeStream->peek(); }

void SerialCommunicator::flush() { activeStream->flush(); }

size_t SerialCommunicator::write(uint8_t c) {
  size_t result = activeStream->write(c);
  if (activeStream != mockStream) { mockStream->write(c); }
  return result;
}

size_t SerialCommunicator::write(const uint8_t *buffer, size_t size) {
  size_t result = activeStream->write(buffer, size);
  if (activeStream != mockStream) { mockStream->write(buffer, size); }
  return result;
}