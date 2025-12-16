#include "UartMux.h"

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SERIAL_PORT_UART_MUX, Serial);
Stream& UartMux::serial = debugger;
#else
Stream& UartMux::serial = SERIAL_PORT_UART_MUX;
#endif

SemaphoreHandle_t uartMutex = xSemaphoreCreateMutex();

void UartMux::begin() { log_v("UartMux::begin"); }

UartMux::UartMux(UartMuxAddress address) : uart_mux_address(address) {
  baudRate = getBaudRate(address);
};

bool UartMux::setBaudRate(int baud) {
  if (lastBaud == baud) { return false; }
  log_v(
      "UartMux::setUartAddress: Configuring UART: baud=%d, rx=%d, tx=%d", baud, UART_RX_PIN,
      UART_TX_PIN);
  SERIAL_PORT_UART_MUX.begin(baud, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  lastBaud = baud;
  return true;
}

bool UartMux::setAddress(int address) {
  if (lastAddress == address) { return false; }
  log_v("UartMux::setUartAddress: Switching mux to address %d", address);

  expander1.digitalWrite(UartMuxPins::EN, HIGH);

  for (int i = 0; i < UART_MUX_ADDRESS_TABLE[address].size(); i++) {
    expander1.digitalWrite(UART_MUX_ADDRESS_PINS[i], UART_MUX_ADDRESS_TABLE[address][i]);
  }

  delayMicroseconds(100);
  expander1.digitalWrite(UartMuxPins::EN, LOW);
  lastAddress = address;
  return true;
}

void UartMux::setUartAddress() {
  const int target_address = static_cast<int>(uart_mux_address);

  if (target_address < 0 || target_address >= static_cast<int>(UART_MUX_ADDRESS_TABLE.size())) {
    log_e("UartMux::setUartAddress: Invalid address index %d", target_address);
    return;
  }

  if (setAddress(target_address)) {
    delay(2);
    // Clear any residual data in the serial buffer
    while (serial.available()) { serial.read(); }
  }
  // BaudRate selection moved to the bottom to ensure we still operate the MUX even if baud rate is
  // invalid
  int baud = baudRate;
  if (baud == (int)BaudRateType::RS485_MODBUS_TYPE) {
    if (baud <= 0) {
      log_e("UartMux::setUartAddress: Invalid baud rate for RS485 Modbus profile: %d", baud);
      return;
    }
  };

  setBaudRate(baud);
}
