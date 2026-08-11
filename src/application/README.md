# application/

Top-level application/business logic: orchestrates `middlewares/` and
`core/` services, owns RTOS/thread setup, and implements the actual product
behavior.

**Dependency rule:** may depend on `common/`, `core/`, and `middlewares/`.
Nothing else in `src/` should depend on `application/`.

Add new files flat in this folder (no `Inc`/`Src` split); wire them into
[CMakeLists.txt](CMakeLists.txt) as they're added.
