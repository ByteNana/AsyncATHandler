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

[group('quality')]
changelog:
    #!/usr/bin/env bash
    set -euo pipefail
    repo="https://github.com/ByteNana/AsyncATHandler"
    printf "\n\033[1;33m Generating changelog\033[0m\n\n"
    export GITHUB_TOKEN=$(gh auth token 2>/dev/null || echo "")
    branch=$(git branch --show-current)
    if [[ "$branch" =~ ^rc-(.+)$ ]]; then
        version="v${BASH_REMATCH[1]}"
        printf "  On release branch — tagging as \033[1m%s\033[0m\n\n" "$version"
        git-cliff --config .config/cliff.toml --tag "$version" -o CHANGELOG.md
    else
        git-cliff --config .config/cliff.toml -o CHANGELOG.md
    fi
    # Post-process: replace commit hash links with PR links where the
    # parent commit is a squash-merge with (#N) in the title.
    while IFS= read -r hash; do
        title=$(git log -1 --format="%s" "$hash" 2>/dev/null) || continue
        if [[ "$title" =~ \(#([0-9]+)\) ]]; then
            pr="${BASH_REMATCH[1]}"
            sed -i '' "s|\[${hash}\](${repo}/commit/${hash})|[#${pr}](${repo}/pull/${pr})|g" CHANGELOG.md
        fi
    done < <(grep -oE '\[[a-f0-9]{7}\]' CHANGELOG.md | tr -d '[]' | sort -u)
    printf "\033[1;32m CHANGELOG.md updated\033[0m\n"
