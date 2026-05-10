# Changelog

All notable changes to the `rodgert/cpuaff` fork are documented in this
file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
with a `-htaa.N` build identifier marking fork-maintained releases.

This is a maintained fork of [dcdillon/cpuaff](https://github.com/dcdillon/cpuaff),
which has been effectively unmaintained since 2017.

## [Unreleased]

The v2 release cycle is in flight on the [`v2`](https://github.com/rodgert/cpuaff/tree/v2)
branch. Each phase ships as a `v2.0.0-htaa.alpha.*` / `beta.*` /
`rc.*` pre-release tag; entries below are per-tag.

### Planned for 2.0.0

- Cutover: fast-forward `v2` → `master`, tag `v2.0.0-htaa.1`.

## [2.0.0-htaa.rc.1] — 2026-05-10

Phase 5: test + CI hardening, plus three trivial doc/source fixes
from the adversarial review of beta.2. No behavioural change to the
library; this is the release-candidate gate before v2.0.0-htaa.1
final.

### Added — tests

- `cpu_set_compat` TEST_CASE: protects beta.2's basic_cpu_set
  source-compat restoration. Compile-time tripwires
  (`static_assert`s on operator==/operator<=>/swap/move-construct)
  plus runtime exercises for ==/!=, <=>, member + ADL swap, merge,
  contains/find/count, get_allocator/key_comp/value_comp, and
  iteration via the using-declared iterators. If a future refactor
  drops any of these, the test fails to compile rather than
  silently regressing.
- `round_robin_invariant` TEST_CASE: replaces the v1
  non-negative-id smoke test with the *actual* invariant — group
  cpus by processing_unit, hand out all PU=0 cpus before any PU=1
  cpu, then wrap. Plus tests for the empty-allocator case
  (`affinity_errc::allocator_empty` from `try_allocate()`,
  empty-set success from `try_allocate(count)`).
- `pthread_t_overloads` TEST_CASE: spawn a worker that blocks on a
  condition variable; from the main thread, drive its affinity via
  `try_get_affinity(pthread_t)` / `try_set_affinity(pthread_t,
  cpus)` / `try_pin(pthread_t, cpu)` /
  `try_get_available_cpus(pthread_t)`. These overloads previously
  had zero internal coverage.
- `error_category` TEST_CASE: exercises both halves of
  `affinity_category()` — cpuaff-internal codes (≥ 10000) match by
  direct enum comparison and don't masquerade as `std::errc`;
  errno-shaped codes round-trip through `std::generic_category`;
  `error_from_errno` produces matching codes; the message()
  fall-through for unrecognised values uses `std::generic_category`
  rather than the previous "unknown" placeholder.
- `expected_polyfill` TEST_CASE: validates the std::expected-parity
  contract — happy paths for value and void specialisations;
  `value()` throws `bad_expected_access<E>` on missing-value (the
  beta.2 alignment with std::expected). Runs identically whether
  `cpuaff::expected` aliases `std::expected` or uses the polyfill,
  so the contract is held to on both paths.
- `try_get_available_cpus` TEST_CASE: cgroup/cpuset-aware accessor
  coverage — default-available equals topology when unrestricted;
  pinning to a single cpu narrows the available set to that cpu
  (simulates cgroup restriction without requiring cgroup setup);
  pthread_t overload reports the worker's available cpus.

### Added — CI

- GitHub Actions build matrix expanded from 2 → 5 compilers:
  gcc-12 (htaabp-core minimum), gcc-13, gcc-14, clang-17, clang-18
  on ubuntu-24.04. Each runs configure + build + ctest + install
  smoke test (now accepting both `lib/` and `lib64/` layouts).
- New `docs` CI job: installs doxygen, runs
  `cmake --build build --target cpuaff_docs`. The
  `doxygen_add_docs()` target sets `WARN_AS_ERROR=FAIL_ON_WARNINGS`,
  so missing `\param` tags / broken `\ref`s / stale doxygen
  comments fail CI rather than slipping into the generated HTML.
  Generated `build/docs/html` is uploaded as a GitHub Actions
  artefact for download.

### Changed

- `CMakeLists.txt` now `find_package(Threads REQUIRED)` and
  propagates `Threads::Threads` via the cpuaff::cpuaff INTERFACE
  target. On modern glibc (≥ 2.34) pthread is folded into libc; on
  older glibc / musl / etc., `-pthread` is required. Consumers
  doing `find_package(cpuaff CONFIG)` no longer need to link
  pthread separately.
- `CMakeLists.txt` adds an optional `cpuaff_docs` target (only
  built when find_package(Doxygen) succeeds and
  PROJECT_IS_TOP_LEVEL is set, so add_subdirectory / FetchContent
  consumers don't pick it up).

### Fixed (from adversarial review of beta.2)

- `basic_affinity_manager.hpp`:
  `try_get_available_cpus(pthread_t)` was tagged
  `\since v2.0.0-htaa.beta.1` but is brand-new in beta.2 (the
  doxygen pass cargo-culted from the sibling overload). Corrected
  to `beta.2`.
- `basic_round_robin_allocator.hpp`: the comment block introducing
  the Phase 4 try_* surface still claimed
  "Returns std::errc::no_message_available if the allocator is
  empty" — beta.2 replaced that with `affinity_errc::allocator_empty`.
  Code was already correct; comment was stale.
- `basic_affinity_manager.hpp`: removed the dead
  `   public:\n\n   private:` access-specifier churn left over
  from inserting the new private helpers.

## [2.0.0-htaa.beta.2] — 2026-05-10

Phase 4 hotfix milestone. Addresses real findings from the
adversarial review of beta.1 — a source-compat regression in
`basic_cpu_set`, divergences between the cpuaff::expected polyfill
and `std::expected`, the empty-stack/empty-allocator error code
choice, and a missing pthread_t overload for `try_get_available_cpus`.
Plus a full doxygen-source-commentary sweep across the public-API
headers.

### Fixed

- `basic_cpu_set` lost comparison operators (`==`, `<=>`),
  `swap`, `merge`, `extract`, `node_type`, `insert_return_type`,
  `key_compare`, `value_compare`, `get_allocator`, `key_comp`,
  `value_comp` in beta.1's `public std::set` → `private std::set`
  refactor. The synthesised free comparison operators on
  `std::set` worked through ADL on the public base; with private
  inheritance they aren't visible to non-friends. Restored as
  hidden friends that `static_cast` through to the base, plus a
  forwarding member `swap` and ADL-found friend swap, plus
  using-declarations for the missing type aliases and observers,
  plus a forwarding `merge<Source>` template.
- `cpuaff::expected` polyfill now matches `std::expected` on misuse
  paths:
  - `[[nodiscard]]` on the class (was missing — std::expected has it).
  - `expected<void, E>::value()` throws `bad_expected_access<E>` on
    missing-value (was a silent no-op).
  - `operator*` and `operator->` are both unchecked (UB on missing
    value) via `std::get_if` + dereference, matching std::expected.
    Previously `operator*` threw `bad_variant_access` while
    `operator->` returned `nullptr` — the same misuse path produced
    different failure modes depending on accessor and toolchain.
  - `cpuaff::bad_expected_access<E>` exception class added (alias of
    `std::bad_expected_access` when the std path is active).
- `detail/expected.hpp`: `#include <exception>` was inside
  `namespace cpuaff` (it pulled the included declarations into the
  cpuaff namespace, undefined behaviour for any standard header).
  Moved to the top-level include block.
- `try_pop_affinity` and `try_allocate` now return fork-specific
  `cpuaff::affinity_errc::stack_empty` / `allocator_empty` rather
  than `std::errc::no_message_available` (which is "the I/O channel
  has no data" — a stretch). Internal codes (≥ 10000) stay in the
  cpuaff::affinity_category for matching; errno-shaped values
  (< 10000) still map to `std::generic_category` so
  `ec == std::errc::invalid_argument` works for kernel-sourced
  errors. Unrecognised error values now fall through to
  `std::generic_category().message(ev)` rather than the previous
  `"unknown cpuaff::affinity error"`.

### Added

- `try_get_available_cpus(pthread_t)` overload — symmetry gap from
  beta.1; the rest of the try_* family had both calling-thread and
  pthread_t variants.
- `cpuaff::affinity_errc::stack_empty` (10001) and
  `affinity_errc::allocator_empty` (10002) — fork-specific error
  codes for the two cpuaff-internal "no data" cases.

### Documentation

- Full doxygen pass across the public-API headers and the
  internal `linux_impl/` helpers. Every public class, free function,
  and method now has `\brief` plus `\param` / `\return` /
  `\deprecated` / `\warning` / `\since` tags as appropriate.
  `\since v2.0.0-htaa.beta.1` marks the new Phase 4 surface;
  `\internal` flags the linux_impl / detail headers.
- Notable inline fixes during the sweep:
  `sysfs_reader.hpp` and `set_reader.hpp` file-level docstrings
  originally referenced `"/sys/devices/system/{node,cpu}/*"` whose
  literal `/*` was a nested-comment opener inside the `/*! ... */`
  block (caught by clang's -Wcomment); rephrased.
  `cpu_spec.hpp` stream operators had broken `\parse` tags;
  fleshed out properly.

Verified
* cmake build / ctest clean (no warnings, no errors).
* All 149 assertions across 4 test cases pass.
* tools/leakcheck.sh clean (369 files scanned).

## [2.0.0-htaa.beta.1] — 2026-05-09

Phase 4: C++20 API modernization. Adds the `try_*` family of
error-returning methods alongside the existing bool API (now
[[deprecated]]); adds `pthread_t` overloads for arbitrary-thread
pinning; adds cgroup/cpuset-aware `try_get_available_cpus()`; bumps
the baseline to C++20.

### Added

- `cpuaff::expected<T, E>` — aliases `std::expected` when the
  toolchain ships it (`__cpp_lib_expected >= 202202L`, libstdc++ ≥ 12
  or libc++ ≥ 16 in C++23 mode); otherwise falls through to a minimal
  `std::variant`-/`std::optional`-backed polyfill in
  `include/cpuaff/detail/expected.hpp` covering the surface cpuaff
  uses internally (has_value / value / operator* / operator-> /
  error / void specialisation).
- `cpuaff::affinity_errc` enum and `cpuaff::affinity_category()` —
  POSIX-errno-shaped `std::error_code`s for sched_*affinity /
  pthread_*affinity_np / `CPU_ALLOC` failures; mapped onto
  `std::generic_category` so `ec == std::errc::invalid_argument` etc.
  still matches.
- `basic_affinity_manager` `try_*` family:
  ```
  expected<cpu_set_type, error_code> try_get_affinity() const;
  expected<cpu_set_type, error_code> try_get_affinity(pthread_t) const;
  expected<void, error_code>         try_set_affinity(cpu_set_type) const;
  expected<void, error_code>         try_set_affinity(pthread_t,
                                                       cpu_set_type) const;
  expected<void, error_code>         try_pin(cpu_type) const;
  expected<void, error_code>         try_pin(pthread_t, cpu_type) const;
  expected<cpu_set_type, error_code> try_get_available_cpus() const;
  ```
  All `[[nodiscard]]`. Errors carry the underlying errno so callers
  can distinguish cgroup restriction (`EINVAL`) from missing
  privilege (`EPERM`) from a vanished target thread (`ESRCH`) —
  diagnostics the legacy bool API silently dropped.
- `basic_affinity_stack` `try_*` family mirroring the manager:
  `try_push_affinity`, `try_pop_affinity`, `try_get_affinity`,
  `try_set_affinity`. `try_pop_affinity` returns
  `std::errc::no_message_available` when the stack is empty.
- `basic_round_robin_allocator` `try_allocate()` and
  `try_allocate(uint32_t count)` — return
  `std::errc::no_message_available` on an empty allocator (the
  legacy `allocate()` was UB in that case). Plus public `empty()`
  / `[[nodiscard]] size()` getters.
- `cgroup/cpuset awareness` via `try_get_available_cpus()` — returns
  the intersection of the topology's CPUs (loaded from `/sys` at
  construction) with the inherited affinity mask from
  `sched_getaffinity`. Use this rather than `get_cpus()` when
  scheduling work under systemd `CPUAffinity=` or Docker
  `--cpuset-cpus`, where the topology will list CPUs that
  `try_set_affinity()` would refuse with `EINVAL`.
- `pthread_t` overloads on the `linux_impl` `get_affinity` /
  `set_affinity` functors (`apply(t, ...)` / `query(t, ...)`),
  routed through `pthread_setaffinity_np` /
  `pthread_getaffinity_np`.
- `cpuaff::cpu_set::contains` (C++20 std::set::contains exposed via
  the using-declaration sweep on the cpu_set composition refactor
  below).

### Changed

- C++ baseline bumped from C++17 to **C++20**
  (`target_compile_features(cpuaff INTERFACE cxx_std_20)`).
- `basic_cpu_set` no longer publicly inherits `std::set` — the
  derived-to-base conversion that enabled the non-virtual-destructor
  UB hazard is gone, but every member of the public surface is
  re-exposed via using-declarations so external callers see no
  source-level change. Iterators, lookup, mutation, and the
  ostream operator<< all still resolve under their familiar names.
- Internal callers (`basic_native_cpu_mapper` walk fallback,
  `basic_affinity_stack` legacy methods, the bundled examples, the
  test suite) all migrated to the new `try_*` API. The bundled
  examples now demonstrate the v2 idiom and print
  `errc.message()` on failure paths so the example doubles as
  documentation for the error surface.
- Bundled examples build with no `-Wdeprecated-declarations`
  warnings.

### Deprecated

- All bool-returning methods in the affinity / pin / allocate /
  stack surface: `get_affinity`, `set_affinity`, `pin`,
  `push_affinity`, `pop_affinity`, `allocate`. Each carries a
  `[[deprecated]]` attribute pointing at its `try_*` replacement.
  Will be removed in v3.

## [2.0.0-htaa.alpha.3] — 2026-05-09

Phase 3: Linux backend correctness fixes. No public API change.

### Fixed

- `linux_impl/set_reader.hpp`: cpulist parsing rewritten using
  `std::string_view` + `std::from_chars`. The previous implementation
  copied the input into fixed `char buf[2048]` / `char buf[32]` stack
  buffers via `strcpy` (overflow risk on big or hot-plugged systems),
  tokenised with the non-reentrant `strtok`, and converted with
  `atoi` (silent 0 on garbage → phantom CPU 0 in the result set). The
  new implementation is bounds-checked and returns false (with empty
  result) on malformed input.
- `linux_impl/linux.hpp` `get_affinity` / `set_affinity`: switched
  from stack-allocated `cpu_set_t` (`CPU_SETSIZE = 1024` bits) to
  dynamic `CPU_ALLOC` / `CPU_ALLOC_SIZE` sized via
  `sysconf(_SC_NPROCESSORS_CONF)`. `set_affinity` further grows the
  allocation if the input set references CPU ids beyond the
  configured count. Hosts with more than 1024 CPUs are now correctly
  handled; previously `sched_*affinity` returned `EINVAL` and the
  call silently dropped.
- `linux_impl/sysfs_reader.hpp` last-resort fallback: directory entries
  are now strictly matched against `cpu<digits>` via `std::from_chars`
  with a full-consumption check. Previously any name starting with
  `"cpu"` (e.g. `cpufreq`, `cpuidle`) was accepted, with `atoi("freq")`
  silently returning 0 and producing a phantom CPU 0. The `DIR*` is
  now closed on every exit path (it was previously leaked).
- `basic_native_cpu_mapper::initialize()`: identity-map short-circuit.
  When `TRAITS::has_identity_native_mapping` is true (now the case
  for `linux_impl::traits`), the mapper is built directly from the
  affinity_manager's enumerated cpus rather than by walking every
  CPU and calling `sched_setaffinity` on each one. The walk path was
  documented as possibly hanging on the wrong host; it is preserved
  only as the fallback for non-Linux backends. Production
  initialization is now O(N) map insertions with no thread-affinity
  disturbance.

### Changed

- `linux_impl::traits` gained
  `static constexpr bool has_identity_native_mapping = true;` to
  enable the mapper short-circuit above.

## [2.0.0-htaa.alpha.2] — 2026-05-09

Phase 2: build system replacement. CMake replaces autotools end to
end; a `cpuaff::cpuaff` `INTERFACE` target is exported with both
CMake package-config and pkg-config metadata.

### Added

- `CMakeLists.txt` — header-only `INTERFACE` target
  `cpuaff::cpuaff`, `find_package(cpuaff CONFIG)` support via
  `cpuaffConfig.cmake` / `cpuaffConfigVersion.cmake` (SameMajorVersion
  compatibility), pkg-config support via `cpuaff.pc`.
- `cmake/cpuaffConfig.cmake.in` — package-config skeleton.
- `cpuaff.pc.in` — pkg-config template; uses `${pcfiledir}` so the
  installed tree is relocatable (verified by moving the install dir
  and re-querying).
- `examples/CMakeLists.txt`, `tests/CMakeLists.txt`.
- `.github/workflows/ci.yml` — matrix CI over GCC 13 / Clang 18 on
  ubuntu-24.04 with Ninja, including a `cmake --install` smoke test
  that asserts the headers, `cpuaffConfig.cmake`, and `cpuaff.pc`
  all land where consumers expect.
- `CPUAFF_BUILD_EXAMPLES` and `CPUAFF_BUILD_TESTS` options
  (default ON when top-level project, off when consumed via
  `add_subdirectory` / `FetchContent`).

### Changed

- Test executable renamed from `test` to `cpuaff_tests` to avoid
  collision with CMake's built-in `test` target.
- `CONTRIBUTING.md` build instructions switched from autotools to
  CMake.

### Removed

- `configure.ac`, all `Makefile.am` files, `bootstrap.sh`,
  `cleanup.sh`.
- `ChangeLog`, `NEWS`, `README`, `INSTALL` — empty
  autotools-convention placeholders (the real docs are
  `CHANGELOG.md` and `README.md`).
- `packaging/debian.{trusty,xenial,zesty,artful}/` — three of these
  Ubuntu releases are EOL and all four pointed at upstream's PPA.
- `.travis.yml`, `.circleci/` — superseded by GitHub Actions.
- The autotools artefact section of `.gitignore`.

## [2.0.0-htaa.alpha.1] — 2026-05-09

Phase 1: backend amputation. Library is now Linux-only. No behavior
change for any Linux consumer that didn't use the PCI surface
(including `htaabp-core`).

### Removed

- `include/cpuaff/impl/hwloc_impl/` and
  `include/cpuaff/impl/null_impl/` backends. Both contained latent
  compile errors in their PCI configurations and neither was used by
  the primary consumer.
- The entire PCI surface: `basic_pci_*`, `pci_device_*`,
  `pci_name_resolver`, `linux_impl/pci_device_reader`. PCI device
  enumeration with NUMA-locality is on the
  [libexa](https://github.com/rodgert/libexa) roadmap; revisit there.
- `include/cpuaff/options.hpp` and the `CPUAFF_USE_HWLOC` /
  `CPUAFF_PCI_SUPPORTED` defines it housed.
- `--with-hwloc` configure option and the non-Linux hwloc library
  check in `configure.ac`.
- Examples `list_pci_devices` and `list_nearby_cpus` (both
  PCI-dependent — the latter despite its misleading name).
- Now-unused `<cstring>`, `<fstream>`, `<iomanip>` includes from
  `linux_impl/linux.hpp` (pulled in only by the deleted PCI loader).

### Changed

- cpuaff is now Linux-only. Non-Linux platforms get a hard `#error`
  at `config.hpp` pointing at `v1.0.6-htaa.1` for older platform
  support; `configure.ac` hard-errors on non-Linux hosts.

## [2.0.0-htaa.alpha.0] — 2026-05-09

Phase 0.5: project hygiene scaffolding. No source-code changes.

### Added

- `CHANGELOG.md` (Keep a Changelog format).
- `CONTRIBUTING.md` — build, branch model, code style, hygiene gates,
  PR conventions, license.
- `SECURITY.md` — vulnerability reporting via GitHub private
  advisories.
- `.github/workflows/format.yml` — clang-format-17 CI gate.
- `.github/workflows/security.yml` — gitleaks + custom leakcheck CI
  gates.
- `tools/leakcheck.sh` + `tools/leakcheck-patterns.txt` — pattern
  scanner for absolute paths, internal references, etc.
- `tools/git-hooks/pre-commit` + `tools/setup-hooks.sh` — opt-in
  local hook running the same gates pre-commit.
- Secret-class and editor-noise patterns added to `.gitignore`.

## [1.0.6-htaa.1] — 2026-05-09

The fork's divergence baseline. Last release on the 1.x line.

### Changed

- Update bundled Catch2 single header to v2.13.10 to fix glibc
  `SIGSTKSZ`-no-longer-being-a-constant breakage. (`5694f09`)

### Fixed

- `linux_impl/linux.hpp`: initialize `cpu_identifier_wrapper::id_(-1)`
  to silence an uninitialized-member warning. (`5694f09`)

[Unreleased]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.rc.1...v2
[2.0.0-htaa.rc.1]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.beta.2...v2.0.0-htaa.rc.1
[2.0.0-htaa.beta.2]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.beta.1...v2.0.0-htaa.beta.2
[2.0.0-htaa.beta.1]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.alpha.3...v2.0.0-htaa.beta.1
[2.0.0-htaa.alpha.3]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.alpha.2...v2.0.0-htaa.alpha.3
[2.0.0-htaa.alpha.2]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.alpha.1...v2.0.0-htaa.alpha.2
[2.0.0-htaa.alpha.1]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.alpha.0...v2.0.0-htaa.alpha.1
[2.0.0-htaa.alpha.0]: https://github.com/rodgert/cpuaff/compare/v1.0.6-htaa.1...v2.0.0-htaa.alpha.0
[1.0.6-htaa.1]: https://github.com/rodgert/cpuaff/releases/tag/v1.0.6-htaa.1
