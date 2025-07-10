.PHONY: all clean build setup format check-format test

SRC_DIRS := src test
EXTENSIONS := c cpp h hpp cc cxx hxx hh
BUILD_DIR := build
CCDB := compile_commands.json

EXT_FILTER := $(foreach ext,$(EXTENSIONS), -name '*.$(ext)' -o)
EXT_FILTER := $(wordlist 1,$(shell echo $$(($(words $(EXT_FILTER)) - 1))),$(EXT_FILTER))

FILES := $(shell find $(SRC_DIRS) -type f \( $(EXT_FILTER) \))

all: build symlink

.symlink:
	@echo "🔗 Symlinking compile_commands.json"
	ln -sf $(BUILD_DIR)/$(CCDB) .

setup: .symlink
	@echo "🔧 Running cmake..."
	cmake -B$(BUILD_DIR) -DNATIVE_BUILD=ON

build: setup
	@echo "🔨 Building..."
	cmake --build $(BUILD_DIR)

# New test command
test: build
	@echo "🧪 Running unit tests..."
	ctest --output-on-failure -j$(sysctl -n hw.ncpu) --test-dir build

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
