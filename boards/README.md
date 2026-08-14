# boards/

Board-specific devicetree overlays (`<board>.overlay`) and Kconfig fragments
(`<board>.conf`) go here, named after the target board identifier.

`nrf5340dk_nrf5340_cpuapp.overlay` adds the input power sense circuit
(voltage divider into `A0`/`AIN0`) that `power_handler` reads - the SoC's
own peripherals (LEDs, buttons, `&adc` itself) already come from the
board's own devicetree, so this is the only board-level addition needed
so far.
