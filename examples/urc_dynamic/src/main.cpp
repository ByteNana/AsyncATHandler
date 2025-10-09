#include <Arduino.h>

#include "AsyncATHandler.h"

AsyncATHandler handler;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  delay(200);

  Serial.println("[EXAMPLE] Dynamic URC registration");

  // Register some typical URCs dynamically
  handler.urc.registerEvent("RING", [](const String& urc) {
    Serial.print("[Event RING] ");
    Serial.println(urc);
  });
  handler.urc.registerEvent("+CLIP:", [](const String& urc) {
    Serial.print("[Event CLIP] ");
    Serial.println(urc);
  });
  handler.urc.registerEvent("+CMTI:", [](const String& urc) {
    Serial.print("[Event CMTI] ");
    Serial.println(urc);
  });

  if (!handler.begin(Serial)) {
    Serial.println("Failed to start AsyncATHandler");
    return;
  }

  // Sanity check
  handler.sendSync("AT", 1000);
  Serial.println("Waiting for URCs...");

  // Example of unregistering later
  // handler.urc.unregisterEvent("RING");
}

void loop() { delay(1000); }
