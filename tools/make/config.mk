# BLE Audio Build Configuration
# ================================

# Real hardware is now the plain nRF5340 DK (not the Audio DK this
# project originally targeted) - a standard, upstream-supported Zephyr
# board, no custom board definition needed. App core. HWMv2 identifier -
# includes slashes.
BOARD ?= nrf5340dk/nrf5340/cpuapp

# Build output directory suffix (slashes in BOARD would otherwise nest
# the build directory under subfolders instead of naming it)
BUILD_SUFFIX ?= $(subst /,_,$(BOARD))
