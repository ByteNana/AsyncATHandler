#pragma once

#include <cstdarg>   // For va_list
#include <cstdio>    // For vsnprintf, strrchr
#include <iostream>  // For std::cout
#include <mutex>     // For std::mutex, std::lock_guard
#include <string>    // For std::string (though strrchr uses C-style strings)

// --- Log Level Definitions ---
enum LogLevel {
  LOG_LEVEL_NONE = 0,
  LOG_LEVEL_ERROR = 1,
  LOG_LEVEL_WARN = 2,
  LOG_LEVEL_INFO = 3,
  LOG_LEVEL_DEBUG = 4,
  LOG_LEVEL_VERBOSE = 5
};

#ifndef CONFIG_MY_LOG_DEFAULT_LEVEL
#define CONFIG_MY_LOG_DEFAULT_LEVEL LOG_LEVEL_INFO
#endif

// --- ANSI Color Codes (for terminal output) ---
#define LOG_COLOR_RED "\x1b[31m"
#define LOG_COLOR_GREEN "\x1b[32m"
#define LOG_COLOR_YELLOW "\x1b[33m"
#define LOG_COLOR_CYAN "\x1b[36m"
#define LOG_COLOR_GRAY "\x1b[37m"  // Used for Verbose
#define LOG_COLOR_RESET "\x1b[0m"  // Resets color

// --- Internal Global Resources ---
static char s_log_buffer[1024];
static std::mutex s_log_output_mutex;

// --- Helper macro to get just the filename from __FILE__ ---
#ifdef _WIN32
// For Windows, find the last backslash
#define __MY_SHORT_FILE__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
// For Unix-like systems, find the last forward slash
#define __MY_SHORT_FILE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

// --- Internal Logging Function ---
static inline void _my_log_write(
    int level, const char* color_code, const char* prefix, const char* file, int line,
    const char* fmt, ...) {
  if (level > CONFIG_MY_LOG_DEFAULT_LEVEL) { return; }

  std::lock_guard<std::mutex> lock(s_log_output_mutex);

  // Print the colored prefix, file (shortened), and line number
  std::cout << color_code << prefix << "[" << file << ":" << line << "] ";

  // Format the user's message using vsnprintf into the buffer
  va_list args;
  va_start(args, fmt);
  int user_msg_len = vsnprintf(s_log_buffer, sizeof(s_log_buffer), fmt, args);
  va_end(args);

  if (user_msg_len < 0) {
    std::cerr << LOG_COLOR_RED << "[ERROR]" << LOG_COLOR_RESET << " Log formatting failed!"
              << std::endl;
  } else {
    std::cout << s_log_buffer;  // Print the user's formatted message
  }

  // Final newline and color reset
  std::cout << LOG_COLOR_RESET << std::endl;
}

// --- Public Logging Macros (modified to use __MY_SHORT_FILE__) ---
#define log_e(format, ...)                                                               \
  do {                                                                                   \
    _my_log_write(                                                                       \
        LOG_LEVEL_ERROR, LOG_COLOR_RED, "[ERROR] ", __MY_SHORT_FILE__, __LINE__, format, \
        ##__VA_ARGS__);                                                                  \
  } while (0)

#define log_w(format, ...)                                                                \
  do {                                                                                    \
    _my_log_write(                                                                        \
        LOG_LEVEL_WARN, LOG_COLOR_YELLOW, "[WARN] ", __MY_SHORT_FILE__, __LINE__, format, \
        ##__VA_ARGS__);                                                                   \
  } while (0)

#define log_i(format, ...)                                                               \
  do {                                                                                   \
    _my_log_write(                                                                       \
        LOG_LEVEL_INFO, LOG_COLOR_GREEN, "[INFO] ", __MY_SHORT_FILE__, __LINE__, format, \
        ##__VA_ARGS__);                                                                  \
  } while (0)

#define log_d(format, ...)                                                                \
  do {                                                                                    \
    _my_log_write(                                                                        \
        LOG_LEVEL_DEBUG, LOG_COLOR_CYAN, "[DEBUG] ", __MY_SHORT_FILE__, __LINE__, format, \
        ##__VA_ARGS__);                                                                   \
  } while (0)

#define log_v(format, ...)                                                                    \
  do {                                                                                        \
    _my_log_write(                                                                            \
        LOG_LEVEL_VERBOSE, LOG_COLOR_GRAY, "[VERBOSE] ", __MY_SHORT_FILE__, __LINE__, format, \
        ##__VA_ARGS__);                                                                       \
  } while (0)

#define log_n(format, ...)                                                               \
  do {                                                                                   \
    _my_log_write(                                                                       \
        LOG_LEVEL_NONE, LOG_COLOR_RESET, "[NONE] ", __MY_SHORT_FILE__, __LINE__, format, \
        ##__VA_ARGS__);                                                                  \
  } while (0)
