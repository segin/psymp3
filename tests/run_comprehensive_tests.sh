#!/bin/bash

# Comprehensive test runner for OGG demuxer implementation
# This script runs all comprehensive tests and validates the implementation

echo "OGG Demuxer Comprehensive Test Suite Runner"
echo "==========================================="
echo

# Check if comprehensive test exists
if [ ! -f "./test_oggdemuxer_comprehensive" ]; then
    echo "Error: test_oggdemuxer_comprehensive not found. Please build it first with:"
    echo "  make test_oggdemuxer_comprehensive"
    exit 1
fi

# Run the comprehensive test
echo "Running comprehensive test suite..."
echo "-----------------------------------"
./test_oggdemuxer_comprehensive

# Check exit code
if [ $? -eq 0 ]; then
    echo
    echo "✓ Comprehensive test suite completed successfully"
    exit 0
else
    echo
    echo "✗ Comprehensive test suite failed"
    exit 1
fi