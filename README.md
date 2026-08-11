# BLE Audio

BLE Audio firmware project. This is a baseline scaffold — application code
has not been added yet (see [CHANGELOG](CHANGELOG)).

## Status

Target board is `ble_audio_board` (nRF52805, based on `vx_001c_g` — see
`zephyr_boards`' `ble_audio_board` branch), built with NCS v2.7.0.

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

This builds the firmware for the configured board (default: `ble_audio_board`,
set in `tools/make/config.mk`).

### Clean Build

```bash
make clean
```

### Configure Board

Edit `tools/make/config.mk` to change the target board:

```makefile
BOARD ?= ble_audio_board
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
_build_ble_audio_<board>/
```

## Folder Layout

- `src/` — application source code, layered:
  - `common/` — shared types, macros, and dependency-free helpers (includes
    firmware version metadata, `app_version.c/.h`)
  - `core/` — low-level hardware/peripheral handling and system bring-up
  - `middlewares/` — reusable services (BLE, storage, power, ...), one subfolder each
  - `application/` — top-level business logic that orchestrates the above

  Each layer may only depend on the ones above it in this list (see the
  `README.md` in each folder). `main.c` stays a thin entry point that wires
  everything together.
- `boards/` — board-specific devicetree overlays and Kconfig fragments
- `docs/` — design notes, test reports, and other project documentation
- `release/` — packaged release artifacts (populated by `prepare_release.sh`)
- `tools/` — build system helpers (CMake, Make, gitlint, clang-format/cmake-format)
