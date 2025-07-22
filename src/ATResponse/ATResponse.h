#pragma once
#include <Arduino.h>
#include <vector>

#include "ATResponse.settings.h"

class ATResponse {
 private:
  std::vector<ResponseLine> lines;
  bool completed = false;
  bool success = false;
  uint32_t commandId = 0;

 public:
  ATResponse(uint32_t id) : commandId(id) {}

  void addLine(const ResponseLine& line);
  String getFullResponse() const;
  String getDataOnly() const;
  std::vector<String> getDataLines() const;
  bool containsResponse(const String& expected) const;

  bool isCompleted() const { return completed; }
  bool isSuccess() const { return success; }
  uint32_t getId() const { return commandId; }
};
