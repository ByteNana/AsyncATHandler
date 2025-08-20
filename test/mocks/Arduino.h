#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <random>
#include <thread>

// Arduino-specific functions
#define min(a, b) std::min(a, b)
#define max(a, b) std::max(a, b)

// Arduino type definitions
typedef bool boolean;
typedef uint8_t byte;
typedef uint16_t word;

#include "WString.h"

inline bool isSpace(char c) { return isspace(static_cast<unsigned char>(c)); }

inline bool isHexadecimalDigit(char c) {
  return isdigit(static_cast<unsigned char>(c)) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

inline bool isDigit(char c) { return isdigit(static_cast<unsigned char>(c)); }

inline bool isAlpha(char c) { return isalpha(static_cast<unsigned char>(c)); }

inline bool isAlphaNumeric(char c) { return isalnum(static_cast<unsigned char>(c)); }

inline unsigned long millis() {
  static auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

inline void delay(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

inline void randomSeed(unsigned long seed) {
  static std::mt19937 generator;
  generator.seed(seed);
}

inline long random(long max) {
  static std::mt19937 generator(millis());  // Seed with current time
  std::uniform_int_distribution<long> distribution(0, max - 1);
  return distribution(generator);
}

inline long random(long min, long max) {
  static std::mt19937 generator(millis());  // Seed with current time
  std::uniform_int_distribution<long> distribution(min, max - 1);
  return distribution(generator);
}
