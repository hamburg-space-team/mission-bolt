# Building the Flight Software

Local builds of the four flight binaries. Runs on Linux, macOS, and
Windows (WSL).

Why this toolchain: [ADR-006](../decisions/ADR-006-cmsis-toolbox.md).

## Prereqs

| Tool | Version |
|---|---|
| `arm-none-eabi-gcc` | `ARM_GNU_VERSION` in [.devcontainer/Dockerfile](../../.devcontainer/Dockerfile) |
| CMSIS-Toolbox | `CMSIS_TOOLBOX_VERSION` in [.devcontainer/Dockerfile](../../.devcontainer/Dockerfile) |
| Python | 3.10+ |
| OpenOCD | 0.12.0 (for flashing; see [debugging guide](debugging-with-openocd.md)) |

Verify:

```bash
arm-none-eabi-gcc --version
cbuild --version
```

## First-Time Setup

```bash
cd flight-software
cpackget update-index
cpackget add ARM::CMSIS@6.3.0 ARM::CMSIS-Compiler@2.2.0 \
             ARM::CMSIS-Driver_STM32@1.3.0 Keil::STM32L4xx_DFP@3.1.0
```

Once per machine. `cbuild` finds packs from the local store after
that.

## Targets

| Target | Device | Binary |
|---|---|---|
| `btc` | STM32L476RGT6 | `btc.elf` |
| `exp1-space-disco` | STM32L476RGT6 | `exp1.elf` |
| `exp2-bouncy-castle` | STM32L476RGT6 | `exp2.elf` |
| `exp3-floaty-boi` | STM32L476RGT6 | `exp3.elf` |
| `test-nucleo-h753zi` | STM32H753ZI | `test.elf` (dev board) |

## Building

One target:

```bash
cbuild bolt.csolution.yml --context btc.Debug+btc
```

Swap the context name for the rest:

```bash
cbuild bolt.csolution.yml --context exp1.Debug+exp1-space-disco
cbuild bolt.csolution.yml --context exp2.Debug+exp2-bouncy-castle
cbuild bolt.csolution.yml --context exp3.Debug+exp3-floaty-boi
```

Output lands in `out/<target>/<project>/<config>/`.

Release build for flight images:

```bash
cbuild bolt.csolution.yml --context btc.Release+btc
```

All four at once:

```bash
for ctx in btc.Debug+btc \
           exp1.Debug+exp1-space-disco \
           exp2.Debug+exp2-bouncy-castle \
           exp3.Debug+exp3-floaty-boi; do
  cbuild bolt.csolution.yml --context "$ctx" || break
done
```

VS Code: the `Build <target>.Debug` tasks wrap each.

## clangd Database

clangd wants a top-level `compile_commands.json` over all targets.
There's a VS Code task that builds it. Or run:

```bash
python3 -c "import json,glob,os; \
  data=[e for p in glob.glob('out/**/compile_commands.json',recursive=True) \
        + glob.glob('build/**/compile_commands.json',recursive=True) \
        for e in json.load(open(p)) if os.path.isdir(e['directory'])]; \
  json.dump(data,open('compile_commands.json','w'))"
```

Run it after a fresh `cbuild`.

## CubeMX

Per-target CubeMX projects live under `<target>/generated/`. Open
the `.ioc` in STM32CubeMX, regenerate, commit. Don't hand-edit
anything outside the user sections.

The `.noinit` section
([ADR-008](../decisions/ADR-008-noinit-ram-recovery.md)) lives in
the linker user section.

## Verifying a Change

`scripts/verify.sh` is the one gate for everything - run it before
every push. CI (`.github/workflows/verify.yml`) runs the same script
inside the devcontainer on every PR and push to main:

```sh
scripts/verify.sh              # flight builds, host tests, schema drift,
                               # interfaces header check (flight + reflection
                               # view), cargo build/test/fmt/clippy, extension
                               # build, clang-tidy + clang-format
scripts/verify.sh --no-lint    # skip the lint pass (slowest section)
scripts/lint-flight.sh --fix   # apply clang-format / tidy fixes in place
scripts/rebuild-extension.sh   # rebuild the boltscope extension + install it
                               # into VS Code (then reload the window)
```

Sections report PASS/FAIL/SKIP; missing optional tooling SKIPs
without failing. The schema-drift section regenerates the schemagen
artifacts and fails if they were stale - review and commit the
regenerated files in that case.

## Common Issues

- **`cbuild: command not found`** -- toolbox not on PATH. Source its
  `setup_*.sh`.
- **Pack version mismatch** -- either install the pinned version or
  update `bolt.csolution.yml` (and explain in the PR).
- **Missing newlib headers** -- `libnewlib-arm-none-eabi` on
  Debian/Ubuntu.
- **`.elf` is huge** -- you built Debug. Release strips symbols.

## Next

- [Running flight unit tests](running-flight-unit-tests.md)
- [Debugging with OpenOCD](debugging-with-openocd.md)
- [`flight-software/README.md`](../../flight-software/README.md)
