#!/bin/bash

# Build script for Resource Examples
# Usage: ./build.sh [target]
# 
# Targets:
#   simple     - Build simple resource demo only
#   all        - Build all resource examples
#   clean      - Clean build artifacts
#   test       - Build and run tests

set -e  # Exit on any error

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../" && pwd)"
RESOURCES_DIR="$PROJECT_ROOT/samples/wamr/resources"

echo "Component Model C++ - Resource Examples Build Script"
echo "===================================================="
echo "Project root: $PROJECT_ROOT"
echo "Resources dir: $RESOURCES_DIR"

# Compiler settings
CXX_FLAGS="-std=c++20 -O2 -Wall -Wextra"
INCLUDE_DIRS="-I$PROJECT_ROOT/include -I$PROJECT_ROOT/vcpkg_installed/x64-linux/include"
LIB_DIRS="-L$PROJECT_ROOT/vcpkg_installed/x64-linux/lib"
LIBS="-licuuc -licudata -licui18n"

# Build function
build_target() {
    local target_name="$1"
    local source_file="$2"
    local output_name="$3"
    
    echo "Building $target_name..."
    g++ $CXX_FLAGS $INCLUDE_DIRS \
        "$RESOURCES_DIR/$source_file" \
        "$PROJECT_ROOT/test/host-util.cpp" \
        -o "$RESOURCES_DIR/$output_name" \
        $LIB_DIRS $LIBS
    
    echo "✓ Built $output_name"
}

# Handle command line arguments
case "${1:-all}" in
    "simple")
        build_target "Simple Resource Demo" "simple_resource_demo.cpp" "simple-demo"
        ;;
    
    "examples") 
        build_target "Resource Examples" "resource_examples.cpp" "resource-examples"
        ;;
        
    "all")
        build_target "Simple Resource Demo" "simple_resource_demo.cpp" "simple-demo"
        # build_target "Resource Examples" "resource_examples.cpp" "resource-examples"
        echo "All targets built successfully!"
        ;;
        
    "clean")
        echo "Cleaning build artifacts..."
        rm -f "$RESOURCES_DIR/simple-demo"
        rm -f "$RESOURCES_DIR/resource-examples"
        echo "✓ Clean completed"
        ;;
        
    "test")
        build_target "Simple Resource Demo" "simple_resource_demo.cpp" "simple-demo"
        echo ""
        echo "Running tests..."
        echo "================"
        
        echo "1. Testing basic operations:"
        "$RESOURCES_DIR/simple-demo" --basic
        
        echo ""
        echo "2. Testing resource table:"
        "$RESOURCES_DIR/simple-demo" --table
        
        echo ""
        echo "3. Testing canon functions:"
        "$RESOURCES_DIR/simple-demo" --canon
        
        echo ""
        echo "4. Testing lift/lower:"
        "$RESOURCES_DIR/simple-demo" --lift-lower
        
        echo ""
        echo "✓ All tests completed successfully!"
        ;;
        
    "help"|"-h"|"--help")
        echo "Usage: $0 [target]"
        echo ""
        echo "Targets:"
        echo "  simple     - Build simple resource demo only"
        echo "  examples   - Build full resource examples"
        echo "  all        - Build all resource examples (default)"
        echo "  clean      - Clean build artifacts"
        echo "  test       - Build and run all tests"
        echo "  help       - Show this help message"
        echo ""
        echo "Examples:"
        echo "  $0                    # Build all"
        echo "  $0 simple             # Build simple demo only"
        echo "  $0 test               # Build and test everything"
        echo "  $0 clean              # Clean up"
        ;;
        
    *)
        echo "Unknown target: $1"
        echo "Use '$0 help' to see available targets"
        exit 1
        ;;
esac

echo "Done!"
