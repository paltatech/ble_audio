# Testing Guide

A practical reference for running, understanding, and extending
`ble_audio`'s test suite. For the *why* behind each technique (real bugs
found, decisions made, what's verified vs. not) see
[testing_ecosystem.md](testing_ecosystem.md) — this document is the *how*.

---

## What needs to be configured first

Everything below assumes you've already run the one-time setup in the
root [README.md](../README.md) (NCS toolchain, `source
./start-zephyr-env.sh`, `make west-update`). On top of that, tests need:

| Tool | Needed for | Install |
|---|---|---|
| `gcc-multilib` / `g++-multilib` | `native_sim` (builds 32-bit by default) | `sudo apt-get install gcc-multilib g++-multilib` |
| `qemu-system-arm` | `mps3/an547` / `qemu_cortex_m3` legs | `sudo apt-get install qemu-system-arm` |
| `twister_harness` (pip) | `make test-hil` only | `pip install zephyr/scripts/pylib/pytest-twister-harness` |
| `tools/hardware-map.yml` | `make test-hil` only | copy `tools/hardware-map.example.yml`, fill in your J-Link serial |

Nothing else is test-specific — `make test` reuses the same NCS toolchain
as `make build`, plus a `libffi` shim the `Makefile` sets up for you (the
toolchain's bundled Python needs `libffi.so.7`, not on the default library
path — see `testing_ecosystem.md` if curious).

---

## The mental model

Two independent things run under the name "test" here, and it matters
which one you're in:

```
┌──────────────────────────────────────────────────────────────────┐
│  make test  — tests/*/  (isolated test images, host + QEMU only) │
│                                                                    │
│   src/middlewares/codec_handler/codec_handler.c  ──┐              │
│   src/application/app_streamctrl.c               ──┼─▶ compiled   │
│   src/middlewares/led_handler/led_handler.c       ──┘   for REAL, │
│                                                          everything│
│                                                          else FFF- │
│                                                          faked or  │
│                                                          devicetree│
│                                                          -emulated │
│                             │                                     │
│                             ▼                                     │
│                    west twister -T tests                          │
│                             │                                     │
│              ┌──────────────┼──────────────┐                      │
│              ▼              ▼              ▼                      │
│         native_sim   native_sim/     mps3/an547 /                 │
│         (32-bit x86) native/64       qemu_cortex_m3                │
│                       (64-bit x86)   (real ARM cross-compile,     │
│                                       QEMU-simulated Cortex-M)     │
│              │              │              │                      │
│              └──────────────┴──────────────┘                      │
│                             ▼                                     │
│                     PASS/FAIL per test case                       │
└──────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────┐
│  make test-hil  — sample.yaml (the REAL production image)        │
│                                                                    │
│   Same CMakeLists.txt/src/prj.conf as `make build` ──▶ flashed to │
│                                                          physical  │
│                                                          nRF5340DK │
│                             │                                     │
│                             ▼                                     │
│                  UART boot log (extra_configs force              │
│                  CONFIG_LOG_BACKEND_UART=y for this               │
│                  build only - shipped image is RTT)               │
│                             │                                     │
│                             ▼                                     │
│         pytest/test_boot.py (twister_harness.DeviceAdapter)       │
│         waits for "Bluetooth initialized", "Advertising started"  │
│                             │                                     │
│                             ▼                                     │
│                     PASS/FAIL on real hardware                    │
└──────────────────────────────────────────────────────────────────┘
```

The top path never touches real hardware and runs on every push (CI's
`test.yml`). The bottom path needs a physical DK wired up locally and is
never run automatically — it's `make test-hil`, on demand.

---

## Test suites, one row per technique

| Suite | Real module compiled | Technique | Platforms | Hardware needed |
|---|---|---|---|---|
| `codec_handler` | `middlewares/codec_handler/codec_handler.c` (real `liblc3`) | Value-parameterized (10-case LC3 freq/duration grid) | `native_sim`, `native_sim/native/64`, `mps3/an547` | none |
| `app_streamctrl` | `application/app_streamctrl.c` | FFF mocking (4 active middleware fakes; `audio_handler`'s are commented out, see below) + `ztress` concurrency | `native_sim`, `native_sim/native/64`, `qemu_cortex_m3` | none |
| `gpio_handlers` | `middlewares/led_handler/`, `middlewares/button_handler/` (real) | Devicetree GPIO fakes (`zephyr,gpio-emul`) | `native_sim` only | none (emulated GPIO) |
| `ble_audio.hil_boot` (`sample.yaml`, project root) | the real production image, unmodified | `harness: pytest`, boot-log assertions | `nrf5340dk/nrf5340/cpuapp` | real DK + J-Link |

Every `tests/*/prj.conf` also sets `CONFIG_ZTEST_SHUFFLE=y` — each suite
runs 3x with shuffled order (Kconfig defaults). A suite that leaks state
between tests or depends on execution order fails here even if a single
fixed-order run passes.

### `codec_handler` — pure logic, no mocks needed

Compiles the real `codec_handler.c` against the real `liblc3` library —
no BLE stack or hardware involved, so this is the cleanest first target
for any new pure-logic module. `test_decode_sample_count_matches_freq_
and_duration` loops over all 10 LC3 frequency/duration combinations
instead of one hardcoded case, catching bugs a single case would miss
(this is literally how a real bug was found — see
`testing_ecosystem.md`'s Priority 1/3 sections).

**Why it needs `native_sim`, `native_sim/native/64`, *and* `mps3/an547`:**
the first two catch pointer/`size_t`-width bugs (32- vs. 64-bit host
compile); `mps3/an547` is a genuinely different axis — a real ARM
cross-compile under QEMU, not just a different word width. It already
caught a stack-overflow bug neither `native_sim` variant could (see
`testing_ecosystem.md`). `qemu_cortex_m3` is deliberately *not* used
here — its SoC has no FPU, and this suite needs `CONFIG_FPU` for
`CONFIG_LIBLC3`.

### `app_streamctrl` — FFF mocking + `ztress`

Compiles the real `app_streamctrl.c` against FFF fakes of everything it
calls (`led_handler`, `button_handler`, `codec_handler`,
`ble_audio_handler` — `audio_handler` is currently disabled, see below).
The fake `ble_audio_handler_start()` captures the callback struct
`app_streamctrl` registers; tests invoke `captured_cb->connected()`,
`->stream_recv()`, etc. directly instead of needing a real BLE
connection.

`test_stream_recv_survives_concurrent_button_presses` uses `ztress` to
run the BT-RX path and a button press *concurrently* — a scenario picked
deliberately: two concurrent `stream_recv` calls can't happen on real
hardware (one RX path only), but audio RX and a button press genuinely
can interleave. Uses range assertions (`ztress_exec_count(n) > 0`), not
exact equality — exact counts aren't meaningful under `ztress`'s timing
jitter.

**`audio_handler` is currently commented out** in both the production
code and this suite's fakes/assertions — the nRF5340 DK (real hardware
now, not the Audio DK this project originally targeted) has no I2S codec
chip. See `testing_ecosystem.md`'s "Board swap" section.

### `gpio_handlers` — devicetree fakes, not FFF

Compiles the real `led_handler.c`/`button_handler.c` against
`native_sim`'s built-in `zephyr,gpio-emul` controller — real GPIO driver
calls (`gpio_pin_configure_dt`, `gpio_add_callback`, real interrupt
firing), just against an emulated controller instead of silicon.
`boards/native_sim.overlay` adds the `sw0`/button devicetree node
`native_sim` doesn't define by default. `gpio-emul` is host-simulation
only (no QEMU equivalent), so this suite is `native_sim`-only by design,
not by omission.

### `ble_audio.hil_boot` — the only one that touches real hardware

See the [HIL section](#running-hil-tests-real-hardware) below.

---

## Running tests

```bash
make test                          # everything in tests/, all platforms
west twister -T tests/codec_handler -p native_sim   # one suite, one platform
west twister -T tests -s ble_audio.app_streamctrl   # one test suite by ID
make test-clean                    # remove twister-out/
```

`make test` wraps the `libffi` shim and `NCS_TOOLCHAIN_VERSION=NONE`
workarounds for you — prefer it over calling `west twister` directly
unless you're iterating on one suite.

## Running HIL tests (real hardware)

```bash
cp tools/hardware-map.example.yml tools/hardware-map.yml
# edit tools/hardware-map.yml: fill in your J-Link serial (nrfjprog --ids)
pip install zephyr/scripts/pylib/pytest-twister-harness   # once
make test-hil
```

This flashes the *actual production firmware* (not a test-only image) and
watches its real boot log over UART. It's the only test in this repo that
proves the shipped image actually boots on real silicon — everything
under `tests/` is a substitute for hardware, not a replacement for
checking it works on hardware.

---

## Common failures

| Symptom | Likely cause |
|---|---|
| `libffi.so.7: cannot open shared object file` | `NCS_TOOLCHAIN` not exported, or the shim in `.cache/libffi-shim` is stale — `make test` regenerates it, just re-run |
| `bits/libc-header-start.h: No such file` | `gcc-multilib`/`g++-multilib` not installed — needed for `native_sim`'s 32-bit build |
| `QEMU-NOTFOUND` at run time (build succeeded) | `qemu-system-arm` not installed |
| `DT_ALIAS(sw0)` / similar undeclared | Missing board overlay for the platform you're targeting — `native_sim/native/64` needs its own overlay per qualifier, doesn't inherit `native_sim`'s (see `gpio_handlers`, deliberately left off `native/64` for this reason) |
| HIL test times out waiting for a log line | Check `extra_configs` in `sample.yaml` route console to UART — the shipped image logs over RTT only |
| `make test-hil` exits immediately, "No hardware map" | Copy `tools/hardware-map.example.yml` to `tools/hardware-map.yml` and fill it in |
| A suite passes alone but fails under `make test` | Shuffle-dependent state leak — suites run 3x with `CONFIG_ZTEST_SHUFFLE`; fix with a `before` hook that resets *all* module state, not just once in `setup` |

---

## How to add a new test

1. **Pick the technique based on what you're testing**, in this order of
   preference (cheapest/fastest first):
   - **Pure logic, no dependencies on other modules** → real code +
     `native_sim` only, like `codec_handler`. No mocks needed.
   - **Orchestration logic that calls other modules** → FFF-mock the
     dependencies, like `app_streamctrl`. Compile the real file under
     test; fake everything it calls.
   - **Real GPIO/peripheral driver code** → devicetree fakes
     (`zephyr,gpio-emul` or similar), like `gpio_handlers`. Compiles the
     real driver-facing code against an emulated controller.
   - **Needs real silicon** (timing, real peripherals with no
     `native_sim` emulation) → HIL, extending `sample.yaml`/`pytest/`.
     Last resort — everything else is faster to run and doesn't need
     hardware in hand.

2. **Scaffold the directory** (for `tests/`-style suites):
   ```
   tests/<name>/
   ├── CMakeLists.txt   # compile the real file(s) under test + fakes
   ├── prj.conf         # CONFIG_ZTEST=y, CONFIG_ZTEST_SHUFFLE=y, + deps
   ├── testcase.yaml    # platform_allow, tags
   └── src/main.c
   ```
   Copy the closest existing suite as a template — `codec_handler` for
   pure logic, `app_streamctrl` for FFF mocking, `gpio_handlers` for
   devicetree fakes.

3. **`CMakeLists.txt`**: `target_sources` only the real file(s) under
   test plus `src/main.c` — never the whole `src/` tree. Add
   `target_include_directories` for every header the real file needs,
   including the middlewares it depends on (even the faked ones, for
   their public header).

4. **`prj.conf`**: always include `CONFIG_ZTEST=y` and
   `CONFIG_ZTEST_SHUFFLE=y` (established convention — every suite in
   this repo has it, don't add a new one without it). Add
   `CONFIG_FPU=y`/`CONFIG_LIBLC3=y` only if the real code under test
   needs them (check what it `#include`s). If a `before`/`setup` hook
   resets a large static buffer inside a `ZTEST` body, make it `static`
   — `CONFIG_ZTEST_STACK_SIZE`'s default is small on non-x86 targets and
   this repo has hit that overflow twice already.

5. **`testcase.yaml`**: `platform_allow` should include, at minimum,
   `native_sim` and `native_sim/native/64` unless there's a specific
   reason not to (FPU need with no QEMU FPU target available, or a
   `native_sim`-only dependency like `gpio-emul`) — see the table above
   for precedent on when each platform applies. Comment *why*, not just
   what, for any platform you exclude.

6. **Write tests that don't depend on execution order** — `before` hooks
   should reset *all* mutable state the module under test owns, every
   test, not just once in `setup`. `CONFIG_ZTEST_SHUFFLE` will find the
   gap if you don't (see the two real bugs this caught, in
   `testing_ecosystem.md`'s Priority 1/3 sections).

7. **Verify before committing:**
   ```bash
   west twister -T tests/<name> -p native_sim -p native_sim/native/64
   make test          # confirm it doesn't break anything else
   make lint-ci        # format
   make lint-cmake      # format CMakeLists.txt
   ```

8. **If the suite demonstrates a new testing technique** (not just more
   cases of an existing one), add a paragraph to
   `testing_ecosystem.md` under the relevant priority — this repo's
   convention is documenting *why*, not just *that*, a technique was
   used.

### Adding a case to an existing suite

Prefer extending a parameterized test's data table over hand-writing a
near-duplicate case — a single-case test that overlaps what a
parameterized grid already covers is redundant coverage, not extra
safety (this repo has removed one of these already; see
`testing_ecosystem.md`'s Priority 3 section for why).
