#!/bin/bash

#
# run_mpris_logging_tests.sh - Comprehensive MPRIS logging system test runner
# This file is part of PsyMP3.
# Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
TEST_DIR="$(dirname "$0")"
BUILD_DIR="${TEST_DIR}/.."
LOG_DIR="/tmp/mpris_logging_tests_$$"
RESULTS_FILE="${LOG_DIR}/test_results.log"

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Create log directory
mkdir -p "${LOG_DIR}"

echo -e "${BLUE}MPRIS Logging System Test Suite${NC}"
echo "=================================="
echo "Log directory: ${LOG_DIR}"
echo "Results file: ${RESULTS_FILE}"
echo ""

# Function to run a test and capture results
run_test() {
    local test_name="$1"
    local test_binary="$2"
    local description="$3"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo -n "Running ${test_name}: ${description}... "
    
    if [ ! -f "${test_binary}" ]; then
        echo -e "${RED}SKIP${NC} (binary not found)"
        echo "SKIP: ${test_name} - Binary not found: ${test_binary}" >> "${RESULTS_FILE}"
        return
    fi
    
    local test_log="${LOG_DIR}/${test_name}.log"
    local test_err="${LOG_DIR}/${test_name}.err"
    
    if "${test_binary}" > "${test_log}" 2> "${test_err}"; then
        echo -e "${GREEN}PASS${NC}"
        echo "PASS: ${test_name}" >> "${RESULTS_FILE}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}FAIL${NC}"
        echo "FAIL: ${test_name}" >> "${RESULTS_FILE}"
        echo "  Error output:" >> "${RESULTS_FILE}"
        sed 's/^/    /' "${test_err}" >> "${RESULTS_FILE}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
}

# Function to check if D-Bus is available
check_dbus_availability() {
    if ! command -v dbus-daemon >/dev/null 2>&1; then
        echo -e "${YELLOW}WARNING: D-Bus daemon not found. Some tests may be skipped.${NC}"
        return 1
    fi
    
    if [ -z "$DBUS_SESSION_BUS_ADDRESS" ]; then
        echo -e "${YELLOW}WARNING: No D-Bus session bus. Starting test session bus.${NC}"
        # Start a test D-Bus session
        eval $(dbus-launch --sh-syntax)
        export DBUS_SESSION_BUS_ADDRESS
        echo "Started test D-Bus session: $DBUS_SESSION_BUS_ADDRESS"
    fi
    
    return 0
}

# Function to build tests if needed
build_tests() {
    echo "Building MPRIS logging tests..."
    
    cd "${BUILD_DIR}"
    
    if ! make -j$(nproc) test_mpris_logger_basic test_mpris_logger_dbus_tracing test_mpris_logger_performance 2>/dev/null; then
        echo -e "${RED}ERROR: Failed to build MPRIS logging tests${NC}"
        echo "Make sure you have run ./configure with --enable-dbus and have D-Bus development libraries installed."
        exit 1
    fi
    
    echo -e "${GREEN}Tests built successfully${NC}"
    echo ""
}

# Function to run performance benchmarks
run_performance_benchmarks() {
    echo -e "${BLUE}Performance Benchmarks${NC}"
    echo "======================"
    
    local perf_log="${LOG_DIR}/performance_benchmark.log"
    
    echo "Running performance benchmarks..." > "${perf_log}"
    echo "Timestamp: $(date)" >> "${perf_log}"
    echo "" >> "${perf_log}"
    
    # Run performance test with timing
    if [ -f "${TEST_DIR}/test_mpris_logger_performance" ]; then
        echo "Measuring logging performance..."
        /usr/bin/time -v "${TEST_DIR}/test_mpris_logger_performance" >> "${perf_log}" 2>&1
        echo -e "${GREEN}Performance benchmark completed${NC}"
    else
        echo -e "${YELLOW}Performance test binary not found${NC}"
    fi
    
    echo ""
}

# Function to run stress tests
run_stress_tests() {
    echo -e "${BLUE}Stress Tests${NC}"
    echo "============"
    
    local stress_log="${LOG_DIR}/stress_test.log"
    
    echo "Running stress tests..." > "${stress_log}"
    echo "Timestamp: $(date)" >> "${stress_log}"
    echo "" >> "${stress_log}"
    
    # Run multiple instances of logging tests simultaneously
    local pids=()
    for i in {1..4}; do
        if [ -f "${TEST_DIR}/test_mpris_logger_basic" ]; then
            "${TEST_DIR}/test_mpris_logger_basic" >> "${stress_log}" 2>&1 &
            pids+=($!)
        fi
    done
    
    # Wait for all stress tests to complete
    for pid in "${pids[@]}"; do
        wait $pid
    done
    
    echo -e "${GREEN}Stress tests completed${NC}"
    echo ""
}

# Function to validate log output
validate_log_output() {
    echo -e "${BLUE}Log Output Validation${NC}"
    echo "====================="
    
    local validation_log="${LOG_DIR}/log_validation.log"
    
    echo "Validating log file formats and content..." > "${validation_log}"
    
    # Check for any log files created during tests
    local log_files=($(find "${LOG_DIR}" -name "*.log" -type f))
    
    for log_file in "${log_files[@]}"; do
        echo "Validating: $(basename "${log_file}")" >> "${validation_log}"
        
        # Check for proper timestamp format
        if grep -q '\[20[0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9]\.[0-9][0-9][0-9]\]' "${log_file}"; then
            echo "  ✓ Timestamp format valid" >> "${validation_log}"
        else
            echo "  ✗ Timestamp format invalid or missing" >> "${validation_log}"
        fi
        
        # Check for proper log level indicators
        if grep -qE '\[(TRACE|DEBUG|INFO|WARN|ERROR|FATAL)\]' "${log_file}"; then
            echo "  ✓ Log level indicators present" >> "${validation_log}"
        else
            echo "  ✗ Log level indicators missing" >> "${validation_log}"
        fi
        
        # Check for component names
        if grep -qE '\]\s+\[[A-Za-z][A-Za-z0-9_]*\]' "${log_file}"; then
            echo "  ✓ Component names present" >> "${validation_log}"
        else
            echo "  ✗ Component names missing" >> "${validation_log}"
        fi
    done
    
    echo -e "${GREEN}Log validation completed${NC}"
    echo ""
}

# Function to cleanup test environment
cleanup() {
    echo "Cleaning up test environment..."
    
    # Kill any remaining test D-Bus session
    if [ -n "$DBUS_SESSION_BUS_PID" ]; then
        kill "$DBUS_SESSION_BUS_PID" 2>/dev/null || true
    fi
    
    # Clean up temporary files (but keep logs for inspection)
    echo "Test logs preserved in: ${LOG_DIR}"
}

# Set up cleanup trap
trap cleanup EXIT

# Main test execution
main() {
    echo "Starting MPRIS logging system tests..."
    echo "Timestamp: $(date)"
    echo ""
    
    # Check D-Bus availability
    check_dbus_availability
    
    # Build tests
    build_tests
    
    # Run basic logging tests
    echo -e "${BLUE}Basic Logging Tests${NC}"
    echo "==================="
    run_test "basic_logging" "${TEST_DIR}/test_mpris_logger_basic" "Basic logging functionality"
    echo ""
    
    # Run D-Bus tracing tests
    echo -e "${BLUE}D-Bus Tracing Tests${NC}"
    echo "==================="
    run_test "dbus_tracing" "${TEST_DIR}/test_mpris_logger_dbus_tracing" "D-Bus message tracing"
    echo ""
    
    # Run performance tests
    echo -e "${BLUE}Performance Tests${NC}"
    echo "=================="
    run_test "performance" "${TEST_DIR}/test_mpris_logger_performance" "Performance metrics and lock contention"
    echo ""
    
    # Run performance benchmarks
    run_performance_benchmarks
    
    # Run stress tests
    run_stress_tests
    
    # Validate log output
    validate_log_output
    
    # Print summary
    echo -e "${BLUE}Test Summary${NC}"
    echo "============"
    echo "Total tests: ${TOTAL_TESTS}"
    echo -e "Passed: ${GREEN}${PASSED_TESTS}${NC}"
    echo -e "Failed: ${RED}${FAILED_TESTS}${NC}"
    
    if [ ${FAILED_TESTS} -eq 0 ]; then
        echo -e "\n${GREEN}All tests passed!${NC}"
        exit 0
    else
        echo -e "\n${RED}Some tests failed. Check ${RESULTS_FILE} for details.${NC}"
        exit 1
    fi
}

# Run main function
main "$@"