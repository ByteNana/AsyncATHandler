#!/usr/bin/env bash
# Pre-push hook: Version validation for rc-* branches
#
# Ensures the VERSION in CMakeLists.txt matches the rc-* branch name.
#   Branch rc-1.2.3 → CMakeLists.txt must contain: project(... VERSION 1.2.3 ...)
#
# The -rc.N suffix (PROJECT_VERSION_SUFIX) is allowed and NOT checked.

set -euo pipefail

BRANCH=$(git rev-parse --abbrev-ref HEAD)

# Only validate on rc-* branches
if [[ "$BRANCH" != rc-* ]]; then
  exit 0
fi

# Extract expected version from branch name: rc-X.Y.Z → X.Y.Z
EXPECTED_VERSION="${BRANCH#rc-}"

# Extract actual version from CMakeLists.txt
CMAKE_FILE="CMakeLists.txt"

if [ ! -f "$CMAKE_FILE" ]; then
  echo ""
  echo "  ERROR: ${CMAKE_FILE} not found."
  echo ""
  exit 1
fi

# Match: project(... VERSION X.Y.Z ...)
PROJECT_LINE=$(grep -i 'project' "$CMAKE_FILE" | grep 'VERSION' | head -1)
ACTUAL_VERSION=""
VERSION_REGEX='VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+)'
if [[ "$PROJECT_LINE" =~ $VERSION_REGEX ]]; then
  ACTUAL_VERSION="${BASH_REMATCH[1]}"
fi

if [ -z "$ACTUAL_VERSION" ]; then
  echo ""
  echo "  ERROR: Could not extract VERSION from ${CMAKE_FILE}."
  echo "  Expected pattern: project(... VERSION X.Y.Z ...)"
  echo ""
  exit 1
fi

if [ "$ACTUAL_VERSION" != "$EXPECTED_VERSION" ]; then
  echo ""
  echo "  ERROR: Version mismatch!"
  echo ""
  echo "  Branch:           ${BRANCH}"
  echo "  Expected VERSION: ${EXPECTED_VERSION}"
  echo "  Actual VERSION:   ${ACTUAL_VERSION}  (from ${CMAKE_FILE})"
  echo ""
  echo "  Update the VERSION in ${CMAKE_FILE} to match the branch name."
  echo ""
  exit 1
fi

exit 0
