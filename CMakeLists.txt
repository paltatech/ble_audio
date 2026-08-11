cmake_minimum_required(VERSION 3.13.1)

# TODO: set once the target board/MCU is decided (see boards/)
set(BOARD_ROOT $ENV{ZEPHYR_BOARD_ROOT})

find_package(
  Zephyr
  REQUIRED
  HINTS
  $ENV{ZEPHYR_BASE}
)
project(
  ble_audio
)

include(tools/cmake/version.cmake)

target_sources(
  app
  PRIVATE src/main.c
)

zephyr_include_directories(src)

add_subdirectory(src/common)
add_subdirectory(src/core)
add_subdirectory(src/middlewares)
add_subdirectory(src/application)
