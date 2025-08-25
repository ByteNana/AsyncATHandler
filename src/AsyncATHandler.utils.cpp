#include "AsyncATHandler.h"

#include "esp_log.h"

bool AsyncATHandler::isLineComplete(String& buffer) {
  if (buffer[0] == '>') {
    buffer.trim();
    buffer += "\r\n";  // Treat as complete line
    return true;  // Empty line
  }
  return buffer.endsWith("\r\n");
}

ResponseType AsyncATHandler::classifyLine(const String& line) {
  String trimmed = line;
  trimmed.trim();

  // Check for final responses first
  if (trimmed == "OK") return ResponseType::FINAL_OK;
  if (trimmed == "ERROR") return ResponseType::FINAL_ERROR;
  if (trimmed.startsWith("+CME ERROR:")) return ResponseType::FINAL_CME_ERROR;

  // Check for explicit URCs.
  if (trimmed.startsWith("+CMT:") ||    // SMS notification
      trimmed.startsWith("+CMTI:") ||   // SMS index notification
      trimmed.startsWith("+CLIP:") ||   // Calling line identification
      trimmed.startsWith("+CREG:") ||   // Network registration (when unsolicited)
      trimmed.startsWith("+CGREG:") ||  // GPRS registration (when unsolicited)
      trimmed.startsWith("+CEREG:") ||  // EPS registration (when unsolicited)
      trimmed.startsWith("+QIURC:") ||  // Quectel socket URC
      trimmed.startsWith("+QIOPEN:") || trimmed.startsWith("+QIRD:") ||
      trimmed.startsWith("+QICLOSE")) {  // Quectel open URC
    return ResponseType::UNSOLICITED;
  }

  // Default: intermediate data
  return ResponseType::INTERMEDIATE_DATA;
}

ATPromise* AsyncATHandler::findPromiseForResponse(const String& line) {
  if (pendingPromises.empty()) return nullptr;

  // Find the promise that is explicitly waiting for this line first
  for (auto& promise : pendingPromises) {
    if (promise && !promise->isCompleted()) {
      if (promise->matchesExpected(line)) { return promise.get(); }
    }
  }

  // Fallback: if no specific match, find the oldest incomplete promise
  for (auto& promise : pendingPromises) {
    if (promise && !promise->isCompleted()) { return promise.get(); }
  }
  return nullptr;
}
