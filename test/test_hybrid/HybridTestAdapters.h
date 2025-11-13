#pragma once
#include "AsyncATHandler.h"
#include "Stream.h"

#ifdef HTEST_ENV_HARDWARE
#include <HardwareSerial.h>
inline Stream* HTEST_CreateStream() {
  static HardwareSerial hwSerial(1);
  hwSerial.begin(115200);
  return &hwSerial;
}
#else /* Native */
#include <gmock/gmock.h>
using ::testing::NiceMock;
inline NiceMock<MockStream>* HTEST_CreateStream() {
  auto* mock = new NiceMock<MockStream>();
  mock->SetupDefaults();
  return mock;
}
#endif
