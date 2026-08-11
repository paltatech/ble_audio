# middlewares/

Reusable services that sit between `core/` and `application/` — e.g. BLE
audio streaming, storage, power management. Each middleware should live in
its own subfolder (e.g. `middlewares/ble/`, `middlewares/storage/`) with
its own `CMakeLists.txt`, and be wired in via `add_subdirectory()` in
[CMakeLists.txt](CMakeLists.txt).

**Dependency rule:** may depend on `common/` and `core/`; must not depend
on `application/`. Middlewares should not depend on each other unless
unavoidable — prefer the application layer to coordinate between them.
