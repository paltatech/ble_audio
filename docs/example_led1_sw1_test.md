# Worked Example: Adding `led1`/`sw1` Test Coverage

A concrete, step-by-step walkthrough of extending `led_handler`/
`button_handler` to a second LED and button, and testing both on
`native_sim` via `zephyr,gpio-emul` - following the general process in
[testing_guide.md](testing_guide.md#how-to-add-a-new-test), applied to a
real case. Use this as a template for adding the remaining `led2`/`led3`/
`sw2`/`sw3` later, or any other indexed-hardware addition.

## Why this isn't just "add a test"

`led_handler`/`button_handler` currently only know about *one* LED and
*one* button each (`led_handler_set(bool on)`, a single hardcoded
`DT_ALIAS(led0)`). The nRF5340 DK has `led0`-`led3`/`sw0`-`sw3`
available (see `testing_ecosystem.md`'s "Board swap" section), but
nothing in this codebase can address the other three. Testing `led1`
means the production API has to be able to *reach* `led1` first - this
is a small production-code change, not just a test addition, and the
plan below treats it that way.

## Step 1 — Decide the API shape

Two options for going from "one LED" to "more than one":

- **Indexed** (`led_handler_set(uint8_t led_id, bool on)`) - one
  function, scales to `led2`/`led3` for free, but changes the existing
  signature (breaking change for the two existing call sites in
  `app_streamctrl.c` and the FFF fake in `tests/app_streamctrl/`).
- **Parallel functions** (`led_handler_set`/`led_handler_set_led1`) -
  no breaking change, but doesn't scale (a fifth function for `led2`,
  a sixth for `led3`), and duplicates the exact same logic four times
  over.

**Decision: indexed.** The blast radius of the breaking change is small
and mechanical (2 call sites in production code, 1 FFF fake signature,
a handful of test assertions) - see Step 4/5 below - and it's the only
option that doesn't get worse every time another LED/button gets used.
Applies to `button_handler`'s callback too: `button_handler_pressed_cb_t`
gains a `uint8_t button_id` parameter instead of adding a second,
parallel callback type.

## Step 2 — Devicetree: give `native_sim` a `led1`/`sw1`

`native_sim`'s own `.dts` only defines `led0` (see
`zephyr/boards/native/native_sim/native_sim.dts`) and no buttons at all
- `tests/gpio_handlers/boards/native_sim.overlay` already adds
`sw0`/`button0` for this reason (see its own comment). Add `led1` and
`sw1`/`button1` the same way, on unused pins of the same emulated
`gpio0` controller (`led0`=pin 0, existing `button0`=pin 1 - use pins 2
and 3).

The real `nrf5340dk` already has `led1`/`sw1` as standard aliases
(confirmed in `testing_ecosystem.md`'s "Board swap" section) - no
board-specific overlay needed there, only `native_sim` is missing them.

## Step 3 — Production code: `led_handler`

- `led_handler.h`: `led_handler_set(bool on)` →
  `led_handler_set(uint8_t led_id, bool on)`.
- `led_handler.c`: replace the single `static const struct gpio_dt_spec
  led` with an array `leds[]` (`GPIO_DT_SPEC_GET(DT_ALIAS(led0), ...)`,
  `GPIO_DT_SPEC_GET(DT_ALIAS(led1), ...)`); `led_handler_init()` loops
  over it configuring each as output; `led_handler_set()` bounds-checks
  `led_id` against `ARRAY_SIZE(leds)` (returns `-EINVAL` if out of
  range) and calls `gpio_pin_set_dt(&leds[led_id], on)`.

## Step 4 — Production code: `button_handler`

- `button_handler.h`: `button_handler_pressed_cb_t` becomes `void
  (*)(uint8_t button_id)`; `button_handler_init()`'s signature is
  unchanged (still one callback, for all buttons - the callback itself
  now reports *which* button fired).
- `button_handler.c`: replace the single `gpio_dt_spec`/
  `gpio_callback` pair with arrays. Each array entry pairs a
  `gpio_callback` with its own `button_id` in a small struct, so the
  shared interrupt handler can recover *which* button fired via
  `CONTAINER_OF` - the same pattern `ztest-hil-nrf52833`'s
  `test_gpio_loopback.c` uses
  (`CONTAINER_OF(cb, struct gpio_loopback_fixture, cb)`) to recover
  fixture context from a `gpio_callback` pointer.
  `button_handler_init()` loops over both buttons, configuring and
  registering each.

## Step 5 — Update the one real caller: `app_streamctrl.c`

- `led_handler_set(true)`/`led_handler_set(false)` → `led_handler_set(0,
  true)`/`led_handler_set(0, false)` - LED0 keeps meaning "connection
  status", now explicit instead of implicit.
- `on_button_pressed(void)` → `on_button_pressed(uint8_t button_id)` -
  still just logs (no new business logic added - button1 is
  available-but-unused, same as the board-swap note said), now
  includes which button fired in the log line since it's free.

## Step 6 — Update `tests/app_streamctrl/`'s FFF fakes to match

Compiles the real `app_streamctrl.c` against faked middlewares (see
`testing_guide.md`) - the fakes' signatures have to track the real
ones or this won't compile:

- `FAKE_VALUE_FUNC(int, led_handler_set, bool)` →
  `FAKE_VALUE_FUNC(int, led_handler_set, uint8_t, bool)`. Every
  assertion reading `led_handler_set_fake.arg0_val` (the old bool) now
  needs `arg1_val` instead - `arg0_val` is `led_id`.
- The captured button callback is invoked directly by tests
  (`captured_button_cb()`) - now needs an id argument
  (`captured_button_cb(0)`, matching `sw0`/button 0).

## Step 7 — `tests/gpio_handlers/`: the actual new test coverage

This is the real deliverable. Extend the existing suite (don't create a
new one - it's still testing the same two modules, just more of their
surface):

- Update existing `led0`/`button0` cases for the new indexed calls
  (`led_handler_set(0, true)`, assert `button_id == 0` in the pressed
  callback).
- Add `test_led1_set_true_drives_pin_active` /
  `test_led1_set_false_drives_pin_inactive` - same shape as the `led0`
  cases, different `led_id` and emulated pin.
- Add `test_button1_press_triggers_callback_with_correct_id` - drives
  the `sw1` emulated pin, asserts the callback fired with `button_id ==
  1` (not just that *a* callback fired - which button is the actual
  new behavior worth testing).
- Add one cross-check:
  `test_button0_press_does_not_report_as_button1` (or similar) -
  presses `button0`, asserts the reported id is `0`, not `1`. This is
  the case that would actually catch a copy-paste bug in the
  `CONTAINER_OF`/array-indexing logic from Step 4 - a test that only
  ever presses one button at a time can't tell two buttons apart if the
  id reporting is wrong in a way that happens to always report the
  same (wrong) id.

## Step 8 — Verify

```bash
west twister -T tests/gpio_handlers -p native_sim
west twister -T tests/app_streamctrl -p native_sim -p native_sim/native/64 -p qemu_cortex_m3
make test        # confirm nothing else broke
make lint-ci      # format
make lint-cmake    # CMakeLists.txt, if touched
```

Then a real build for the production board, to confirm the indexed
`led_handler`/`button_handler` still compile correctly against
`nrf5340dk`'s real `led1`/`sw1` (not just `native_sim`'s emulated ones):

```bash
west build -p always . --build-dir /tmp/led1_check \
  -DBOARD=nrf5340dk/nrf5340/cpuapp -DNCS_TOOLCHAIN_VERSION=NONE
```
