# Zephyr Testing Ecosystem — Implementation Guide

## Objective

`ble_audio` is the working example for the priority progression in
[zephyr_testing_ecosystem.md](zephyr_testing_ecosystem.md). The
goal is twofold, and neither half is optional:

1. **Build a real BLE Audio headphone** — an LE Audio unicast server
   (headset), not a toy app. Real hardware constraints and real protocol
   behavior are what make the testing work meaningful. Originally targeted
   at the nRF5340 Audio DK; real hardware turned out to be the plain
   nRF5340 DK instead (`nrf5340dk/nrf5340/cpuapp` - standard, upstream
   board, no custom board definition needed). That DK has no I2S codec
   chip, so `audio_handler` (I2S output) is commented out for now - see
   Priority 4 below for what that changed.
2. **Work through the five testing priorities in order**, each landing as
   a concrete, runnable deliverable in this repo — not an abstract
   exercise done elsewhere and described here after the fact.

The full phase-by-phase implementation plan (board choice, architecture,
what happens in each phase) lives in the session's plan file and is
summarized per-priority below as it's implemented. This document is the
durable, in-repo record — the plan file is not.

This document is the *why* (history, real bugs found, decisions made).
For the *how* — running tests day to day, what each suite does, how to
add a new one — see [testing_guide.md](testing_guide.md).

## Status at a glance

| Priority | Topic | Status |
|---|---|---|
| 1 | Unit tests & `native_sim` | ✅ Done — `tests/codec_handler/` |
| 2 | Twister, 32/64-bit targets | ✅ Done — `make test` |
| 3 | FFF mocking, devicetree fakes, parameterized tests | ✅ Done — `tests/app_streamctrl/`, `tests/gpio_handlers/` |
| 4 | HIL, pytest/Robot | ✅ Done — `make test-hil` (schema/build verified; on-device run needs real hardware) |
| 5 | CI/CD, `ztress`, shuffle | ✅ Done — shuffle + `ztress` verified locally; CI workflows syntax-checked, not run for real |

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

  Both are now wrapped into `make test` (see below) so they don't need
  to be remembered by hand.

## Priority 2: Twister Test Runner — done

**What was built:** `make test` — runs `tests/` under `west twister`
across `native_sim`, `native_sim/native/64`, and `mps3/an547` (32-bit
QEMU), with the Priority 1 environment workarounds baked in
(`LD_LIBRARY_PATH` `libffi` shim, `NCS_TOOLCHAIN_VERSION=NONE`). `make
test-clean` removes `twister-out/`.

**Correction found later, during Priority 5 (documented here since
it's a Priority 2 fact, not a Priority 5 one):** this section
originally called plain `native_sim` "64-bit host" and described it as
the 64-bit counterpart to `mps3/an547`'s 32-bit leg. That was wrong.
Checking two commits on a sibling project
(`paltatech/vx_smart-pro-box-2-fw-host@e1219e8`,
`@16f0c74`) that hit this exact issue prompted verifying it here
directly: `file` on a built `tests/app_streamctrl` binary showed `ELF
32-bit LSB executable, Intel 80386` for plain `native_sim`.
`native_sim`'s `board.yml` defines the SoC variant `"64"` as an opt-in
qualifier (`native_sim/native/64`) - without it, the build defaults to
32-bit, and needs `gcc-multilib`/`g++-multilib` on the host (already
installed in this dev environment, which is exactly why the mislabeling
went unnoticed - the 32-bit build "just worked"). This means Priority 1
and the original version of Priority 2 never actually had a 64-bit leg
at all: `native_sim` (32-bit x86) and `mps3/an547` (32-bit ARM) were
both 32-bit, so the "catch pointer/alignment/data-sizing bugs early"
goal this whole section describes was never actually being met. Fixed
by adding `native_sim/native/64` to `TEST_PLATFORMS` and to
`codec_handler`'s/`app_streamctrl`'s `platform_allow` (both build and
pass cleanly on it). `gpio_handlers` was deliberately left off -
its `boards/native_sim.overlay` doesn't apply to the `native/64`
qualifier (overlay filenames are qualifier-specific; the sibling
project's first commit hit the same thing from the other direction,
consolidating two qualifier-specific overlays into one when *they*
dropped their 64-bit variant), and `gpio_handlers` is about GPIO
simulation, not pointer/size-width bugs, so there's nothing to gain by
chasing the overlay naming down. `.github/workflows/test.yml` also
needed `gcc-multilib`/`g++-multilib` added (the sibling's second commit
- their CI failed at `bits/libc-header-start.h` without it, on a fresh
runner that doesn't have these preinstalled the way this dev
environment does).

**Checked whether `mps3/an547` is still worth keeping, now that
`gcc-multilib` gives real 32-bit `native_sim` coverage - it is,
they're orthogonal.** `gcc-multilib` only changes `native_sim`'s
pointer width; it doesn't touch `mps3/an547` at all. Confirmed via
`file` on both suites' output binaries: `native_sim` (either variant)
is always a host-`gcc`-compiled x86 ELF, while `mps3/an547` is a genuine
`ARM, EABI5` binary from the real `arm-zephyr-eabi-gcc` cross-toolchain,
run under QEMU's Cortex-M55 simulation. Word width (32 vs. 64-bit,
covered by the two `native_sim` variants) and target toolchain/ISA/
embedded memory model (host-native vs. real-cross-compiled-ARM, only
`mps3/an547` covers this) are two different axes, not one - and
`mps3/an547` already caught a real bug neither `native_sim` variant
could have (the `CONFIG_ZTEST_STACK_SIZE` overflow above: an embedded
target's real stack is far smaller than a host process's default one).
Removing it would remove that entire category of bug-catching, for a
CI time cost that's small in practice (~4-5s added to the whole `make
test` run when `native_sim/native/64` was added, going by wall-clock
before/after).

**Machine-level tool required, not just Python/west packages:**
`qemu-system-arm` has to actually be installed on the host
(`sudo apt-get install qemu-system-arm` on Linux) — it isn't bundled
with the NCS toolchain or pulled in by `west update`. Without it,
`make test` still *builds* the `mps3/an547` image successfully but
fails at run time with `QEMU-NOTFOUND`. Listed in the root `README.md`'s
Prerequisites now so it isn't rediscovered the hard way.

**`qemu_cortex_m3` turned out to be the wrong 32-bit target.** It was
the obvious first choice (it's what the doc names, and what Nordic's own
`macros` test uses), but its SoC (TI LM3S6965) has no FPU, and
`codec_handler` needs `CONFIG_FPU` for `CONFIG_LIBLC3`. Verified this by
checking `soc/ti/lm3s6965`'s Kconfig for `CPU_HAS_FPU` (absent) before
spending time chasing a build failure. `mps3/an547` (Cortex-M55) is
QEMU-simulated too and its SoC Kconfig does `select CPU_HAS_FPU` —
confirmed by an actual build+run, not just reading Kconfig. Any future
suite that doesn't need FPU can still target `qemu_cortex_m3` directly;
`TEST_PLATFORMS` in the `Makefile` is per-project, not per-suite, so a
suite can also override its own `platform_allow` in `testcase.yaml`.

**A real bug this caught:** the first `make test` run against
`mps3/an547` hit a genuine stack overflow —
`test_decode_packet_loss_produces_concealment` faulted with `USAGE FAULT
Stack overflow`. LC3's real stack footprint is larger than
`CONFIG_ZTEST_STACK_SIZE`'s default (1024 B on non-x86 targets); it just
happened to fit under `native_sim`'s far larger default host stack, so
Priority 1 never surfaced it. This is the same issue Nordic worked
around in their own `sw_codec_lc3` test (`CONFIG_MAIN_STACK_SIZE=80000`,
commented "Added large stack sizes. Can be optimized"). Fixed here with
`CONFIG_ZTEST_STACK_SIZE=8192` in `tests/codec_handler/prj.conf` — this
is exactly the class of bug the 32-bit/QEMU leg exists to catch, and it
would not have been found by `native_sim` alone.

**Another gap found along the way:** `select-ncs-toolchain.sh` computed
`NCS_TOOLCHAIN` but never `export`ed it, so it was invisible to `make`
(a child process) even though it worked fine interactively (`echo
$NCS_TOOLCHAIN` in the same shell). `make test` needs it to locate the
`libffi` shim source. Fixed with one `export NCS_TOOLCHAIN` line.

## Priority 3: Mocking & Value-Parameterized Tests — done

**What was built:** three additions, one per technique named in the doc.

**`tests/app_streamctrl/` (FFF mocking).** Compiles the real
`app_streamctrl.c` against FFF fakes of all five middlewares it depends
on (`led_handler`, `button_handler`, `audio_handler`, `codec_handler`,
`ble_audio_handler`) instead of their real implementations - no BT stack
or hardware involved. The fake `ble_audio_handler_start()` captures the
`ble_audio_handler_cb` struct `app_streamctrl` registers; tests then
invoke `captured_cb->connected()`, `->stream_recv()`, etc. directly to
drive the state machine and assert on what it called downstream (LED on
connect, codec configured on stream setup, decoded audio forwarded to
`audio_handler_write()`, decode errors *not* forwarded, codec reset on
stream stop). No FPU dependency, so unlike `codec_handler` this suite
runs on plain `qemu_cortex_m3` - confirmation that FPU only mattered for
the one suite that touches `liblc3` directly.

**`tests/gpio_handlers/` (devicetree fakes).** Compiles the real
`led_handler.c`/`button_handler.c` against `native_sim`'s built-in
`zephyr,gpio-emul` controller (`gpio0` - it already ships a `led0`
alias; `boards/native_sim.overlay` adds the `sw0`/button node it doesn't
provide by default). Drives the button via `gpio_emul_input_set()` and
reads the LED back via `gpio_emul_output_get()` - real GPIO driver calls,
no physical hardware. `gpio-emul` is host-simulation-only (no QEMU
equivalent), so this suite is `native_sim`-only by design.

**Extended `tests/codec_handler/`'s existing suite (value-parameterized
tests).** One `ZTEST` loops over all 10 combinations LC3 supports (`8/16/
24/32/48` kHz × `7.5/10` ms - `lc3.h`'s own documented grid), encoding a
real tone and decoding it through `codec_handler` for each, instead of
10 hand-copied test functions.

**Two real bugs found, both in code Priority 1/2 already "covered":**

- `codec_handler_decode()` always returned the hardcoded
  `AUDIO_MAX_SAMPLES_PER_FRAME` (480, valid only for the 48 kHz/10 ms
  case every earlier test happened to use) instead of the actual decoded
  sample count. Since the codec cap declares
  `BT_AUDIO_CODEC_CAP_FREQ_ANY`, a real peer negotiating any other
  frequency would get a wrong sample count back - e.g. 480 instead of 60
  for 8 kHz/7.5 ms, confirmed by the first run of the new parameterized
  test before it was fixed. Fixed by computing the real count via
  `lc3_frame_samples(configured_frame_us, configured_freq_hz)`. This is
  exactly what value-parameterized testing is for: the 48 kHz/10 ms-only
  coverage from Priority 1/2 could not have caught it.
- `button_handler_init()` registers a GPIO callback on a `static`
  struct via `gpio_add_callback()`, which - like `codec_handler`'s
  decoder - is only safe to call once per struct without an intervening
  `gpio_remove_callback()`. The first version of `gpio_handlers`'s
  `before` hook called `button_handler_init()` on every test, which
  corrupted the (intrusive, singly-linked) GPIO callback list on the
  second call. Fixed by moving `led_handler_init()`/
  `button_handler_init()` into the suite-level `setup` (once), keeping
  only pin/counter state reset in `before` (per test) - the same
  distinction Priority 1 already established for `codec_handler`, now
  confirmed to generalize to a second, unrelated module.

**A third, narrower bug:** the parameterized test's first draft declared
its `lc3_encoder_mem_48k_t` (several KB) as a stack-local inside the
`ZTEST` function body. That alone was enough to overflow
`CONFIG_ZTEST_STACK_SIZE=8192` under `mps3/an547`, even though the
*production* code and every earlier test in the file keep this class of
LC3 memory buffer `static`. Fixed by making the parameterized test's
buffers `static` too, matching that established convention instead of
reintroducing the pattern that caused Priority 2's stack overflow.

**Redundancy found and removed later, during Priority 5's platform
review:** `test_decode_valid_frame_produces_pcm` (a single hand-written
case at 48 kHz/10 ms, present since Priority 1) and the parameterized
test above overlapped - the parameterized grid's `{48000, 10000}` case
already exercises the identical encode→decode round trip and sample
count. The two weren't *quite* identical: the single-case test also
checked the PCM buffer was actually overwritten (not left at a
sentinel value), a check the parameterized test didn't do for any of
its 10 cases. Rather than just deleting the duplicate and losing that
check, folded the sentinel check into every parameterized case (now
broader coverage, not narrower) and removed the single-case test as
fully subsumed - along with the `encode_test_frame()` helper and
`codec_handler_test_setup()`, which existed only to serve it and would
otherwise have been dead code. Verified: `make test` passes clean (35
test cases, down from 38 - exactly the one removed case across the 3
platforms it ran on - 0 failed).

## Priority 4: HIL & Multi-Harness Integration — done

**Update: `make test-hil` was later actually run against real
hardware** (not just `--build-only`) - see
[hil_testing.md](hil_testing.md) for the setup steps, the bugs hit
getting it running at all (a stale `.egg-info` causing pytest plugin
double-registration - environment tooling, not this codebase), and a
real, still-unresolved bug in the UART logging `extra_configs` below
that's currently the reason the test doesn't pass yet even though the
device flashes and boots correctly.

**Board note (added later, when real hardware turned out to be the
plain nRF5340 DK, not the Audio DK):** everything below originally
targeted `ble_audio_board/nrf5340/cpuapp`, a custom board in the
`zephyr_boards` repo trimmed from Nordic's own nRF5340 Audio DK
definition. All board references here have been updated to
`nrf5340dk/nrf5340/cpuapp` (standard, upstream Zephyr board - no custom
board needed at all now) and re-verified against it (`--build-only`
below was re-run, not just relabeled). See "Board swap" below and the
root `README.md`'s Status section for what else that board swap changed
(`audio_handler` disabled - no I2S codec chip on this DK).

**What was built:** `make test-hil` — a separate Twister invocation, not
part of `make test`, that flashes the *real production image* (this
project's own `CMakeLists.txt`/`src/`/`prj.conf`, not a stub under
`tests/`) onto a physical nRF5340 DK and checks its actual boot log
over UART via a `pytest` harness. Pieces:

- `sample.yaml` (project root, not under `tests/`) — defines
  `ble_audio.hil_boot`, `harness: pytest`, `platform_allow:
  nrf5340dk/nrf5340/cpuapp`.
- `pytest/test_boot.py` — two cases against the real
  `twister_harness.DeviceAdapter` API: wait for `"Bluetooth initialized"`,
  then `"Advertising started"`, via `dut.readlines_until(regex=...,
  timeout=...)`.
- `tools/hardware-map.example.yml` — schema-correct template (J-Link
  `nrfjprog` runner, `serial`/`baud`) per
  `zephyr/scripts/schemas/twister/hwmap-schema.yaml`. Copy to
  `tools/hardware-map.yml` (gitignored — machine-specific) and fill in a
  real J-Link serial to use it.
- `Makefile`'s `test-hil` target: refuses to run without
  `tools/hardware-map.yml` present, otherwise wraps the same
  `libffi`/`NCS_TOOLCHAIN_VERSION=NONE` workarounds as `make test`.

**Placement follows a real sibling precedent, not a guess:** `blg_beacon`
puts its own HIL `sample.yaml` at the project root and reuses the real
app sources, rather than duplicating the app under `tests/`. Copied that
structure here for the same reason it exists there — a HIL test that
exercises a rebuilt/rewritten copy of the app isn't actually testing what
ships.

**A real integration bug found and fixed before ever touching
hardware:** the shipped `prj.conf` logs over RTT only
(`CONFIG_UART_CONSOLE` unset, no `CONFIG_LOG_BACKEND_UART`) — confirmed
via `grep` on `src/middlewares/ble_audio_handler/ble_audio_handler.c`'s
own `LOG_INF` calls. Twister's hardware pytest harness reads a UART
device, not RTT, so as written the harness would attach to a port that
never receives the log lines it's waiting for and time out — a bug that
would only have surfaced during an actual on-device run, potentially
burning a debugging session on hardware nobody had access to yet.
Fixed by adding `extra_configs` (`CONFIG_LOG_BACKEND_UART=y`,
`CONFIG_LOG_BACKEND_RTT=n`, `CONFIG_UART_CONSOLE=y`) scoped to just this
`sample.yaml` entry — modeled on
`zephyr/samples/subsys/testsuite/pytest/shell/testcase.yaml`'s use of the
same mechanism — so the HIL test build's console differs from the
shipped image's, without touching the product's own `prj.conf`.

**A Twister CLI quirk, found by reading `testplan.py` rather than
guessing (historical - applied to the old custom board, kept here since
it'll matter again if a custom board is ever reintroduced):**
`--board-root` on the `twister` CLI needs the trailing `/boards` segment
(e.g. `$ZEPHYR_BOARD_ROOT/boards`), unlike CMake's `BOARD_ROOT`, which
does not. Twister's own board-scanning code computes `board_roots =
[Path(os.path.dirname(root)) for root in self.env.board_roots]` with an
explicit comment that "internally in twister a board root includes the
`boards` folder but in Zephyr build system, the board root is without
the `boards`". Without the suffix, Twister reported the old
`ble_audio_board/nrf5340/cpuapp` as an "unrecognized platform" even
though the identical path worked fine for `west build -DBOARD_ROOT=...`.
Moot now that the board is `nrf5340dk` (standard, no board root needed)
- `test-hil`'s `Makefile` target no longer passes `--board-root` at all.

**What was actually verified, and what wasn't — stated plainly because
no nRF5340 DK was available in this environment:**

- Verified: `twister -T . --list-tests` discovers `ble_audio.hil_boot`
  correctly alongside all existing `native_sim`/`mps3` cases — the
  `sample.yaml` schema and placement are valid.
- Verified: `twister -T . -s ble_audio.hil_boot -p
  nrf5340dk/nrf5340/cpuapp --build-only` builds the **real production
  firmware** for the real board successfully, with the `extra_configs`
  actually applied — confirmed by grepping the generated `.config` for
  `CONFIG_UART_CONSOLE=y`, `CONFIG_LOG_BACKEND_UART=y`, and `#
  CONFIG_LOG_BACKEND_RTT is not set`. Re-run against `nrf5340dk` after
  the board swap, not just relabeled from the old result.
- Verified: the regular `make test` run is unaffected by the new
  root-level `sample.yaml` — it still finds exactly the same 3 test
  scenarios / 6 configurations under `-T tests` as before this phase, so
  the HIL sample isn't accidentally being picked up by CI-facing runs.
- **Not verified:** actually flashing a device, reading real UART output,
  or the `pytest` cases passing against live hardware. `test_boot.py` is
  written against the real `twister_harness` API and this project's real
  log strings, but that's as far as it can be checked without a DK. The
  `twister_harness` pip package (`zephyr/scripts/pylib/pytest-
  twister-harness`) is also not installed in this environment — noted as
  a prerequisite in the `Makefile` and README, not silently assumed.

**Scope decision:** the original plan mentioned driving a second device
as the LE Audio unicast client to prove a stream actually starts
end-to-end. That's deliberately deferred, not forgotten — this app is
sink-only (unicast server/headset), and standing up a second client
image is a separate build target and test topology, not a small addition
to this phase. `ble_audio.hil_boot` proves the one thing that's
meaningful without a second device: the real image boots and its BLE
stack comes up on real silicon. The two-device stream test is a
candidate for a later phase once single-device HIL is confirmed working
against real hardware.

## Priority 5: Regression, `ztress`, Shuffle — done

**What was built:** four pieces, split across what could be fully
verified in this environment and what genuinely couldn't.

**`CONFIG_ZTEST_SHUFFLE=y`** added to all three suites' `prj.conf`
(`codec_handler`, `app_streamctrl`, `gpio_handlers`). With the Kconfig
defaults (`ZTEST_SHUFFLE_SUITE_REPEAT_COUNT=3`,
`ZTEST_SHUFFLE_TEST_REPEAT_COUNT=3`), every suite now runs 3x with
shuffled order each time - a suite whose state leaks between tests or
depends on execution order fails here even if a single fixed-order run
passes. **Verified:** `make test` still passes clean (24 test cases,
4/4 executed configurations) - confirmed via the handler log that the
new ztress test case (below) actually executed 9 times (3 suite repeats
× 3 test repeats), not just once. This is the direct payoff of the
per-test `before`-hook resets fixed back in Priority 1/3 (`codec_handler`
release-on-reconfigure, `led_handler`/`button_handler_init()`
moved to suite-level `setup`) - those fixes are exactly what makes
shuffle safe to turn on now instead of exposing new failures.

**A `ztress` test added to `tests/app_streamctrl/`**
(`test_stream_recv_survives_concurrent_button_presses`). Two `ZTRESS_THREAD`
contexts run at once: one repeatedly drives `stream_recv` (the BT ISO RX
path), the other repeatedly drives a button press (the GPIO IRQ/workqueue
path). This pairing was chosen deliberately, not the first thing that
compiled: two *concurrent* `stream_recv` calls can't happen on real
hardware (there's exactly one RX path), so stressing that would only
prove a violation of a contract nothing ever exercises. Audio RX and a
button press, on the other hand, genuinely are two independent contexts
that can interleave on real hardware - a realistic scenario worth
covering. Followed upstream `tests/ztest/ztress`'s own idiom of
range/existence assertions (`ztress_exec_count(n) > 0`) rather than exact
call-count equality, since exact counts aren't meaningful under ztress's
timing jitter - asserting equality here would make the test itself
flaky, not app_streamctrl.

**Real gap found while wiring this up (not a bug in ble_audio's own
code, but a real gap in its CI-readiness):** the project had no root
`.gitlint` file. `tools/gitlint/gitlint-rules.py` (the custom
uppercase-type rule already followed in every commit this session) was
present, but nothing wired it in - every sibling project
(`blg_beacon`, `vx_ioboard_fw`, and the canonical
`paltatech/paltatech-guidelines-docs`) has a root `.gitlint` pointing
`extra-path` at that file. Without it, a `gitlint` CI job would validate
commits against gitlint's bare defaults and silently not enforce the
`TYPE(scope): outline` convention this whole session has followed.
Fixed by adding `.gitlint`, diffed byte-for-byte identical against
`paltatech/paltatech-guidelines-docs`'s copy to confirm it's not a
guess.

**Five CI workflows added under `.github/workflows/`**, one per
concern, matching how `paltatech/paltatech-guidelines-docs`'s own
`workflow-templates/` and every sibling project split things up (rather
than one monolithic workflow):

- `gitlint.yml` - copied verbatim from
  `paltatech/paltatech-guidelines-docs`'s `workflow-templates/gitlint.yml`.
  Public `jorisroovers/gitlint` Docker image, no secrets needed.
- `clang-format.yml` - installs Ubuntu's `clang-format-15` package (no
  private release repo dependency) and symlinks it to `./clang-format`
  so `make lint-ci` (which expects a local binary - the same contract
  sibling projects use for their own pinned copy) doesn't need
  changing. Version choice isn't arbitrary:
  `paltatech-guidelines-docs/src/coding-style.md` explicitly pins clang-format
  `15.0.7` ("not all clang-format versions produces the same result"),
  and Ubuntu's `clang-format-15` package matches that major/minor -
  confirmed locally (`clang-format --version` → `15.0.7` exactly).
- `cmake-format.yml` - `pip install cmakelang` then `make lint-cmake`,
  reusing the project's own existing target rather than a third-party
  Action.
- `test.yml` - the actual Priority 5 deliverable: installs
  `qemu-system-arm` and an NCS toolchain via `nrfutil` (the same
  mechanism every sibling Viaanix-era workflow already uses, and it's a
  public Nordic tool with no paltatech/Viaanix branding involved), sets
  up a fresh west workspace, and runs `make test`.
- `compile.yml` - builds the real production firmware on every push,
  reusing `test.yml`'s toolchain/west scaffolding but running
  `./prepare_release.sh` instead. Two decisions here were the user's
  call, not guessed: fails the job on **any** compiler warning
  (`vx_ioboard_fw`'s stricter behavior, not `blg_beacon`'s
  log-only-don't-fail one), and uses `ubuntu-latest` + `nrfutil` rather
  than `self-hosted` (matching `test.yml`, for the same
  can't-confirm-a-runner-is-provisioned reason). Deliberately skips the
  private `Viaanix/gcc-problem-matcher` dependency both sibling
  `compile.yml`s use for inline PR annotations - warnings still fail the
  job and show in the raw log, just without the annotation nicety.
  Uploads the built firmware (`release/*` from `prepare_release.sh`) as
  a downloadable Actions artifact, which neither sibling workflow does -
  a "compile" job that doesn't leave a moved firmware image behind
  doesn't produce anything worth keeping.

**What was actually verified, and what wasn't - same honesty standard
as Priority 4's HIL section:**

- Verified: `make test` passes with `CONFIG_ZTEST_SHUFFLE` enabled and
  the new ztress test in place (24 test cases, 4/4 configurations, 0
  failed) - re-ran from a clean `twister-out`/`.cache` to rule out stale
  state.
- Verified: the ztress test's two contexts both actually ran under load
  (`ztress_exec_count(0)`/`(1)` both `> 0`, `audio_handler_write`
  actually got called), across all 9 shuffled repeats, not just once.
- Verified: `make lint-ci` (with a locally-installed `clang-format-15`
  symlinked to `./clang-format`, matching what `clang-format.yml` does
  in CI) reformats only files legitimately touched this phase and
  leaves everything else untouched - the CI job's "fail on any diff"
  logic would have passed on this repo's current state.
- Verified: `make lint-cmake` produces no diff on this repo's current
  state - the `cmake-format.yml` job's logic would pass too.
- Verified: all five workflow YAML files parse as valid YAML.
- **Not verified:** an actual GitHub Actions run of any of the five
  workflows. There's no way to trigger real Actions runs from this
  environment. Both `test.yml` and `compile.yml`'s "Init west workspace"
  steps assume a `MY_GITHUB_TOKEN` secret exists with read access to
  `paltatech/vx_sdk_nrf` and `paltatech/vx_zephyr_boards` - the same
  secret name every sibling Viaanix-era workflow uses for the same
  purpose, so it's a reasoned choice, not a random guess, but it's
  genuinely unconfirmed whether that secret is configured for the
  `ble_audio` repo specifically. If it isn't, `west update` will fail at
  the private-repo clone step in both workflows. Whoever adds this repo
  to GitHub Actions should confirm that secret (or an equivalent) is set
  before relying on either of them.
- **Scope decision:** `paltatech-guidelines-docs/workflow-templates/compile.yml`
  and its siblings' equivalents use `runs-on: self-hosted` with a
  pre-provisioned image containing the toolchain. `test.yml` and
  `compile.yml` here use `ubuntu-latest` with `nrfutil` instead
  (confirmed with the user), since self-hosted runner availability for
  this specific repo couldn't be confirmed either. If paltatech's
  self-hosted runners already have an NCS toolchain pre-installed,
  switching both to `self-hosted` and dropping the `nrfutil` install
  step would likely be both simpler and faster.

## Board swap: nRF5340 Audio DK → nRF5340 DK

Real hardware turned out to be the plain nRF5340 DK, not the Audio DK
this project originally targeted. Changes made across all five
priorities:

- **Board target:** `BOARD` in `tools/make/config.mk` changed from the
  custom `ble_audio_board/nrf5340/cpuapp` (a `zephyr_boards`-repo copy of
  Nordic's Audio DK board definition) to the standard, upstream
  `nrf5340dk/nrf5340/cpuapp` — no custom board needed at all now.
  Verified: `west build` succeeds for the real production app with no
  `--board-root`/`BOARD_ROOT` needed. `zephyr_boards`
  (`vx_zephyr_boards@ble_audio_board`) is still pulled by `west.yml` but
  is no longer referenced anywhere in this repo's build path - left in
  place pending a decision on whether to drop it (see root
  `README.md`'s Status section).
- **`audio_handler` (I2S output) disabled:** the nRF5340 DK has no I2S
  codec chip wired up - its `i2s0` devicetree node exists (SoC-level)
  but is left `status = "disabled"`, with no board overlay enabling it.
  Confirmed via a real build failure before commenting anything out:
  compiling `audio_handler.c` against `nrf5340dk` fails at
  `DEVICE_DT_GET(DT_NODELABEL(i2s0))` (`__device_dts_ord_... undeclared`
  - the same class of error `native_sim/native/64` hit for
  `gpio_handlers`'s missing overlay, above). Fixed by commenting out (not
  deleting) `audio_handler`'s inclusion in
  `src/middlewares/CMakeLists.txt` and its call sites in
  `app_streamctrl.c` (`audio_handler_init()`, `audio_handler_write()`) -
  `codec_handler_decode()` still runs, so the BLE receive → LC3 decode
  pipeline is still fully exercised; only the final I2S write is
  disabled.
- **`tests/app_streamctrl/` updated to match:** the FFF fakes/assertions
  for `audio_handler_init`/`audio_handler_write` are commented out too,
  not left asserting behavior that no longer happens.
  `test_stream_recv_decodes_and_writes_audio` was renamed to
  `test_stream_recv_decodes_frame` (it no longer writes audio) with the
  write-specific assertions commented out.
  `test_stream_recv_skips_audio_write_on_decode_error` is commented out
  entirely - with `audio_handler_write()` never called regardless of
  decode outcome, that test no longer distinguishes anything, so it
  would just be a check that trivially always passes (the same
  redundancy standard applied earlier in Priority 3). The `ztress`
  test's final assertion was switched from
  `audio_handler_write_fake.call_count` (now always 0) to
  `codec_handler_decode_fake.call_count` (still active) as the "real
  work happened under load" signal. Verified: `make test` passes clean
  after all of this (6/6 executed configurations, 0 failed).
- **4 LEDs / 4 buttons available, not yet used:** the nRF5340 DK exposes
  `led0`-`led3` and `sw0`-`sw3` (`button0`-`button3`) as standard
  aliases - confirmed by reading
  `zephyr/boards/nordic/nrf5340dk/nrf5340_cpuapp_common.dtsi` directly.
  `led_handler.c`/`button_handler.c` already used `DT_ALIAS(led0)`/
  `DT_ALIAS(sw0)` (standard names, not board-specific ones), so both
  needed zero changes to keep working on the new board. The app still
  only drives one LED (connection status) and one button (generic press
  callback) - the other 3 of each are physically available but nothing
  in this codebase uses them yet.
- **HIL test (`sample.yaml`, `tools/hardware-map.example.yml`,
  `pytest/test_boot.py`) retargeted and re-verified**, not just
  relabeled - see the board note under Priority 4 above for what was
  re-run.

## Real hardware bug: network core had no ISO/Extended Advertising support

Found via a real manual `make build`/`make flash` + J-Link RTT session on
the actual nRF5340 DK (not `make test-hil` - a plain manual flash) -
exactly the kind of thing HIL testing exists to catch, caught here by a
human reading a live log instead.

**Symptom:** `"Bluetooth initialized"` printed, but `"Advertising
started"` never did, with no error either - looked like a hang.
Button-press logging kept working throughout (registered earlier in
`app_streamctrl_start()`, in an independent GPIO interrupt path), which
is what made it look like a partial/silent hang rather than a clean
failure at first glance.

**Diagnosis:** a first attempt at more logging (`CONFIG_LOG_DEFAULT_LEVEL=4`)
was the wrong tool - it also enabled the kernel's own `os` module debug
logging (`z_impl_k_mutex_lock`/`unlock`), which fires fast enough to
flood the deferred log buffer and drop the very lines needed ("N
messages dropped" between nearly every line). Switched to a targeted
`CONFIG_BT_HCI_CORE_LOG_LEVEL_DBG=y` instead (per-module Kconfig,
doesn't touch the kernel's own logging) and got a clean capture. That
showed:
- `bt_ascs: Failed to register ISO server -134` (`-ENOTSUP`) - the
  controller has no ISO support.
- `opcode 0x2036 status 0x01` (`LE_Set_Extended_Advertising_Parameters`,
  `Unknown HCI Command`) - no LE Extended Advertising support either.
- Both errors are non-fatal individually, but the second one aborts
  `bt_le_ext_adv_create()` → `ble_audio_handler_start()` →
  `app_streamctrl_start()` with a real `-EIO`, so nothing after it in
  the startup sequence ever runs.

**Root cause:** the network core (`hci_ipc`, built automatically via
`CONFIG_NCS_INCLUDE_RPMSG_CHILD_IMAGE`) was using the *default* upstream
`hci_ipc` sample config, which enables no ISO or Extended Advertising
support in the controller at all. `prj.conf`'s
`CONFIG_BT_ISO_PERIPHERAL`/`CONFIG_BT_EXT_ADV` only configure the
*host*, running on the app core - the controller is a completely
separate build (a different Kconfig namespace entirely) that never
inherited any of it. This isn't specific to the board swap - the
default `hci_ipc` sample would have had the same gap regardless of
which nRF5340 board was targeted; it just hadn't been exercised on real
hardware until now.

**Fix:** `child_image/hci_ipc.conf` - NCS's own mechanism
(`nrf/cmake/multi_image.cmake`) for injecting Kconfig into a child
image by file path alone, no `--sysbuild` needed (consistent with how
this project already builds). Verified the controller is actually
Nordic's SoftDevice Controller (`CONFIG_BT_LL_SOFTDEVICE=y`), not
Zephyr's own open-source split link layer, by reading the real
`.config` from an actual local build
(`_build_ble_audio_nrf5340dk_nrf5340_cpuapp/hci_ipc/zephyr/.config`) -
important, because Zephyr's own upstream reference fragment for this
exact role
(`zephyr/samples/bluetooth/hci_ipc/nrf5340_cpunet_iso_peripheral-bt_ll_sw_split.conf`)
targets the *other* controller and forces `CONFIG_BT_LL_SW_SPLIT=y`,
which would have fought the already-selected SDC. Used SDC-appropriate
options instead (`CONFIG_BT_EXT_ADV`, `CONFIG_BT_ISO_PERIPHERAL`,
`CONFIG_BT_CTLR_ADV_EXT`, `CONFIG_BT_CTLR_PERIPHERAL_ISO`,
`CONFIG_BT_CTLR_SDC_PERIPHERAL_COUNT`), cross-checked against real
Nordic ISO sample configs
(`nrf/samples/bluetooth/iso_time_sync/sysbuild/hci_ipc/prj.conf`) for
the SDC-specific symbols.

**Verified, end to end:** built the fragment in, confirmed via a fresh
`.config` that `CONFIG_BT_CTLR_ADV_EXT=y`/`CONFIG_BT_CTLR_PERIPHERAL_ISO=y`
now land; `make test` unaffected (6/6 configurations, 0 failed - this
only touches the network-core build, not `tests/`). The user then
reflashed real hardware and confirmed via a new RTT log:
`le_read_maximum_adv_data_len_complete: status 0x00` (previously
`0x01`), the extended-advertising HCI sequence (opcodes `0x2035`/
`0x2037`/`0x2039`) all completing `status 0x00`, and
`"Advertising started"` printing - genuine on-device confirmation, not
just a clean build.

## Real hardware bug: advertising didn't resume after disconnect

Found continuing the same real-hardware session as the fix above - once
advertising actually started, a real central connected (confirmed via
RTT log: `bt_hci_le_adv_set_terminated` when the connection formed,
`le_phy_update_complete` negotiating 2M PHY, real ACL data flowing both
directions), stayed connected for ~13 seconds, then disconnected
cleanly (`hci_disconn_complete: status 0x00 ... reason 0x13` -
BT_HCI_ERR_REMOTE_USER_TERM_CONN, the peer disconnected intentionally).
Immediately after: `ble_audio_handler: Failed to start advertising: -12`
(`-ENOMEM`) - the device stopped being discoverable/connectable the
moment anything disconnected from it.

**Root cause:** `disconnected()` in `ble_audio_handler.c` called
`start_advertising()` (→ `bt_le_ext_adv_start()`) directly and
synchronously. Zephyr's own `bt_conn_cb.disconnected` doc comment
(`zephyr/include/zephyr/bluetooth/conn.h`) warns against exactly this:
the just-freed connection object isn't necessarily back in the pool yet
when `disconnected()` runs, so immediately trying to advertise (which
needs to reserve a connection object for the next potential connection)
can race and fail with `-ENOMEM`. The same header documents the fix:
`bt_conn_cb.recycled` - "A connection object has been returned to the
pool... Use this to e.g. re-start connectable advertising."

**Fix:** moved the `start_advertising()` call from `disconnected()` to
a new `recycled()` callback, registered in the same
`BT_CONN_CB_DEFINE(conn_callbacks)`. `disconnected()` still runs the
app-level callback (LED off, etc.) - only the re-advertising call moved.

**Verified:** builds clean for `nrf5340dk/nrf5340/cpuapp` (same FLASH
size as before - the change is a few bytes), `make test` unaffected
(`ble_audio_handler.c` isn't compiled into any suite - only its header,
for `app_streamctrl`'s FFF fakes). Not yet re-verified on hardware
against an actual disconnect/reconnect cycle - the fix is grounded
directly in Zephyr's own documented API contract, not just a build-clean
guess, but a second real-hardware confirmation (connect → disconnect →
confirm advertising resumes) is the natural next check.

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
- `zephyr/tests/lib/acpi/unit` — real in-tree FFF usage (fake
  declarations, `custom_fake`, resetting fakes in a `before` hook);
  template for `tests/app_streamctrl/`.
- `zephyr/boards/native/native_sim/native_sim.dts` and
  `zephyr/include/zephyr/drivers/gpio/gpio_emul.h` — native_sim's
  built-in `zephyr,gpio-emul` controller and its `gpio_emul_input_set()`/
  `gpio_emul_output_get()` API; used directly by `tests/gpio_handlers/`.
- `blg_beacon`'s root-level `sample.yaml` — precedent for placing a HIL
  test's `sample.yaml` at the project root and reusing the real app
  sources, instead of duplicating the app under `tests/`; template for
  this project's own `sample.yaml`.
- `zephyr/samples/subsys/testsuite/pytest/shell/testcase.yaml` — real
  in-tree use of `extra_configs` to change a test build's console/log
  backend without touching the product's own `prj.conf`; template for
  the RTT→UART override here.
- `zephyr/scripts/schemas/twister/hwmap-schema.yaml` and
  `zephyr/scripts/pylib/pytest-twister-harness` — hardware-map field
  schema and the `twister_harness.DeviceAdapter` API used by
  `tools/hardware-map.example.yml` and `pytest/test_boot.py`.
- `zephyr/scripts/pylib/twister/twisterlib/testplan.py` — source of the
  `--board-root` trailing-`/boards` requirement (see above).
- `zephyr/tests/ztest/ztress/src/main.c` — real in-tree `ztress` usage;
  template for `test_stream_recv_survives_concurrent_button_presses`'s
  `ZTRESS_THREAD`/`ZTRESS_EXECUTE` calls and its range-based
  (`ztress_exec_count(n) > 0`, not exact-equality) assertion style.
- `paltatech/paltatech-guidelines-docs` (`workflow-templates/`,
  `src/coding-style.md`, `.gitlint`) — authoritative source for the CI
  workflow structure (one workflow per concern), the clang-format 15.0.7
  version pin, and the gitlint config; cross-checked against sibling
  projects (`blg_beacon`, `vx_ioboard_fw`) for the parts the guidelines
  repo doesn't cover (there's no west/NCS/twister CI template upstream -
  `test.yml` here is new ground, modeled on the sibling Viaanix-era
  `compile.yml`'s `nrfutil`/`west init` mechanics instead).
- `paltatech/vx_smart-pro-box-2-fw-host@e1219e8` and `@16f0c74` — source
  of the `native_sim` 32-vs-64-bit correction above: the first commit
  switched that project from `native_sim/native/64` to plain
  `native_sim` (discovering the latter is the 32-bit variant, and the
  cost of running both is a devicetree overlay that has to be named per
  qualifier); the second added `gcc-multilib`/`g++-multilib` to their CI
  after hitting a real `bits/libc-header-start.h` failure without it.
  `boards/native/native_sim/board.yml` and a direct `file` check on a
  built `ble_audio` binary (`ELF 32-bit ... Intel 80386`) confirmed the
  same is true here, independent of trusting the commit messages alone.
- `zephyr/samples/bluetooth/hci_ipc/nrf5340_cpunet_iso_peripheral-bt_ll_sw_split.conf`
  and `nrf/samples/bluetooth/iso_time_sync/sysbuild/hci_ipc/prj.conf` —
  source for `child_image/hci_ipc.conf`'s Kconfig options (split link
  layer reference for the option *names*/relationships; the SDC sample
  for which symbols actually apply to this board's real controller).
  `nrf/subsys/bluetooth/controller/Kconfig` — source for
  `CONFIG_BT_CTLR_SDC_PERIPHERAL_COUNT`'s meaning/defaults.
- `zephyr/include/zephyr/bluetooth/conn.h` — `bt_conn_cb.disconnected`'s
  and `.recycled`'s doc comments; source for both the diagnosis and the
  fix of the advertising-doesn't-resume-after-disconnect bug above.
