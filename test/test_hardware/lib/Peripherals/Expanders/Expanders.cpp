#include "Expanders.h"

#include <map>

Expanders::Expanders(ExpanderAddress address, const char* name)
    : Muxs(MuxAddress::ONBOARD_MODULES),
      expander_address(static_cast<int>(address)),
      pins((address == ExpanderAddress::MCP1) ? MCP1_PINS : MCP2_PINS) {}

bool Expanders::begin() {
  setMuxAddress();
  log_v("Expanders::begin");
  if (!mcp.begin_I2C(expander_address)) {
    log_e("Could not find MCP23X17 at address 0x%x", expander_address);
    return false;
  }

  for (MCP_PinConfig pin : pins) {
    mcp.pinMode(pin.pin, pin.mode);
    log_d("Setting '%s' pinMode %d", pin.name, pin.pin);
    if (pin.initialState == -1) { continue; }
    log_d("Setting '%s' initial state %d", pin.name, pin.initialState);
    mcp.digitalWrite(pin.pin, pin.initialState);
  }

  checkConnecteds();

  return true;
}

void Expanders::pinMode(int pin, int mode) {
  log_v("Expanders::pinMode: pin: %d value: %d\n", pin, mode);
  setMuxAddress();
  mcp.pinMode(pin, mode);
}

void Expanders::digitalWrite(int pin, int value) {
  log_v("Expanders::digitalWrite: pin: %d value: %d\n", pin, value);
  setMuxAddress();
  mcp.digitalWrite(pin, value);
}

uint8_t Expanders::digitalRead(int pin) {
  log_v("Expanders::digitalRead: pin: %d\n", pin);
  setMuxAddress();
  return mcp.digitalRead(pin);
}

void Expanders::checkConnecteds() {
  setMuxAddress();
  log_v("Expanders::checkConnecteds");
  for (MCP_PinConfig pin : pins) {
    if (pin.initialState != -1) { continue; }
    log_i(
        "'%s'(pin %d): %s\n", pin.name, pin.pin,
        !mcp.digitalRead(pin.pin) ? "Connected" : "Not connected");
  }
}

Expanders expander1(ExpanderAddress::MCP1, "MCP1");
Expanders expander2(ExpanderAddress::MCP2, "MCP2");
