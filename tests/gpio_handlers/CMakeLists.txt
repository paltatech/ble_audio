cmake_minimum_required(VERSION 3.20.0)

find_package(
  Zephyr
  REQUIRED
  HINTS
  $ENV{ZEPHYR_BASE}
)
project(
  gpio_handlers_test
)

set(BLE_AUDIO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

# Real led_handler.c/button_handler.c, run against native_sim's built-in
# zephyr,gpio-emul controller (see boards/native_sim.overlay for the button node
# it doesn't define by default).
target_sources(
  app
  PRIVATE src/main.c
          ${BLE_AUDIO_SRC}/middlewares/led_handler/led_handler.c
          ${BLE_AUDIO_SRC}/middlewares/button_handler/button_handler.c
)

target_include_directories(
  app
  PRIVATE ${BLE_AUDIO_SRC}/middlewares/led_handler
          ${BLE_AUDIO_SRC}/middlewares/button_handler
)
