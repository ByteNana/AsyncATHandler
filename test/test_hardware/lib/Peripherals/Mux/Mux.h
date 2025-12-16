#pragma once
/*
 * Mux.h
 * This file contains the definition of the Muxs class, which is used to access, read and control
 * multiple peripheral devices.
 *
 * It includes methods for switching between different Mux addresses.
 *
 * The Muxs class should be inherited by other classes that need to use the Mux.
 */
#include <Arduino.h>

#include "Mux.settings.h"

class Muxs {
  static void setEnable(bool enable);

 protected:
  MuxAddress mux_address;
  void setMuxAddress();

 public:
  static void begin();

  Muxs(MuxAddress address) : mux_address(address){};
};
