# HIL Testing (`make test-hil`) — Setup, Bugs Found, and Current Status

Findings from actually running `make test-hil` against a real nRF5340 DK,
not just building it. See [testing_guide.md](testing_guide.md) for how
`test-hil` works mechanically (`sample.yaml`, `harness: console`, what
gets flashed). This document is specifically about *making it run at
all* and what's still broken.

## Status

| Piece | Status |
|---|---|
| Build (both cores) | ✅ Works |
| Flash | ✅ Works |
| Harness (Twister's `harness: console`) | ✅ Works, no setup needed |
| Actual test result | ❌ Still fails - UART logging bug, unresolved |

---

## Tools that need to be installed

**None**, beyond what `README.md`'s Prerequisites already cover, plus
`tools/hardware-map.yml` (copy from `tools/hardware-map.example.yml`,
fill in your J-Link serial from `nrfjprog --ids` and UART path from
`ls /dev/serial/by-id/`).

`sample.yaml` uses Twister's built-in `harness: console` (ordered regex
matching against the UART stream) - no `pytest`, no `twister_harness`
plugin, nothing extra to install. That wasn't the original design (see
"History" below); it's the result of simplifying away a real, separate
source of fragility once it was found.

---

## History: this used to need `pytest`, and that was a real problem

The original `sample.yaml` used `harness: pytest` with a
`pytest/test_boot.py` doing the same two-line check
`harness: console` does now. `harness: pytest` is the right tool when a
HIL scenario needs actual multi-step Python logic (driving a second
device, complex parsing, retries) - a fixed boot-log check was never
one of those, and the extra machinery cost real debugging time:

### Bug: `pytest-twister-harness` plugin registered twice

**Symptom:** `make test-hil` failed in under a second, before any build
or flash activity, with:
```
ValueError: Plugin already registered under a different name: twister_harness=<module 'twister_harness.plugin' from ...>
```

**Root cause:** an earlier troubleshooting attempt (trying to `pip
install` the harness package using this toolchain's Python, which is
missing `libssl.so.1.1` entirely and can't reach the network) failed
partway through, but not before `setup.py egg_info` had already run and
written a `pytest_twister_harness.egg-info/` directory **directly
inside** `zephyr/scripts/pylib/pytest-twister-harness/src/` - right next
to the real `twister_harness/` package. Twister's own generated pytest
command puts that exact `src/` directory on `PYTHONPATH`. Python's
`importlib.metadata` scans every `PYTHONPATH` entry for
`.egg-info`/`.dist-info` folders, found the stale one, saw it declared a
`pytest11` entry point, and tried to auto-register the *same*
already-loaded plugin a second time under a different internal key.
Confirmed by directly enumerating entry points with the exact
interpreter Twister uses - it pointed straight at the stale directory.

Also ruled out, in case this resurfaces and looks similar: pytest/pluggy
version mismatch. Pinned both to NCS's exact versions
(`pytest==7.4.2`, `pluggy==1.3.0` from
`nrf/scripts/requirements-fixed.txt`) before finding the real cause -
the crash persisted identically. Versions weren't it.

**This class of bug is now moot** - `sample.yaml` doesn't use `pytest`
at all anymore, so there's no plugin to double-register. Documented here
so the history (and the diagnostic technique - enumerating
`importlib.metadata` entry points directly, and reading raw device
output with `stty`/`cat` to sidestep a test framework entirely) isn't
lost, and in case a future HIL scenario genuinely needs `harness:
pytest` again.

---

## Bug found, still open: UART backend doesn't deliver `LOG_INF` output

**Symptom:** build and flash succeed every time; the actual check never
passes. Confirmed two independent ways:

1. Reading `/dev/ttyACM0` directly with `stty`/`cat`, bypassing any test
   framework entirely:
   ```bash
   stty -F /dev/ttyACM0 115200 raw -echo
   cat /dev/ttyACM0 &        # start listening first
   nrfjprog --reset --snr <your-serial>
   ```
   Only the two-line boot banner (`*** Booting nRF Connect SDK ... ***`)
   ever appears, waited up to 25 seconds.
2. `make test-hil` itself (after the `harness: console` simplification
   above) fails with a plain `Timeout`, and `handler.log` shows exactly
   the same two banner lines and nothing else.

Neither the application's `LOG_INF` calls (`"Bluetooth initialized"`,
etc.) reach UART - even though they're confirmed working over **RTT**
(see `testing_ecosystem.md`'s two real-hardware BLE bug writeups, both
found via RTT logs on this exact build).

**Diagnostic tried:** `CONFIG_LOG_MODE_IMMEDIATE=y` instead of the
default `CONFIG_LOG_MODE_DEFERRED=y`, as a one-off scratch build (not
committed), to check whether this was a deferred-logging-thread
starvation issue. Result was worse, not better: the boot banner printed
**four times in a row**, meaning the device resets repeatedly under that
config - not just "logging doesn't show up," but "immediate-mode UART
logging causes a reset loop" at this point in boot.

**Not yet resolved.** Specific to `sample.yaml`'s `extra_configs`
(`CONFIG_LOG_BACKEND_UART=y`, `CONFIG_LOG_BACKEND_RTT=n`,
`CONFIG_UART_CONSOLE=y`), which had never actually been exercised
against real hardware before this investigation (the build-only
verification in `testing_ecosystem.md`'s Priority 4 section checked the
`.config` was correct, not that logging actually worked over the wire).
Suspected next step: RTT-based debugging of exactly what happens right
after the boot banner with the UART `extra_configs` applied (RTT still
works even when the UART backend is selected, since they're independent
backends and only the *default* build disables RTT - a one-off
diagnostic build could keep both backends active at once to compare).

---

## Running it yourself

```bash
cd ble_audio
source ./start-zephyr-env.sh
cp tools/hardware-map.example.yml tools/hardware-map.yml
# edit tools/hardware-map.yml: id (nrfjprog --ids), serial (ls /dev/serial/by-id/)
make test-hil
```

## Expected result today

Build and flash succeed; the test fails with `Timeout` (the open bug
above). This is a genuine product-code/config bug, not a setup problem.

## Expected result once the UART bug is fixed

```
PASSED
```
with `handler.log` showing the full boot sequence through
`"Advertising started"`, matching what's already confirmed working over
RTT elsewhere in this project.
