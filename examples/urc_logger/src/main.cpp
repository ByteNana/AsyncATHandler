#include <Arduino.h>
#include "AsyncATHandler.h"

AsyncATHandler handler;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  delay(200);

  Serial.println("[EXAMPLE] URC logger");

  handler.onURC([](const String& urc) {
    Serial.print("[URC] ");
    Serial.println(urc);
  });

  if (!handler.begin(Serial)) {
    Serial.println("Failed to start AsyncATHandler");
    return;
  }

  // Optional: enable some module URCs here if supported by your modem.
  // Example (may vary by module): network registration URCs
  // handler.sendSync("AT+CREG=2", 1000);
  // handler.sendSync("AT+CGREG=2", 1000);

  // Basic sanity check
  handler.sendSync("AT", 1000);
  Serial.println("URC logger ready. Waiting for unsolicited messages...");
}

void loop() {
  delay(1000);
}

