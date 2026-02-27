ccdb := "compile_commands.json"
default_log_level := "3"

[group('build')]
setup log_level=default_log_level build_tests="OFF":
    @printf "\n\033[1;33m Setting up build environment\033[0m\n\n"
    cmake -E time cmake -DLOG_LEVEL={{log_level}} -DASYNCAT_HANDLER_BUILD_TESTS_NATIVE={{build_tests}} -B build

[group('build')]
build log_level=default_log_level: (setup log_level)
    @printf "\n\033[1;33m Building project\033[0m\n\n"
    cmake -E time cmake --build build

[group('build')]
clean:
    @printf "\n\033[1;33m Cleaning build artifacts\033[0m\n\n"
    rm -rf build {{ccdb}}
