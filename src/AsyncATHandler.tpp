#include "esp_log.h"

inline void appendTo(String& dst, const char* s)        { dst += s; }
inline void appendTo(String& dst, const std::string& s) { dst += s.c_str(); }
inline void appendTo(String& dst, const String& s)      { dst += s; }

// generic arithmetic
template <typename T>
inline std::enable_if_t<std::is_arithmetic_v<T>, void>
appendTo(String& dst, T v) { dst += String(v); }

template <typename... Args>
void AsyncATHandler::sendAT(Args... cmd) {
  if (_stream == nullptr) {
    log_e("Stream not initialized");
    return;
  }
  String command;
  (appendTo(command, std::forward<Args>(cmd)), ...); // C++17 fold
  command += "\r\n";
  _stream->print(command);
  _stream->flush();
  log_d("Sent command: %s", command.c_str());
}

// Helper functions for different string types
inline const char* to_cstring(const String& str) { return str.c_str(); }
inline const char* to_cstring(const std::string& str) { return str.c_str(); }
inline const char* to_cstring(const char* str) { return str; }
inline const char* to_cstring(char* str) { return str; }
inline std::string to_cstring(int value) { return std::to_string(value); }

template <typename FirstArg, typename... RestArgs>
int8_t AsyncATHandler::waitResponse(FirstArg &&first, RestArgs &&...rest) {
  if constexpr (std::is_arithmetic_v<std::decay_t<FirstArg>>) {
    // First argument is timeout
    const char *responses[] = {to_cstring(rest)...};
    return waitResponseMultiple(static_cast<unsigned long>(first), responses, sizeof...(rest));
  } else {
    // First argument is a response string
    const char *responses[] = {to_cstring(first), to_cstring(rest)...};
    return waitResponseMultiple(AT_DEFAULT_TIMEOUT, responses, sizeof...(RestArgs) + 1);
  }
}

template <typename... Args>
bool AsyncATHandler::sendCommand(
    String &response, const String &expectedResponse, uint32_t timeout, Args &&...parts) {
  String cmd;
  size_t reserveSize = 0;
  (void)std::initializer_list<int>{(reserveSize += String(parts).length(), 0)...};
  cmd.reserve(reserveSize);
  (void)std::initializer_list<int>{(cmd += String(parts), 0)...};
  return sendCommand(cmd, response, expectedResponse, timeout);
}
