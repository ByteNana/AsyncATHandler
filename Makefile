.PHONY: all clean build setup format check-format test esp32

# === CONFIG ===
SRC_DIRS := src test examples
EXTENSIONS := c cpp h hpp cc cxx hxx hh
BUILD_DIR := build
CCDB := compile_commands.json

# Support for replacing spaces
space := $(empty) $(empty)

# === FILE COLLECTION ===
FILES := $(shell git ls-files --cached --others --exclude-standard $(SRC_DIRS) | grep -E '\.($(subst $(space),|,$(EXTENSIONS)))$$')

# === TARGETS ===

all: build

setup:
	@echo "🔧 Running cmake..."
	cmake -B$(BUILD_DIR)

build: setup
	@echo "🔨 Building..."
	cmake --build $(BUILD_DIR)

test: build
	@echo "🧪 Running unit tests..."
	ctest --output-on-failure -j$(shell sysctl -n hw.ncpu 2>/dev/null || nproc) --test-dir build

esp32:
	@echo "🔨 Building for ESP32..."
	pio ci  examples/basic/src/main.cpp --lib="." --board=esp32dev

esp32-test: esp32
	@echo "🚀 Flashing hardware test"
	pio test -d examples/basic

clean:
	@echo "🧹 Cleaning up..."
	rm -rf $(BUILD_DIR) $(CCDB)

format:
	@echo "🗄️ Running clang-format on:"
	@for file in $(FILES); do \
		echo " - $$file"; \
		clang-format -i $$file; \
	done

check-format:
	@echo "🔍 Checking formatting (dry run):"
	@FAILED=0; \
	for file in $(FILES); do \
		echo " - $$file"; \
		if ! clang-format -n --Werror $$file; then FAILED=1; fi; \
	done; \
	exit $$FAILED

