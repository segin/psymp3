#!/bin/bash
#
# run_demuxer_tests.sh - Run all demuxer architecture tests
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

set -e

echo "=========================================="
echo "PsyMP3 Demuxer Architecture Test Suite"
echo "=========================================="
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test results tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Function to run a test
run_test() {
    local test_name="$1"
    local test_executable="$2"
    
    echo -e "${YELLOW}Running $test_name...${NC}"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if ./"$test_executable"; then
        echo -e "${GREEN}✓ $test_name PASSED${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${RED}✗ $test_name FAILED${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
    echo
}

# Build tests first
echo "Building demuxer architecture tests..."
make -j$(nproc) \
    test_demuxer_unit \
    test_demuxer_factory_unit \
    test_media_factory_unit \
    test_demuxed_stream_unit \
    test_demuxer_integration \
    test_media_factory_integration \
    test_demuxer_performance

echo
echo "Starting test execution..."
echo

# Unit Tests
echo "=========================================="
echo "UNIT TESTS"
echo "=========================================="

run_test "Demuxer Unit Tests" "test_demuxer_unit"
run_test "DemuxerFactory Unit Tests" "test_demuxer_factory_unit"
run_test "MediaFactory Unit Tests" "test_media_factory_unit"
run_test "DemuxedStream Unit Tests" "test_demuxed_stream_unit"

# Integration Tests
echo "=========================================="
echo "INTEGRATION TESTS"
echo "=========================================="

run_test "Demuxer Integration Tests" "test_demuxer_integration"
run_test "MediaFactory Integration Tests" "test_media_factory_integration"

# Performance Tests
echo "=========================================="
echo "PERFORMANCE & REGRESSION TESTS"
echo "=========================================="

run_test "Demuxer Performance Tests" "test_demuxer_performance"

# Summary
echo "=========================================="
echo "TEST SUMMARY"
echo "=========================================="
echo "Total tests: $TOTAL_TESTS"
echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed: ${RED}$FAILED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed.${NC}"
    exit 1
fi