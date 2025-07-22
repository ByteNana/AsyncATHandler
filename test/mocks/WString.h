#pragma once
#include <string>
#include <type_traits>

class String : public std::string {
 public:
  String() : std::string() {}
  String(const char* str) : std::string(str) {}
  String(const std::string& str) : std::string(str) {}

  template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
  explicit String(T value) : std::string(std::to_string(value)) {}

  template <typename... Args>
  String(Args&&... args) : std::string(std::forward<Args>(args)...) {}

  String& operator=(const char* str) {
    std::string::operator=(str);
    return *this;
  }

  // Add only the missing operators that are essential
  String& operator+=(const String& str) {
    std::string::operator+=(str);
    return *this;
  }

  String& operator+=(const char* str) {
    std::string::operator+=(str);
    return *this;
  }

  String& operator+=(char c) {
    std::string::operator+=(c);
    return *this;
  }

  char charAt(int index) const {
    if (index >= 0 && static_cast<size_t>(index) < size()) { return (*this)[index]; }
    return '\0';
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

  int indexOf(char c, int fromIndex) const {
    if (fromIndex < 0) fromIndex = 0;
    size_t pos = find(c, static_cast<size_t>(fromIndex));
    return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
  }

  int indexOf(const String& str, int fromIndex) const {
    if (fromIndex < 0) fromIndex = 0;
    size_t pos = find(str, static_cast<size_t>(fromIndex));
    return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
  }

  int lastIndexOf(char c) const {
    size_t pos = rfind(c);
    return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
  }

  // Add only the missing lastIndexOf that you actually need
  int lastIndexOf(const String& str) const {
    size_t pos = rfind(str);
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

  bool reserve(size_t size) {
    try {
      std::string::reserve(size);
      return true;
    } catch (...) { return false; }
  }

  bool concat(char c) {
    try {
      *this += c;
      return true;
    } catch (...) { return false; }
  }

  bool concat(const String& str) {
    try {
      *this += str;
      return true;
    } catch (...) { return false; }
  }

  bool concat(const char* str) {
    try {
      *this += str;
      return true;
    } catch (...) { return false; }
  }

  // length() method (alias for size())
  size_t length() const { return size(); }

  void replace(char find, char replace) {
    for (size_t i = 0; i < length(); i++) {
      if ((*this)[i] == find) { (*this)[i] = replace; }
    }
  }

  void replace(const String& find, const String& replace) {
    if (find.empty()) return;
    size_t pos = 0;
    while ((pos = std::string::find(find, pos)) != std::string::npos) {
      std::string::replace(pos, find.length(), replace);
      pos += replace.length();
    }
  }

  void replace(const char* find, const String& replace) { this->replace(String(find), replace); }

  void replace(const char* find, const char* replace) {
    this->replace(String(find), String(replace));
  }

  void remove(unsigned int index) {
    if (index < length()) { erase(index, 1); }
  }

  void remove(unsigned int index, unsigned int count) {
    if (index < length()) {
      // Ensure we don't remove more characters than available
      size_t available = length() - index;
      size_t to_remove = (count > available) ? available : count;
      erase(index, to_remove);
    }
  }

  bool isEmpty() const { return empty(); }
};
