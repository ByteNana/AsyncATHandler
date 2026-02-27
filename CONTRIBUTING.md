# Contributing to AsyncATHandler

## Prerequisites

| Tool | Version | Install |
|---|---|---|
| CMake | 3.15+ | [cmake.org](https://cmake.org/download/) |
| C++11 compiler | GCC / Clang / MSVC | — |
| clang-format | any recent | `brew install clang-format` |
| just | any recent | `brew install just` |
| Lefthook | any recent | `brew install lefthook` |
| bump-my-version | any recent | `pip install bump-my-version` |

## Setup

```bash
git clone <repo-url> && cd AsyncATHandler
just setup          # installs Lefthook hooks + configures CMake
```

## Branching Model

| Branch | Purpose | Push rules |
|---|---|---|
| `master` | Stable releases only | No direct pushes — PR squash-merge only |
| `rc-X.Y.Z` | Release candidate for version X.Y.Z | No force pushes; normal pushes allowed |
| `feature/*`, other | Feature development | No restrictions |

## Development Workflow

1. **Branch** from the current `rc-*` branch (or `master` if no rc exists):
   ```bash
   git checkout rc-0.1.1
   git checkout -b feature/my-change
   ```

2. **Code** — make your changes.

3. **Format** before committing:
   ```bash
   just format        # auto-fix formatting
   just check         # verify (same check the hook runs)
   ```

4. **Commit and push**:
   ```bash
   git add <files>
   git commit -m "feat: description of change"
   git push -u origin feature/my-change
   ```

5. **Open a PR** into the `rc-*` branch.

## Version Bumping

Version is managed by [bump-my-version](https://github.com/callowayproject/bump-my-version) and stored in two places in `CMakeLists.txt`:

- `project(... VERSION X.Y.Z ...)` — the major.minor.patch
- `set(PROJECT_VERSION_SUFIX "-rc.N")` — the pre-release suffix

### Commands

**Increment the release-candidate number** (e.g. `rc.1` → `rc.2`):

```bash
bump-my-version bump pre_n --dry-run --verbose   # preview
bump-my-version bump pre_n                        # apply
```

**Promote to final release** (drops the `-rc.N` suffix):

```bash
bump-my-version bump patch    # 0.1.1-rc.N → 0.1.1
bump-my-version bump minor    # 0.1.1-rc.N → 0.2.0
bump-my-version bump major    # 0.1.1-rc.N → 1.0.0
```

> `commit = false` and `tag = false` are set in `.bumpversion.toml` — you review and commit the changes yourself. Tagging is handled automatically by CI on merge to `master`.

### Check the current version

```bash
bump-my-version show current_version
bump-my-version show-bump
```

## Release Process

1. **Create an rc branch** from `master`:
   ```bash
   git checkout master && git pull
   git checkout -b rc-0.2.0
   ```

2. **Bump the version** to match the branch:
   ```bash
   bump-my-version bump minor          # updates CMakeLists.txt
   git add CMakeLists.txt
   git commit -m "chore: bump version to 0.2.0-rc.1"
   git push -u origin rc-0.2.0
   ```

3. **Develop on feature branches**, merge PRs into `rc-*`. Bump `pre_n` as needed for each RC iteration.

4. **When ready to release**, promote to final:
   ```bash
   bump-my-version bump patch          # drops -rc.N suffix
   git add CMakeLists.txt
   git commit -m "chore: release 0.2.0"
   ```

5. **Open a PR** from `rc-0.2.0` → `master`. Squash-merge it.
   > The **Version Check** CI job will fail if the `vX.Y.Z` tag already exists — bump the version in `CMakeLists.txt` before merging.

6. The **auto-tag** GitHub Action creates a `vX.Y.Z` tag and GitHub Release automatically.

## Git Hooks

Lefthook enforces the following hooks:

| Hook | Check | What it does |
|---|---|---|
| `pre-commit` | `format-check` | Runs `just check` on staged `.c/.cpp/.h/.hpp` files |
| `pre-push` | `branch-protection` | Blocks direct pushes to `master`; blocks force pushes to `rc-*` |
| `pre-push` | `version-validation` | On `rc-*` branches, ensures `CMakeLists.txt` VERSION matches the branch name |

### Emergency bypass

```bash
git commit --no-verify    # skip pre-commit hooks
git push --no-verify      # skip pre-push hooks
```

Use sparingly and only when you understand why the hook is failing.

## Code Style

- All C/C++ source files are formatted with **clang-format**
- Run `just format` to auto-format all files in `src/`, `test/`, and `examples/`
- Run `just check` to verify formatting without modifying files
- The `pre-commit` hook runs `just check` automatically
