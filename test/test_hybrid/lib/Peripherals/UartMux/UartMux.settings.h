#pragma once

/*
 * UartMux.settings.h
 * This file contains the configuration for the UART multiplexer.
 *
 * It defines the pin configurations for the UART multiplexer, including the address table and baud
 * rates.
 *
 * The UART multiplexer is used to switch between different UART devices.
 */

#include <Arduino.h>

#include <variant>
#include <vector>

#include "../Pins.h"

#define SERIAL_PORT_UART_MUX Serial1
#define UART_RX_PIN 14
#define UART_TX_PIN 32

#define SERIAL_PORT_UART_MODEM Serial2
#define UART_RX_MOD_PIN 23
#define UART_TX_MOD_PIN 19

// Avoid 0 here to prevent eModbus from dividing by zero when calculating intervals
enum class BaudRateType { RS485_MODBUS_TYPE = 255 };

using BaudRate = std::variant<int, BaudRateType>;

// COMM = LORA_WAN_MODULE
enum class UartMuxAddress {
  RS485 = 1,
  RS232 = 2,
  COMM_MODULE = 3,
  AC_MODULE = 4,
  GPS = 5,
};

constexpr std::array<std::pair<UartMuxAddress, BaudRate>, 5> baudRateMap = {{
    {UartMuxAddress::RS485, BaudRateType::RS485_MODBUS_TYPE},
    {UartMuxAddress::RS232, 115200},
    {UartMuxAddress::COMM_MODULE, 115200},
    {UartMuxAddress::AC_MODULE, 9600},
    {UartMuxAddress::GPS, 9600},
}};

constexpr int getBaudRate(UartMuxAddress address) {
  for (const auto& pair : baudRateMap) {
    if (pair.first == address) {
      if (std::holds_alternative<int>(pair.second)) {
        return std::get<int>(pair.second);
      } else if (std::holds_alternative<BaudRateType>(pair.second)) {
        return int(BaudRateType::RS485_MODBUS_TYPE);
      }
    }
  }
  return -1;
}

struct UartMuxPins {
  static constexpr int EN = MCP1_uart_mux_en;
  static constexpr int A0 = MCP1_uart_mux_a0;
  static constexpr int A1 = MCP1_uart_mux_a1;
  static constexpr int A2 = MCP1_uart_mux_a2;
};

constexpr std::array<int, 3> UART_MUX_ADDRESS_PINS = {
    UartMuxPins::A2, UartMuxPins::A1, UartMuxPins::A0};

constexpr std::array<std::array<bool, 3>, 8> UART_MUX_ADDRESS_TABLE = {{
    {LOW, LOW, LOW},    // 0 -> A2 A1 A0 = 000
    {LOW, LOW, HIGH},   // 1 -> A2 A1 A0 = 001
    {LOW, HIGH, LOW},   // 2 -> A2 A1 A0 = 010
    {LOW, HIGH, HIGH},  // 3 -> A2 A1 A0 = 011
    {HIGH, LOW, LOW},   // 4 -> A2 A1 A0 = 100
    {HIGH, LOW, HIGH},  // 5 -> A2 A1 A0 = 101
    {HIGH, HIGH, LOW},  // 6 -> A2 A1 A0 = 110
    {HIGH, HIGH, HIGH}  // 7 -> A2 A1 A0 = 111
}};
