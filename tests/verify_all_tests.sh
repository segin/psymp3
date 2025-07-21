#!/bin/bash

# Final verification script to ensure all tests pass
# This script runs both individual tests and the test harness

set -e

echo "=== PsyMP3 Final Test Verification ==="
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

run_test() {
    local test_name="$1"
    local test_command="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -n "Testing $test_name... "
    
    if eval "$test_command"; then
        echo -e "${GREEN}PASS${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
}

echo -e "${BLUE}1. Running individual tests...${NC}"

# Find all executable test files
for test_file in $(find . -type f -executable -name "test_*" ! -name "*.sh"); do
    if [ -x "$test_file" ]; then
        run_test "$(basename $test_file)" "$test_file"
    fi
done

echo

echo -e "${BLUE}2. Running test harness...${NC}"

# Run the test harness if it exists
if [ -x "./test-harness" ]; then
    run_test "test harness" "./test-harness"
else
    echo -e "${YELLOW}Test harness not found. Skipping harness execution.${NC}"
fi

echo

echo -e "${BLUE}=== Verification Summary ===${NC}"
echo "Total tests: $TOTAL_TESTS"
echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed: ${RED}$FAILED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
    echo "The test suite is working correctly."
    exit 0
else
    echo -e "\n${RED}Some tests failed.${NC}"
    echo "Please review the failures above and fix any issues."
    exit 1
fi