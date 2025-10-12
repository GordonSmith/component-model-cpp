#!/bin/bash
# Script to summarize wit-stub-generation-test failures
# Outputs a markdown summary suitable for PR comments
# 
# Usage: summarize-test-failures.sh <test_output_file> <summary_file>
#   test_output_file: File containing captured test output (required)
#   summary_file: File to write markdown summary (default: test_summary.md)

set +e  # Don't exit on error

TEST_OUTPUT_FILE="${1}"
SUMMARY_FILE="${2:-test_summary.md}"

if [ -z "$TEST_OUTPUT_FILE" ] || [ ! -f "$TEST_OUTPUT_FILE" ]; then
    echo "Error: Test output file required as first argument"
    exit 1
fi

echo "## wit-stub-generation-test Results" > "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"
echo "This test is expected to have failures due to known issues with tuple wrapper types." >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"

# Check if test passed or failed
TEST_EXIT_CODE=1
if grep -q "100% tests passed" "$TEST_OUTPUT_FILE"; then
    TEST_EXIT_CODE=0
fi

if [ $TEST_EXIT_CODE -eq 0 ]; then
    echo "✅ **All tests passed!**" >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"
    echo "Great news! The wit-stub-generation-test is now fully passing." >> "$SUMMARY_FILE"
else
    # Extract compilation statistics from ninja build output
    # Look for lines like "4: [6/13] Building CXX"
    BUILD_LINES=$(grep "Building CXX" "$TEST_OUTPUT_FILE" | head -1)
    
    if [ -n "$BUILD_LINES" ]; then
        # Extract total from format [X/TOTAL]
        TOTAL_TARGETS=$(echo "$BUILD_LINES" | grep -oP '\[\d+/\K\d+' | head -1)
    else
        TOTAL_TARGETS="0"
    fi
    
    FAILED_TARGETS=$(grep -c "^4: FAILED:" "$TEST_OUTPUT_FILE" || echo "0")
    PASSED_TARGETS=$((TOTAL_TARGETS - FAILED_TARGETS))
    
    if [ $TOTAL_TARGETS -gt 0 ]; then
        PASS_PERCENT=$(awk "BEGIN {printf \"%.1f\", ($PASSED_TARGETS / $TOTAL_TARGETS) * 100}")
        FAIL_PERCENT=$(awk "BEGIN {printf \"%.1f\", ($FAILED_TARGETS / $TOTAL_TARGETS) * 100}")
    else
        PASS_PERCENT="0.0"
        FAIL_PERCENT="0.0"
    fi
    
    # Extract error counts
    TOTAL_ERRORS=$(grep -c "error:" "$TEST_OUTPUT_FILE" || echo "0")
    
    # Show compilation statistics
    echo "### Compilation Summary" >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"
    echo "| Metric | Value |" >> "$SUMMARY_FILE"
    echo "|--------|-------|" >> "$SUMMARY_FILE"
    echo "| Total targets | $TOTAL_TARGETS |" >> "$SUMMARY_FILE"
    echo "| ✅ Passed | $PASSED_TARGETS ($PASS_PERCENT%) |" >> "$SUMMARY_FILE"
    echo "| ❌ Failed | $FAILED_TARGETS ($FAIL_PERCENT%) |" >> "$SUMMARY_FILE"
    echo "| Total errors | $TOTAL_ERRORS |" >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"
    
    # Count specific error patterns
    NO_MATCHING_FUNCTION=$(grep "error: no matching function for call to 'get<" "$TEST_OUTPUT_FILE" | wc -l || echo "0")
    RESULT_OK_ERRORS=$(grep "result_ok_wrapper" "$TEST_OUTPUT_FILE" | wc -l || echo "0")
    RESULT_ERR_ERRORS=$(grep "result_err_wrapper" "$TEST_OUTPUT_FILE" | wc -l || echo "0")
    MISSING_FILES=$(grep "fatal error:.*No such file or directory" "$TEST_OUTPUT_FILE" | wc -l || echo "0")
    
    echo "### Error Breakdown" >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"
    echo "| Error Type | Count |" >> "$SUMMARY_FILE"
    echo "|------------|-------|" >> "$SUMMARY_FILE"
    echo "| \`std::get<>\` function call errors | $NO_MATCHING_FUNCTION |" >> "$SUMMARY_FILE"
    echo "| \`result_ok_wrapper\` related | $RESULT_OK_ERRORS |" >> "$SUMMARY_FILE"
    echo "| \`result_err_wrapper\` related | $RESULT_ERR_ERRORS |" >> "$SUMMARY_FILE"
    echo "| Missing file errors | $MISSING_FILES |" >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"
    
    # Extract sample errors (first 5 unique error messages, excluding file paths)
    echo "### Sample Errors" >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"
    echo '```' >> "$SUMMARY_FILE"
    grep "error:" "$TEST_OUTPUT_FILE" | grep -v "No such file or directory" | sed 's|/home/[^/]*/component-model-cpp/||g' | sed 's/^4: //' | sort -u | head -5 >> "$SUMMARY_FILE"
    echo '```' >> "$SUMMARY_FILE"
    echo "" >> "$SUMMARY_FILE"
    
    # Check if errors increased, decreased, or stayed the same
    if [ -f "previous_test_summary.txt" ]; then
        PREV_ERRORS=$(cat previous_test_summary.txt)
        
        echo "### Trend Analysis" >> "$SUMMARY_FILE"
        echo "" >> "$SUMMARY_FILE"
        
        if [ "$TOTAL_ERRORS" -lt "$PREV_ERRORS" ]; then
            DIFF=$((PREV_ERRORS - TOTAL_ERRORS))
            echo "✅ **Improvement**: $DIFF fewer errors than previous run ($PREV_ERRORS → $TOTAL_ERRORS)" >> "$SUMMARY_FILE"
        elif [ "$TOTAL_ERRORS" -gt "$PREV_ERRORS" ]; then
            DIFF=$((TOTAL_ERRORS - PREV_ERRORS))
            echo "⚠️ **Regression**: $DIFF more errors than previous run ($PREV_ERRORS → $TOTAL_ERRORS)" >> "$SUMMARY_FILE"
        else
            echo "➡️ **No change**: Same number of errors as previous run ($TOTAL_ERRORS)" >> "$SUMMARY_FILE"
        fi
        echo "" >> "$SUMMARY_FILE"
    fi
    
    # Save current error count for next run
    echo "$TOTAL_ERRORS" > previous_test_summary.txt
fi

echo "### Known Issues" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"
echo "The main issue is that \`result_ok_wrapper\` and \`result_err_wrapper\` types don't support \`std::get<>\` operations needed for tuple unpacking in generated code." >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"
echo "**Goal**: These failures should ideally decrease over time or remain stable. Any increase warrants investigation." >> "$SUMMARY_FILE"

echo "Summary written to $SUMMARY_FILE"
cat "$SUMMARY_FILE"

# Always exit successfully so the workflow continues
exit 0
