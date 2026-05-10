# Contributing to cpuaff

This is a maintained fork of [dcdillon/cpuaff](https://github.com/dcdillon/cpuaff),
which has been effectively unmaintained since 2017. The fork serves as
a production dependency for HFT systems and stays Linux-only.

## Building

cpuaff is a header-only C++ library — to consume it, point your compiler
at `include/`, or pull it into a CMake project via:

```cmake
find_package(cpuaff 2.0 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE cpuaff::cpuaff)
```

To build the bundled examples and tests locally:

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Optional knobs:
- `-DCPUAFF_BUILD_EXAMPLES=OFF` skip examples
- `-DCPUAFF_BUILD_TESTS=OFF`    skip tests

Examples and tests build only when this is the top-level project, so
`FetchContent` consumers don't pull them in by default.

## Branch model

- `master` — stable. Currently at `v1.0.6-htaa.1`.
- `v2` — active v2 release cycle. Pre-release tags
  (`v2.0.0-htaa.alpha.*` → `beta.*` → `rc.*`) mark phase boundaries.
  Will fast-forward to `master` and tag `v2.0.0-htaa.1` when the cycle
  closes.

PRs during the v2 cycle target the `v2` branch. After cutover, PRs
target `master`.

## Code style

All C++ source files are formatted with
[clang-format](https://clang.llvm.org/docs/ClangFormat.html) per
`.clang-format` in the repo root. CI rejects diffs that aren't
clang-format-clean.

To format your changes locally:

```sh
./tools/format_code
```

The script formats every `.hpp`/`.cpp`/`.inl` file under the tree and
deliberately skips `tests/catch.hpp` (the vendored Catch2 single
header) — please don't reformat it.

## Hygiene gates

CI runs two checks beyond formatting:

- **gitleaks** — scans the repo and the diff for credential patterns
  (cloud keys, GitHub PATs, generic high-entropy secrets).
- **leakcheck** — `tools/leakcheck.sh` scans tracked files for local
  paths (`/home/...`, `/Users/...`), maintainer org-tree references
  (`~/org/...`), and downstream-internal project names that have no
  business in a public OSS dependency's repo. Patterns live in
  `tools/leakcheck-patterns.txt`.

To run the same checks locally before every commit, install the in-tree
hooks once per clone:

```sh
./tools/setup-hooks.sh
```

That sets `core.hooksPath` to `tools/git-hooks/` for this clone only.
The `pre-commit` hook will then run `leakcheck` on staged content (and
`gitleaks protect --staged` if `gitleaks` is on your `$PATH`). Revert
with `git config --unset core.hooksPath`.

## Pull requests

- One logical change per PR. Keep deletions, refactors, and behavior
  changes in separate commits where possible.
- Update `CHANGELOG.md` under `[Unreleased]` if the change is
  user-visible (API addition, deprecation, behavior change, bug fix).
- CI must pass — formatting, security, build, and tests.
- For non-trivial changes, open an issue first to align on direction
  before writing the patch.

## Security

For vulnerability reports, see [`SECURITY.md`](SECURITY.md). Please use
GitHub's private vulnerability reporting rather than opening a public
issue.

## License

cpuaff is BSD-3-Clause. Inbound contributions are accepted under the
same license; by submitting a PR you agree your contribution may be
distributed under those terms.
