#include <Arduino.h>
#include "AsyncATHandler.h"

AsyncATHandler handler;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  delay(200);

  Serial.println("[EXAMPLE] AsyncATHandler basic");

  if (!handler.begin(Serial)) {
    Serial.println("Failed to start AsyncATHandler");
    return;
  }

  String response;
  bool ok = handler.sendSync("AT", response, 1000);
  Serial.print("sendSync(AT) => ");
  Serial.println(ok ? "OK" : "FAIL");
  if (response.length()) {
    Serial.println("Full response:");
    Serial.println(response);
  }
}

void loop() {
  delay(1000);
}

