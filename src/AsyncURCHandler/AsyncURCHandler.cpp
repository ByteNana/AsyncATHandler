#include "AsyncURCHandler.h"

#include <esp_log.h>

std::vector<URCHandler>::iterator AsyncURCHandler::findPattern(const String& pattern) {
  return std::find_if(handlers.begin(), handlers.end(), [&pattern](const URCHandler& handler) {
    return handler.pattern == pattern && handler.cb;
  });
}

std::vector<URCHandler>::iterator AsyncURCHandler::findMatch(const String& line) {
  return std::find_if(handlers.begin(), handlers.end(), [&line](const URCHandler& handler) {
    return line.startsWith(handler.pattern);
  });
}

bool AsyncURCHandler::isPattern(const String& pattern) {
  auto _ = lock.guard();
  return findPattern(pattern) != handlers.end();
}

bool AsyncURCHandler::isMatch(const String& line) {
  auto g = lock.guard();
  return findMatch(line) != handlers.end();
}

void AsyncURCHandler::registerEvent(const String& pattern, URCCallback cb) {
  if (pattern.isEmpty() || !cb) {
    log_w("Attempted to register URC with empty pattern or null callback");
    return;
  }

  auto _ = lock.guard();
  auto it = findPattern(pattern);

  // If found, update the callback
  if (it != handlers.end()) {
    it->cb = cb;
    log_i("Updated URC handler for pattern: %s", pattern.c_str());
    return;
  }

  // If not found, add new handler
  handlers.push_back(URCHandler{pattern, cb});
  log_i("Registered URC handler for pattern: %s", pattern.c_str());
}

void AsyncURCHandler::unregisterEvent(const String& pattern) {
  auto _ = lock.guard();
  auto it = findPattern(pattern);

  if (it == handlers.end()) {
    log_w("No URC handler found for pattern: %s", pattern.c_str());
    return;
  }

  handlers.erase(it);
  log_i("Unregistered URC handler for pattern: %s", pattern.c_str());
}

void AsyncURCHandler::handleUnsolicitedResponse(const String& line) {
  std::vector<URCCallback> toInvoke;
  // Lock while gathering matching handlers
  {
    auto _ = lock.guard();
    // Gather all matching handlers. findMatch would return only the first match.
    for (const auto& h : handlers) {
      if (line.startsWith(h.pattern) && h.cb) { toInvoke.push_back(h.cb); }
    }
  }
  // Invoke outside the lock to avoid potential deadlocks
  for (auto& cb : toInvoke) { cb(line); }
}
