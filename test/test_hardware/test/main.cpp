// This file is the single entry point for all hardware tests.

#include <Arduino.h>
#include <gtest/gtest.h>

#include "BoneBuilder.h"

void setup() {
  Serial.begin(115200);
  ::testing::InitGoogleTest();
  RUN_ALL_TESTS();
}

void loop() {}