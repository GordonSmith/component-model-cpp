#!/bin/bash
# Generate C++ stub files for all WIT files in the grammar test suite

set -e

# Configuration
WIT_TEST_DIR="${WIT_TEST_DIR:-../ref/wit-bindgen/tests/codegen}"
OUTPUT_DIR="${OUTPUT_DIR:-generated_stubs}"
CODEGEN_TOOL="${CODEGEN_TOOL:-../build/tools/wit-codegen/wit-codegen}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if wit-codegen exists
if [ ! -f "$CODEGEN_TOOL" ]; then
    echo -e "${RED}Error: wit-codegen not found at $CODEGEN_TOOL${NC}"
    echo "Please build it first with: cmake --build build --target wit-codegen"
    exit 1
fi

# Check if test directory exists
if [ ! -d "$WIT_TEST_DIR" ]; then
    echo -e "${RED}Error: Test directory not found at $WIT_TEST_DIR${NC}"
    echo "Please ensure the wit-bindgen submodule is initialized"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "WIT Grammar Test Stub Generator"
echo "================================"
echo "Test directory: $WIT_TEST_DIR"
echo "Output directory: $OUTPUT_DIR"
echo "Code generator: $CODEGEN_TOOL"
echo ""

# Find all .wit files
WIT_FILES=($(find "$WIT_TEST_DIR" -name "*.wit" | sort))
TOTAL=${#WIT_FILES[@]}

if [ $TOTAL -eq 0 ]; then
    echo -e "${RED}No .wit files found in $WIT_TEST_DIR${NC}"
    exit 1
fi

echo "Found $TOTAL WIT files"
echo ""

# Process each WIT file
SUCCESS_COUNT=0
FAILURE_COUNT=0
SKIPPED_COUNT=0
FAILURES=()

for WIT_FILE in "${WIT_FILES[@]}"; do
    # Get the basename without extension
    BASENAME=$(basename "$WIT_FILE" .wit)
    
    # Get relative path for better organization
    REL_PATH=$(realpath --relative-to="$WIT_TEST_DIR" "$WIT_FILE")
    DIR_PATH=$(dirname "$REL_PATH")
    
    # Create subdirectory structure in output
    if [ "$DIR_PATH" != "." ]; then
        mkdir -p "$OUTPUT_DIR/$DIR_PATH"
        OUTPUT_PREFIX="$OUTPUT_DIR/$DIR_PATH/$BASENAME"
    else
        OUTPUT_PREFIX="$OUTPUT_DIR/$BASENAME"
    fi
    
    echo -n "Processing: $REL_PATH ... "
    
    # Run wit-codegen
    if "$CODEGEN_TOOL" "$WIT_FILE" "$OUTPUT_PREFIX" > /dev/null 2>&1; then
        # Check if at least one file was generated
        if [ -f "${OUTPUT_PREFIX}.hpp" ] || [ -f "${OUTPUT_PREFIX}.cpp" ] || [ -f "${OUTPUT_PREFIX}_bindings.cpp" ]; then
            echo -e "${GREEN}✓${NC}"
            SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        else
            echo -e "${YELLOW}⊘ (no output)${NC}"
            SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
        fi
    else
        echo -e "${RED}✗${NC}"
        FAILURE_COUNT=$((FAILURE_COUNT + 1))
        FAILURES+=("$REL_PATH")
    fi
done

# Print summary
echo ""
echo "================================"
echo "Summary:"
echo "  Total files:      $TOTAL"
echo -e "  ${GREEN}Successful:       $SUCCESS_COUNT${NC}"
echo -e "  ${YELLOW}Skipped:          $SKIPPED_COUNT${NC}"
echo -e "  ${RED}Failed:           $FAILURE_COUNT${NC}"

if [ $FAILURE_COUNT -gt 0 ]; then
    echo ""
    echo "Failed files:"
    for FAILED in "${FAILURES[@]}"; do
        echo "  - $FAILED"
    done
    echo ""
    exit 1
fi

echo ""
echo -e "${GREEN}✓ All stubs generated successfully!${NC}"
echo "Output directory: $OUTPUT_DIR"
