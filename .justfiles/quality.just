[group('quality')]
format:
    @printf "\n\033[1;33m Formatting source files\033[0m\n\n"
    clang-format -i $(git ls-files '*.cpp' '*.h' '*.c' '*.hpp')
    @printf "\033[1;32m All files properly formatted\033[0m\n"

[group('quality')]
check:
    @printf "\n\033[1;33m Checking formatting\033[0m\n\n"
    clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h')
    @printf "\033[1;32m All files passed\033[0m\n"
