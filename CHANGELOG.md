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

- CMake-only build with package-config and pkg-config exports;
  autotools removed.
- C++20 modernization: `std::expected`-based error reporting,
  `pthread_setaffinity_np` overloads for arbitrary-thread pinning,
  cgroup/cpuset awareness, dynamic `cpu_set_t` via `CPU_ALLOC` for
  hosts with more than 1024 CPUs.
- Linux backend correctness fixes (sysfs parsing, `native_cpu_mapper`
  short-circuit, sysfs-fallback hardening).

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

[Unreleased]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.alpha.1...v2
[2.0.0-htaa.alpha.1]: https://github.com/rodgert/cpuaff/compare/v2.0.0-htaa.alpha.0...v2.0.0-htaa.alpha.1
[2.0.0-htaa.alpha.0]: https://github.com/rodgert/cpuaff/compare/v1.0.6-htaa.1...v2.0.0-htaa.alpha.0
[1.0.6-htaa.1]: https://github.com/rodgert/cpuaff/releases/tag/v1.0.6-htaa.1
