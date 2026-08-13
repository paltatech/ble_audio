# BLE Audio

An LE Audio unicast server (headset) firmware project, with a working
example of the Zephyr testing ecosystem progression built alongside it -
see [docs/testing_ecosystem.md](docs/testing_ecosystem.md) for the *why*
(phase-by-phase history, real bugs found) and
[docs/testing_guide.md](docs/testing_guide.md) for the *how* (running
and adding tests, day to day).

## Status

Target board is `nrf5340dk/nrf5340/cpuapp` - a standard, upstream-supported
Zephyr board, no custom board definition needed. Real hardware is the plain
nRF5340 DK, not the nRF5340 Audio DK this project originally targeted; the
DK has no I2S codec chip wired up, so `audio_handler` (I2S output) is
commented out in `app_streamctrl.c` and excluded from the build in
`src/middlewares/CMakeLists.txt` until real audio hardware is available.
The DK has 4 LEDs and 4 buttons (`led0`-`led3`, `sw0`-`sw3`); the app
currently only uses one of each (`led0` for connection status, `sw0`/
`button0` wired to a generic press callback).

[west.yml](west.yml) pulls `nrf` (`vx_sdk_nrf@io_board`) and
`zephyr_boards` (`vx_zephyr_boards@ble_audio_board`) from `paltatech`. The
`nrf` pin should move back to `develop`/`manifest-rev` once that branch is
merged; `zephyr_boards` is no longer needed for the primary build target
now that it's the standard `nrf5340dk` (its `ble_audio_board` definition
isn't referenced anywhere in this repo's build path anymore) - still
pulled for now, pending a decision on whether to drop it.

## SDK Version

- **Zephyr RTOS**: 3.6.99
- **nRF Connect SDK**: v2.7.0
- **Zephyr SDK (Toolchain)**: 0.16.5

## Prerequisites

- **NCS Toolchain** installed at:
  - Linux: `~/ncs/toolchains/<hash>`
  - Windows: `C:/ncs/toolchains/<hash>`
- **J-Link** debugger for flashing and debugging
- **`gcc-multilib`/`g++-multilib`** for `make test`'s `native_sim` leg,
  which builds 32-bit (`sudo apt-get install gcc-multilib g++-multilib`
  on Linux) — see [docs/testing_guide.md](docs/testing_guide.md)
- **`qemu-system-arm`** to run `make test`'s QEMU leg (`sudo apt-get
  install qemu-system-arm` on Linux) — see
  [docs/testing_ecosystem.md](docs/testing_ecosystem.md)

## Project Setup

### 1. Initialize Environment

Source the environment script to set up paths and toolchain:

```bash
source ./start-zephyr-env.sh
```

This script:
- Adds NCS toolchain binaries to PATH
- Sets up Zephyr SDK and toolchain variant
- Exports board root for custom board definitions
- Sources the Zephyr environment

### 2. Fetch Dependencies

On first setup, update the west manifest and fetch all dependencies:

```bash
make west-update
```

## Build Commands

### Build the Project

```bash
make build
```

This builds the firmware for the configured board (default:
`nrf5340dk/nrf5340/cpuapp`, set in `tools/make/config.mk`). The
network core's Bluetooth controller image is built automatically as a
child image (`CONFIG_NCS_INCLUDE_RPMSG_CHILD_IMAGE`) - no `--sysbuild`
needed for this NCS revision. Output is `zephyr/merged.hex`, combining
both cores.

### Clean Build

```bash
make clean
```

### Configure Board

Edit `tools/make/config.mk` to change the target board:

```makefile
BOARD ?= nrf5340dk/nrf5340/cpuapp
```

Or override on the command line:

```bash
make build BOARD=<other-board>
```

## Flash Commands

### Flash with Default J-Link

```bash
make flash
```

### Flash with Specific J-Link Serial Number

```bash
make flash JLINK_SERIAL=683980738
```

## Test Commands

### Run All Tests

```bash
make test
```

Runs `tests/` under Twister across `native_sim` (32-bit host, needs
`gcc-multilib`), `native_sim/native/64` (64-bit host), and `mps3/an547`
(32-bit QEMU, real ARM cross-compile) — see
[docs/testing_guide.md](docs/testing_guide.md) for how tests are
organized and [docs/testing_ecosystem.md](docs/testing_ecosystem.md) for
why those platforms. Requires `qemu-system-arm` (see Prerequisites above)
for the QEMU leg.

### Clean Test Output

```bash
make test-clean
```

### Run the HIL Boot Test

```bash
make test-hil
```

Flashes the real production image onto a physical
`nrf5340dk/nrf5340/cpuapp` and checks its boot log (Bluetooth init,
advertising start) over UART — via Twister's built-in `harness: console`
regex matching, no pytest involved. Not part of `make test` — needs real
hardware and a filled-in `tools/hardware-map.yml` (copy
`tools/hardware-map.example.yml` and fill in your board's J-Link serial).
See [docs/hil_testing.md](docs/hil_testing.md) for setup and current
status.

## Continuous Integration

Five workflows under `.github/workflows/`, one per concern:

- `gitlint.yml` — commit message format (`tools/gitlint/`, `.gitlint`)
- `clang-format.yml` — C/C++ formatting (`make lint-ci`)
- `cmake-format.yml` — CMake formatting (`make lint-cmake`)
- `test.yml` — `make test` (Twister across `native_sim`/`mps3/an547`)
- `compile.yml` — builds the real production firmware
  (`./prepare_release.sh`), fails on any compiler warning, uploads the
  result as a downloadable Actions artifact

`test.yml` and `compile.yml` need a `MY_GITHUB_TOKEN` secret with read
access to `paltatech/vx_sdk_nrf` and `paltatech/vx_zephyr_boards` (same
convention every sibling paltatech firmware repo's CI uses) — see
[docs/testing_ecosystem.md](docs/testing_ecosystem.md) for what's
verified about these workflows vs. not (none have been run for real; no
way to trigger a GitHub Actions run from the environment they were
written in).

## Debug Commands

### Start GDB Server

```bash
make start-gdb-server
```

### Connect GDB to Target

```bash
make debug
```

## Release

```bash
./prepare_release.sh
```

Builds the firmware and packages a versioned release artifact into
`release/`.

## Utility Commands

### Show Build Path

```bash
make print-build-path
```

### Show All Available Targets

```bash
make help
```

## Build Output

Build artifacts are placed in:
```
_build_ble_audio_nrf5340dk_nrf5340_cpuapp/
```
(`BOARD`'s slashes are replaced with underscores for the directory name.)

## Folder Layout

- `src/` — application source code, layered:
  - `common/` — shared types, macros, and dependency-free helpers (includes
    firmware version metadata, `app_version.c/.h`)
  - `core/` — reserved for low-level system bring-up; currently empty
  - `middlewares/` — one `<x>_handler/` subfolder per service:
    `led_handler`, `button_handler`, `audio_handler` (I2S output),
    `codec_handler` (LC3), `ble_audio_handler` (LE Audio unicast server)
  - `application/` — `app_streamctrl.c/.h`, the headset state machine that
    orchestrates the middlewares above

  Each layer may only depend on the ones above it in this list (see the
  `README.md` in each folder). `main.c` stays a thin entry point that wires
  everything together.
- `boards/` — board-specific devicetree overlays and Kconfig fragments
- `docs/` — design notes, test reports, and other project documentation
- `release/` — packaged release artifacts (populated by `prepare_release.sh`)
- `tests/` — `ztest` suites, one per subfolder, run via `make test`
- `tools/` — build system helpers (CMake, Make, gitlint, clang-format/cmake-format)
