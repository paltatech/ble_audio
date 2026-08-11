# middlewares/

Reusable services, one per subfolder, each with its own `CMakeLists.txt`
wired in via `add_subdirectory()` in [CMakeLists.txt](CMakeLists.txt).
Following this workspace's convention (see sibling projects), every
hardware-adjacent or protocol-adjacent service lives here as `<x>_handler/`
— `core/` is reserved for lower-level system bring-up only, and currently
has no code.

Current handlers:
- `led_handler/` — status LED GPIO wrapper
- `button_handler/` — button GPIO + interrupt wrapper
- `audio_handler/` — I2S audio output
- `codec_handler/` — LC3 decode (via `liblc3`)
- `ble_audio_handler/` — LE Audio unicast server (BAP/ASCS/PACS), sink only

**Dependency rule:** may depend on `common/` and `core/`; must not depend
on `application/`. Middlewares should not depend on each other unless
unavoidable — prefer the application layer to coordinate between them (see
`application/app_streamctrl.c`, which wires `ble_audio_handler`'s received
frames through `codec_handler` and into `audio_handler`).
