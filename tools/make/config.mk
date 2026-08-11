# BLE Audio Build Configuration
# ================================

# Custom board (boards/paltatech/ble_audio_board in zephyr_boards), app core.
# HWMv2 identifier - includes slashes.
BOARD ?= ble_audio_board/nrf5340/cpuapp

# Build output directory suffix (slashes in BOARD would otherwise nest
# the build directory under subfolders instead of naming it)
BUILD_SUFFIX ?= $(subst /,_,$(BOARD))
