#include <Arduino.h>

#include "AsyncATHandler.h"

AsyncATHandler handler;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  delay(200);

  Serial.println("[EXAMPLE] Async basic using ATPromise");

  if (!handler.begin(Serial)) {
    Serial.println("Failed to start AsyncATHandler");
    return;
  }

  // Send a command asynchronously and wait on its promise
  ATPromise* p = handler.sendCommand("AT+GMR");  // Device info (varies by module)
  if (!p) {
    Serial.println("Failed to create promise");
    return;
  }

  p->timeout(3000);  // 3s timeout
  bool completed = p->wait();
  if (!completed) {
    Serial.println("Promise timed out");
  } else if (p->getResponse()) {
    Serial.print("Success: ");
    Serial.println(p->getResponse()->isSuccess() ? "YES" : "NO");
    Serial.println("Full response:");
    Serial.println(p->getResponse()->getFullResponse());
  }

  // Take ownership and let it destruct when `owned` goes out of scope
  auto owned = handler.popCompletedPromise(p->getId());
}

void loop() { delay(1000); }
