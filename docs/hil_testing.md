# HIL Testing (`make test-hil`) — Setup, Bugs Found, and Current Status

Findings from actually running `make test-hil` against a real nRF5340 DK,
not just building it. See [testing_guide.md](testing_guide.md) for how
`test-hil` works mechanically (`sample.yaml`, `pytest/test_boot.py`,
what gets flashed). This document is specifically about *making it run
at all* and what's still broken.

## Status

| Piece | Status |
|---|---|
| Build (both cores) | ✅ Works |
| Flash | ✅ Works |
| `pytest`/`twister_harness` plugin loading | ✅ Fixed (was broken) |
| Actual test result | ❌ Still fails - UART logging bug, unresolved |

---

## Tools that need to be installed

This toolchain's bundled Python (`~/ncs/toolchains/<hash>/usr/local/bin/python3`,
the one `source ./start-zephyr-env.sh` puts on `PATH`) **cannot install
`twister_harness` via `pip`** - it's missing `libssl.so.1.1` entirely
(`ImportError: libssl.so.1.1: cannot open shared object file`), so every
network `pip install` fails. This is a separate, deeper issue than the
`libffi.so.7` gap `make test`/`make test-hil` already work around - that
one has a shim; this one doesn't, because Twister's `Pytest` harness
class shells out to a plain `pytest` command
(`zephyr/scripts/pylib/twister/twisterlib/harness.py`), not
`sys.executable -m pytest` - so **it doesn't have to be the toolchain's
own Python**. Use the system one instead:

```bash
# Matches NCS's own pins (nrf/scripts/requirements-fixed.txt) - newer
# pytest/pluggy weren't the actual root cause found below, but pinning
# to what NCS itself uses is still the right default to avoid finding
# out the hard way later.
/usr/bin/python3 -m pip install --user "pytest==7.4.2" "pluggy==1.3.0"
/usr/bin/python3 -m pip install --user pykwalify pyserial psutil
```

Do **not** `pip install` the `pytest-twister-harness` package itself
(`zephyr/scripts/pylib/pytest-twister-harness`) - see Bug 1 below for
why. Its dependencies (`pyserial`, `psutil`, `pykwalify` - the last one
pulled in transitively by Zephyr's own `domains.py` helper) are all you
need; Twister's generated pytest command already puts the harness
source on `PYTHONPATH` directly.

Then, every time before running `make test-hil`, make sure the
system-installed `pytest` is what actually resolves in `PATH` (the
toolchain has its own `pytest` binary too, and it comes first once
`start-zephyr-env.sh` is sourced):

```bash
source ./start-zephyr-env.sh
export PATH="$HOME/.local/bin:$PATH"
```

Plus the usual `tools/hardware-map.yml` (copy from
`tools/hardware-map.example.yml`, fill in your J-Link serial from
`nrfjprog --ids` and UART path from `ls /dev/serial/by-id/`).

---

## Bugs found, in the order they showed up

### Bug 1: `pytest-twister-harness` plugin registered twice

**Symptom:** `make test-hil` failed in under a second, before any build
or flash activity, with:
```
ValueError: Plugin already registered under a different name: twister_harness=<module 'twister_harness.plugin' from ...>
```

**Root cause:** an earlier troubleshooting attempt (trying to `pip
install` the harness package using the toolchain's broken-SSL Python,
see above) failed partway through, but not before `setup.py egg_info`
had already run and written a
`pytest_twister_harness.egg-info/` directory **directly inside**
`zephyr/scripts/pylib/pytest-twister-harness/src/` - right next to the
real `twister_harness/` package. Twister's own generated pytest command
puts that exact `src/` directory on `PYTHONPATH` (so the harness module
is importable without installing it) *and* explicitly loads it via
`-p twister_harness.plugin`. Python's `importlib.metadata` scans every
`PYTHONPATH` entry for `.egg-info`/`.dist-info` folders - it found the
stale one, saw it declared a `pytest11` entry point
(`twister_harness = twister_harness.plugin`, from the package's own
`setup.cfg`), and tried to auto-register the *same* already-loaded
plugin a second time under a different internal key. Confirmed by
directly enumerating entry points with the exact interpreter Twister
uses - it pointed straight at the stale directory:
```python
import importlib.metadata
for dist in importlib.metadata.distributions():
    for ep in dist.entry_points:
        if ep.group == 'pytest11' and ep.name == 'twister_harness':
            print(dist._path)
# -> .../zephyr/scripts/pylib/pytest-twister-harness/src/pytest_twister_harness.egg-info
```

**Fix:**
```bash
rm -rf zephyr/scripts/pylib/pytest-twister-harness/src/pytest_twister_harness.egg-info
```
This is an environment/tooling bug, not a `ble_audio` code or config
bug - if you never `pip install` the harness package directly (see
Tools section above), it shouldn't recur. If it does, the same fix
applies.

### Bug 2 (ruled out, documented so it isn't re-suspected): pytest/pluggy version mismatch

Before finding Bug 1's real cause, newer `pytest`/`pluggy` (9.1.1/1.6.0,
whatever `pip install pytest` grabs by default today) looked like a
plausible suspect, since NCS pins much older versions
(`pytest==7.4.2`, `pluggy==1.3.0`). Pinned both to match exactly -
**the crash persisted identically.** So version mismatch was not
actually the cause here. Left the pins in place anyway (Tools section
above) since matching what NCS itself validates against is the right
default regardless, but if this exact error resurfaces, check for a
stale `.egg-info` (Bug 1) before re-suspecting versions.

### Bug 3 (open, unresolved): UART backend doesn't deliver `LOG_INF` output

**Symptom:** once Bug 1 was fixed, `make test-hil` ran for real - built,
flashed both cores successfully, then both `pytest/test_boot.py`
assertions timed out (`TwisterHarnessTimeoutException: Read from device
timeout occurred`).

**Investigation:** read `/dev/ttyACM0` directly with `stty`/`cat`,
bypassing `pytest`/`twister_harness` entirely, to see the raw device
output:
```bash
stty -F /dev/ttyACM0 115200 raw -echo
cat /dev/ttyACM0 &        # start listening first
nrfjprog --reset --snr <your-serial>
```
Result: only the two-line boot banner
(`*** Booting nRF Connect SDK ... ***`) ever appears, waited up to 25
seconds. None of the application's `LOG_INF` calls
(`"Bluetooth initialized"`, etc.) reach UART at all - even though
they're confirmed working over **RTT** (see
`testing_ecosystem.md`'s two real-hardware BLE bug writeups, both found
via RTT logs on this exact build).

Tried `CONFIG_LOG_MODE_IMMEDIATE=y` instead of the default
`CONFIG_LOG_MODE_DEFERRED=y` as a diagnostic (not committed - a
one-off `west build -DCONFIG_LOG_MODE_IMMEDIATE=y ...` in a scratch
build dir) to see if this was a deferred-logging-thread starvation
issue. Result was worse, not better: the boot banner printed **four
times in a row**, meaning the device resets repeatedly under that
config - not just "logging doesn't show up," but "immediate-mode UART
logging causes a reset loop" at this point in boot.

**Not yet resolved.** This is a real, separate bug from Bug 1 - it's
specific to `sample.yaml`'s `extra_configs` (`CONFIG_LOG_BACKEND_UART=y`,
`CONFIG_LOG_BACKEND_RTT=n`, `CONFIG_UART_CONSOLE=y`), which had never
actually been exercised against real hardware before this session (the
build-only verification in `testing_ecosystem.md`'s Priority 4 section
checked the `.config` was correct, not that logging actually worked
over the wire). Suspected next step: RTT-based debugging of exactly
what happens right after the boot banner with the UART `extra_configs`
applied (RTT still works even when the UART backend is selected, since
they're independent backends and only the *default* build disables
RTT - a one-off diagnostic build could keep both backends active at
once to compare).

---

## Running it yourself

```bash
cd ble_audio
source ./start-zephyr-env.sh
export PATH="$HOME/.local/bin:$PATH"

cp tools/hardware-map.example.yml tools/hardware-map.yml
# edit tools/hardware-map.yml: id (nrfjprog --ids), serial (ls /dev/serial/by-id/)

make test-hil
```

## Expected result today

Build and flash succeed; both `pytest` cases fail with
`TwisterHarnessTimeoutException` (Bug 3 above). This is a genuine
product-code/config bug, not a setup problem - if you hit anything
*other* than a timeout after a successful flash (a plugin
`ValueError`, an install failure, etc.), that's Bug 1/2 territory
above, not Bug 3.

## Expected result once Bug 3 is fixed

```
test_bluetooth_initializes PASSED
test_advertising_starts PASSED
```
matching the real boot sequence already confirmed working over RTT
elsewhere in this project.
