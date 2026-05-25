# ADR-006: CMSIS-Toolbox + CubeMX

## Status

Accepted

## Date

2026-05-13

## Context

Build system has to:

- produce reproducible flight images from CI
- not require a vendor IDE on every dev machine
- play nice with `clang-tidy` and `clangd`
- pin every dependency by version

## Decision

**CMSIS-Toolbox 2.12.0** with `cbuild`. Per-target `*.cproject.yml`
files composed by `bolt.csolution.yml` and a shared
`shared.clayer.yml`.

Toolchain: **arm-none-eabi-gcc 13.3.1**.
Host tests: **CMake 3.22 + Catch2 v3.7.1**.
HAL: **STM32CubeMX** generator, output under `<target>/generated/`.

CMSIS packs pinned:

| Pack | Version |
|---|---|
| ARM::CMSIS | 6.3.0 |
| ARM::CMSIS-Compiler | 2.2.0 |
| ARM::CMSIS-Driver_STM32 | 1.3.0 |
| Keil::STM32L4xx_DFP | 3.1.0 |
| Keil::STM32H7xx_DFP | 4.1.3 (test board only) |

Two build configs: `Debug` (no opt, symbols) and `Release` (`-O2`).
Flight images use Release.

## What we looked at

- **CubeMX without CMSIS** -- generate the project from CubeMX and
  build the resulting Makefile directly. Workable, but we'd lose
  the pack manager and the layered project layout that lets the
  same source tree compile into four binaries cleanly. Pack
  versions would also stop showing up in diffs.
- **PlatformIO** -- abstracts too much; we want control of the
  linker script and pack versions.
- **RIOT-OS** -- pulls in an RTOS, conflicts with
  [ADR-001](ADR-001-tick-architecture.md).

## Consequences

Good:
- One command builds locally and in CI.
- Pack versions show up in diffs.
- `compile_commands.json` plugs into clang tooling.

Bad:
- CMSIS-Toolbox is newer than CubeIDE; onboarding has a
  learning curve.
- We're at the mercy of ARM pack updates when something
  upstream breaks.

## Notes

`compile_commands.json` is reassembled from per-target outputs by
the "Merge compile_commands.json" VS Code task. Run it after a
fresh build before expecting clangd to make sense of things.

Host tests use plain CMake under `flight-software/CMakeLists.txt`.
Flight never uses CMake; tests never use cbuild.

## References

- [ADR-001 Tick Loop](ADR-001-tick-architecture.md)
- CMSIS-Toolbox: <https://github.com/Open-CMSIS-Pack/cmsis-toolbox>
- [`bolt.csolution.yml`](../../flight-software/bolt.csolution.yml)
