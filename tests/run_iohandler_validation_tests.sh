#!/bin/bash
#
# run_iohandler_validation_tests.sh - Run IOHandler backward compatibility and performance validation tests
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#

echo "Running IOHandler Validation Test Suite"
echo "======================================="
echo

# Run legacy compatibility tests
echo "Running Legacy Compatibility Tests..."
if ./test_iohandler_legacy_compatibility; then
    echo "✓ Legacy compatibility tests PASSED"
else
    echo "✗ Legacy compatibility tests FAILED"
    exit 1
fi

echo
echo "Running Performance Validation Tests..."
if ./test_iohandler_performance_validation; then
    echo "✓ Performance validation tests PASSED"
else
    echo "✗ Performance validation tests FAILED"
    exit 1
fi

echo
echo "======================================="
echo "All IOHandler validation tests PASSED!"
echo "✓ Backward compatibility verified"
echo "✓ Performance validation completed"
echo "✓ IOHandler subsystem ready for production"