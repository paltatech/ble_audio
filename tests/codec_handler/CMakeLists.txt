cmake_minimum_required(VERSION 3.20.0)

find_package(
  Zephyr
  REQUIRED
  HINTS
  $ENV{ZEPHYR_BASE}
)
project(
  codec_handler_test
)

set(BLE_AUDIO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

target_sources(
  app
  PRIVATE src/main.c
          ${BLE_AUDIO_SRC}/middlewares/codec_handler/codec_handler.c
)

target_include_directories(
  app
  PRIVATE ${BLE_AUDIO_SRC}/middlewares/codec_handler
          ${BLE_AUDIO_SRC}/common
)
