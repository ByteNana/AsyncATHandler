.PHONY: all clean build setup format check-format test esp32

# === CONFIG ===
SRC_DIRS := src test examples
EXTENSIONS := c cpp h hpp cc cxx hxx hh
BUILD_DIR := build
CCDB := compile_commands.json

# Support for replacing spaces
space := $(empty) $(empty)

# === ARGUMENT CAPTURE ===
# Get the log level from command line arguments
LOG_LEVEL_ARG := $(filter-out test build setup all clean format check-format esp32 esp32-test, $(MAKECMDGOALS))

# Set default log level if no argument provided
LOG_LEVEL := $(if $(LOG_LEVEL_ARG),$(LOG_LEVEL_ARG),3)
# === FILE COLLECTION ===
FILES := $(shell git ls-files --cached --others --exclude-standard $(SRC_DIRS) | grep -E '\.($(subst $(space),|,$(EXTENSIONS)))$$')

# === TARGETS ===

all: build

setup:
	@echo "🔧 Running cmake..."
	cmake -DLOG_LEVEL=$(LOG_LEVEL) -B$(BUILD_DIR)

build: setup
	@echo "🔨 Building...LOG_LEVEL=$(LOG_LEVEL)"
	cmake --build $(BUILD_DIR)

test: build
	@echo "🧪 Running unit tests..."
	GTEST_COLOR=1 ctest --output-on-failure --test-dir build -V

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

%:
	@:
