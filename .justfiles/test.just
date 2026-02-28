[group('test')]
test log_level=default_log_level: (setup log_level "ON")
    @printf "\n\033[1;33m Building and Running Unit Tests\033[0m\n\n"
    cmake -E time cmake --build build
    GTEST_COLOR=1 ctest --output-on-failure --test-dir build -V
