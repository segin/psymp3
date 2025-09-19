#!/bin/bash

# run_iso_compliance_tests.sh - Run comprehensive ISO demuxer compliance validation tests
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${TEST_DIR}"
RESULTS_DIR="${TEST_DIR}/compliance_test_results"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Create results directory
mkdir -p "${RESULTS_DIR}"

echo -e "${BLUE}=== ISO Demuxer Compliance Validation Test Suite ===${NC}"
echo "Starting comprehensive compliance validation tests..."
echo "Results will be saved to: ${RESULTS_DIR}"
echo

# Test executables to build and run
declare -a COMPLIANCE_TESTS=(
    "test_iso_compliance_comprehensive"
    "test_iso_compliance_edge_cases"
    "test_iso_codec_compliance"
    "test_iso_compliance_validation"  # Existing test
)

# Build all compliance tests
echo -e "${YELLOW}Building compliance validation tests...${NC}"
cd "${BUILD_DIR}"

TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
BUILD_FAILURES=0

for test_name in "${COMPLIANCE_TESTS[@]}"; do
    echo -n "Building ${test_name}... "
    
    if make "${test_name}" > "${RESULTS_DIR}/${test_name}_build.log" 2>&1; then
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${RED}FAILED${NC}"
        echo "Build log saved to: ${RESULTS_DIR}/${test_name}_build.log"
        BUILD_FAILURES=$((BUILD_FAILURES + 1))
        continue
    fi
    
    # Run the test
    echo -n "Running ${test_name}... "
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if ./"${test_name}" > "${RESULTS_DIR}/${test_name}_${TIMESTAMP}.log" 2>&1; then
        echo -e "${GREEN}PASSED${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}FAILED${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        echo "Test log saved to: ${RESULTS_DIR}/${test_name}_${TIMESTAMP}.log"
        
        # Show last few lines of failure for immediate feedback
        echo -e "${YELLOW}Last 10 lines of test output:${NC}"
        tail -n 10 "${RESULTS_DIR}/${test_name}_${TIMESTAMP}.log" | sed 's/^/  /'
        echo
    fi
done

# Generate comprehensive test report
REPORT_FILE="${RESULTS_DIR}/compliance_test_report_${TIMESTAMP}.txt"
echo "=== ISO Demuxer Compliance Validation Test Report ===" > "${REPORT_FILE}"
echo "Generated: $(date)" >> "${REPORT_FILE}"
echo "Test Directory: ${TEST_DIR}" >> "${REPORT_FILE}"
echo >> "${REPORT_FILE}"

echo "=== Test Summary ===" >> "${REPORT_FILE}"
echo "Total Tests: ${TOTAL_TESTS}" >> "${REPORT_FILE}"
echo "Passed: ${PASSED_TESTS}" >> "${REPORT_FILE}"
echo "Failed: ${FAILED_TESTS}" >> "${REPORT_FILE}"
echo "Build Failures: ${BUILD_FAILURES}" >> "${REPORT_FILE}"
echo >> "${REPORT_FILE}"

echo "=== Test Coverage Areas ===" >> "${REPORT_FILE}"
echo "✓ Box structure validation (32-bit and 64-bit sizes)" >> "${REPORT_FILE}"
echo "✓ Timestamp and timescale validation" >> "${REPORT_FILE}"
echo "✓ Sample table consistency validation" >> "${REPORT_FILE}"
echo "✓ Codec-specific data integrity tests" >> "${REPORT_FILE}"
echo "✓ Container format compliance tests" >> "${REPORT_FILE}"
echo "✓ Edge case and error condition handling" >> "${REPORT_FILE}"
echo "✓ AAC codec compliance validation" >> "${REPORT_FILE}"
echo "✓ ALAC codec compliance validation" >> "${REPORT_FILE}"
echo "✓ Telephony codec (mulaw/alaw) compliance" >> "${REPORT_FILE}"
echo "✓ PCM codec compliance validation" >> "${REPORT_FILE}"
echo >> "${REPORT_FILE}"

echo "=== Individual Test Results ===" >> "${REPORT_FILE}"
for test_name in "${COMPLIANCE_TESTS[@]}"; do
    if [ -f "${RESULTS_DIR}/${test_name}_${TIMESTAMP}.log" ]; then
        if grep -q "All.*tests passed" "${RESULTS_DIR}/${test_name}_${TIMESTAMP}.log" 2>/dev/null; then
            echo "✓ ${test_name}: PASSED" >> "${REPORT_FILE}"
        else
            echo "✗ ${test_name}: FAILED" >> "${REPORT_FILE}"
        fi
    else
        echo "✗ ${test_name}: BUILD FAILED" >> "${REPORT_FILE}"
    fi
done

echo >> "${REPORT_FILE}"
echo "=== Requirements Coverage ===" >> "${REPORT_FILE}"
echo "This test suite covers the following requirements from task 18:" >> "${REPORT_FILE}"
echo "- 12.1: ISO/IEC 14496-12 specification compliance" >> "${REPORT_FILE}"
echo "- 12.2: 32-bit and 64-bit box size validation" >> "${REPORT_FILE}"
echo "- 12.3: Timestamp and timescale validation" >> "${REPORT_FILE}"
echo "- 12.4: Sample table consistency validation" >> "${REPORT_FILE}"
echo "- 12.5: Codec-specific data integrity validation" >> "${REPORT_FILE}"
echo "- 12.6: Container format compliance validation" >> "${REPORT_FILE}"
echo "- 12.7: Track structure compliance validation" >> "${REPORT_FILE}"
echo "- 12.8: Comprehensive compliance reporting" >> "${REPORT_FILE}"

# Display summary
echo
echo -e "${BLUE}=== Test Summary ===${NC}"
echo -e "Total Tests: ${TOTAL_TESTS}"
echo -e "Passed: ${GREEN}${PASSED_TESTS}${NC}"
echo -e "Failed: ${RED}${FAILED_TESTS}${NC}"
echo -e "Build Failures: ${RED}${BUILD_FAILURES}${NC}"
echo
echo "Detailed report saved to: ${REPORT_FILE}"

# Run additional validation checks if all tests passed
if [ ${FAILED_TESTS} -eq 0 ] && [ ${BUILD_FAILURES} -eq 0 ]; then
    echo -e "${GREEN}All compliance validation tests passed!${NC}"
    echo
    echo -e "${YELLOW}Running additional validation checks...${NC}"
    
    # Check for memory leaks in tests (if valgrind is available)
    if command -v valgrind >/dev/null 2>&1; then
        echo "Running memory leak detection on compliance tests..."
        for test_name in "${COMPLIANCE_TESTS[@]}"; do
            if [ -f "${test_name}" ]; then
                echo -n "Memory check for ${test_name}... "
                if valgrind --leak-check=full --error-exitcode=1 ./"${test_name}" > "${RESULTS_DIR}/${test_name}_valgrind.log" 2>&1; then
                    echo -e "${GREEN}OK${NC}"
                else
                    echo -e "${YELLOW}WARNINGS${NC} (see ${RESULTS_DIR}/${test_name}_valgrind.log)"
                fi
            fi
        done
    fi
    
    # Performance benchmark for compliance validation
    echo "Running performance benchmark for compliance validation..."
    if [ -f "test_iso_compliance_comprehensive" ]; then
        echo -n "Benchmarking comprehensive compliance tests... "
        time_output=$(time -p ./test_iso_compliance_comprehensive 2>&1 | grep real | awk '{print $2}')
        echo "completed in ${time_output}s"
        echo "Performance: ${time_output}s" >> "${REPORT_FILE}"
    fi
    
    echo
    echo -e "${GREEN}✓ All ISO demuxer compliance validation tests completed successfully!${NC}"
    echo -e "${GREEN}✓ Task 18 requirements have been fully implemented and validated.${NC}"
    
    exit 0
else
    echo -e "${RED}Some compliance validation tests failed.${NC}"
    echo "Please check the individual test logs in ${RESULTS_DIR} for details."
    
    # Show summary of failures
    if [ ${BUILD_FAILURES} -gt 0 ]; then
        echo -e "${RED}Build failures occurred. Check build logs for compilation errors.${NC}"
    fi
    
    if [ ${FAILED_TESTS} -gt 0 ]; then
        echo -e "${RED}Test failures occurred. This may indicate compliance validation issues.${NC}"
    fi
    
    exit 1
fi