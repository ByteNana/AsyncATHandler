#include "ATResponse.h"

void ATResponse::addLine(const ResponseLine& line) {
  lines.push_back(line);
  if (line.isFinalResponse()) {
    completed = true;
    success = (line.type == ResponseType::FINAL_OK);
  }
}

String ATResponse::getFullResponse() const {
  String result = "";
  for (const auto& line : lines) { result += line.content; }
  return result;
}

String ATResponse::getDataOnly() const {
  String result = "";
  for (const auto& line : lines) {
    if (line.type == ResponseType::INTERMEDIATE_DATA) { result += line.content; }
  }
  return result;
}

std::vector<String> ATResponse::getDataLines() const {
  std::vector<String> result;
  for (const auto& line : lines) {
    if (line.type == ResponseType::INTERMEDIATE_DATA) { result.push_back(line.content); }
  }
  return result;
}

bool ATResponse::containsResponse(const String& expected) const {
  for (const auto& line : lines) {
    if (line.content.indexOf(expected) != -1) { return true; }
  }
  return false;
}
