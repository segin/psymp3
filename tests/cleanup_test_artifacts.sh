#!/bin/bash

# Cleanup script for test build artifacts
# This script removes obsolete test build artifacts and temporary files

echo "Cleaning up test build artifacts..."

# Remove object files
find . -name "*.o" -type f -delete

# Remove executables without extension (test binaries)
find . -type f -executable -not -name "*.sh" -not -name "test-harness" -delete

# Remove temporary files
find . -name "*.tmp" -type f -delete
find . -name "*.log" -type f -delete
find . -name "*.xml" -type f -delete
find . -name "*.json" -type f -delete
find . -name "*.csv" -type f -delete

# Remove static libraries
find . -name "*.a" -type f -delete

# Clean up any mock test files that might have been left behind
find . -name "test_mock_*" -type f -delete

echo "Cleanup complete!"