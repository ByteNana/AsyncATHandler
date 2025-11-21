# ==============================================================================
# Variables
# ==============================================================================

SRC_DIRS := src test examples
EXTENSIONS := c cpp h hpp cc cxx hxx hh
BUILD_DIR := build
CCDB := compile_commands.json
HARDW_TEST_DIR := test/test_hardware

# ==============================================================================
# Argument Capture
# ==============================================================================

# Get the log level from command line arguments (exclude non-level targets)
LOG_LEVEL_ARG := $(filter-out \
  help build setup all clean format check check-format \
  test-esp32 flash-esp32-test esp32 esp32-test \
  build-native test-native build-test-native setup-native setup-test-native, \
  $(MAKECMDGOALS))

# Set default log level if no argument provided
LOG_LEVEL := $(if $(LOG_LEVEL_ARG),$(LOG_LEVEL_ARG),3)

# ==============================================================================
# Make Configuration
# ==============================================================================

# Discover all example projects (directories containing platformio.ini)
EXAMPLE_DIRS := $(shell find examples -mindepth 1 -maxdepth 1 -type d -exec sh -c 'test -f "$$1/platformio.ini" && echo "$$1"' _ {} \;)

# Suppress recursive make directory noise
MAKEFLAGS += --no-print-directory

# Set default target to 'help'
.DEFAULT_GOAL := help
