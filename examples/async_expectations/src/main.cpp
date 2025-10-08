#include <Arduino.h>

#include "AsyncATHandler.h"

AsyncATHandler handler;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  delay(200);

  Serial.println("[EXAMPLE] Async with expectations");

  if (!handler.begin(Serial)) {
    Serial.println("Failed to start AsyncATHandler");
    return;
  }

  // Example: query network registration and expect a +CREG response before OK
  // Note: exact commands/URCs may vary by module/firmware.
  ATPromise* p = handler.sendCommand("AT+CREG?");
  if (!p) {
    Serial.println("Failed to create promise");
    return;
  }

  p->expect("+CREG:")->expect("OK")->timeout(5000);

  bool completed = p->wait();
  if (!completed) {
    Serial.println("Promise timed out waiting for expectations");
  } else if (p->getResponse()) {
    Serial.print("Success: ");
    Serial.println(p->getResponse()->isSuccess() ? "YES" : "NO");

    Serial.println("Data lines only:");
    for (const auto& line : p->getResponse()->getDataLines()) { Serial.println(line); }

    Serial.println("Full response:");
    Serial.println(p->getResponse()->getFullResponse());
  }

  auto owned = handler.popCompletedPromise(p->getId());
}

void loop() { delay(1000); }
