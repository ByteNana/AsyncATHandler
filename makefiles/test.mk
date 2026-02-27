# ==============================================================================
# Development Targets
#
# Description: This Makefile provides targets
# ==============================================================================
.PHONY: test test-esp32 flash-esp32-test test-esp32-examples

#=============================================================================
# Native Targets
#=============================================================================

## Run unit tests
test: build
	@printf "\n\033[1;33m🧪 Running Unit Tests\033[0m\n\n"
	@echo "🧪 Running unit tests..."
	GTEST_COLOR=1 ctest --output-on-failure --test-dir build -V

#=============================================================================
# Hardware Targets
#=============================================================================

## Test ESP32 build
test-esp32:
	@printf  "\033[1;33m🔨 Checking Build for ESP32...\033[0m\n"
	@for d in $(EXAMPLE_DIRS); do \
	  printf "\n\n\033[1;32m▶ $$d \033[0m\n\n"; \
	  pio ci $$d/src/main.cpp -c $$d/platformio.ini --lib="."; \
	done
	@printf "\n\n\033[1;32m▶ test/ \033[0m\n\n"
	@cd ${HARDW_TEST_DIR} && pio test -e ci --without-uploading --without-testing

## Flash and run ESP32 hardware test
flash-esp32-test: test-esp32
	@printf "\033[1;33m🚀 Flashing hardware test to ESP32...\033[0m\n"
	@pio test -d ${HARDW_TEST_DIR} -e test

test-remote-esp32:
	pio remote --agent IOT01 test -d ${HARDW_TEST_DIR} -e test --upload-port /dev/ttyCarelIR33 --test-port /dev/ttyCarelIR33 -vv

