#!/bin/bash
#
# validate_mock_framework.sh - Validate MPRIS mock framework implementation
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#

set -e

echo "Validating MPRIS Mock Framework Implementation..."
echo "================================================"

# Function to check if a file exists and is readable
check_file() {
    local file="$1"
    local description="$2"
    
    if [ -f "$file" ] && [ -r "$file" ]; then
        echo "✓ $description: $file"
        return 0
    else
        echo "✗ $description: $file (missing or not readable)"
        return 1
    fi
}

# Function to check if a header file has required includes
check_header_includes() {
    local header="$1"
    local required_includes=("${@:2}")
    
    echo "Checking includes in $header..."
    
    for include in "${required_includes[@]}"; do
        if grep -q "#include.*$include" "$header"; then
            echo "  ✓ $include"
        else
            echo "  ✗ $include (missing)"
            return 1
        fi
    done
    
    return 0
}

# Function to check if a source file compiles
check_compilation() {
    local source="$1"
    local description="$2"
    
    echo "Checking compilation of $description..."
    
    # Try to compile with basic flags
    if g++ -I../include -std=c++17 -c "$source" -o /tmp/test_compile_$$.o 2>/dev/null; then
        echo "✓ $description compiles successfully"
        rm -f /tmp/test_compile_$$.o
        return 0
    else
        echo "✗ $description has compilation errors"
        return 1
    fi
}

validation_errors=0

# Check header files
echo ""
echo "Checking Header Files"
echo "===================="

headers=(
    "test_framework_threading.h:Threading safety test utilities"
    "mock_dbus_connection.h:Mock D-Bus connection"
    "mock_player.h:Mock Player class"
    "mpris_test_fixtures.h:MPRIS test fixtures"
)

for header_info in "${headers[@]}"; do
    IFS=':' read -r header description <<< "$header_info"
    if ! check_file "$header" "$description"; then
        ((validation_errors++))
    fi
done

# Check source files
echo ""
echo "Checking Source Files"
echo "===================="

sources=(
    "test_framework_threading.cpp:Threading utilities implementation"
    "mock_dbus_connection.cpp:Mock D-Bus connection implementation"
    "mock_player.cpp:Mock Player implementation"
    "test_mpris_mock_framework.cpp:Mock framework test suite"
    "test_mpris_stress_testing.cpp:Stress testing suite"
)

for source_info in "${sources[@]}"; do
    IFS=':' read -r source description <<< "$source_info"
    if ! check_file "$source" "$description"; then
        ((validation_errors++))
    fi
done

# Check test scripts
echo ""
echo "Checking Test Scripts"
echo "===================="

scripts=(
    "run_mpris_mock_tests.sh:Mock framework test runner"
    "run_mpris_stress_tests.sh:Stress test runner"
    "validate_mock_framework.sh:Framework validation script"
)

for script_info in "${scripts[@]}"; do
    IFS=':' read -r script description <<< "$script_info"
    if check_file "$script" "$description"; then
        # Check if script is executable
        if [ -x "$script" ]; then
            echo "  ✓ Executable permissions"
        else
            echo "  ⚠ Missing executable permissions (fixing...)"
            chmod +x "$script"
        fi
    else
        ((validation_errors++))
    fi
done

# Check Makefile integration
echo ""
echo "Checking Makefile Integration"
echo "============================"

if [ -f "Makefile.am" ]; then
    echo "✓ Makefile.am exists"
    
    # Check if mock framework sources are included
    if grep -q "test_framework_threading.cpp" Makefile.am; then
        echo "✓ Threading utilities included in build"
    else
        echo "✗ Threading utilities not included in build"
        ((validation_errors++))
    fi
    
    if grep -q "mock_dbus_connection.cpp" Makefile.am; then
        echo "✓ Mock D-Bus connection included in build"
    else
        echo "✗ Mock D-Bus connection not included in build"
        ((validation_errors++))
    fi
    
    if grep -q "mock_player.cpp" Makefile.am; then
        echo "✓ Mock Player included in build"
    else
        echo "✗ Mock Player not included in build"
        ((validation_errors++))
    fi
    
    if grep -q "test_mpris_mock_framework" Makefile.am; then
        echo "✓ Mock framework tests included in build"
    else
        echo "✗ Mock framework tests not included in build"
        ((validation_errors++))
    fi
    
    if grep -q "test_mpris_stress_testing" Makefile.am; then
        echo "✓ Stress tests included in build"
    else
        echo "✗ Stress tests not included in build"
        ((validation_errors++))
    fi
else
    echo "✗ Makefile.am not found"
    ((validation_errors++))
fi

# Check for required dependencies
echo ""
echo "Checking Dependencies"
echo "===================="

# Check if psymp3.h exists (master header)
if [ -f "../include/psymp3.h" ]; then
    echo "✓ Master header (psymp3.h) available"
else
    echo "✗ Master header (psymp3.h) not found"
    ((validation_errors++))
fi

# Check if MPRIS headers exist
mpris_headers=(
    "../include/MPRISManager.h"
    "../include/MPRISTypes.h"
    "../include/PropertyManager.h"
    "../include/MethodHandler.h"
)

for header in "${mpris_headers[@]}"; do
    if [ -f "$header" ]; then
        echo "✓ $(basename "$header") available"
    else
        echo "⚠ $(basename "$header") not found (may be created later)"
    fi
done

# Check threading safety compliance
echo ""
echo "Checking Threading Safety Compliance"
echo "===================================="

# Check if mock classes follow public/private lock pattern
if grep -q "_unlocked" mock_player.cpp 2>/dev/null; then
    echo "✓ Mock Player follows public/private lock pattern"
else
    echo "✗ Mock Player does not follow threading safety guidelines"
    ((validation_errors++))
fi

if grep -q "std::lock_guard" mock_player.cpp 2>/dev/null; then
    echo "✓ Mock Player uses RAII lock guards"
else
    echo "✗ Mock Player does not use proper lock guards"
    ((validation_errors++))
fi

if grep -q "_unlocked" mock_dbus_connection.cpp 2>/dev/null; then
    echo "✓ Mock D-Bus connection follows public/private lock pattern"
else
    echo "✗ Mock D-Bus connection does not follow threading safety guidelines"
    ((validation_errors++))
fi

# Check test framework completeness
echo ""
echo "Checking Test Framework Completeness"
echo "===================================="

required_test_types=(
    "ThreadSafetyTester"
    "LockContentionAnalyzer"
    "RaceConditionDetector"
    "ThreadingBenchmark"
)

for test_type in "${required_test_types[@]}"; do
    if grep -q "$test_type" test_framework_threading.h 2>/dev/null; then
        echo "✓ $test_type implemented"
    else
        echo "✗ $test_type missing"
        ((validation_errors++))
    fi
done

# Summary
echo ""
echo "Validation Summary"
echo "=================="

if [ $validation_errors -eq 0 ]; then
    echo "✓ All validation checks PASSED!"
    echo "The MPRIS mock framework implementation is complete and follows project standards."
    exit 0
else
    echo "✗ $validation_errors validation error(s) found!"
    echo "Please fix the issues above before using the mock framework."
    exit 1
fi