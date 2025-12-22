#include "Mux.h"

void Muxs::begin() {
  log_v("Muxs::begin");
  pinMode(MuxPins::EN, OUTPUT);
  digitalWrite(MuxPins::EN, LOW);
  for (int pin : MUX_ADDRESS_PINS) { pinMode(pin, OUTPUT); }
}

void Muxs::setEnable(bool enable) { digitalWrite(MuxPins::EN, enable ? LOW : HIGH); }

void Muxs::setMuxAddress() {
  int address = static_cast<int>(mux_address);
  log_v("Muxs::setMuxAddress: Address: %d\n", address);
  for (int i = 0; i < MUX_ADDRESS_TABLE[address].size(); i++) {
    log_v(
        "Muxs::setMuxAddress: pin: %d value: %d\n", MUX_ADDRESS_PINS[i],
        MUX_ADDRESS_TABLE[address][i]);
    digitalWrite(MUX_ADDRESS_PINS[i], MUX_ADDRESS_TABLE[address][i]);
  }
}
