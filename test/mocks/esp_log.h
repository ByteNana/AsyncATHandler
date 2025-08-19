#pragma once

#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>

enum LogLevel {
  LOG_LEVEL_NONE = 0,
  LOG_LEVEL_ERROR = 1,
  LOG_LEVEL_WARN = 2,
  LOG_LEVEL_INFO = 3,
  LOG_LEVEL_DEBUG = 4,
  LOG_LEVEL_VERBOSE = 5
};

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define LOG_COLOR_RED "\x1b[31m"
#define LOG_COLOR_GREEN "\x1b[32m"
#define LOG_COLOR_YELLOW "\x1b[33m"
#define LOG_COLOR_CYAN "\x1b[36m"
#define LOG_COLOR_GRAY "\x1b[37m"
#define LOG_COLOR_RESET "\x1b[0m"

static char s_log_buffer[1024];
static std::mutex s_log_output_mutex;

#define __SHORT_FILE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

static inline void _log_write(
    int level, const char* color, const char* prefix, const char* file, int line, const char* fmt,
    ...) {
  if (level > LOG_LEVEL) return;

  std::lock_guard<std::mutex> lock(s_log_output_mutex);

  std::cout << color << prefix << "[" << file << ":" << line << "] ";

  va_list args;
  va_start(args, fmt);
  int result = vsnprintf(s_log_buffer, sizeof(s_log_buffer), fmt, args);
  va_end(args);

  if (result < 0) {
    std::cerr << LOG_COLOR_RED << "[ERROR]" << LOG_COLOR_RESET << " Log formatting failed!"
              << std::endl;
  } else {
    std::cout << s_log_buffer;
  }

  std::cout << LOG_COLOR_RESET << std::endl;
}

#define log_e(fmt, ...) \
  _log_write(           \
      LOG_LEVEL_ERROR, LOG_COLOR_RED, "[ERROR] ", __SHORT_FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define log_w(fmt, ...) \
  _log_write(           \
      LOG_LEVEL_WARN, LOG_COLOR_YELLOW, "[WARN] ", __SHORT_FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define log_i(fmt, ...) \
  _log_write(           \
      LOG_LEVEL_INFO, LOG_COLOR_GREEN, "[INFO] ", __SHORT_FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define log_d(fmt, ...) \
  _log_write(           \
      LOG_LEVEL_DEBUG, LOG_COLOR_CYAN, "[DEBUG] ", __SHORT_FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define log_v(fmt, ...)                                                               \
  _log_write(                                                                         \
      LOG_LEVEL_VERBOSE, LOG_COLOR_GRAY, "[VERBOSE] ", __SHORT_FILE__, __LINE__, fmt, \
      ##__VA_ARGS__)

#define log_n(fmt, ...) \
  _log_write(           \
      LOG_LEVEL_NONE, LOG_COLOR_RESET, "[NONE] ", __SHORT_FILE__, __LINE__, fmt, ##__VA_ARGS__)
