# ==============================================================================
# Development Targets
#
# Description: This Makefile provides targets
# ==============================================================================
.PHONY: test test-esp32 flash-esp32-test

#=============================================================================
# Native Targets
#=============================================================================

## Run unit tests
test: build
	@echo "🧪 Running unit tests..."
	GTEST_COLOR=1 ctest --output-on-failure --test-dir build -V

#=============================================================================
# Hardware Targets
#=============================================================================

## Test ESP32 build
test-esp32:
	@echo "🔨 Building for ESP32..."
	@pio ci ${HARDW_TEST_DIR}/src/main.cpp  -c ${HARDW_TEST_DIR}/platformio.ini -e ci --lib="."
	@pio test -d ${HARDW_TEST_DIR} -e test --without-uploading --without-testing -vv

## Flash and run ESP32 hardware test
flash-esp32-test: test-esp32
	@echo "🚀 Flashing hardware test"
	@pio test -d ${HARDW_TEST_DIR} -e test
