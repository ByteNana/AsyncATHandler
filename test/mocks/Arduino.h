#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>


// Arduino type definitions
typedef bool boolean;
typedef uint8_t byte;
typedef uint16_t word;

#include "mockString.h"

// Arduino millis function
inline unsigned long millis() {
  static auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

// Arduino delay function
inline void delay(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
