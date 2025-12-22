#pragma once

/*
 * Mux.settings.h
 * This file contains the configuration for the I2C multiplexer.
 *
 * It defines the multiplexer address, pins, and address table.
 *
 * The multiplexer is used to switch between different groups of I2C devices.
 *
 */

#include <Arduino.h>

#include "../Pins.h"

enum class MuxAddress {
  EXTERNAL_I2C_3_3V = 1,
  EXTERNAL_I2C_5V = 2,
  EXTENSION_MODULES = 6,
  ONBOARD_MODULES = 7
};

struct MuxPins {
  static constexpr int EN = MUX_ONBOARD_EN;
  static constexpr int A0 = MUX_ONBOARD_A0;
  static constexpr int A1 = MUX_ONBOARD_A1;
  static constexpr int A2 = MUX_ONBOARD_A2;
};

constexpr std::array<int, 3> MUX_ADDRESS_PINS = {MuxPins::A2, MuxPins::A1, MuxPins::A0};

constexpr std::array<std::array<bool, 3>, 8> MUX_ADDRESS_TABLE = {{
    {LOW, LOW, LOW},    // 0 -> A2 A1 A0 = 000
    {LOW, LOW, HIGH},   // 1 -> A2 A1 A0 = 001
    {LOW, HIGH, LOW},   // 2 -> A2 A1 A0 = 010
    {LOW, HIGH, HIGH},  // 3 -> A2 A1 A0 = 011
    {HIGH, LOW, LOW},   // 4 -> A2 A1 A0 = 100
    {HIGH, LOW, HIGH},  // 5 -> A2 A1 A0 = 101
    {HIGH, HIGH, LOW},  // 6 -> A2 A1 A0 = 110
    {HIGH, HIGH, HIGH}  // 7 -> A2 A1 A0 = 111
}};
