#!/bin/bash

# Run script for WAMR Component Model Sample
# Usage: ./run.sh [debug|gdb|valgrind]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
WAMR_DIR="$BUILD_DIR/samples/wamr"
WAMR_BINARY="$WAMR_DIR/wamr"

echo "WAMR Component Model Sample - Run Script"
echo "========================================"

# Check if binary exists
if [[ ! -f "$WAMR_BINARY" ]]; then
    echo "Error: WAMR binary not found at $WAMR_BINARY"
    echo "Please run ./build.sh first"
    exit 1
fi

# Check if WASM file exists
WASM_FILE="$BUILD_DIR/samples/bin/sample.wasm"
if [[ ! -f "$WASM_FILE" ]]; then
    echo "Error: WASM file not found at $WASM_FILE"
    echo "Please run ./build.sh first"
    exit 1
fi

cd "$WAMR_DIR"

case "${1:-normal}" in
    "debug")
        echo "Running with debug output..."
        WAMR_LOG_LEVEL=5 "$WAMR_BINARY"
        ;;
    "gdb")
        echo "Running with GDB debugger..."
        gdb --args "$WAMR_BINARY"
        ;;
    "valgrind")
        echo "Running with Valgrind..."
        valgrind --leak-check=full --show-leak-kinds=all "$WAMR_BINARY"
        ;;
    "strace")
        echo "Running with strace..."
        strace -o wamr.trace "$WAMR_BINARY"
        echo "Trace saved to wamr.trace"
        ;;
    *)
        echo "Running WAMR sample..."
        echo "Available WASM file: $(ls -lh ../bin/sample.wasm)"
        echo ""
        "$WAMR_BINARY"
        ;;
esac
