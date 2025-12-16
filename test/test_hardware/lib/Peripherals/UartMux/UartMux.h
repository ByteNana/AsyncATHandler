#pragma once

/*
 * UartMux.h
 * This file contains the definition of the UartMux class, which is used to interface with
 * UART multiplexers.
 */
#include <Arduino.h>

#include "../Expanders/Expanders.h"
#include "UartMux.settings.h"

extern SemaphoreHandle_t uartMutex;

class UartLock {
 public:
  UartLock() { xSemaphoreTake(uartMutex, portMAX_DELAY); }
  ~UartLock() { xSemaphoreGive(uartMutex); }
};

class UartMux {
 public:
  static void begin();
  void setUartAddress();
  UartMux(UartMuxAddress address);

 private:
  inline static int lastBaud = -1;
  inline static int lastAddress = -1;
  bool setBaudRate(int baud);
  bool setAddress(int address);

 protected:
  UartMuxAddress uart_mux_address;
  int baudRate;
  static Stream& serial;
};
