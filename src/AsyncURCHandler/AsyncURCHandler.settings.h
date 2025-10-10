#pragma once
#include <Arduino.h>

using URCCallbackFn = void(const String& urc);
typedef std::function<void(const String& urc)> URCCallback;

struct URCHandler {
  String pattern;
  URCCallback cb;
  // Compare only patterns for equality
  bool operator==(const URCHandler& other) const { return pattern == other.pattern; }
};
