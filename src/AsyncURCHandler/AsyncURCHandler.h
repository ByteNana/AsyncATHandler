/**
 * @file AsyncURCHandler.h
 * @brief Asynchronous Unsolicited Result Code (URC) Handler for Async Serial AT communication
 */
#pragma once

#include <Arduino.h>
#include <AsyncSmartLock/AsyncSmartLock.h>

#include "AsyncURCHandler.settings.h"

class AsyncURCHandler {
 private:
  std::vector<URCHandler> handlers;
  AsyncSmartLock lock;

  std::vector<URCHandler>::iterator findPattern(const String& pattern);
  std::vector<URCHandler>::iterator findMatch(const String& line);

 public:
  AsyncURCHandler() = default;
  ~AsyncURCHandler() = default;

  void handleUnsolicitedResponse(const String& line);
  void registerEvent(const String& pattern, URCCallback cb);
  void unregisterEvent(const String& pattern);
  bool isPattern(const String& pattern);
  bool isMatch(const String& line);
};
