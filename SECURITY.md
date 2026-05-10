# Security policy

## Reporting a vulnerability

cpuaff is a header-only library with a small attack surface — most code
paths read `/sys` and call `sched_setaffinity`; nothing accepts network
input. That said, if you find something exploitable, please use
[GitHub's private vulnerability reporting](https://github.com/rodgert/cpuaff/security/advisories/new)
rather than opening a public issue.

We aim to acknowledge reports within one week and ship a fix in the next
release on the active branch.

## Supported versions

| Version                      | Supported                                    |
|------------------------------|----------------------------------------------|
| `master` (1.0.6-htaa.x)      | Yes — current stable                         |
| `v2` pre-release tags        | Best-effort during the v2 release cycle      |
| Older 1.x (upstream releases)| No                                           |

## Scope

In scope:

- The library headers under `include/cpuaff/`
- The bundled `examples/` and `tests/` as written in this repo

Out of scope:

- Vulnerabilities in third-party tools used to build or test (Catch2,
  GCC, Clang, autotools, CMake)
- General Linux kernel CPU-affinity behaviour
- Issues that require root or a malicious local user with sysfs write
  access
