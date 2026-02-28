# AsyncATHandler

[![CI](https://github.com/ByteNana/AsyncATHandler/actions/workflows/ci.yml/badge.svg)](https://github.com/ByteNana/AsyncATHandler/actions/workflows/ci.yml)
[![Formatting](https://github.com/ByteNana/AsyncATHandler/actions/workflows/formatting.yml/badge.svg)](https://github.com/ByteNana/AsyncATHandler/actions/workflows/formatting.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-compatible-orange)](https://platformio.org)
[![ESP32](https://img.shields.io/badge/platform-ESP32-green)](https://www.espressif.com/en/products/socs/esp32)

AsyncATHandler is a C++ library for robust, asynchronous AT command communication on ESP32/Arduino systems. It provides safe command queueing, response parsing, and unsolicited response code (URC) handling — all driven by a dedicated FreeRTOS reader task.

## Features

- Asynchronous AT command processing with a dedicated reader task
- Unsolicited Response Code (URC) handling via user-provided callbacks
- Promise-style async API with convenient synchronous helper (`sendSync`)
- Native unit tests using GoogleTest with FreeRTOS/Arduino shim
- ESP32/Arduino support via PlatformIO

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Development](#development)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [Changelog](#changelog)
- [License](#license)

## Installation

### PlatformIO (recommended)

Add the library to your `platformio.ini`:

```ini
lib_deps = https://github.com/ByteNana/AsyncATHandler.git
```

### Manual

Clone the repository into your project's library directory:

```bash
git clone https://github.com/ByteNana/AsyncATHandler.git
```

## Quick Start

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

## Development

<details>
<summary>Development guide</summary>

### Prerequisites

- cmake >= 3.15
- C++17 compiler (e.g., `g++` / `clang++`)
- [just](https://github.com/casey/just)
- clang-format (optional)
- PlatformIO CLI (optional)

### Commands

| Command          | Description         |
| ---------------- | ------------------- |
| `just build`     | Build native        |
| `just test`      | Run tests           |
| `just format`    | Format code         |
| `just check`     | Check formatting    |
| `just changelog` | Generate changelog  |

See [CONTRIBUTING.md](CONTRIBUTING.md) for full details.

</details>

## Documentation

See the [docs/README.md](docs/README.md) for full documentation.

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on how to get started, coding standards, and the pull request process.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for a list of notable changes to this project.

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.
