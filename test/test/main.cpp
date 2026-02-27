// This file is the single entry point for all hardware tests.
// On native builds, CMake globs only test_*.cpp so this file is excluded.

#ifdef ESP32
#include <Arduino.h>
#include <gtest/gtest.h>

void setup() {
  Serial.begin(115200);
  ::testing::InitGoogleTest();
  RUN_ALL_TESTS();
}

void loop() {}
#endif
