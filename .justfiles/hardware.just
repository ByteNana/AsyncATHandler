hardw_test_dir := "test"

[group('hardware')]
test-esp32:
    @printf "\033[1;33m Checking Build for ESP32...\033[0m\n"
    @find examples -mindepth 1 -maxdepth 1 -type d | while read -r d; do \
        test -f "$d/platformio.ini" || continue; \
        printf "\n\n\033[1;32m %s \033[0m\n\n" "$d"; \
        pio ci "$d/src/main.cpp" -c "$d/platformio.ini" --lib="."; \
    done
    @printf "\n\n\033[1;32m test/ \033[0m\n\n"
    cd {{hardw_test_dir}} && pio test -e ci --without-uploading --without-testing

[group('hardware')]
flash-esp32-test: test-esp32
    @printf "\033[1;33m Flashing hardware test to ESP32...\033[0m\n"
    pio test -d {{hardw_test_dir}} -e test

[group('hardware')]
test-remote-esp32:
    pio remote --agent IOT01 test -d {{hardw_test_dir}} -e test --upload-port /dev/ttyCarelIR33 --test-port /dev/ttyCarelIR33 -vv
