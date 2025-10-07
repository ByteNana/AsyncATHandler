#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <random>
#include <thread>

// Arduino-specific functions
// Note: Don't use macros for min/max as they conflict with C++ std library
// Arduino code should use std::min/std::max directly or include <algorithm>
#ifndef min
inline int min(int a, int b) { return (a < b) ? a : b; }
inline long min(long a, long b) { return (a < b) ? a : b; }
inline float min(float a, float b) { return (a < b) ? a : b; }
inline double min(double a, double b) { return (a < b) ? a : b; }
#endif

#ifndef max
inline int max(int a, int b) { return (a > b) ? a : b; }
inline long max(long a, long b) { return (a > b) ? a : b; }
inline float max(float a, float b) { return (a > b) ? a : b; }
inline double max(double a, double b) { return (a > b) ? a : b; }
#endif

// Arduino type definitions
typedef bool boolean;
typedef uint8_t byte;
typedef uint16_t word;

#include "Stream.h"
#include "WString.h"
#include "times.h"

inline bool isSpace(char c) { return isspace(static_cast<unsigned char>(c)); }

inline bool isHexadecimalDigit(char c) {
  return isdigit(static_cast<unsigned char>(c)) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

inline bool isDigit(char c) { return isdigit(static_cast<unsigned char>(c)); }

inline bool isAlpha(char c) { return isalpha(static_cast<unsigned char>(c)); }

inline bool isAlphaNumeric(char c) { return isalnum(static_cast<unsigned char>(c)); }

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
