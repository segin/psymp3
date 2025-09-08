#!/bin/bash
#
# run_mpris_stress_tests.sh - Run MPRIS stress tests
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#

set -e

echo "Running MPRIS Stress Tests..."
echo "============================="

# Set up test environment for stress testing
export MPRIS_STRESS_TEST_MODE=1
export MPRIS_TEST_THREADS=8
export MPRIS_TEST_DURATION=10000  # 10 seconds
export MPRIS_TEST_OPERATIONS=50000

# Function to run stress test with resource monitoring
run_stress_test() {
    local test_name="$1"
    local test_executable="$2"
    
    echo ""
    echo "Running $test_name..."
    echo "----------------------------------------"
    
    if [ ! -x "$test_executable" ]; then
        echo "⚠ $test_name SKIPPED (executable not found)"
        return 0
    fi
    
    # Monitor system resources during test
    local start_time=$(date +%s)
    local pid_file="/tmp/stress_test_$$.pid"
    
    # Start resource monitoring in background
    (
        while [ -f "$pid_file" ]; do
            if command -v ps &> /dev/null; then
                ps -o pid,pcpu,pmem,comm -p $(cat "$pid_file" 2>/dev/null) 2>/dev/null || true
            fi
            sleep 1
        done
    ) &
    local monitor_pid=$!
    
    # Run the stress test
    echo $$ > "$pid_file"
    local test_result=0
    
    if timeout 60s ./"$test_executable"; then
        echo "✓ $test_name PASSED"
        test_result=0
    else
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo "⚠ $test_name TIMEOUT (may indicate deadlock or infinite loop)"
        else
            echo "✗ $test_name FAILED (exit code: $exit_code)"
        fi
        test_result=1
    fi
    
    # Clean up monitoring
    rm -f "$pid_file"
    kill $monitor_pid 2>/dev/null || true
    wait $monitor_pid 2>/dev/null || true
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    echo "Test duration: ${duration}s"
    
    return $test_result
}

# Track test results
total_tests=0
passed_tests=0
failed_tests=0

# Run stress tests
if run_stress_test "MPRIS Stress Testing Suite" "test_mpris_stress_testing"; then
    ((passed_tests++))
else
    ((failed_tests++))
fi
((total_tests++))

# Run additional stress scenarios if available
stress_test_programs=(
    "test_mpris_manager_integration"
    "test_threading_safety_baseline"
    "test_system_wide_threading_integration"
)

for test_prog in "${stress_test_programs[@]}"; do
    if [ -x "$test_prog" ]; then
        if run_stress_test "$(echo $test_prog | sed 's/_/ /g' | sed 's/\b\w/\U&/g')" "$test_prog"; then
            ((passed_tests++))
        else
            ((failed_tests++))
        fi
        ((total_tests++))
    fi
done

# System resource check after stress tests
echo ""
echo "Post-test System Check"
echo "====================="

if command -v free &> /dev/null; then
    echo "Memory usage:"
    free -h
fi

if command -v uptime &> /dev/null; then
    echo "System load:"
    uptime
fi

# Summary
echo ""
echo "Stress Test Summary"
echo "=================="
echo "Total tests: $total_tests"
echo "Passed: $passed_tests"
echo "Failed: $failed_tests"

if [ $failed_tests -eq 0 ]; then
    echo ""
    echo "✓ All MPRIS stress tests PASSED!"
    echo "System remained stable under stress conditions."
    exit 0
else
    echo ""
    echo "✗ $failed_tests stress test(s) FAILED!"
    echo "System may have stability issues under high load."
    exit 1
fi