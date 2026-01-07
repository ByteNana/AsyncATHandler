// This file is the single entry point for all hardware tests.

#include <Arduino.h>
#include <gtest/gtest.h>

#include "BoneBuilder.h"

// ENV_BONES() will expand to the setup() and loop() functions
// necessary to run the tests on the Arduino framework.
ENV_BONES()
