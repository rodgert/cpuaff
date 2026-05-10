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

- Test surface hardening: round-robin invariant test, cgroup
  interaction tests, `try_*` API tests, errno-propagation tests,
  GitHub Actions matrix expansion.

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

[Unreleased]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.beta.1...v2
[2.0.0-htaa.beta.1]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.alpha.3...v2.0.0-htaa.beta.1
[2.0.0-htaa.alpha.3]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.alpha.2...v2.0.0-htaa.alpha.3
[2.0.0-htaa.alpha.2]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.alpha.1...v2.0.0-htaa.alpha.2
[2.0.0-htaa.alpha.1]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.alpha.0...v2.0.0-htaa.alpha.1
[2.0.0-htaa.alpha.0]: https://github.com/rodgert/cpuaff/compare/v1.0.6-htaa.1...v2.0.0-htaa.alpha.0
[1.0.6-htaa.1]: https://github.com/rodgert/cpuaff/releases/tag/v1.0.6-htaa.1
