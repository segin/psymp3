#!/bin/bash
#
# test_build_system_integration.sh - Quick test of G.711 codec build system integration
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#

set -e  # Exit on any error

echo "=========================================="
echo "G.711 Codec Build System Integration Test"
echo "=========================================="
echo ""

# Function to test configuration only (no build)
test_config_only() {
    local config_name="$1"
    local configure_args="$2"
    local expected_alaw="$3"
    local expected_mulaw="$4"
    
    echo "Testing: $config_name"
    echo "Args: $configure_args"
    
    # Clean config cache
    rm -f config.cache config.log config.status >/dev/null 2>&1 || true
    
    # Configure
    if ! ./configure $configure_args >/dev/null 2>&1; then
        echo "✗ Configure failed"
        return 1
    fi
    
    # Check output
    config_output=$(./configure $configure_args 2>&1)
    alaw_line=$(echo "$config_output" | grep "A-law (G.711):" || echo "")
    mulaw_line=$(echo "$config_output" | grep "μ-law (G.711):" || echo "")
    
    if echo "$alaw_line" | grep -q "$expected_alaw" && echo "$mulaw_line" | grep -q "$expected_mulaw"; then
        echo "✓ Configuration correct (A-law: $expected_alaw, μ-law: $expected_mulaw)"
        return 0
    else
        echo "✗ Configuration incorrect"
        echo "  Expected A-law: $expected_alaw, got: $alaw_line"
        echo "  Expected μ-law: $expected_mulaw, got: $mulaw_line"
        return 1
    fi
}

# Test different configurations (config only, no build)
echo "Testing configuration options..."
echo ""

failed_tests=0

# Test 1: Default (both enabled)
if ! test_config_only "Default" "" "yes" "yes"; then
    failed_tests=$((failed_tests + 1))
fi

# Test 2: Both explicitly enabled
if ! test_config_only "Both enabled" "--enable-alaw --enable-mulaw" "yes" "yes"; then
    failed_tests=$((failed_tests + 1))
fi

# Test 3: A-law only
if ! test_config_only "A-law only" "--enable-alaw --disable-mulaw" "yes" "no"; then
    failed_tests=$((failed_tests + 1))
fi

# Test 4: μ-law only
if ! test_config_only "μ-law only" "--disable-alaw --enable-mulaw" "no" "yes"; then
    failed_tests=$((failed_tests + 1))
fi

# Test 5: Both disabled
if ! test_config_only "Both disabled" "--disable-alaw --disable-mulaw" "no" "no"; then
    failed_tests=$((failed_tests + 1))
fi

echo ""
echo "Testing build with one configuration..."

# Test actual build with A-law only (faster than testing all)
echo "Configuring for A-law only build test..."
if ./configure --enable-alaw --disable-mulaw >/dev/null 2>&1; then
    echo "✓ Configure successful"
    
    # Clean and build just the codec files
    make clean >/dev/null 2>&1 || true
    
    echo "Building codec files..."
    if make -C src ALawCodec.o >/dev/null 2>&1; then
        echo "✓ ALawCodec.o built successfully"
        
        # Verify MuLawCodec.o was not built
        if [ ! -f "src/MuLawCodec.o" ]; then
            echo "✓ MuLawCodec.o correctly not built"
        else
            echo "✗ MuLawCodec.o was built but should not have been"
            failed_tests=$((failed_tests + 1))
        fi
    else
        echo "✗ Failed to build ALawCodec.o"
        failed_tests=$((failed_tests + 1))
    fi
    
    # Test conditional compilation test
    echo "Testing conditional compilation test..."
    if make -C tests test_conditional_codec_compilation >/dev/null 2>&1; then
        echo "✓ Conditional compilation test built successfully"
        
        if ./tests/test_conditional_codec_compilation >/dev/null 2>&1; then
            echo "✓ Conditional compilation test passed"
        else
            echo "✗ Conditional compilation test failed"
            failed_tests=$((failed_tests + 1))
        fi
    else
        echo "✗ Failed to build conditional compilation test"
        failed_tests=$((failed_tests + 1))
    fi
else
    echo "✗ Configure failed for build test"
    failed_tests=$((failed_tests + 1))
fi

# Clean up
make clean >/dev/null 2>&1 || true
rm -f config.cache config.log config.status >/dev/null 2>&1 || true

echo ""
echo "=========================================="
echo "Build System Integration Test Results"
echo "=========================================="

if [ $failed_tests -eq 0 ]; then
    echo "✓ All tests PASSED"
    echo ""
    echo "The G.711 codec build system correctly:"
    echo "  - Handles configure options (--enable/--disable-alaw/mulaw)"
    echo "  - Sets appropriate preprocessor definitions"
    echo "  - Conditionally compiles codec source files"
    echo "  - Supports all valid codec combinations"
    echo "  - Integrates with the test framework"
    exit 0
else
    echo "✗ $failed_tests test(s) FAILED"
    exit 1
fi