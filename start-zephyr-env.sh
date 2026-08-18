# This script should be sourced, not executed.
# Usage: source ./start-zephyr-env.sh

source ./select-ncs-toolchain.sh

GREEN="\033[0;32m"
YELLOW="\033[0;33m"
NC="\033[0m"

# As this script is not using shebang instead of use $0 lets use BASH_SOURCE
WEST_WORKSPACE=$(dirname "$(dirname "$(realpath "$(dirname "${BASH_SOURCE[0]}")")")")
echo "West workspace: $WEST_WORKSPACE"

create_python_symlink() {
    local ncs_toolchain_binaries="$1"
    local python_link="$ncs_toolchain_binaries/python"
    local python3="$ncs_toolchain_binaries/python3"

    if [ ! -f "$python_link" ]; then
        if [ -f "$python3" ]; then
            ln -s "$python3" "$python_link"
            echo "Created symbolic link: $python_link -> $python3"
        else
            echo "Error: $python3 does not exist. Cannot create symbolic link."
        fi
    else
        echo "Symbolic link $python_link already exists."
    fi
}

# Add path folder to PATH if it's not already present
add_to_path() {
    local new_path=$1
    if [[ ":$PATH:" != *":$new_path:"* ]]; then
        export PATH="$new_path:$PATH"
        echo -e "${GREEN}Added $new_path to PATH${NC}"
    else
        echo -e "${YELLOW}$new_path is already in PATH${NC}"
    fi
}

if [[ "$OSTYPE" == "linux"* ]]; then
    echo "OS: Linux / GNU"
    NCS_TOOLCHAIN_BINARIES=$NCS_TOOLCHAIN/usr/local/bin
    add_to_path "$NCS_TOOLCHAIN_BINARIES"
    add_to_path "$NCS_TOOLCHAIN/usr/local/cmake"
    export LD_LIBRARY_PATH=$NCS_TOOLCHAIN/usr/local/lib:$LD_LIBRARY_PATH
    create_python_symlink "$NCS_TOOLCHAIN_BINARIES"
    PYTHON_BIN=$(which python)
else # mysys for Windows
    echo "OS: Windows"
    add_to_path "$NCS_TOOLCHAIN/opt/bin"
    add_to_path "$NCS_TOOLCHAIN/opt/bin/Scripts"
    PYTHON_BIN=$(which python)
fi

echo "CMake binary located at: $(which cmake)"
echo "Using Python interpreter at: $PYTHON_BIN"
echo "Using west interpreter at: $(which west)"

export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_SDK_INSTALL_DIR=$NCS_TOOLCHAIN/opt/zephyr-sdk
add_to_path "$ZEPHYR_SDK_INSTALL_DIR"

source $WEST_WORKSPACE/zephyr/zephyr-env.sh
west zephyr-export

echo ""
echo -e "${GREEN}=== BLE Audio Environment Ready ===${NC}"
echo -e "${YELLOW}First time setup: run 'make west-update' to fetch dependencies${NC}"
echo "Run 'make build' to build the project"
echo "Run 'make help' to see available targets"
