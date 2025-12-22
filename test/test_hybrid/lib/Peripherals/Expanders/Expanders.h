#pragma once

/*
 * Expanders.h
 * This file contains the definition of the Expanders class, which is used to interface with
 * MCP23X17 I/O expanders.
 *
 * It includes methods for initializing the expanders, setting pin modes, reading and writing
 * digital values, and checking connected devices.
 *
 * The Expanders class inherits from Muxs, because the Expander CI is connected through the Mux at
 * the ONBOARD_MODULES address.
 *
 * The inheritance provides an easy-to-switch address feature.
 */

#include <Adafruit_MCP23X17.h>
#include <Arduino.h>
#include <Wire.h>

#include "../Mux/Mux.h"
#include "Expanders.settings.h"

class Expanders : public Muxs {
  int expander_address;
  const std::vector<MCP_PinConfig>& pins;

  void checkConnecteds();

 public:
  Adafruit_MCP23X17 mcp;

 public:
  Expanders(ExpanderAddress address, const char* name);
  bool begin();
  void pinMode(int pin, int mode);
  void digitalWrite(int pin, int value);
  uint8_t digitalRead(int pin);
};

extern Expanders expander1;
extern Expanders expander2;
