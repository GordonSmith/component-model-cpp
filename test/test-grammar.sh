#!/bin/bash#!/bin/bash

# Test the WIT grammar against all wit-bindgen test files# Test the WIT grammar against all wit-bindgen test files



set -eset -e



# Colors for output# Colors for output

RED='\033[0;31m'RED='\033[0;31m'

GREEN='\033[0;32m'GREEN='\033[0;32m'

YELLOW='\033[1;33m'YELLOW='\033[1;33m'

NC='\033[0m' # No ColorNC='\033[0m' # No Color



echo "WIT Grammar Test Script"echo "WIT Grammar Test Script"

echo "======================="echo "======================="

echo ""echo ""



# Determine project root (script is in test/ directory)# Determine project root (script is in test/ directory)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"



cd "$PROJECT_ROOT" || exit 1cd "$PROJECT_ROOT" || exit 1



# Check if build directory exists# Check if build directory exists

if [ ! -d "build" ]; thenif [ ! -d "build" ]; then

    echo -e "${RED}Error: build directory not found${NC}"    echo -e "${RED}Error: build directory not found${NC}"

    echo "Please run CMake first:"    echo "Please run CMake first:"

    echo "  cmake -B build -DBUILD_GRAMMAR=ON"    echo "  cmake -B build -DBUILD_GRAMMAR=ON"

    exit 1    exit 1

fifi



# Check if grammar has been built# Check if grammar has been built

if [ ! -f "build/test/test-wit-grammar" ]; thenif [ ! -f "build/test/test-wit-grammar" ]; then

    echo -e "${YELLOW}Grammar test not built. Building now...${NC}"    echo -e "${YELLOW}Grammar test not built. Building now...${NC}"

    echo ""    echo ""

    cmake --build build --target test-wit-grammar    cmake --build build --target test-wit-grammar

    echo ""    echo ""

fifi



# Check if wit-bindgen submodule exists# Check if wit-bindgen submodule exists

if [ ! -d "ref/wit-bindgen/tests/codegen" ]; thenif [ ! -d "ref/wit-bindgen/tests/codegen" ]; then

    echo -e "${RED}Error: wit-bindgen test files not found${NC}"    echo -e "${RED}Error: wit-bindgen test files not found${NC}"

    echo "Please ensure the wit-bindgen submodule is initialized:"    echo "Please ensure the wit-bindgen submodule is initialized:"

    echo "  git submodule update --init ref/wit-bindgen"    echo "  git submodule update --init ref/wit-bindgen"

    exit 1    exit 1

fifi



# Run the test# Run the test

echo "Running grammar tests..."echo "Running grammar tests..."

echo ""echo ""



if ./build/test/test-wit-grammar "$@"; thenif ./build/test/test-wit-grammar "$@"; then

    echo ""    echo ""

    echo -e "${GREEN}✓ All grammar tests passed!${NC}"    echo -e "${GREEN}✓ All grammar tests passed!${NC}"

    exit 0    exit 0

elseelse

    echo ""    echo ""

    echo -e "${RED}✗ Grammar tests failed${NC}"    echo -e "${RED}✗ Grammar tests failed${NC}"

    exit 1    exit 1

fifi

