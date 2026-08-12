cmake_minimum_required(VERSION 3.20.0)

find_package(
  Zephyr
  REQUIRED
  HINTS
  $ENV{ZEPHYR_BASE}
)
project(
  app_streamctrl_test
)

set(BLE_AUDIO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

# Only the source under test is compiled for real. Its dependencies
# (led_handler, button_handler, audio_handler, codec_handler, ble_audio_handler)
# are FFF fakes defined in src/main.c instead of the real implementations, so
# this exercises app_streamctrl's own logic in isolation, with no BLE stack or
# hardware involved.
target_sources(
  app
  PRIVATE src/main.c
          ${BLE_AUDIO_SRC}/application/app_streamctrl.c
)

target_include_directories(
  app
  PRIVATE ${BLE_AUDIO_SRC}/application
          ${BLE_AUDIO_SRC}/common
          ${BLE_AUDIO_SRC}/middlewares/led_handler
          ${BLE_AUDIO_SRC}/middlewares/button_handler
          ${BLE_AUDIO_SRC}/middlewares/audio_handler
          ${BLE_AUDIO_SRC}/middlewares/codec_handler
          ${BLE_AUDIO_SRC}/middlewares/ble_audio_handler
)
