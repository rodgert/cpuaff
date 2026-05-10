#!/usr/bin/env bash
#
# Wire up cpuaff's in-tree git hooks for this clone.
#
# Sets `core.hooksPath` to `tools/git-hooks/`, which currently provides:
#   * pre-commit — runs leakcheck and (if installed) gitleaks
#
# To revert: `git config --unset core.hooksPath`

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

git config core.hooksPath tools/git-hooks
chmod +x tools/git-hooks/pre-commit

echo "Hooks installed: core.hooksPath = tools/git-hooks"
echo "Run 'git config --unset core.hooksPath' to revert."
