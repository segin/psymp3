#!/bin/bash
#
# run_mpris_integration_tests.sh - Comprehensive MPRIS integration test suite
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test configuration
TEST_DIR="$(dirname "$0")"
BUILD_DIR="${TEST_DIR}/.."
LOG_DIR="/tmp/mpris_integration_tests_$$"
RESULTS_FILE="${LOG_DIR}/integration_results.log"
DBUS_TEST_SESSION_DIR="${LOG_DIR}/dbus_session"

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Test categories
BASIC_TESTS=0
SPEC_COMPLIANCE_TESTS=0
STRESS_TESTS=0
RECONNECTION_TESTS=0
MEMORY_TESTS=0

# Create log and session directories
mkdir -p "${LOG_DIR}"
mkdir -p "${DBUS_TEST_SESSION_DIR}"

echo -e "${BLUE}MPRIS Integration Test Suite${NC}"
echo "============================="
echo "Log directory: ${LOG_DIR}"
echo "Results file: ${RESULTS_FILE}"
echo "D-Bus session dir: ${DBUS_TEST_SESSION_DIR}"
echo ""

# Function to log test results
log_result() {
    local result="$1"
    local test_name="$2"
    local message="$3"
    
    echo "${result}: ${test_name} - ${message}" >> "${RESULTS_FILE}"
    echo "$(date '+%Y-%m-%d %H:%M:%S'): ${result}: ${test_name}" >> "${RESULTS_FILE}"
}

# Function to run a test with timeout and resource monitoring
run_test() {
    local test_name="$1"
    local test_binary="$2"
    local description="$3"
    local timeout_seconds="${4:-30}"
    local category="${5:-BASIC}"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    case "$category" in
        "BASIC") BASIC_TESTS=$((BASIC_TESTS + 1)) ;;
        "SPEC") SPEC_COMPLIANCE_TESTS=$((SPEC_COMPLIANCE_TESTS + 1)) ;;
        "STRESS") STRESS_TESTS=$((STRESS_TESTS + 1)) ;;
        "RECONNECT") RECONNECTION_TESTS=$((RECONNECTION_TESTS + 1)) ;;
        "MEMORY") MEMORY_TESTS=$((MEMORY_TESTS + 1)) ;;
    esac
    
    echo -n "[$category] Running ${test_name}: ${description}... "
    
    if [ ! -f "${test_binary}" ]; then
        echo -e "${YELLOW}SKIP${NC} (binary not found)"
        log_result "SKIP" "${test_name}" "Binary not found: ${test_binary}"
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        return 0
    fi
    
    local test_log="${LOG_DIR}/${test_name}.log"
    local test_err="${LOG_DIR}/${test_name}.err"
    local memory_log="${LOG_DIR}/${test_name}_memory.log"
    
    # Start memory monitoring in background
    local monitor_pid=""
    if command -v ps >/dev/null 2>&1; then
        (
            while sleep 1; do
                ps -o pid,pcpu,pmem,vsz,rss,comm -p $$ 2>/dev/null || break
            done > "${memory_log}"
        ) &
        monitor_pid=$!
    fi
    
    # Run test with timeout
    local test_result=0
    if timeout "${timeout_seconds}s" "${test_binary}" > "${test_log}" 2> "${test_err}"; then
        echo -e "${GREEN}PASS${NC}"
        log_result "PASS" "${test_name}" "${description}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo -e "${RED}TIMEOUT${NC}"
            log_result "TIMEOUT" "${test_name}" "Test timed out after ${timeout_seconds}s"
        else
            echo -e "${RED}FAIL${NC}"
            log_result "FAIL" "${test_name}" "Exit code: ${exit_code}"
        fi
        FAILED_TESTS=$((FAILED_TESTS + 1))
        test_result=1
    fi
    
    # Stop memory monitoring
    if [ -n "$monitor_pid" ]; then
        kill "$monitor_pid" 2>/dev/null || true
        wait "$monitor_pid" 2>/dev/null || true
    fi
    
    return $test_result
}

# Function to check D-Bus availability and start test session
setup_dbus_environment() {
    echo -e "${CYAN}Setting up D-Bus test environment...${NC}"
    
    if ! command -v dbus-daemon >/dev/null 2>&1; then
        echo -e "${YELLOW}WARNING: D-Bus daemon not found. D-Bus tests will be skipped.${NC}"
        return 1
    fi
    
    # Start a clean D-Bus session for testing
    local dbus_config="${DBUS_TEST_SESSION_DIR}/session.conf"
    
    cat > "${dbus_config}" << 'EOF'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=/tmp</listen>
  <standard_session_servicedirs />
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
</busconfig>
EOF
    
    # Start D-Bus daemon
    dbus-daemon --config-file="${dbus_config}" --print-address --fork > "${DBUS_TEST_SESSION_DIR}/address"
    
    if [ -f "${DBUS_TEST_SESSION_DIR}/address" ]; then
        export DBUS_SESSION_BUS_ADDRESS=$(cat "${DBUS_TEST_SESSION_DIR}/address")
        echo "Started test D-Bus session: $DBUS_SESSION_BUS_ADDRESS"
        return 0
    else
        echo -e "${RED}ERROR: Failed to start D-Bus test session${NC}"
        return 1
    fi
}

# Function to build all integration tests
build_integration_tests() {
    echo -e "${CYAN}Building MPRIS integration tests...${NC}"
    
    cd "${BUILD_DIR}"
    
    # List of integration test binaries to build
    local test_binaries=(
        "test_mpris_integration"
        "test_mpris_spec_compliance"
        "test_mpris_stress_testing"
        "test_mpris_reconnection_behavior"
        "test_mpris_memory_validation"
        "test_mpris_concurrent_clients"
        "test_mpris_real_dbus_clients"
    )
    
    local build_failed=0
    for binary in "${test_binaries[@]}"; do
        if ! make -j$(nproc) "${binary}" 2>/dev/null; then
            echo -e "${YELLOW}Warning: Failed to build ${binary}${NC}"
            build_failed=1
        fi
    done
    
    if [ $build_failed -eq 1 ]; then
        echo -e "${YELLOW}Some integration tests could not be built. Continuing with available tests.${NC}"
    else
        echo -e "${GREEN}All integration tests built successfully${NC}"
    fi
    
    echo ""
}

# Function to run basic MPRIS functionality tests
run_basic_functionality_tests() {
    echo -e "${BLUE}Basic MPRIS Functionality Tests${NC}"
    echo "==============================="
    
    run_test "mpris_basic_integration" "${TEST_DIR}/test_mpris_integration" \
        "Basic MPRIS integration with Player class" 30 "BASIC"
    
    run_test "mpris_dbus_connection" "${TEST_DIR}/test_dbus_connection_manager_comprehensive" \
        "D-Bus connection management and error handling" 30 "BASIC"
    
    run_test "mpris_property_management" "${TEST_DIR}/test_property_manager_comprehensive" \
        "Property caching and thread-safe access" 30 "BASIC"
    
    run_test "mpris_method_handling" "${TEST_DIR}/test_method_handler_comprehensive" \
        "D-Bus method call processing" 30 "BASIC"
    
    run_test "mpris_signal_emission" "${TEST_DIR}/test_signal_emitter_comprehensive" \
        "Asynchronous signal emission" 30 "BASIC"
    
    echo ""
}

# Function to run MPRIS specification compliance tests
run_spec_compliance_tests() {
    echo -e "${BLUE}MPRIS Specification Compliance Tests${NC}"
    echo "===================================="
    
    run_test "mpris_spec_compliance" "${TEST_DIR}/test_mpris_spec_compliance" \
        "MPRIS D-Bus interface specification compliance" 45 "SPEC"
    
    # Test with real D-Bus introspection if available
    if command -v dbus-send >/dev/null 2>&1 && [ -n "$DBUS_SESSION_BUS_ADDRESS" ]; then
        echo "Testing D-Bus interface introspection..."
        local introspect_log="${LOG_DIR}/dbus_introspection.log"
        
        # Start a test MPRIS service in background
        "${TEST_DIR}/test_mpris_integration" --daemon > "${introspect_log}" 2>&1 &
        local mpris_pid=$!
        
        sleep 2  # Give service time to start
        
        # Test introspection
        if dbus-send --session --print-reply --dest=org.mpris.MediaPlayer2.psymp3 \
           /org/mpris/MediaPlayer2 org.freedesktop.DBus.Introspectable.Introspect \
           >> "${introspect_log}" 2>&1; then
            echo -e "D-Bus introspection: ${GREEN}PASS${NC}"
            log_result "PASS" "dbus_introspection" "D-Bus interface introspection successful"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            echo -e "D-Bus introspection: ${RED}FAIL${NC}"
            log_result "FAIL" "dbus_introspection" "D-Bus interface introspection failed"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
        
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        SPEC_COMPLIANCE_TESTS=$((SPEC_COMPLIANCE_TESTS + 1))
        
        # Clean up test service
        kill "$mpris_pid" 2>/dev/null || true
        wait "$mpris_pid" 2>/dev/null || true
    fi
    
    echo ""
}

# Function to run stress tests with multiple concurrent clients
run_stress_tests() {
    echo -e "${BLUE}MPRIS Stress Tests${NC}"
    echo "=================="
    
    run_test "mpris_stress_testing" "${TEST_DIR}/test_mpris_stress_testing" \
        "High-concurrency stress testing" 60 "STRESS"
    
    run_test "mpris_concurrent_clients" "${TEST_DIR}/test_mpris_concurrent_clients" \
        "Multiple concurrent D-Bus clients" 45 "STRESS"
    
    # Manual stress test with multiple processes
    echo "Running multi-process stress test..."
    local stress_log="${LOG_DIR}/multi_process_stress.log"
    local pids=()
    
    # Start multiple MPRIS test instances
    for i in {1..4}; do
        if [ -f "${TEST_DIR}/test_mpris_integration" ]; then
            "${TEST_DIR}/test_mpris_integration" --stress-mode >> "${stress_log}" 2>&1 &
            pids+=($!)
        fi
    done
    
    # Wait for all processes with timeout
    local all_completed=1
    for pid in "${pids[@]}"; do
        if ! timeout 30s bash -c "wait $pid"; then
            kill "$pid" 2>/dev/null || true
            all_completed=0
        fi
    done
    
    if [ $all_completed -eq 1 ]; then
        echo -e "Multi-process stress test: ${GREEN}PASS${NC}"
        log_result "PASS" "multi_process_stress" "Multiple MPRIS processes completed successfully"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "Multi-process stress test: ${RED}FAIL${NC}"
        log_result "FAIL" "multi_process_stress" "Some processes failed or timed out"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    STRESS_TESTS=$((STRESS_TESTS + 1))
    
    echo ""
}

# Function to test reconnection behavior
run_reconnection_tests() {
    echo -e "${BLUE}MPRIS Reconnection Tests${NC}"
    echo "========================"
    
    run_test "mpris_reconnection_behavior" "${TEST_DIR}/test_mpris_reconnection_behavior" \
        "D-Bus service restart and reconnection" 60 "RECONNECT"
    
    # Test D-Bus service restart simulation
    if [ -n "$DBUS_SESSION_BUS_ADDRESS" ] && command -v dbus-send >/dev/null 2>&1; then
        echo "Testing D-Bus service restart simulation..."
        local reconnect_log="${LOG_DIR}/dbus_restart_simulation.log"
        
        # Start MPRIS service
        "${TEST_DIR}/test_mpris_integration" --daemon > "${reconnect_log}" 2>&1 &
        local mpris_pid=$!
        
        sleep 2
        
        # Simulate D-Bus restart by killing and restarting our test session
        local old_address="$DBUS_SESSION_BUS_ADDRESS"
        
        # Kill old session
        pkill -f "dbus-daemon.*${DBUS_TEST_SESSION_DIR}" || true
        sleep 1
        
        # Start new session
        if setup_dbus_environment; then
            # Test if MPRIS can reconnect
            sleep 3
            
            if dbus-send --session --print-reply --dest=org.mpris.MediaPlayer2.psymp3 \
               /org/mpris/MediaPlayer2 org.freedesktop.DBus.Properties.Get \
               string:org.mpris.MediaPlayer2.Player string:PlaybackStatus \
               >> "${reconnect_log}" 2>&1; then
                echo -e "D-Bus restart simulation: ${GREEN}PASS${NC}"
                log_result "PASS" "dbus_restart_simulation" "MPRIS reconnected after D-Bus restart"
                PASSED_TESTS=$((PASSED_TESTS + 1))
            else
                echo -e "D-Bus restart simulation: ${RED}FAIL${NC}"
                log_result "FAIL" "dbus_restart_simulation" "MPRIS failed to reconnect after D-Bus restart"
                FAILED_TESTS=$((FAILED_TESTS + 1))
            fi
        else
            echo -e "D-Bus restart simulation: ${YELLOW}SKIP${NC} (failed to restart D-Bus)"
            log_result "SKIP" "dbus_restart_simulation" "Failed to restart D-Bus session"
            SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        fi
        
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        RECONNECTION_TESTS=$((RECONNECTION_TESTS + 1))
        
        # Clean up
        kill "$mpris_pid" 2>/dev/null || true
        wait "$mpris_pid" 2>/dev/null || true
    fi
    
    echo ""
}

# Function to run memory validation tests
run_memory_validation_tests() {
    echo -e "${BLUE}MPRIS Memory Validation Tests${NC}"
    echo "============================="
    
    run_test "mpris_memory_validation" "${TEST_DIR}/test_mpris_memory_validation" \
        "Memory usage and leak detection" 45 "MEMORY"
    
    # Run memory leak detection with valgrind if available
    if command -v valgrind >/dev/null 2>&1; then
        echo "Running memory leak detection with Valgrind..."
        local valgrind_log="${LOG_DIR}/valgrind_memcheck.log"
        
        if valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
           --track-origins=yes --verbose --log-file="${valgrind_log}" \
           "${TEST_DIR}/test_mpris_integration" > /dev/null 2>&1; then
            
            # Check for memory leaks in valgrind output
            if grep -q "ERROR SUMMARY: 0 errors" "${valgrind_log}" && \
               grep -q "definitely lost: 0 bytes" "${valgrind_log}"; then
                echo -e "Valgrind memory check: ${GREEN}PASS${NC}"
                log_result "PASS" "valgrind_memcheck" "No memory leaks detected"
                PASSED_TESTS=$((PASSED_TESTS + 1))
            else
                echo -e "Valgrind memory check: ${RED}FAIL${NC}"
                log_result "FAIL" "valgrind_memcheck" "Memory leaks or errors detected"
                FAILED_TESTS=$((FAILED_TESTS + 1))
            fi
        else
            echo -e "Valgrind memory check: ${RED}FAIL${NC}"
            log_result "FAIL" "valgrind_memcheck" "Valgrind execution failed"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
        
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        MEMORY_TESTS=$((MEMORY_TESTS + 1))
    else
        echo -e "${YELLOW}Valgrind not available - memory leak detection skipped${NC}"
    fi
    
    echo ""
}

# Function to test with real D-Bus clients
run_real_client_tests() {
    echo -e "${BLUE}Real D-Bus Client Tests${NC}"
    echo "======================="
    
    # Test with common media control tools if available
    local client_tools=("playerctl" "dbus-send" "gdbus")
    local clients_tested=0
    
    for tool in "${client_tools[@]}"; do
        if command -v "$tool" >/dev/null 2>&1; then
            echo "Testing with $tool..."
            local client_log="${LOG_DIR}/${tool}_test.log"
            
            # Start MPRIS service
            "${TEST_DIR}/test_mpris_integration" --daemon > "${client_log}" 2>&1 &
            local mpris_pid=$!
            
            sleep 2
            
            local test_passed=0
            case "$tool" in
                "playerctl")
                    if playerctl -p psymp3 status >> "${client_log}" 2>&1; then
                        test_passed=1
                    fi
                    ;;
                "dbus-send")
                    if dbus-send --session --print-reply \
                       --dest=org.mpris.MediaPlayer2.psymp3 \
                       /org/mpris/MediaPlayer2 \
                       org.freedesktop.DBus.Properties.Get \
                       string:org.mpris.MediaPlayer2.Player string:PlaybackStatus \
                       >> "${client_log}" 2>&1; then
                        test_passed=1
                    fi
                    ;;
                "gdbus")
                    if gdbus call --session \
                       --dest org.mpris.MediaPlayer2.psymp3 \
                       --object-path /org/mpris/MediaPlayer2 \
                       --method org.freedesktop.DBus.Properties.Get \
                       org.mpris.MediaPlayer2.Player PlaybackStatus \
                       >> "${client_log}" 2>&1; then
                        test_passed=1
                    fi
                    ;;
            esac
            
            if [ $test_passed -eq 1 ]; then
                echo -e "$tool client test: ${GREEN}PASS${NC}"
                log_result "PASS" "${tool}_client_test" "Real client interaction successful"
                PASSED_TESTS=$((PASSED_TESTS + 1))
            else
                echo -e "$tool client test: ${RED}FAIL${NC}"
                log_result "FAIL" "${tool}_client_test" "Real client interaction failed"
                FAILED_TESTS=$((FAILED_TESTS + 1))
            fi
            
            TOTAL_TESTS=$((TOTAL_TESTS + 1))
            clients_tested=$((clients_tested + 1))
            
            # Clean up
            kill "$mpris_pid" 2>/dev/null || true
            wait "$mpris_pid" 2>/dev/null || true
        fi
    done
    
    if [ $clients_tested -eq 0 ]; then
        echo -e "${YELLOW}No real D-Bus client tools found - real client tests skipped${NC}"
    fi
    
    echo ""
}

# Function to generate comprehensive test report
generate_test_report() {
    local report_file="${LOG_DIR}/integration_test_report.html"
    
    cat > "${report_file}" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>MPRIS Integration Test Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .header { background-color: #f0f0f0; padding: 10px; border-radius: 5px; }
        .summary { background-color: #e8f5e8; padding: 10px; margin: 10px 0; border-radius: 5px; }
        .category { margin: 20px 0; }
        .pass { color: green; font-weight: bold; }
        .fail { color: red; font-weight: bold; }
        .skip { color: orange; font-weight: bold; }
        .timeout { color: purple; font-weight: bold; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
    </style>
</head>
<body>
    <div class="header">
        <h1>MPRIS Integration Test Report</h1>
        <p>Generated: $(date)</p>
        <p>Test Session: $$</p>
    </div>
    
    <div class="summary">
        <h2>Test Summary</h2>
        <table>
            <tr><th>Category</th><th>Total</th><th>Passed</th><th>Failed</th><th>Skipped</th></tr>
            <tr><td>Overall</td><td>$TOTAL_TESTS</td><td class="pass">$PASSED_TESTS</td><td class="fail">$FAILED_TESTS</td><td class="skip">$SKIPPED_TESTS</td></tr>
            <tr><td>Basic Functionality</td><td>$BASIC_TESTS</td><td>-</td><td>-</td><td>-</td></tr>
            <tr><td>Spec Compliance</td><td>$SPEC_COMPLIANCE_TESTS</td><td>-</td><td>-</td><td>-</td></tr>
            <tr><td>Stress Testing</td><td>$STRESS_TESTS</td><td>-</td><td>-</td><td>-</td></tr>
            <tr><td>Reconnection</td><td>$RECONNECTION_TESTS</td><td>-</td><td>-</td><td>-</td></tr>
            <tr><td>Memory Validation</td><td>$MEMORY_TESTS</td><td>-</td><td>-</td><td>-</td></tr>
        </table>
    </div>
    
    <div class="category">
        <h2>Detailed Results</h2>
        <pre>
$(cat "${RESULTS_FILE}")
        </pre>
    </div>
    
    <div class="category">
        <h2>Log Files</h2>
        <ul>
$(find "${LOG_DIR}" -name "*.log" -type f | sed 's|.*/||' | sort | sed 's/^/<li>/' | sed 's/$/<\/li>/')
        </ul>
    </div>
</body>
</html>
EOF
    
    echo "Detailed test report generated: ${report_file}"
}

# Function to cleanup test environment
cleanup() {
    echo -e "${CYAN}Cleaning up test environment...${NC}"
    
    # Kill any remaining D-Bus session
    pkill -f "dbus-daemon.*${DBUS_TEST_SESSION_DIR}" 2>/dev/null || true
    
    # Kill any remaining test processes
    pkill -f "test_mpris_" 2>/dev/null || true
    
    echo "Test logs preserved in: ${LOG_DIR}"
}

# Set up cleanup trap
trap cleanup EXIT

# Main test execution function
main() {
    echo "Starting MPRIS integration test suite..."
    echo "Timestamp: $(date)"
    echo ""
    
    # Initialize results file
    echo "MPRIS Integration Test Results" > "${RESULTS_FILE}"
    echo "=============================" >> "${RESULTS_FILE}"
    echo "Started: $(date)" >> "${RESULTS_FILE}"
    echo "" >> "${RESULTS_FILE}"
    
    # Set up D-Bus environment
    if setup_dbus_environment; then
        echo -e "${GREEN}D-Bus test environment ready${NC}"
    else
        echo -e "${YELLOW}D-Bus environment setup failed - some tests will be skipped${NC}"
    fi
    echo ""
    
    # Build integration tests
    build_integration_tests
    
    # Run test categories
    run_basic_functionality_tests
    run_spec_compliance_tests
    run_stress_tests
    run_reconnection_tests
    run_memory_validation_tests
    run_real_client_tests
    
    # Generate comprehensive report
    generate_test_report
    
    # Print final summary
    echo -e "${BLUE}Final Test Summary${NC}"
    echo "=================="
    echo "Total tests: $TOTAL_TESTS"
    echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed: ${RED}$FAILED_TESTS${NC}"
    echo -e "Skipped: ${YELLOW}$SKIPPED_TESTS${NC}"
    echo ""
    echo "Test breakdown by category:"
    echo "  Basic Functionality: $BASIC_TESTS tests"
    echo "  Spec Compliance: $SPEC_COMPLIANCE_TESTS tests"
    echo "  Stress Testing: $STRESS_TESTS tests"
    echo "  Reconnection: $RECONNECTION_TESTS tests"
    echo "  Memory Validation: $MEMORY_TESTS tests"
    echo ""
    
    # Final result
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${GREEN}✓ All MPRIS integration tests PASSED!${NC}"
        echo "MPRIS system is ready for production use."
        exit 0
    else
        echo -e "${RED}✗ $FAILED_TESTS integration test(s) FAILED!${NC}"
        echo "Review test logs and fix issues before deployment."
        exit 1
    fi
}

# Run main function
main "$@"