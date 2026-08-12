.PHONY: all build clean flash start-gdb-server debug west-update help print-build-path print-project-name print-board-name lint lint-ci lint-cmake test test-clean test-hil

all: west-update

PROJECT_NAME = ble_audio
ZEPHYR_PROJECT_PATH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

include tools/make/config.mk

ZEPHYR_BOARD_ROOT ?= $(realpath $(ZEPHYR_PROJECT_PATH)/../../zephyr_boards)
ZEPHYR_BUILD_PATH = $(ZEPHYR_PROJECT_PATH)/_build_$(PROJECT_NAME)_$(BUILD_SUFFIX)

# ============================================================================
# Help
# ============================================================================
help:
	@echo "BLE Audio Build System"
	@echo "======================="
	@echo ""
	@echo "Setup:"
	@echo "  source ./start-zephyr-env.sh   - Initialize Zephyr environment"
	@echo ""
	@echo "Build targets:"
	@echo "  make build                     - Build for configured BOARD"
	@echo "  make clean                     - Clean build directory"
	@echo ""
	@echo "Flash targets:"
	@echo "  make flash                     - Flash using default J-Link"
	@echo "  make flash JLINK_SERIAL=<sn>   - Flash using specific J-Link"
	@echo ""
	@echo "Debug targets:"
	@echo "  make start-gdb-server          - Start J-Link GDB server"
	@echo "  make debug                     - Start GDB and connect to target"
	@echo ""
	@echo "Test targets:"
	@echo "  make test                      - Run tests/ under Twister ($(TEST_PLATFORMS))"
	@echo "  make test-clean                - Remove Twister output"
	@echo "  make test-hil                  - Run the HIL boot test on real hardware"
	@echo "                                    (needs tools/hardware-map.yml, see"
	@echo "                                    tools/hardware-map.example.yml)"
	@echo ""
	@echo "Utility targets:"
	@echo "  make west-update               - Update west manifest and dependencies"
	@echo "  make print-build-path          - Show build directory path"
	@echo "  make lint                      - Format C/C++ code with clang-format"
	@echo "  make lint-ci                   - Format C/C++ code (CI mode, local binary)"
	@echo "  make lint-cmake                - Format CMake files"
	@echo ""
	@echo "Configuration (edit tools/make/config.mk):"
	@echo "  BOARD: $(BOARD)"
	@echo "  Build path: $(ZEPHYR_BUILD_PATH)"

# ============================================================================
# Build targets
# ============================================================================
print-build-path:
	@echo "Build directory is: $(ZEPHYR_BUILD_PATH)"

print-project-name:
	@echo "Project name is: $(PROJECT_NAME)"

print-board-name:
	@echo "Board name is: $(BOARD)"

build:
	west build -p always . \
		--build-dir $(ZEPHYR_BUILD_PATH) \
		-DNCS_TOOLCHAIN_VERSION=NONE \
		-DBOARD_ROOT=$(ZEPHYR_BOARD_ROOT) \
		-DBOARD=$(BOARD)

clean:
	rm -rf $(ZEPHYR_BUILD_PATH)

# ============================================================================
# Lint / Format targets
# ============================================================================
lint:
	set -f; \
	find . -not \( -path "./_build*/*" -prune \) -not \( -path "./build*/*" -prune \) \
		\( -type f -name "*.c" -o -name "*.h" \) \
		-exec clang-format -style=file:tools/format/.clang-format --verbose -i {} \;

lint-ci:
	set -f; \
	find . -not \( -path "./_build*/*" -prune \) -not \( -path "./build*/*" -prune \) \
		\( -type f -name "*.c" -o -name "*.h" \) \
		-exec ./clang-format -style=file:tools/format/.clang-format --verbose -i {} \;

lint-cmake:
	# pip install cmakelang
	set -f; \
	find . -not \( -path "./_build*/*" -prune \) \
		\( -type f -name '*.cmake' -o -name 'CMakeLists.txt' \) \
		-exec cmake-format -c tools/format/.cmake-format.py -i {} \;

# ============================================================================
# Test targets
# ============================================================================
# Platforms Twister runs tests/ against. native_sim actually builds
# 32-bit (needs gcc-multilib on the host) - native_sim/native/64 is the
# true 64-bit variant. Both plus a 32-bit QEMU target catch
# pointer/alignment/data-sizing bugs early - see docs/testing_ecosystem.md
# for how this was found to be mislabeled in every phase before this one.
# mps3/an547 (Cortex-M55), not qemu_cortex_m3 for the QEMU leg - the
# latter's SoC has no FPU, and any suite depending on CONFIG_LIBLC3
# (which needs CONFIG_FPU) can't run there. Not every suite targets
# every platform - see each tests/*/testcase.yaml's own platform_allow.
TEST_PLATFORMS ?= native_sim native_sim/native/64 mps3/an547
TWISTER_OUT ?= $(ZEPHYR_PROJECT_PATH)/twister-out

# Twister's own multiprocessing needs libffi.so.7, which this toolchain's
# bundled Python doesn't ship (only a newer libffi.so.8 is on the host).
# Stage an isolated copy - not the whole directory it lives in, which
# also ships an old libstdc++ that breaks ccache if put on
# LD_LIBRARY_PATH - and point LD_LIBRARY_PATH at just that.
LIBFFI_SHIM_DIR := $(ZEPHYR_PROJECT_PATH)/.cache/libffi-shim
LIBFFI_SRC := $(NCS_TOOLCHAIN)/opt/nanopb/generator-bin/libffi.so.7

test:
	@mkdir -p $(LIBFFI_SHIM_DIR)
	@[ -f $(LIBFFI_SHIM_DIR)/libffi.so.7 ] || cp $(LIBFFI_SRC) $(LIBFFI_SHIM_DIR)/
	LD_LIBRARY_PATH="$(LIBFFI_SHIM_DIR):$$LD_LIBRARY_PATH" \
	python3 $(ZEPHYR_BASE)/scripts/twister \
		-T tests \
		$(foreach p,$(TEST_PLATFORMS),-p $(p)) \
		--extra-args NCS_TOOLCHAIN_VERSION=NONE \
		-O $(TWISTER_OUT)

test-clean:
	rm -rf $(TWISTER_OUT)

# ============================================================================
# HIL test target (Priority 4 - needs real hardware, not run by `make test`)
# ============================================================================
# Copy tools/hardware-map.example.yml to tools/hardware-map.yml, fill in
# your board's J-Link serial/UART path, then run `make test-hil`. Also
# needs the pytest_twister_harness plugin installed once:
#   pip install zephyr/scripts/pylib/pytest-twister-harness
HW_MAP ?= tools/hardware-map.yml

test-hil:
	@if [ ! -f "$(HW_MAP)" ]; then \
		echo "No hardware map at $(HW_MAP)."; \
		echo "Copy tools/hardware-map.example.yml there and fill in your board's id/serial."; \
		exit 1; \
	fi
	@mkdir -p $(LIBFFI_SHIM_DIR)
	@[ -f $(LIBFFI_SHIM_DIR)/libffi.so.7 ] || cp $(LIBFFI_SRC) $(LIBFFI_SHIM_DIR)/
	LD_LIBRARY_PATH="$(LIBFFI_SHIM_DIR):$$LD_LIBRARY_PATH" \
	python3 $(ZEPHYR_BASE)/scripts/twister \
		-T . \
		-s ble_audio.hil_boot \
		-p ble_audio_board/nrf5340/cpuapp \
		--board-root $(ZEPHYR_BOARD_ROOT)/boards \
		--device-testing \
		--hardware-map $(HW_MAP) \
		--extra-args NCS_TOOLCHAIN_VERSION=NONE \
		-O $(TWISTER_OUT)

# ============================================================================
# Flash targets
# ============================================================================
# J-Link serial number (override with JLINK_SERIAL=xxx)
JLINK_SERIAL ?=

# nRF5340 app core (see boards/paltatech/ble_audio_board/board.cmake in
# zephyr_boards for the matching jlink runner args)
JLINK_DEVICE ?= nrf5340_xxaa_app

ifdef JLINK_SERIAL
    JLINK_OPT = --tool-opt '-usb $(JLINK_SERIAL)'
else
    JLINK_OPT =
endif

flash:
	west -v flash \
		--runner nrfjprog \
		$(JLINK_OPT) \
		--build-dir $(ZEPHYR_BUILD_PATH)

# ============================================================================
# Debug targets
# ============================================================================
start-gdb-server:
	west debugserver --runner jlink \
		--device=$(JLINK_DEVICE) \
		--speed 400 \
		--tool-opt '-nosinglerun -nosilent -vd' \
		--build-dir $(ZEPHYR_BUILD_PATH)

debug:
	arm-none-eabi-gdb $(ZEPHYR_BUILD_PATH)/zephyr/zephyr.elf

# ============================================================================
# West management
# ============================================================================
west-update:
	@relpath=$$(realpath --relative-to="$(realpath $(ZEPHYR_PROJECT_PATH)/../..)" "$(CURDIR)"); \
	sed -i "s|^path = .*|path = $$relpath|" ../../.west/config; \
	grep '^path =' ../../.west/config
	west update
