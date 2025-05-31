#!/bin/bash

# Build script for WAMR Component Model Sample
# Usage: ./build.sh [clean]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

echo "WAMR Component Model Sample - Build Script"
echo "=========================================="

# Check if clean is requested
if [[ "$1" == "clean" ]]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory if it doesn't exist
if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

# Configure and build
echo "Configuring project..."
cd "$PROJECT_ROOT"
cmake -B "$BUILD_DIR" -S .

echo "Building WASM guest module..."
cmake --build "$BUILD_DIR" --target wasm --parallel

echo "Building WAMR host application..."
cmake --build "$BUILD_DIR" --target wamr --parallel

echo "Build completed successfully!"
echo ""
echo "To run the sample:"
echo "  cd $BUILD_DIR/samples/wamr"
echo "  ./wamr"
