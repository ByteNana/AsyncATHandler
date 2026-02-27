#!/usr/bin/env bash
# Pre-push hook: Branch protection
#
# Rules:
#   master  — block ALL direct pushes (must use PR squash-merge)
#   rc-*    — block force pushes (non-fast-forward); normal pushes allowed
#   other   — allow everything

set -euo pipefail

BRANCH=$(git rev-parse --abbrev-ref HEAD)
REMOTE="${1:-origin}"

ZERO="0000000000000000000000000000000000000000"

# --- master: no direct pushes ---
if [ "$BRANCH" = "master" ]; then
  echo ""
  echo "  ERROR: Direct pushes to 'master' are not allowed."
  echo "  Use a PR with squash-merge instead."
  echo ""
  exit 1
fi

# --- rc-*: block force pushes ---
if [[ "$BRANCH" == rc-* ]]; then
  while read -r local_ref local_oid remote_ref remote_oid; do
    # Skip deletes
    if [ "$local_oid" = "$ZERO" ]; then
      continue
    fi

    # New branch push (remote_oid is zero) — always allow
    if [ "$remote_oid" = "$ZERO" ]; then
      continue
    fi

    # Check if remote_oid is an ancestor of local_oid (fast-forward check)
    if ! git merge-base --is-ancestor "$remote_oid" "$local_oid" 2>/dev/null; then
      echo ""
      echo "  ERROR: Force push to '${BRANCH}' is not allowed."
      echo "  The branch '${BRANCH}' is a release candidate — rebasing the remote is forbidden."
      echo "  Use a normal (fast-forward) push instead."
      echo ""
      exit 1
    fi
  done

  exit 0
fi

# --- Feature branches: allow everything ---
exit 0
