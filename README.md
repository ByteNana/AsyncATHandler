# AsyncATHandler

AsyncATHandler is a C++ library for robustly handling AT command communication with ESP32/Arduino systems. It supports asynchronous AT command dispatching, safe command queueing and response parsing.

## Overview & Main Features
- Asynchronous AT command processing with a dedicated reader task.
- Unsolicited response (URC) handling via user-provided callback.
- Promise-style async API plus a convenient synchronous helper (`sendSync`).
- Native tests using GoogleTest (fetched via CMake) and an in-repo FreeRTOS/Arduino shim.
- ESP32/Arduino support via PlatformIO.

## Setup & Build
- Prerequisites (native): `cmake >= 3.15`, a C++17 compiler (e.g., `g++`/`clang++`), `just`.
- Optional tools: `clang-format` for formatting checks; PlatformIO CLI (`pio`) for ESP32 builds.
- Build (native): `just build`
  - Optional log level (0–5): `just build 3` (defaults to 3)
- Clean artifacts: `just clean`
- Format code: `just format`
- Check formatting: `just check`

## Quick Start (ESP32, PlatformIO)
```cpp
#include <Arduino.h>
#include "AsyncATHandler.h"

AsyncATHandler handler;

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  handler.begin(Serial);

  String response;
  bool ok = handler.sendSync("AT", response, 1000);
  if (ok) {
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.println("AT command failed or timed out.");
  }
}

void loop() {}
```

## How To Run Tests
- Native unit tests (GoogleTest): `just test`
  - Uses CTest to run all tests in `test/test`.
- ESP32 build sanity (no upload/run): `just test-esp32`
- ESP32 flash and run hardware test: `just flash-esp32-test`

## Documentation
- Docs index: `docs/README.md`

- Core components:
  - `docs/development/AsyncATHandler.md`
  - `docs/development/ATPromise.md`
  - `docs/development/ATResponse.md`

Notes
- Native tests fetch GoogleTest/Unity via CMake’s FetchContent on first configure.
- Examples are PlatformIO-only; place PlatformIO example projects under `examples/`.
