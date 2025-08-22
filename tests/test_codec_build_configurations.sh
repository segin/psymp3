#!/bin/bash
#
# test_codec_build_configurations.sh - Test various G.711 codec build configurations
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

set -e  # Exit on any error

echo "=========================================="
echo "G.711 Codec Build Configuration Test"
echo "=========================================="
echo ""

# Function to clean build artifacts
clean_build() {
    echo "Cleaning build artifacts..."
    make clean >/dev/null 2>&1 || true
    rm -f config.cache config.log config.status >/dev/null 2>&1 || true
    rm -f src/*.o tests/*.o >/dev/null 2>&1 || true
}

# Function to test a specific configuration
test_configuration() {
    local config_name="$1"
    local configure_args="$2"
    local expected_alaw="$3"
    local expected_mulaw="$4"
    
    echo "----------------------------------------"
    echo "Testing configuration: $config_name"
    echo "Configure args: $configure_args"
    echo "Expected A-law: $expected_alaw"
    echo "Expected μ-law: $expected_mulaw"
    echo "----------------------------------------"
    
    # Clean previous build
    clean_build
    
    # Configure with specific options
    echo "Configuring..."
    if ! ./configure $configure_args >/dev/null 2>&1; then
        echo "ERROR: Configure failed for $config_name"
        return 1
    fi
    
    # Check configuration output
    echo "Checking configuration..."
    config_output=$(./configure $configure_args 2>&1)
    
    # Verify A-law configuration
    alaw_line=$(echo "$config_output" | grep "A-law (G.711):" || echo "")
    if echo "$alaw_line" | grep -q "$expected_alaw"; then
        echo "✓ A-law configuration correct: $expected_alaw"
    else
        echo "✗ A-law configuration incorrect"
        echo "Expected: $expected_alaw"
        echo "Found: $alaw_line"
        return 1
    fi
    
    # Verify μ-law configuration  
    mulaw_line=$(echo "$config_output" | grep "μ-law (G.711):" || echo "")
    if echo "$mulaw_line" | grep -q "$expected_mulaw"; then
        echo "✓ μ-law configuration correct: $expected_mulaw"
    else
        echo "✗ μ-law configuration incorrect"
        echo "Expected: $expected_mulaw"
        echo "Found: $mulaw_line"
        return 1
    fi
    
    # Build the project
    echo "Building..."
    if ! make -j$(nproc) >/dev/null 2>&1; then
        echo "ERROR: Build failed for $config_name"
        return 1
    fi
    
    # Check that appropriate source files were compiled
    if [ "$expected_alaw" = "yes" ]; then
        if [ -f "src/ALawCodec.o" ]; then
            echo "✓ ALawCodec.o compiled as expected"
        else
            echo "✗ ALawCodec.o missing but A-law enabled"
            return 1
        fi
    else
        if [ ! -f "src/ALawCodec.o" ]; then
            echo "✓ ALawCodec.o not compiled as expected"
        else
            echo "✗ ALawCodec.o compiled but A-law disabled"
            return 1
        fi
    fi
    
    if [ "$expected_mulaw" = "yes" ]; then
        if [ -f "src/MuLawCodec.o" ]; then
            echo "✓ MuLawCodec.o compiled as expected"
        else
            echo "✗ MuLawCodec.o missing but μ-law enabled"
            return 1
        fi
    else
        if [ ! -f "src/MuLawCodec.o" ]; then
            echo "✓ MuLawCodec.o not compiled as expected"
        else
            echo "✗ MuLawCodec.o compiled but μ-law disabled"
            return 1
        fi
    fi
    
    # Build and run the conditional compilation test
    echo "Building conditional compilation test..."
    if ! make -C tests test_conditional_codec_compilation >/dev/null 2>&1; then
        echo "ERROR: Failed to build conditional compilation test for $config_name"
        return 1
    fi
    
    echo "Running conditional compilation test..."
    if ! ./tests/test_conditional_codec_compilation >/dev/null 2>&1; then
        echo "ERROR: Conditional compilation test failed for $config_name"
        return 1
    fi
    
    echo "✓ Configuration $config_name PASSED"
    echo ""
    return 0
}

# Test different configurations
failed_tests=0

# Test 1: Both codecs enabled (default)
if ! test_configuration "Both enabled (default)" "" "yes" "yes"; then
    failed_tests=$((failed_tests + 1))
fi

# Test 2: Both codecs explicitly enabled
if ! test_configuration "Both explicitly enabled" "--enable-alaw --enable-mulaw" "yes" "yes"; then
    failed_tests=$((failed_tests + 1))
fi

# Test 3: Only A-law enabled
if ! test_configuration "A-law only" "--enable-alaw --disable-mulaw" "yes" "no"; then
    failed_tests=$((failed_tests + 1))
fi

# Test 4: Only μ-law enabled
if ! test_configuration "μ-law only" "--disable-alaw --enable-mulaw" "no" "yes"; then
    failed_tests=$((failed_tests + 1))
fi

# Test 5: Both codecs disabled
if ! test_configuration "Both disabled" "--disable-alaw --disable-mulaw" "no" "no"; then
    failed_tests=$((failed_tests + 1))
fi

# Clean up after tests
clean_build

# Report results
echo "=========================================="
echo "Build Configuration Test Results"
echo "=========================================="
if [ $failed_tests -eq 0 ]; then
    echo "✓ All build configuration tests PASSED"
    echo ""
    echo "The G.711 codec build system correctly handles:"
    echo "  - Default configuration (both codecs enabled)"
    echo "  - Explicit enable/disable options"
    echo "  - Individual codec enable/disable"
    echo "  - Complete codec disable"
    echo "  - Conditional compilation guards"
    echo "  - Proper object file generation"
    exit 0
else
    echo "✗ $failed_tests build configuration test(s) FAILED"
    echo ""
    echo "Please check the build system configuration."
    exit 1
fi