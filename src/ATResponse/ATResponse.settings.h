#pragma once

#include <Arduino.h>
#include <functional>

enum class ResponseType {
  FINAL_OK,
  FINAL_ERROR,
  FINAL_CME_ERROR,
  INTERMEDIATE_DATA,
  UNSOLICITED,
};

struct ResponseLine {
  String content;
  ResponseType type;
  uint32_t commandId;  // 0 for unsolicited
  unsigned long timestamp;

  bool isFinalResponse() const {
    return type == ResponseType::FINAL_OK || type == ResponseType::FINAL_ERROR ||
           type == ResponseType::FINAL_CME_ERROR;
  }
};

typedef std::function<void(const String& urc)> URCCallback;
