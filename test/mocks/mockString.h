#pragma once

#include <string>

class String {
 private:
  std::string data;  // Composition instead of inheritance

 public:
  // Constructors
  String() : data() {}
  String(const char* str) : data(str ? str : "") {}
  String(const std::string& str) : data(str) {}

  // Copy constructor - CRITICAL for memcpy safety
  String(const String& other) : data(other.data) {}

  // Move constructor
  String(String&& other) noexcept : data(std::move(other.data)) {}

  // Assignment operators
  String& operator=(const char* str) {
    data = (str ? str : "");
    return *this;
  }

  String& operator=(const String& other) {
    if (this != &other) { data = other.data; }
    return *this;
  }

  String& operator=(String&& other) noexcept {
    if (this != &other) { data = std::move(other.data); }
    return *this;
  }

  // All your existing Arduino-compatible methods
  char charAt(int index) const {
    if (index >= 0 && static_cast<size_t>(index) < data.size()) { return data[index]; }
    return '\0';
  }

  bool startsWith(const String& prefix) const { return data.find(prefix.data) == 0; }

  bool endsWith(const String& suffix) const {
    return data.size() >= suffix.data.size() &&
           data.compare(data.size() - suffix.data.size(), suffix.data.size(), suffix.data) == 0;
  }

  void trim() {
    auto start = data.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
      data.clear();
      return;
    }
    auto end = data.find_last_not_of(" \t\r\n");
    data = data.substr(start, end - start + 1);
  }

  int indexOf(char c) const {
    size_t pos = data.find(c);
    return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
  }

  int indexOf(const String& str) const {
    size_t pos = data.find(str.data);
    return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
  }

  int lastIndexOf(char c) const {
    size_t pos = data.rfind(c);
    return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
  }

  String substring(int from) const {
    if (from < 0) from = 0;
    return String(data.substr(from));
  }

  String substring(int from, int to) const {
    if (from < 0) from = 0;
    if (to < 0) to = 0;
    if (to < from) return String("");
    return String(data.substr(from, to - from));
  }

  int toInt() const {
    try {
      return std::stoi(data);
    } catch (...) { return 0; }
  }

  // Essential std::string-like methods
  const char* c_str() const { return data.c_str(); }
  size_t size() const { return data.size(); }
  size_t length() const { return data.length(); }
  bool empty() const { return data.empty(); }
  void clear() { data.clear(); }

  // Operators
  String& operator+=(const String& other) {
    data += other.data;
    return *this;
  }

  String& operator+=(const char* str) {
    if (str) data += str;
    return *this;
  }

  String& operator+=(char c) {
    data += c;
    return *this;
  }

  String operator+(const String& other) const {
    String result(*this);
    result += other;
    return result;
  }

  String operator+(const char* str) const {
    String result(*this);
    result += str;
    return result;
  }

  bool operator==(const String& other) const { return data == other.data; }

  bool operator==(const char* str) const { return data == (str ? str : ""); }

  bool operator!=(const String& other) const { return data != other.data; }

  bool operator!=(const char* str) const { return data != (str ? str : ""); }

  // Global operators for const char* + String and const char* == String
  friend String operator+(const char* lhs, const String& rhs) {
    String result(lhs);
    result += rhs;
    return result;
  }

  friend bool operator==(const char* lhs, const String& rhs) { return rhs == lhs; }

  friend bool operator!=(const char* lhs, const String& rhs) { return !(rhs == lhs); }

  char& operator[](size_t index) { return data[index]; }
  const char& operator[](size_t index) const { return data[index]; }

  // Stream output
  friend std::ostream& operator<<(std::ostream& os, const String& str) { return os << str.data; }

  // Conversion to std::string when needed
  operator std::string() const { return data; }
  const std::string& str() const { return data; }
};
