.PHONY: all build clean flash start-gdb-server debug west-update help print-build-path print-project-name print-board-name lint lint-ci lint-cmake

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
# Flash targets
# ============================================================================
# J-Link serial number (override with JLINK_SERIAL=xxx)
JLINK_SERIAL ?=

# TODO: set once the target board/MCU is decided
JLINK_DEVICE ?= TODO_DEVICE

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
