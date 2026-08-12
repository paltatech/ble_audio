# BLE Audio

BLE Audio firmware project. This is a baseline scaffold — application code
has not been added yet (see [CHANGELOG](CHANGELOG)).

## Status

Target board is `ble_audio_board` (nRF5340, based on the nRF5340 Audio DK —
see `zephyr_boards`' `ble_audio_board` branch), built with NCS v2.7.0. This
is Nordic's real LE Audio-capable part; a real LE Audio stack (LC3 codec,
ISO channels) needs far more RAM/compute than a single-core nRF52 chip
provides.

[west.yml](west.yml) pulls `nrf` (`vx_sdk_nrf@io_board`) and
`zephyr_boards` (`vx_zephyr_boards@ble_audio_board`) from `paltatech`. Both
pins should move back to `develop`/`manifest-rev` once those branches are
merged.

## SDK Version

- **Zephyr RTOS**: 3.6.99
- **nRF Connect SDK**: v2.7.0
- **Zephyr SDK (Toolchain)**: 0.16.5

## Prerequisites

- **NCS Toolchain** installed at:
  - Linux: `~/ncs/toolchains/<hash>`
  - Windows: `C:/ncs/toolchains/<hash>`
- **J-Link** debugger for flashing and debugging
- **`qemu-system-arm`** to run `make test`'s 32-bit leg (`sudo apt-get
  install qemu-system-arm` on Linux) — see
  [docs/testing_ecosystem.md](docs/testing_ecosystem.md)
- **`twister_harness`** pytest plugin, only needed for `make test-hil`
  (`pip install zephyr/scripts/pylib/pytest-twister-harness`)

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
`ble_audio_board/nrf5340/cpuapp`, set in `tools/make/config.mk`). The
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
BOARD ?= ble_audio_board/nrf5340/cpuapp
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

Runs `tests/` under Twister across `native_sim` (64-bit host) and
`mps3/an547` (32-bit QEMU) — see
[docs/testing_ecosystem.md](docs/testing_ecosystem.md) for what's covered
and why those platforms. Requires `qemu-system-arm` (see Prerequisites
above) for the 32-bit leg.

### Clean Test Output

```bash
make test-clean
```

### Run the HIL Boot Test

```bash
make test-hil
```

Flashes the real production image onto a physical
`ble_audio_board/nrf5340/cpuapp` and checks its boot log (Bluetooth init,
advertising start) over UART. Not part of `make test` — needs real
hardware and a filled-in `tools/hardware-map.yml` (copy
`tools/hardware-map.example.yml` and fill in your board's J-Link serial),
plus the `twister_harness` plugin from Prerequisites above. See
[docs/testing_ecosystem.md](docs/testing_ecosystem.md) for what's
verified vs. not without hardware in hand.

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
_build_ble_audio_ble_audio_board_nrf5340_cpuapp/
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
- `pytest/` — hardware-in-the-loop pytest cases, run via `make test-hil`
- `tools/` — build system helpers (CMake, Make, gitlint, clang-format/cmake-format)
