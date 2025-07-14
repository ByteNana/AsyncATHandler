#pragma once

#include <string>

class String : public std::string {
 public:
  String() : std::string() {}
  String(const char* str) : std::string(str) {}
  String(const std::string& str) : std::string(str) {}

  String& operator=(const char* str) {
    std::string::operator=(str);
    return *this;
  }

  // Added charAt support as requested
  char charAt(int index) const {
    if (index >= 0 && static_cast<size_t>(index) < size()) { return (*this)[index]; }
    return '\0';  // Arduino String::charAt returns null terminator for out-of-bounds
  }

  bool startsWith(const String& prefix) const { return find(prefix) == 0; }

  bool endsWith(const String& suffix) const {
    return size() >= suffix.size() && compare(size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  void trim() {
    auto start = find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
      clear();
      return;
    }
    auto end = find_last_not_of(" \t\r\n");
    *this = substr(start, end - start + 1);
  }

  int indexOf(char c) const {
    size_t pos = find(c);
    return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
  }

  int indexOf(const String& str) const {
    size_t pos = find(str);
    return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
  }

  int lastIndexOf(char c) const {
    size_t pos = rfind(c);
    return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
  }

  String substring(int from) const {
    if (from < 0) from = 0;
    return String(substr(from));
  }

  String substring(int from, int to) const {
    if (from < 0) from = 0;
    if (to < 0) to = 0;
    if (to < from) return String("");
    return String(substr(from, to - from));
  }

  int toInt() const {
    try {
      return std::stoi(*this);
    } catch (...) { return 0; }
  }
};
