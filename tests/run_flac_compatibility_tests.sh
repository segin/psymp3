#!/bin/bash
#
# run_flac_compatibility_tests.sh - Run FLAC demuxer compatibility tests
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
NC='\033[0m' # No Color

# Test configuration
TEST_DIR="$(dirname "$0")"
BUILD_DIR="${TEST_DIR}/.."
FLAC_TESTS=(
    "test_flac_demuxer_compatibility"
    "test_flac_demuxer_performance" 
    "test_flac_compatibility_integration"
)

# Statistics
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

echo -e "${BLUE}FLAC Demuxer Compatibility Test Suite${NC}"
echo "======================================"
echo

# Check if FLAC support is enabled
if ! grep -q "HAVE_FLAC" "${BUILD_DIR}/include/config.h" 2>/dev/null; then
    echo -e "${YELLOW}FLAC support not enabled - skipping FLAC tests${NC}"
    exit 0
fi

# Function to run a single test
run_test() {
    local test_name="$1"
    local test_executable="${TEST_DIR}/${test_name}"
    
    echo -n "Running ${test_name}... "
    
    if [ ! -f "$test_executable" ]; then
        echo -e "${YELLOW}SKIPPED${NC} (executable not found)"
        ((SKIPPED_TESTS++))
        return
    fi
    
    if [ ! -x "$test_executable" ]; then
        echo -e "${YELLOW}SKIPPED${NC} (not executable)"
        ((SKIPPED_TESTS++))
        return
    fi
    
    # Run the test and capture output
    local output_file=$(mktemp)
    local start_time=$(date +%s.%N)
    
    if "$test_executable" > "$output_file" 2>&1; then
        local end_time=$(date +%s.%N)
        local duration=$(echo "$end_time - $start_time" | bc -l 2>/dev/null || echo "0")
        
        echo -e "${GREEN}PASSED${NC} (${duration}s)"
        ((PASSED_TESTS++))
        
        # Show summary from test output if available
        if grep -q "Tests:" "$output_file"; then
            grep "Tests:" "$output_file" | head -1 | sed 's/^/  /'
        fi
    else
        local end_time=$(date +%s.%N)
        local duration=$(echo "$end_time - $start_time" | bc -l 2>/dev/null || echo "0")
        
        echo -e "${RED}FAILED${NC} (${duration}s)"
        ((FAILED_TESTS++))
        
        # Show failure details
        echo "  Error output:"
        sed 's/^/    /' "$output_file"
    fi
    
    rm -f "$output_file"
    ((TOTAL_TESTS++))
}

# Function to build tests if needed
build_tests() {
    echo "Building FLAC compatibility tests..."
    
    # Change to build directory
    cd "$BUILD_DIR"
    
    # Build the test executables
    if make -C tests "${FLAC_TESTS[@]}" > /dev/null 2>&1; then
        echo -e "${GREEN}Build successful${NC}"
        return 0
    else
        echo -e "${RED}Build failed${NC}"
        echo "Attempting to build with verbose output:"
        make -C tests "${FLAC_TESTS[@]}"
        return 1
    fi
}

# Function to check dependencies
check_dependencies() {
    echo "Checking dependencies..."
    
    # Check if libFLAC is available
    if ! pkg-config --exists flac 2>/dev/null; then
        echo -e "${YELLOW}Warning: libFLAC not found via pkg-config${NC}"
    fi
    
    # Check if FLAC encoder is available (for integration tests)
    if command -v flac >/dev/null 2>&1; then
        echo "FLAC encoder found: $(which flac)"
    else
        echo -e "${YELLOW}Warning: FLAC encoder not found - some integration tests may be skipped${NC}"
    fi
    
    # Check if bc is available for timing calculations
    if ! command -v bc >/dev/null 2>&1; then
        echo -e "${YELLOW}Warning: bc not found - timing calculations may not work${NC}"
    fi
    
    echo
}

# Function to create test summary
print_summary() {
    echo
    echo "======================================"
    echo "FLAC Compatibility Test Summary"
    echo "======================================"
    echo "Total tests:   $TOTAL_TESTS"
    echo -e "Passed tests:  ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed tests:  ${RED}$FAILED_TESTS${NC}"
    echo -e "Skipped tests: ${YELLOW}$SKIPPED_TESTS${NC}"
    echo
    
    if [ $FAILED_TESTS -eq 0 ]; then
        if [ $PASSED_TESTS -gt 0 ]; then
            echo -e "${GREEN}All tests passed!${NC}"
            return 0
        else
            echo -e "${YELLOW}No tests were run${NC}"
            return 1
        fi
    else
        echo -e "${RED}Some tests failed${NC}"
        return 1
    fi
}

# Function to run performance benchmarks
run_benchmarks() {
    echo
    echo "Running performance benchmarks..."
    echo "================================="
    
    local perf_test="${TEST_DIR}/test_flac_demuxer_performance"
    
    if [ -x "$perf_test" ]; then
        echo "Running FLAC demuxer performance tests..."
        "$perf_test" --benchmark 2>/dev/null || true
    else
        echo -e "${YELLOW}Performance test not available${NC}"
    fi
}

# Function to generate test report
generate_report() {
    local report_file="${TEST_DIR}/flac_compatibility_report.txt"
    
    echo "Generating test report: $report_file"
    
    {
        echo "FLAC Demuxer Compatibility Test Report"
        echo "Generated on: $(date)"
        echo "System: $(uname -a)"
        echo
        echo "Build Configuration:"
        if [ -f "${BUILD_DIR}/config.log" ]; then
            grep -E "(FLAC|flac)" "${BUILD_DIR}/config.log" | head -5 || echo "No FLAC configuration found"
        fi
        echo
        echo "Test Results:"
        echo "Total: $TOTAL_TESTS, Passed: $PASSED_TESTS, Failed: $FAILED_TESTS, Skipped: $SKIPPED_TESTS"
        echo
        
        # Add individual test results if available
        for test in "${FLAC_TESTS[@]}"; do
            echo "Test: $test"
            if [ -x "${TEST_DIR}/$test" ]; then
                "${TEST_DIR}/$test" --summary 2>/dev/null || echo "  No summary available"
            else
                echo "  Not available"
            fi
            echo
        done
    } > "$report_file"
    
    echo "Report saved to: $report_file"
}

# Main execution
main() {
    # Parse command line arguments
    local build_only=false
    local run_benchmarks=false
    local generate_report_flag=false
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --build-only)
                build_only=true
                shift
                ;;
            --benchmarks)
                run_benchmarks=true
                shift
                ;;
            --report)
                generate_report_flag=true
                shift
                ;;
            --help)
                echo "Usage: $0 [OPTIONS]"
                echo "Options:"
                echo "  --build-only    Only build tests, don't run them"
                echo "  --benchmarks    Run performance benchmarks"
                echo "  --report        Generate test report"
                echo "  --help          Show this help"
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    # Check dependencies
    check_dependencies
    
    # Build tests
    if ! build_tests; then
        echo -e "${RED}Failed to build tests${NC}"
        exit 1
    fi
    
    if [ "$build_only" = true ]; then
        echo "Build complete. Use without --build-only to run tests."
        exit 0
    fi
    
    # Run tests
    echo "Running FLAC compatibility tests..."
    echo
    
    for test in "${FLAC_TESTS[@]}"; do
        run_test "$test"
    done
    
    # Run benchmarks if requested
    if [ "$run_benchmarks" = true ]; then
        run_benchmarks
    fi
    
    # Generate report if requested
    if [ "$generate_report_flag" = true ]; then
        generate_report
    fi
    
    # Print summary and exit with appropriate code
    if print_summary; then
        exit 0
    else
        exit 1
    fi
}

# Run main function with all arguments
main "$@"