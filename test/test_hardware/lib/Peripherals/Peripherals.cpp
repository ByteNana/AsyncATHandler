#include "Peripherals.h"

void startPeripherals() {
  pinMode(ENERGY_SAVING_PIN, OUTPUT);
  digitalWrite(ENERGY_SAVING_PIN, HIGH);
  Muxs::begin();
  expander1.begin();
  expander2.begin();
  UartMux::begin();
}
