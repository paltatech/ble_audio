#!/bin/bash

set -e

PROJECT_ROOT_PATH=$(pwd)
BASE_PATH=$PROJECT_ROOT_PATH
RELEASE_DIR=$BASE_PATH/release

APP_VERSION=""

function get_app_version {
    GIT_HASH=`git rev-parse --short=8 HEAD`
    status=$?

    APP_VERSION=$GIT_HASH

    if [ $status -eq 127 ]; then
    echo "Git not installed, and it's required"
    exit 1
    elif [ $status -ne 0 ]; then
    exit 1
    fi

    echo "Git Hash: v-$APP_VERSION"
}

function get_fw_version {
    # v1.0.0-h-9dccfc0c-dirty
    VERSION_FILE=$BASE_PATH"/tools/cmake/version.cmake"
    echo "Version file: $VERSION_FILE"

    VERSION_MAJOR=$(grep -oP 'set\(VERSION_INFO_MAJOR\s+\K\d+' "$VERSION_FILE")
    VERSION_MINOR=$(grep -oP 'set\(VERSION_INFO_MINOR\s+\K\d+' "$VERSION_FILE")
    VERSION_BUILD=$(grep -oP 'set\(VERSION_INFO_BUILD\s+\K\d+' "$VERSION_FILE")

    APP_VERSION=$VERSION_MAJOR.$VERSION_MINOR.$VERSION_BUILD-h-$APP_VERSION
    echo "App version: $APP_VERSION"
}

get_app_version
get_fw_version

COMPILATION_TIME=$(date +"%Y-%m-%d-%H-%M-UTC%z")

OUTPUT=$(make -s print-project-name)
PRODUCT=$(echo "$OUTPUT" | grep "Project name is:" | awk '{print $NF}')

OUTPUT=$(make -s print-board-name)
BOARD_NAME=$(echo "$OUTPUT" | grep "Board name is:" | awk '{print $NF}')

echo "Project:$PRODUCT"
echo "Board:$BOARD_NAME"

RELEASE_NAME=$PRODUCT-release-v-$APP_VERSION-$COMPILATION_TIME
DEST_FW_PATH=$RELEASE_DIR/$RELEASE_NAME

mkdir -p $DEST_FW_PATH
echo "Dest fw directory is: $DEST_FW_PATH"

SRC_FW_PATH=""

function get_src_fw_path {
    OUTPUT=$(make -s print-build-path)
    BUILD_DIR=$(echo "$OUTPUT" | grep "Build directory is:" | awk '{print $NF}')

    SRC_FW_PATH=$BUILD_DIR
    echo "Source Fw directory is: $SRC_FW_PATH"
}

function compile {

    make build

    get_src_fw_path

    cp $SRC_FW_PATH/zephyr/merged.hex $DEST_FW_PATH/$PRODUCT-merged-v-$APP_VERSION.hex
}

compile

# Do not provide absolute path since zip operates
# using working directory. Provide relative path instead
# to not reflect the absolute paths when uncompressing
cd $RELEASE_DIR
zip -r $RELEASE_NAME.zip $RELEASE_NAME/*
