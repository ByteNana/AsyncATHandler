#pragma once

#define MUX_ONBOARD_EN 13
#define MUX_ONBOARD_A0 25
#define MUX_ONBOARD_A1 26
#define MUX_ONBOARD_A2 27

#define MCP1_ADDRESS 0x26
#define MCP2_ADDRESS 0x25

#define MCP1_buzzer 0
#define MCP1_add_butt 1
#define MCP1_gsm_reset 2
#define MCP1_gsm_pwrkey 3
#define MCP1_rs485_status 4
#define MCP1_uart_mux_a2 5
#define MCP1_uart_mux_a1 6
#define MCP1_uart_mux_a0 7
#define MCP1_uart_mux_en 8
#define MCP1_add_led1 9
#define MCP1_add_led2 10
#define MCP1_ext_mux_a2 11
#define MCP1_ext_mux_a1 12
#define MCP1_ext_mux_a0 13
#define MCP1_ext_mux_en 14
#define MCP1_gps_rst 15

#define MCP2_poe_power_detection 0
#define MCP2_relay 1
#define MCP2_energy_meter_det 2
#define MCP2_lora_det 3
#define MCP2_eth_det 4
#define MCP2_gsm_det 5
#define MCP2_solar_det 6
#define MCP2_int_ext_det 15

#define ENERGY_SAVING_PIN 12
