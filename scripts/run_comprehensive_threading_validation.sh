#!/bin/bash

# Comprehensive Threading Safety Validation Script
# 
# This script runs all threading safety checks and tests to validate
# the threading safety refactoring across the entire codebase.
#
# Requirements addressed: 2.3, 5.3

set -e  # Exit on any error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Test result tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
FAILED_TEST_NAMES=()

# Function to run a test and track results
run_test() {
    local test_name="$1"
    local test_command="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    log_info "Running: $test_name"
    
    if eval "$test_command" > /dev/null 2>&1; then
        log_success "$test_name PASSED"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        log_error "$test_name FAILED"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        FAILED_TEST_NAMES+=("$test_name")
        return 1
    fi
}

# Function to run a test with output
run_test_with_output() {
    local test_name="$1"
    local test_command="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    log_info "Running: $test_name"
    
    if eval "$test_command"; then
        log_success "$test_name PASSED"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        log_error "$test_name FAILED"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        FAILED_TEST_NAMES+=("$test_name")
        return 1
    fi
}

# Change to project root
cd "$PROJECT_ROOT"

echo "========================================================"
echo "  PsyMP3 Comprehensive Threading Safety Validation"
echo "========================================================"
echo ""

# Check prerequisites
log_info "Checking prerequisites..."

if ! command -v python3 &> /dev/null; then
    log_error "python3 is required but not installed"
    exit 1
fi

if ! command -v make &> /dev/null; then
    log_error "make is required but not installed"
    exit 1
fi

if [ ! -f "scripts/check_threading_patterns.py" ]; then
    log_error "Threading pattern checker script not found"
    exit 1
fi

if [ ! -f "scripts/analyze_threading_safety.py" ]; then
    log_error "Threading safety analyzer script not found"
    exit 1
fi

log_success "Prerequisites check passed"
echo ""

# Phase 1: Static Analysis
echo "========================================================"
echo "  Phase 1: Static Analysis"
echo "========================================================"

run_test_with_output "Threading Pattern Analysis" \
    "python3 scripts/check_threading_patterns.py --strict --fix-suggestions"

run_test_with_output "Threading Safety Analysis" \
    "python3 scripts/analyze_threading_safety.py src include"

echo ""

# Phase 2: Build System Integration
echo "========================================================"
echo "  Phase 2: Build System Integration"
echo "========================================================"

# Ensure the project is built
log_info "Building project..."
if ! make -j$(nproc) > /dev/null 2>&1; then
    log_error "Project build failed"
    exit 1
fi
log_success "Project build completed"

# Build threading safety tests
log_info "Building threading safety tests..."
if ! make -C tests -j$(nproc) > /dev/null 2>&1; then
    log_error "Threading safety tests build failed"
    exit 1
fi
log_success "Threading safety tests build completed"

echo ""

# Phase 3: Unit Tests
echo "========================================================"
echo "  Phase 3: Threading Safety Unit Tests"
echo "========================================================"

# Basic threading safety tests
run_test "Threading Safety Baseline Test" \
    "cd tests && ./test_threading_safety_baseline"

run_test "Audio Thread Safety Test" \
    "cd tests && ./test_audio_thread_safety"

run_test "IOHandler Thread Safety Test" \
    "cd tests && ./test_iohandler_thread_safety_comprehensive"

run_test "Memory Pool Manager Thread Safety Test" \
    "cd tests && ./test_memory_pool_manager_thread_safety_comprehensive"

run_test "Surface Thread Safety Test" \
    "cd tests && ./test_surface_thread_safety"

echo ""

# Phase 4: Integration Tests
echo "========================================================"
echo "  Phase 4: System-Wide Integration Tests"
echo "========================================================"

run_test "System-Wide Threading Integration Test" \
    "cd tests && ./test_system_wide_threading_integration"

echo ""

# Phase 5: Performance Regression Tests
echo "========================================================"
echo "  Phase 5: Performance Regression Tests"
echo "========================================================"

run_test "Threading Performance Regression Test" \
    "cd tests && ./test_threading_performance_regression"

echo ""

# Phase 6: Documentation and Guidelines Validation
echo "========================================================"
echo "  Phase 6: Documentation Validation"
echo "========================================================"

# Check that threading guidelines exist and are up to date
if [ -f ".kiro/steering/threading-safety-guidelines.md" ]; then
    log_success "Threading safety guidelines found"
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    log_error "Threading safety guidelines not found"
    FAILED_TESTS=$((FAILED_TESTS + 1))
    FAILED_TEST_NAMES+=("Threading Safety Guidelines")
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))

# Check that development standards include threading requirements
if grep -q "Threading Safety" .kiro/steering/development-standards.md 2>/dev/null; then
    log_success "Development standards include threading safety requirements"
    PASSED_TESTS=$((PASSED_TESTS + 1))
else
    log_error "Development standards missing threading safety requirements"
    FAILED_TESTS=$((FAILED_TESTS + 1))
    FAILED_TEST_NAMES+=("Development Standards Threading Requirements")
fi
TOTAL_TESTS=$((TOTAL_TESTS + 1))

echo ""

# Generate comprehensive report
echo "========================================================"
echo "  Generating Comprehensive Report"
echo "========================================================"

REPORT_FILE="threading_safety_validation_report.md"

cat > "$REPORT_FILE" << EOF
# Threading Safety Validation Report

Generated: $(date)

## Summary

- **Total Tests**: $TOTAL_TESTS
- **Passed**: $PASSED_TESTS
- **Failed**: $FAILED_TESTS
- **Success Rate**: $(( (PASSED_TESTS * 100) / TOTAL_TESTS ))%

## Test Results

### Static Analysis
- Threading Pattern Analysis
- Threading Safety Analysis

### Build System Integration
- Project Build
- Threading Safety Tests Build

### Unit Tests
- Threading Safety Baseline Test
- Audio Thread Safety Test
- IOHandler Thread Safety Test
- Memory Pool Manager Thread Safety Test
- Surface Thread Safety Test

### Integration Tests
- System-Wide Threading Integration Test

### Performance Tests
- Threading Performance Regression Test

### Documentation
- Threading Safety Guidelines
- Development Standards Threading Requirements

## Failed Tests

EOF

if [ ${#FAILED_TEST_NAMES[@]} -eq 0 ]; then
    echo "None - All tests passed!" >> "$REPORT_FILE"
else
    for test_name in "${FAILED_TEST_NAMES[@]}"; do
        echo "- $test_name" >> "$REPORT_FILE"
    done
fi

cat >> "$REPORT_FILE" << EOF

## Recommendations

EOF

if [ $FAILED_TESTS -eq 0 ]; then
    cat >> "$REPORT_FILE" << EOF
The threading safety refactoring has been successfully validated:

1. All static analysis checks pass
2. All threading safety tests pass
3. No performance regressions detected
4. Documentation is up to date

The codebase is ready for production use with the new threading safety patterns.
EOF
else
    cat >> "$REPORT_FILE" << EOF
The following issues need to be addressed:

1. Fix failed tests before merging threading safety changes
2. Review and update documentation as needed
3. Consider additional testing for edge cases
4. Ensure all developers are trained on new threading patterns

Please address all failed tests before considering the threading safety refactoring complete.
EOF
fi

log_success "Report generated: $REPORT_FILE"

# Final summary
echo ""
echo "========================================================"
echo "  Final Results"
echo "========================================================"

echo "Total Tests: $TOTAL_TESTS"
echo "Passed: $PASSED_TESTS"
echo "Failed: $FAILED_TESTS"
echo "Success Rate: $(( (PASSED_TESTS * 100) / TOTAL_TESTS ))%"

if [ $FAILED_TESTS -eq 0 ]; then
    echo ""
    log_success "🎉 ALL THREADING SAFETY VALIDATION TESTS PASSED! 🎉"
    echo ""
    echo "The threading safety refactoring has been successfully validated."
    echo "The codebase is ready for production use with improved thread safety."
    exit 0
else
    echo ""
    log_error "❌ THREADING SAFETY VALIDATION FAILED ❌"
    echo ""
    echo "Failed tests:"
    for test_name in "${FAILED_TEST_NAMES[@]}"; do
        echo "  - $test_name"
    done
    echo ""
    echo "Please fix the failed tests before proceeding."
    exit 1
fi