#!/bin/bash

# Exit on error
set -e

# Get the absolute path of the project root
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PIGWEED_DIR="${PROJECT_ROOT}/thirdparty/pigweed"
BUILD_DIR="${PROJECT_ROOT}/build"

# Default values
BOARD="rpi_pico2"
PRISTINE="auto" # west default
DO_FLASH=0
DO_UPDATE=0

# Help function
print_help() {
    echo "Usage: ./build.sh [options]"
    echo "Options:"
    echo "  -b, --board <board>   Target board (default: rpi_pico)"
    echo "  -c, --clean, --rebuild Force a clean (pristine) build"
    echo "  -f, --flash           Flash the device after building"
    echo "  -u, --update          Run 'west update' to fetch dependencies"
    echo "  -h, --help            Show this help message"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--board)
            BOARD="$2"
            shift 2
            ;;
        -c|--clean|--rebuild)
            PRISTINE="always"
            shift
            ;;
        -f|--flash)
            DO_FLASH=1
            shift
            ;;
        -u|--update)
            DO_UPDATE=1
            shift
            ;;
        -h|--help)
            print_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            print_help
            exit 1
            ;;
    esac
done

echo "--- Initializing Environment ---"

# 1. Initialize West if not already done
if [ ! -d "${PROJECT_ROOT}/.west" ]; then
    echo "Initializing west workspace..."
    west init -l "${PROJECT_ROOT}"
    DO_UPDATE=1 # Force update on fresh init
fi

# 2. Update West dependencies if requested
if [ $DO_UPDATE -eq 1 ]; then
    echo "Updating west modules..."
    west update
fi

# 3. Pigweed Environment Setup
if [ ! -d "${PROJECT_ROOT}/environment" ]; then
    echo "Bootstrapping Pigweed environment (this may take a while)..."
    cd "${PROJECT_ROOT}"
    source "${PIGWEED_DIR}/bootstrap.sh"
else
    echo "Activating Pigweed environment..."
    source "${PROJECT_ROOT}/activate.sh"
fi

echo "--- Building Project ---"
echo "Board: $BOARD"
echo "Pristine: $PRISTINE"

# 4. Build with West
west build -p "$PRISTINE" -b "$BOARD" "${PROJECT_ROOT}/src"

# 5. Flash if requested
if [ $DO_FLASH -eq 1 ]; then
    echo "--- Flashing Device ---"
    west flash
fi

echo "--- Done ---"
if [ -f "${BUILD_DIR}/zephyr/zephyr.uf2" ]; then
    echo "UF2 file available at: ${BUILD_DIR}/zephyr/zephyr.uf2"
fi