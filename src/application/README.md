# application/

Top-level application/business logic: orchestrates `middlewares/` and
`core/` services, owns RTOS/thread setup, and implements the actual product
behavior.

**Dependency rule:** may depend on `common/`, `core/`, and `middlewares/`.
Nothing else in `src/` should depend on `application/`.

Add new files flat in this folder (no `Inc`/`Src` split); wire them into
[CMakeLists.txt](CMakeLists.txt) as they're added.

Files here follow the `app_` prefix convention (e.g. `app_streamctrl.c`)
to make the layer identifiable from the symbol/file name alone.

Currently contains `app_streamctrl.c/.h` — the headset state machine:
initializes the LED/button/audio-output middlewares, starts the BLE audio
handler, and wires its callbacks (connect/disconnect/stream config/receive)
to the codec and audio-output handlers.
