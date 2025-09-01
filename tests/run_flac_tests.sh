#!/bin/bash
#
# run_flac_tests.sh - Run all FLAC tests with test data validation
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#

set -e

echo "=== PsyMP3 FLAC Test Suite Runner ==="
echo "Running comprehensive FLAC tests with test data validation"
echo

# Check if test data exists
if [ ! -d "tests/data" ]; then
    echo "ERROR: Test data directory not found!"
    echo "Expected: tests/data/"
    exit 1
fi

# Count available FLAC test files
flac_files=$(find tests/data -name "*.flac" 2>/dev/null | wc -l)
echo "Found $flac_files FLAC test data files"

if [ "$flac_files" -eq 0 ]; then
    echo "WARNING: No FLAC test data files found in tests/data/"
    echo "Some tests may be skipped"
fi

echo

# List of FLAC tests to run (in order of importance)
tests=(
    "test_flac_test_data_validation"
    "test_flac_rfc_compliance"
    "test_flac_comprehensive_validation"
    "test_flac_quality_validation"
    "test_flac_performance_with_real_files"
    "test_flac_codec_integration"
    "test_flac_codec_threading_safety"
    "test_flac_demuxer_simple"
    "test_flac_codec_unit_comprehensive"
    "test_flac_codec_performance_comprehensive"
)

passed=0
failed=0
skipped=0

echo "Running FLAC test suite..."
echo "========================="

for test in "${tests[@]}"; do
    echo
    echo "Running: $test"
    echo "----------------------------------------"
    
    if [ ! -f "$test" ]; then
        echo "SKIP: Test executable not found"
        ((skipped++))
        continue
    fi
    
    if ./"$test"; then
        echo "PASS: $test"
        ((passed++))
    else
        echo "FAIL: $test"
        ((failed++))
    fi
done

echo
echo "=== Test Results Summary ==="
echo "Passed:  $passed"
echo "Failed:  $failed"
echo "Skipped: $skipped"
echo "Total:   $((passed + failed + skipped))"

if [ "$failed" -eq 0 ]; then
    echo
    echo "✓ All FLAC tests passed!"
    exit 0
else
    echo
    echo "✗ Some FLAC tests failed!"
    exit 1
fi