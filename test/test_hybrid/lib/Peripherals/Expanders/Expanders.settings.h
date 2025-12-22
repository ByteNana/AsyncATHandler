#pragma once

/*
 * Expanders.settings.h
 *
 * This file contains the configuration for the MCP23X17 I/O expanders.
 * It defines the pin configurations for two MCP23X17 expanders (MCP1 and MCP2),
 * including their names, pin numbers, modes, and initial states.
 *
 */

#include <Arduino.h>

#include <vector>

#include "../Pins.h"

/* * MCP Pin Configurations
 * name: Name of the pin
 * pin: Pin number on the MCP
 * mode: Pin mode (INPUT, OUTPUT, INPUT_PULLUP)
 * initialState: Initial state for OUTPUT pins (-1 for no initial state)
 */
struct MCP_PinConfig {
  const char* name;
  int pin;
  int mode;
  int initialState;
};

// Expander I2C addresses
enum class ExpanderAddress {
  MCP1 = MCP1_ADDRESS,
  MCP2 = MCP2_ADDRESS,
};

/* * Expander Pin Configurations
 * MCP1: MCP1_ADDRESS
 * MCP2: MCP2_ADDRESS
 * Each pin is defined with its name, pin number, mode, and initial state.
 */
const std::vector<MCP_PinConfig> MCP1_PINS = {{
    {"Buzzer", MCP1_buzzer, OUTPUT, LOW},
    {"Button", MCP1_add_butt, INPUT, HIGH},
    {"GSM Reset", MCP1_gsm_reset, OUTPUT, LOW},
    {"GSM Power Key", MCP1_gsm_pwrkey, OUTPUT, LOW},
    {"RS485 Status", MCP1_rs485_status, OUTPUT, LOW},
    {"UART MUX A2", MCP1_uart_mux_a2, OUTPUT, LOW},
    {"UART MUX A1", MCP1_uart_mux_a1, OUTPUT, LOW},
    {"UART MUX A0", MCP1_uart_mux_a0, OUTPUT, LOW},
    {"UART MUX Enable", MCP1_uart_mux_en, OUTPUT, LOW},
    {"LED1", MCP1_add_led1, OUTPUT, LOW},
    {"LED2", MCP1_add_led2, OUTPUT, LOW},
    {"Ext MUX A2", MCP1_ext_mux_a2, OUTPUT, LOW},
    {"Ext MUX A1", MCP1_ext_mux_a1, OUTPUT, LOW},
    {"Ext MUX A0", MCP1_ext_mux_a0, OUTPUT, LOW},
    {"Ext MUX Enable", MCP1_ext_mux_en, OUTPUT, HIGH},
    {"GPS Reset", MCP1_gps_rst, OUTPUT, LOW},
}};

const std::vector<MCP_PinConfig> MCP2_PINS = {{
    {"POE Power Detection", MCP2_poe_power_detection, INPUT_PULLUP, -1},
    {"Relay", MCP2_relay, OUTPUT, LOW},
    {"Energy Module Detection", MCP2_energy_meter_det, INPUT_PULLUP, -1},
    {"LoRa Comms Module Detection", MCP2_lora_det, INPUT_PULLUP, -1},
    {"Ethernet Comms Module Detection", MCP2_eth_det, INPUT_PULLUP, -1},
    {"GSM EH915U Comms Module Detection", MCP2_gsm_det, INPUT_PULLUP, -1},
    {"Battery/Solar Module Detection", MCP2_solar_det, INPUT_PULLUP, -1},
    {"Module IO Interface Detection", MCP2_int_ext_det, INPUT_PULLUP, -1},
}};
