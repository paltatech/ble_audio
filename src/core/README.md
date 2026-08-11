# core/

Low-level hardware/peripheral handling and system bring-up: board init,
clock/power setup, and thin wrappers around Zephyr drivers used by the rest
of the application.

**Dependency rule:** may depend on `common/`; must not depend on
`middlewares/` or `application/`.

Add new files flat in this folder (no `Inc`/`Src` split); if a module grows
enough to warrant its own subfolder, give it its own `CMakeLists.txt` and
`add_subdirectory()` it from [CMakeLists.txt](CMakeLists.txt).
