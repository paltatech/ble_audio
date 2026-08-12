# Zephyr Testing Ecosystem — Implementation Guide

## Objective

`ble_audio` is the working example for the priority progression in
[zephyr_testing_ecosystem.md](zephyr_testing_ecosystem.md). The
goal is twofold, and neither half is optional:

1. **Build a real BLE Audio headphone** — an LE Audio unicast server
   (headset) on the nRF5340 Audio DK, not a toy app. Real hardware
   constraints and real protocol behavior are what make the testing work
   meaningful.
2. **Work through the five testing priorities in order**, each landing as
   a concrete, runnable deliverable in this repo — not an abstract
   exercise done elsewhere and described here after the fact.

The full phase-by-phase implementation plan (board choice, architecture,
what happens in each phase) lives in the session's plan file and is
summarized per-priority below as it's implemented. This document is the
durable, in-repo record — the plan file is not.

## Status at a glance

| Priority | Topic | Status |
|---|---|---|
| 1 | Unit tests & `native_sim` | ✅ Done — `tests/codec_handler/` |
| 2 | Twister, 32/64-bit targets | 🔜 Next |
| 3 | FFF mocking, devicetree fakes, parameterized tests | Pending |
| 4 | HIL, pytest/Robot | Pending |
| 5 | CI/CD, `ztress`, shuffle | Pending |

## Priority 1: Unit Tests & Native Simulation — done

**What was built:** `tests/codec_handler/` — a `ztest` suite that
compiles and runs the *real* `src/middlewares/codec_handler/codec_handler.c`
(not a stub), on `native_sim`. Test cases:

- `test_decode_before_configure_fails` — decode without configuring first
  returns `-ENODEV`
- `test_configure_valid_params_succeeds` / `test_configure_is_idempotent`
- `test_decode_valid_frame_produces_pcm` — a genuine LC3 encode (via
  `liblc3` directly, test-only) → `codec_handler_decode()` round trip
- `test_decode_packet_loss_produces_concealment` — `NULL` input triggers
  PLC and still returns a full frame

**Why `codec_handler` first:** it's pure logic wrapping a portable C
library (`liblc3`), with no BLE stack or real I2S hardware dependency —
the cleanest possible target for a first `native_sim` suite. `led_handler`,
`button_handler`, and `audio_handler` all need real devicetree nodes
(`native_sim` has none of this board's GPIO/I2S hardware) so they aren't
`native_sim`-testable as written; `ble_audio_handler` needs a real or
mocked BT controller (Priority 3/4 territory, not Priority 1).

**Real bugs this caught**, not just process box-ticking:

- `codec_handler` had no way to release its decoder. Writing the
  "before configure" test exposed that a stream stop never reset it —
  fixed by adding `codec_handler_reset()`, wired into
  `app_streamctrl.c`'s `on_stream_stopped()`.
- The first version of the test suite only passed because `ztest`
  happened to run test cases alphabetically in this build — a suite
  that relies on run order will break under `CONFIG_ZTEST_SHUFFLE`
  (Priority 5). Fixed with a `before` hook that resets module state
  ahead of every test, not just once per suite.

**Key environment facts** (verified against this exact NCS pin, not
assumed):

- `CONFIG_LIBLC3` depends on `CONFIG_FPU`, and **`native_sim` does
  support `CONFIG_FPU`** — confirmed against
  `zephyr/samples/bluetooth/unicast_audio_server/boards/native_sim.conf`,
  which sets both. `tests/codec_handler/prj.conf` does the same.
- Running `twister` directly in this environment needs two workarounds,
  neither related to our code:
  1. The pinned toolchain's bundled Python is missing `libffi.so.7` for
     `ctypes`/`multiprocessing`. Point `LD_LIBRARY_PATH` at an isolated
     directory containing just that `.so` (found at
     `~/ncs/toolchains/<hash>/opt/nanopb/generator-bin/libffi.so.7`) —
     **do not** add that whole `nanopb` directory to `LD_LIBRARY_PATH`,
     it also ships an old `libstdc++` that breaks `ccache`.
  2. Pass `--extra-args NCS_TOOLCHAIN_VERSION=NONE`. Without it, CMake's
     `NcsConfig.cmake` auto-detects a toolchain and can resolve to a
     different (possibly broken) installed one than the pinned
     `select-ncs-toolchain.sh` version — the same root cause as the
     `--sysbuild` issue hit in Phase 0 of the plan.

  Both will be wrapped into a `make test` target in Priority 2 so they
  don't need to be remembered by hand.

## Priority 2: Twister Test Runner — next

Formalize what Priority 1 proved manually: every suite gets a
`testcase.yaml`, discovered and run via `west twister -T tests/`, across
both `native_sim` (64-bit host) and `qemu_cortex_m3` (32-bit) to catch
pointer/alignment bugs early. `make test` in the `Makefile` wraps the
invocation (including the environment workarounds above) so `make test`
is all that's needed going forward.

## Priority 3: Mocking & Value-Parameterized Tests — pending

Plan: FFF mocks for `ble_audio_handler`'s BT calls (`bt_bap_stream_start`
and friends) so `application/app_streamctrl.c`'s state machine can be
tested without a real controller; devicetree fakes (`zephyr,fake-gpio`)
for `led_handler`/`button_handler`; value-parameterized cases for LC3
frame-duration/bitrate combinations instead of hand-copied test cases.

## Priority 4: HIL & Multi-Harness Integration — pending

Plan: once real nRF5340 Audio DK hardware is available, a Twister
hardware-map entry (J-Link/UART) for on-target runs, plus a `pytest`
harness that flashes the headset image, drives a second device as the
LE Audio unicast client, and asserts the stream actually starts.

## Priority 5: Regression, `ztress`, Shuffle — pending

Plan: a CI workflow running `west twister`, `make lint`, `make
lint-cmake`, and gitlint on every push (the tool configs already exist
under `tools/`, just need wiring into a workflow file);
`CONFIG_ZTEST_SHUFFLE` added to every suite (the codec_handler suite is
already shuffle-safe, see above — that fix should be treated as the
baseline expectation for every suite added from here on, not a one-off);
`ztress` once the core logic is stable enough to stress meaningfully.

## Reference material used

- `zephyr/samples/bluetooth/unicast_audio_server` — minimal, confirmed
  buildable LE Audio unicast server; source for `prj.conf` baseline and
  the `ascs_*`/`stream_*` callback shapes in `ble_audio_handler.c`.
- `nrf/applications/nrf5340_audio` — Nordic's full LE Audio reference
  app; source for the module boundaries mapped onto
  `common`/`core`/`middlewares`/`application`.
- `zephyr/boards/nordic/nrf5340_audio_dk` — upstream Zephyr's board
  definition for the real DK; `boards/paltatech/ble_audio_board` (in the
  `zephyr_boards` repo) is a trimmed copy of it.
- `nrf/tests/nrf5340_audio/{macros,sw_codec_lc3}` — Nordic's own
  `native_sim`/`qemu_cortex_m3` and on-target LC3 test suites; structural
  template for `tests/codec_handler/`.
