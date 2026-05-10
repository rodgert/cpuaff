#!/usr/bin/env bash
#
# leakcheck — scan source files for local paths and downstream-internal
# references that should not appear in this public OSS repo.
#
# Usage:
#   tools/leakcheck.sh             # scan every tracked file (default)
#   tools/leakcheck.sh --staged    # scan the staged contents (pre-commit)
#
# Patterns: tools/leakcheck-patterns.txt
# Allowlist: edit the `allow` array below.

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

mode="${1:-tracked}"
patterns_file="tools/leakcheck-patterns.txt"

if [ ! -f "$patterns_file" ]; then
    echo "leakcheck: missing $patterns_file" >&2
    exit 2
fi

# Files allowed to contain pattern-matching content (own definitions,
# license, changelog, the scripts that implement the check).
allow=(
    "LICENSE"
    "CHANGELOG.md"
    "CONTRIBUTING.md"
    "tools/leakcheck.sh"
    "tools/leakcheck-patterns.txt"
    "tools/setup-hooks.sh"
    "tools/git-hooks/pre-commit"
)

is_allowed() {
    local f="$1"
    local a
    for a in "${allow[@]}"; do
        [ "$f" = "$a" ] && return 0
    done
    return 1
}

case "$mode" in
    tracked)
        mapfile -t files < <(git ls-files)
        ;;
    --staged|staged)
        mapfile -t files < <(git diff --cached --name-only --diff-filter=ACM)
        ;;
    *)
        echo "leakcheck: unknown mode '$mode' (expected: tracked | --staged)" >&2
        exit 2
        ;;
esac

mapfile -t patterns < <(grep -Ev '^[[:space:]]*(#|$)' "$patterns_file")

if [ "${#patterns[@]}" -eq 0 ]; then
    echo "leakcheck: no patterns defined in $patterns_file" >&2
    exit 2
fi

fail=0
for f in "${files[@]}"; do
    [ -f "$f" ] || continue
    is_allowed "$f" && continue
    for p in "${patterns[@]}"; do
        if grep -nE -- "$p" "$f" >/dev/null 2>&1; then
            echo "leakcheck: $f contains forbidden pattern: $p"
            grep -nE -- "$p" "$f" | head -3 | sed 's/^/    /'
            fail=1
        fi
    done
done

if [ "$fail" -ne 0 ]; then
    echo
    echo "leakcheck failed. Either remove the offending content, or — if"
    echo "the match is a legitimate false positive — add the file to the"
    echo "'allow' array in tools/leakcheck.sh."
    exit 1
fi

echo "leakcheck: clean (${#files[@]} files scanned)"
