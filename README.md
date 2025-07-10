# AsyncATHandler

AsyncATHandler is a C++ library for robustly handling AT command communication with ESP32/Arduino systems. It supports asynchronous AT command dispatching, safe command queueing, response parsing, and native unit testing with FreeRTOS/Arduino mocks.

## Features
- Asynchronous AT command processing and response
- Unsolicited response handling
- Queue-based command/result management
- Written in portable modern C++ (C++17)
- Native test suite using GoogleTest and GoogleMock (mocked Arduino/FreeRTOS)

## Directory Structure
- `src/` — Implementation of AsyncATHandler
- `test/` — Unit tests, mocks, and native test code
- `examples/` — Example Arduino sketches
- `Makefile`, `CMakeLists.txt` — C++ build and test infrastructure

## Running Native Tests (Linux/macOS)
The following commands build and run the C++ unit tests:

```sh
# Install dependencies (if needed):
sudo apt-get install clang-format cmake g++ make

# Build and run all tests:
make test
# OR equivalent:
cmake -Bbuild -DNATIVE_BUILD=ON
cmake --build build
ctest --output-on-failure --test-dir build
```

## Running Hardware Tests (ESP32)
The following commands build and upload the example sketch to an ESP32 board:
```sh
make esp32
```

## Formatting
Format all code with:
```sh
make format
```
Check code format (dry-run, CI enforced):
```sh
make check-format
```

---
For ESP32/Arduino deployment, see `examples/basic` and use PlatformIO or Arduino IDE as usual.
