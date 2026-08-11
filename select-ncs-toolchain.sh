# TODO: NCS/Zephyr SDK version is not finalized yet - defaulting to the same
# toolchain used by sibling projects in this workspace (Zephyr 3.6.99).

declare -A toolchain_map

toolchain_map["v2.7.0-linux"]="e9dba88316"
toolchain_map["v2.7.0-windows"]="ce3b5ff664"

if [[ "$OSTYPE" == "linux"* ]]; then
    NCS_TOOLCHAIN=$HOME/ncs/toolchains/${toolchain_map["v2.7.0-linux"]}
else # mysys for Window
    NCS_TOOLCHAIN=/c/ncs/toolchains/${toolchain_map["v2.7.0-windows"]}
fi
