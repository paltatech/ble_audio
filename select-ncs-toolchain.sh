# NCS v2.7.0 (Zephyr 3.6.99), matching the toolchain the sdk_nrf pin in
# west.yml was built and validated against.

declare -A toolchain_map

toolchain_map["v2.7.0-linux"]="e9dba88316"
toolchain_map["v2.7.0-windows"]="ce3b5ff664"

if [[ "$OSTYPE" == "linux"* ]]; then
    NCS_TOOLCHAIN=$HOME/ncs/toolchains/${toolchain_map["v2.7.0-linux"]}
else # mysys for Window
    NCS_TOOLCHAIN=/c/ncs/toolchains/${toolchain_map["v2.7.0-windows"]}
fi
