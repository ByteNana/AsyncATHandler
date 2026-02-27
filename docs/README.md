# Documentation Index

Welcome to the AsyncATHandler documentation. This project targets ESP32 (Arduino/PlatformIO) with a native test harness for development.

## Getting Started
- Quick start (ESP32): see the README’s "Quick Start" section.
- PlatformIO hardware tests: `test/` folder.

## Concepts
- AsyncATHandler: coordinator and reader task – `./development/AsyncATHandler.md`
- ATPromise: async/sync command completion – `./development/ATPromise.md`
- ATResponse: response accumulation and status – `./development/ATResponse.md`

## How-To Guides
- Run native tests: `just build && just test`
- Check formatting: `just check`; auto-format: `just format`
- ESP32 CI-style build: `just test-esp32`
- Flash ESP32 hardware tests: `just flash-esp32-test`

