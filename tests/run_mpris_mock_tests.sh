#!/bin/bash
#
# run_mpris_mock_tests.sh - Run MPRIS mock framework tests
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#

set -e

echo "Running MPRIS Mock Framework Tests..."
echo "====================================="

# Check if D-Bus support is available
if ! command -v dbus-daemon &> /dev/null; then
    echo "Warning: D-Bus daemon not found. Some tests may be skipped."
fi

# Set up test environment
export DBUS_SESSION_BUS_ADDRESS="unix:path=/tmp/test-dbus-session"
export MPRIS_TEST_MODE=1

# Function to run a test and report results
run_test() {
    local test_name="$1"
    local test_executable="$2"
    
    echo ""
    echo "Running $test_name..."
    echo "----------------------------------------"
    
    if [ -x "$test_executable" ]; then
        if ./"$test_executable"; then
            echo "✓ $test_name PASSED"
            return 0
        else
            echo "✗ $test_name FAILED"
            return 1
        fi
    else
        echo "⚠ $test_name SKIPPED (executable not found)"
        return 0
    fi
}

# Track test results
total_tests=0
passed_tests=0
failed_tests=0

# Run mock framework tests
if run_test "Mock Framework Basic Tests" "test_mpris_mock_framework"; then
    ((passed_tests++))
else
    ((failed_tests++))
fi
((total_tests++))

# Run existing MPRIS tests with mock framework
test_programs=(
    "test_mpris_types"
    "test_dbus_connection_manager"
    "test_mpris_manager_simple"
    "test_mpris_manager_minimal"
    "test_mpris_error_handling"
)

for test_prog in "${test_programs[@]}"; do
    if run_test "$(echo $test_prog | sed 's/_/ /g' | sed 's/\b\w/\U&/g')" "$test_prog"; then
        ((passed_tests++))
    else
        ((failed_tests++))
    fi
    ((total_tests++))
done

# Summary
echo ""
echo "Test Summary"
echo "============"
echo "Total tests: $total_tests"
echo "Passed: $passed_tests"
echo "Failed: $failed_tests"

if [ $failed_tests -eq 0 ]; then
    echo ""
    echo "✓ All MPRIS mock framework tests PASSED!"
    exit 0
else
    echo ""
    echo "✗ $failed_tests test(s) FAILED!"
    exit 1
fi