#!/bin/bash
#
# run_flac_comprehensive_tests.sh - Run comprehensive FLAC demuxer tests
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#

set -e

echo "FLAC Demuxer Comprehensive Test Suite"
echo "====================================="
echo

# Check if test file exists
TEST_FILE="/mnt/8TB-3/music/almost monday/DIVE/11 life goes by.flac"
if [ ! -f "$TEST_FILE" ]; then
    echo "Warning: Test FLAC file not found at $TEST_FILE"
    echo "Some tests may be skipped."
    echo
fi

# Build tests if needed
echo "Building FLAC tests..."
make -j$(nproc) test_flac_demuxer_unit_comprehensive test_flac_demuxer_integration_comprehensive

echo "Running comprehensive FLAC demuxer tests..."
echo

# Run unit tests
echo "=== Unit Tests ==="
./test_flac_demuxer_unit_comprehensive
echo

# Run integration tests
echo "=== Integration Tests ==="
./test_flac_demuxer_integration_comprehensive
echo

echo "All comprehensive FLAC tests completed!"