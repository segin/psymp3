#!/bin/bash
#
# run_basic_integration_tests.sh - Basic MPRIS integration test runner
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#

set -e

echo "Running Basic MPRIS Integration Tests"
echo "====================================="

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Function to run a test
run_test() {
    local test_name="$1"
    local test_command="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -n "Running $test_name... "
    
    if eval "$test_command" > /dev/null 2>&1; then
        echo "PASS"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo "FAIL"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
}

# Check if D-Bus is available
if ! command -v dbus-daemon >/dev/null 2>&1; then
    echo "D-Bus daemon not found. Integration tests require D-Bus."
    exit 1
fi

# Check if we have a D-Bus session
if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
    echo "No D-Bus session bus found. Starting test session..."
    eval $(dbus-launch --sh-syntax)
    export DBUS_SESSION_BUS_ADDRESS
    echo "Started D-Bus session: $DBUS_SESSION_BUS_ADDRESS"
fi

echo "D-Bus session available: $DBUS_SESSION_BUS_ADDRESS"
echo ""

# Test 1: Basic MPRIS integration (if available)
if [ -x "./test_mpris_integration" ]; then
    run_test "Basic MPRIS Integration" "./test_mpris_integration"
else
    echo "test_mpris_integration not found - skipping"
fi

# Test 2: MPRIS manager tests (if available)
if [ -x "./test_mpris_manager_comprehensive" ]; then
    run_test "MPRIS Manager Comprehensive" "./test_mpris_manager_comprehensive"
else
    echo "test_mpris_manager_comprehensive not found - skipping"
fi

# Test 3: D-Bus connection manager tests (if available)
if [ -x "./test_dbus_connection_manager_comprehensive" ]; then
    run_test "D-Bus Connection Manager" "./test_dbus_connection_manager_comprehensive"
else
    echo "test_dbus_connection_manager_comprehensive not found - skipping"
fi

# Test 4: Property manager tests (if available)
if [ -x "./test_property_manager_comprehensive" ]; then
    run_test "Property Manager" "./test_property_manager_comprehensive"
else
    echo "test_property_manager_comprehensive not found - skipping"
fi

# Test 5: Method handler tests (if available)
if [ -x "./test_method_handler_comprehensive" ]; then
    run_test "Method Handler" "./test_method_handler_comprehensive"
else
    echo "test_method_handler_comprehensive not found - skipping"
fi

# Test 6: Signal emitter tests (if available)
if [ -x "./test_signal_emitter_comprehensive" ]; then
    run_test "Signal Emitter" "./test_signal_emitter_comprehensive"
else
    echo "test_signal_emitter_comprehensive not found - skipping"
fi

# Test 7: MPRIS logging tests (if available)
if [ -x "./test_mpris_logger_basic" ]; then
    run_test "MPRIS Logger Basic" "./test_mpris_logger_basic"
else
    echo "test_mpris_logger_basic not found - skipping"
fi

# Test 8: Run the comprehensive integration test script if available
if [ -x "./run_mpris_integration_tests.sh" ]; then
    echo ""
    echo "Running comprehensive integration test suite..."
    if ./run_mpris_integration_tests.sh; then
        echo "Comprehensive integration tests: PASS"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo "Comprehensive integration tests: FAIL"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
fi

# Manual D-Bus interface test
echo ""
echo "Manual D-Bus Interface Tests"
echo "============================"

# Test if we can query D-Bus services
if command -v dbus-send >/dev/null 2>&1; then
    echo -n "Testing D-Bus service listing... "
    if dbus-send --session --print-reply --dest=org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.ListNames > /dev/null 2>&1; then
        echo "PASS"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo "FAIL"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
else
    echo "dbus-send not available - skipping manual D-Bus tests"
fi

# Summary
echo ""
echo "Integration Test Summary"
echo "======================="
echo "Total tests: $TOTAL_TESTS"
echo "Passed: $PASSED_TESTS"
echo "Failed: $FAILED_TESTS"

if [ $FAILED_TESTS -eq 0 ] && [ $TOTAL_TESTS -gt 0 ]; then
    echo ""
    echo "✓ All available integration tests PASSED!"
    exit 0
elif [ $TOTAL_TESTS -eq 0 ]; then
    echo ""
    echo "⚠ No integration tests were available to run"
    echo "This may indicate that the MPRIS integration tests need to be built"
    exit 0
else
    echo ""
    echo "✗ $FAILED_TESTS integration test(s) FAILED!"
    exit 1
fi