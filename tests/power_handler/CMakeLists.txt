cmake_minimum_required(VERSION 3.20.0)

find_package(
  Zephyr
  REQUIRED
  HINTS
  $ENV{ZEPHYR_BASE}
)
project(
  power_handler_test
)

set(BLE_AUDIO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

# Real power_handler.c, run against native_sim's built-in zephyr,adc-emul
# controller (see boards/native_sim.overlay for the voltage-divider node it
# doesn't define by default).
target_sources(
  app
  PRIVATE src/main.c
          ${BLE_AUDIO_SRC}/middlewares/power_handler/power_handler.c
)

target_include_directories(
  app
  PRIVATE ${BLE_AUDIO_SRC}/middlewares/power_handler
)
