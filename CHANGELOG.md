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
branch. See the `v2.0.0-htaa.alpha.*` / `beta.*` / `rc.*` pre-release
tags for shipped milestones.

### Planned for 2.0.0

- Backend amputation: drop `hwloc_impl/` and `null_impl/` (Linux-only).
- CMake-only build with package-config and pkg-config exports;
  autotools removed.
- C++20 modernization: `std::expected`-based error reporting,
  `pthread_setaffinity_np` overloads for arbitrary-thread pinning,
  cgroup/cpuset awareness, dynamic `cpu_set_t` via `CPU_ALLOC` for
  hosts with more than 1024 CPUs.
- Linux backend correctness fixes (sysfs parsing, `native_cpu_mapper`
  short-circuit, sysfs-fallback hardening).

## [1.0.6-htaa.1] — 2026-05-09

The fork's divergence baseline. Last release on the 1.x line.

### Changed

- Update bundled Catch2 single header to v2.13.10 to fix glibc
  `SIGSTKSZ`-no-longer-being-a-constant breakage. (`5694f09`)

### Fixed

- `linux_impl/linux.hpp`: initialize `cpu_identifier_wrapper::id_(-1)`
  to silence an uninitialized-member warning. (`5694f09`)

[Unreleased]: https://github.com/rodgert/cpuaff/compare/v1.0.6-htaa.1...v2
[1.0.6-htaa.1]: https://github.com/rodgert/cpuaff/releases/tag/v1.0.6-htaa.1
