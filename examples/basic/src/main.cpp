#include <Arduino.h>

#include "AsyncATHandler.h"

AsyncATHandler handler;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("[ESP32] Initializing AsyncATHandler...");
  handler.begin(Serial);

  String response;
  Serial.println("Sending 'AT' command...");
  bool ok = handler.sendCommand("AT", response, "OK", 1000);

  if (ok) {
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.println("AT command failed or timed out.");
  }
}

void loop() {}
