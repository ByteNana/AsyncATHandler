[group('test')]
test log_level=default_log_level: (build log_level)
    @printf "\n\033[1;33m Running Unit Tests\033[0m\n\n"
    GTEST_COLOR=1 ctest --output-on-failure --test-dir build -V
