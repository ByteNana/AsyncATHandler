# Documentation Index

Welcome to the AsyncATHandler documentation. This project targets ESP32 (Arduino/PlatformIO) with a native test harness for development.

## Getting Started
- Quick start (ESP32): see the README’s "Quick Start" section.
- PlatformIO hardware tests: `test/test_hardware` folder.

## Concepts
- AsyncATHandler: coordinator and reader task – `./development/AsyncATHandler.md`
- ATPromise: async/sync command completion – `./development/ATPromise.md`
- ATResponse: response accumulation and status – `./development/ATResponse.md`

## How-To Guides
- Run native tests: `make build && make test`
- Check formatting: `make check`; auto-format: `make format`
- ESP32 CI-style build: `make test-esp32`
- Flash ESP32 hardware tests: `make flash-esp32-test`

