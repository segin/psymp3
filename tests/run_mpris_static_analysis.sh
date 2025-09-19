#!/bin/bash
#
# run_mpris_static_analysis.sh - Static analysis validation for MPRIS threading safety
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(dirname "$0")"
PROJECT_ROOT="${SCRIPT_DIR}/.."
ANALYSIS_DIR="/tmp/mpris_static_analysis_$$"
REPORT_FILE="${ANALYSIS_DIR}/static_analysis_report.txt"

# Create analysis directory
mkdir -p "${ANALYSIS_DIR}"

echo -e "${BLUE}MPRIS Static Analysis Validation${NC}"
echo "================================="
echo "Analysis directory: ${ANALYSIS_DIR}"
echo "Report file: ${REPORT_FILE}"
echo ""

# Initialize report
cat > "${REPORT_FILE}" << 'EOF'
MPRIS Static Analysis Report
============================

This report contains the results of static analysis validation for the MPRIS
refactor, focusing on threading safety patterns and potential issues.

EOF

echo "Timestamp: $(date)" >> "${REPORT_FILE}"
echo "" >> "${REPORT_FILE}"

# Function to log analysis results
log_analysis() {
    local tool="$1"
    local result="$2"
    local details="$3"
    
    echo "${tool}: ${result}" >> "${REPORT_FILE}"
    if [ -n "$details" ]; then
        echo "Details: ${details}" >> "${REPORT_FILE}"
    fi
    echo "" >> "${REPORT_FILE}"
}

# Function to check if a tool is available
check_tool() {
    local tool="$1"
    local description="$2"
    
    if command -v "$tool" >/dev/null 2>&1; then
        echo -e "${GREEN}✓${NC} $description ($tool) available"
        return 0
    else
        echo -e "${YELLOW}⚠${NC} $description ($tool) not available"
        return 1
    fi
}

# Function to analyze threading safety patterns
analyze_threading_patterns() {
    echo -e "${CYAN}Analyzing threading safety patterns...${NC}"
    
    local mpris_files=(
        "src/MPRISManager.cpp"
        "src/PropertyManager.cpp" 
        "src/MethodHandler.cpp"
        "src/SignalEmitter.cpp"
        "src/DBusConnectionManager.cpp"
        "include/MPRISManager.h"
        "include/PropertyManager.h"
        "include/MethodHandler.h"
        "include/SignalEmitter.h"
        "include/DBusConnectionManager.h"
    )
    
    local pattern_violations=0
    local pattern_report="${ANALYSIS_DIR}/threading_pattern_analysis.txt"
    
    echo "Threading Safety Pattern Analysis" > "${pattern_report}"
    echo "=================================" >> "${pattern_report}"
    echo "" >> "${pattern_report}"
    
    for file in "${mpris_files[@]}"; do
        local full_path="${PROJECT_ROOT}/${file}"
        
        if [ ! -f "$full_path" ]; then
            echo "Warning: File not found: $full_path" >> "${pattern_report}"
            continue
        fi
        
        echo "Analyzing: $file" >> "${pattern_report}"
        echo "$(printf '%.40s' "----------------------------------------")" >> "${pattern_report}"
        
        # Check for public/private lock pattern violations
        
        # 1. Check for public methods that acquire locks but don't have _unlocked counterparts
        local public_lock_methods=$(grep -n "std::lock_guard\|std::unique_lock" "$full_path" | grep -v "_unlocked" | grep -v "private:" || true)
        if [ -n "$public_lock_methods" ]; then
            echo "Potential public method lock pattern violations:" >> "${pattern_report}"
            echo "$public_lock_methods" >> "${pattern_report}"
            pattern_violations=$((pattern_violations + 1))
        fi
        
        # 2. Check for _unlocked methods that acquire locks (should not happen)
        local unlocked_with_locks=$(grep -n "_unlocked.*std::lock_guard\|_unlocked.*std::unique_lock" "$full_path" || true)
        if [ -n "$unlocked_with_locks" ]; then
            echo "ERROR: _unlocked methods acquiring locks:" >> "${pattern_report}"
            echo "$unlocked_with_locks" >> "${pattern_report}"
            pattern_violations=$((pattern_violations + 1))
        fi
        
        # 3. Check for public method calls within the same class (potential deadlock)
        local public_calls_in_class=$(grep -n "m_.*->" "$full_path" | grep -v "_unlocked" | grep -v "std::" || true)
        if [ -n "$public_calls_in_class" ]; then
            echo "Potential public method calls (review for deadlock risk):" >> "${pattern_report}"
            echo "$public_calls_in_class" >> "${pattern_report}"
        fi
        
        # 4. Check for manual lock/unlock (should use RAII)
        local manual_locks=$(grep -n "\.lock()\|\.unlock()" "$full_path" | grep -v "lock_guard\|unique_lock" || true)
        if [ -n "$manual_locks" ]; then
            echo "Manual lock/unlock detected (should use RAII):" >> "${pattern_report}"
            echo "$manual_locks" >> "${pattern_report}"
            pattern_violations=$((pattern_violations + 1))
        fi
        
        # 5. Check for callback invocations while holding locks
        local callback_patterns=$(grep -n -A5 -B5 "callback\|invoke\|emit" "$full_path" | grep -B10 -A10 "lock_guard\|unique_lock" || true)
        if [ -n "$callback_patterns" ]; then
            echo "Potential callback invocation under lock (review needed):" >> "${pattern_report}"
            echo "$callback_patterns" >> "${pattern_report}"
        fi
        
        echo "" >> "${pattern_report}"
    done
    
    echo "Total pattern violations found: $pattern_violations" >> "${pattern_report}"
    
    if [ $pattern_violations -eq 0 ]; then
        echo -e "${GREEN}✓${NC} Threading safety patterns: PASS"
        log_analysis "Threading Pattern Analysis" "PASS" "No violations found"
    else
        echo -e "${RED}✗${NC} Threading safety patterns: FAIL ($pattern_violations violations)"
        log_analysis "Threading Pattern Analysis" "FAIL" "$pattern_violations violations found"
    fi
    
    return $pattern_violations
}

# Function to run cppcheck static analysis
run_cppcheck() {
    echo -e "${CYAN}Running cppcheck static analysis...${NC}"
    
    if ! check_tool "cppcheck" "Cppcheck static analyzer"; then
        log_analysis "Cppcheck" "SKIPPED" "Tool not available"
        return 0
    fi
    
    local cppcheck_report="${ANALYSIS_DIR}/cppcheck_report.txt"
    local cppcheck_xml="${ANALYSIS_DIR}/cppcheck_report.xml"
    
    # Run cppcheck with threading-specific checks
    cppcheck --enable=all \
             --inconclusive \
             --std=c++17 \
             --platform=unix64 \
             --suppress=missingIncludeSystem \
             --suppress=unusedFunction \
             --xml \
             --xml-version=2 \
             "${PROJECT_ROOT}/src/" \
             "${PROJECT_ROOT}/include/" \
             2> "${cppcheck_xml}" || true
    
    # Convert XML to readable format
    if [ -f "${cppcheck_xml}" ]; then
        echo "Cppcheck Static Analysis Results" > "${cppcheck_report}"
        echo "================================" >> "${cppcheck_report}"
        echo "" >> "${cppcheck_report}"
        
        # Extract key information from XML
        if command -v xmllint >/dev/null 2>&1; then
            xmllint --format "${cppcheck_xml}" | grep -E "<error|severity|msg" >> "${cppcheck_report}" || true
        else
            cat "${cppcheck_xml}" >> "${cppcheck_report}"
        fi
        
        # Count issues
        local error_count=$(grep -c 'severity="error"' "${cppcheck_xml}" || echo "0")
        local warning_count=$(grep -c 'severity="warning"' "${cppcheck_xml}" || echo "0")
        
        echo "Errors: $error_count, Warnings: $warning_count" >> "${cppcheck_report}"
        
        if [ "$error_count" -eq 0 ] && [ "$warning_count" -eq 0 ]; then
            echo -e "${GREEN}✓${NC} Cppcheck analysis: PASS"
            log_analysis "Cppcheck" "PASS" "No issues found"
        else
            echo -e "${YELLOW}⚠${NC} Cppcheck analysis: WARNINGS ($error_count errors, $warning_count warnings)"
            log_analysis "Cppcheck" "WARNINGS" "$error_count errors, $warning_count warnings"
        fi
    else
        echo -e "${RED}✗${NC} Cppcheck analysis: FAILED (no output generated)"
        log_analysis "Cppcheck" "FAILED" "No output generated"
    fi
}

# Function to run clang-tidy analysis
run_clang_tidy() {
    echo -e "${CYAN}Running clang-tidy analysis...${NC}"
    
    if ! check_tool "clang-tidy" "Clang-Tidy static analyzer"; then
        log_analysis "Clang-Tidy" "SKIPPED" "Tool not available"
        return 0
    fi
    
    local clang_tidy_report="${ANALYSIS_DIR}/clang_tidy_report.txt"
    
    echo "Clang-Tidy Static Analysis Results" > "${clang_tidy_report}"
    echo "===================================" >> "${clang_tidy_report}"
    echo "" >> "${clang_tidy_report}"
    
    # Focus on threading and concurrency checks
    local checks="concurrency-*,thread-safety-*,bugprone-*,performance-*,readability-*"
    
    # Analyze MPRIS source files
    local mpris_sources=(
        "src/MPRISManager.cpp"
        "src/PropertyManager.cpp"
        "src/MethodHandler.cpp"
        "src/SignalEmitter.cpp"
        "src/DBusConnectionManager.cpp"
    )
    
    local total_issues=0
    
    for source in "${mpris_sources[@]}"; do
        local full_path="${PROJECT_ROOT}/${source}"
        
        if [ ! -f "$full_path" ]; then
            continue
        fi
        
        echo "Analyzing: $source" >> "${clang_tidy_report}"
        echo "$(printf '%.50s' "--------------------------------------------------")" >> "${clang_tidy_report}"
        
        # Run clang-tidy on the file
        clang-tidy "$full_path" \
                   --checks="$checks" \
                   --header-filter=".*" \
                   --format-style=file \
                   -- -std=c++17 -I"${PROJECT_ROOT}/include" \
                   >> "${clang_tidy_report}" 2>&1 || true
        
        echo "" >> "${clang_tidy_report}"
        
        # Count issues in this file
        local file_issues=$(clang-tidy "$full_path" --checks="$checks" --header-filter=".*" -- -std=c++17 -I"${PROJECT_ROOT}/include" 2>&1 | grep -c "warning:\|error:" || echo "0")
        total_issues=$((total_issues + file_issues))
    done
    
    echo "Total issues found: $total_issues" >> "${clang_tidy_report}"
    
    if [ "$total_issues" -eq 0 ]; then
        echo -e "${GREEN}✓${NC} Clang-Tidy analysis: PASS"
        log_analysis "Clang-Tidy" "PASS" "No issues found"
    else
        echo -e "${YELLOW}⚠${NC} Clang-Tidy analysis: WARNINGS ($total_issues issues)"
        log_analysis "Clang-Tidy" "WARNINGS" "$total_issues issues found"
    fi
}

# Function to run custom threading safety analysis
run_custom_threading_analysis() {
    echo -e "${CYAN}Running custom threading safety analysis...${NC}"
    
    local threading_report="${ANALYSIS_DIR}/custom_threading_analysis.txt"
    
    echo "Custom Threading Safety Analysis" > "${threading_report}"
    echo "================================" >> "${threading_report}"
    echo "" >> "${threading_report}"
    
    # Check for common threading anti-patterns
    local mpris_files=(
        "src/MPRISManager.cpp"
        "src/PropertyManager.cpp"
        "src/MethodHandler.cpp"
        "src/SignalEmitter.cpp"
        "src/DBusConnectionManager.cpp"
    )
    
    local issues_found=0
    
    for file in "${mpris_files[@]}"; do
        local full_path="${PROJECT_ROOT}/${file}"
        
        if [ ! -f "$full_path" ]; then
            continue
        fi
        
        echo "Analyzing: $file" >> "${threading_report}"
        echo "$(printf '%.40s' "----------------------------------------")" >> "${threading_report}"
        
        # 1. Check for potential race conditions
        local race_patterns=$(grep -n -E "static.*=|global.*=" "$full_path" | grep -v "const\|constexpr" || true)
        if [ -n "$race_patterns" ]; then
            echo "Potential race condition (global/static variables):" >> "${threading_report}"
            echo "$race_patterns" >> "${threading_report}"
            issues_found=$((issues_found + 1))
        fi
        
        # 2. Check for missing const correctness in getters
        local non_const_getters=$(grep -n "get.*(" "$full_path" | grep -v "const" | grep -v "_unlocked" || true)
        if [ -n "$non_const_getters" ]; then
            echo "Potential const correctness issues:" >> "${threading_report}"
            echo "$non_const_getters" >> "${threading_report}"
        fi
        
        # 3. Check for atomic usage patterns
        local atomic_usage=$(grep -n "std::atomic\|atomic" "$full_path" || true)
        if [ -n "$atomic_usage" ]; then
            echo "Atomic variable usage (verify memory ordering):" >> "${threading_report}"
            echo "$atomic_usage" >> "${threading_report}"
        fi
        
        # 4. Check for condition variable usage
        local cv_usage=$(grep -n "condition_variable\|notify_\|wait" "$full_path" || true)
        if [ -n "$cv_usage" ]; then
            echo "Condition variable usage (verify spurious wakeup handling):" >> "${threading_report}"
            echo "$cv_usage" >> "${threading_report}"
        fi
        
        # 5. Check for exception safety in lock contexts
        local exception_in_locks=$(grep -n -A10 -B5 "lock_guard\|unique_lock" "$full_path" | grep -E "throw|new|delete" || true)
        if [ -n "$exception_in_locks" ]; then
            echo "Potential exception safety issues in lock contexts:" >> "${threading_report}"
            echo "$exception_in_locks" >> "${threading_report}"
        fi
        
        echo "" >> "${threading_report}"
    done
    
    echo "Total threading issues found: $issues_found" >> "${threading_report}"
    
    if [ $issues_found -eq 0 ]; then
        echo -e "${GREEN}✓${NC} Custom threading analysis: PASS"
        log_analysis "Custom Threading Analysis" "PASS" "No issues found"
    else
        echo -e "${YELLOW}⚠${NC} Custom threading analysis: WARNINGS ($issues_found issues)"
        log_analysis "Custom Threading Analysis" "WARNINGS" "$issues_found issues found"
    fi
}

# Function to validate MPRIS specification compliance
validate_mpris_compliance() {
    echo -e "${CYAN}Validating MPRIS specification compliance...${NC}"
    
    local compliance_report="${ANALYSIS_DIR}/mpris_compliance_analysis.txt"
    
    echo "MPRIS Specification Compliance Analysis" > "${compliance_report}"
    echo "=======================================" >> "${compliance_report}"
    echo "" >> "${compliance_report}"
    
    local compliance_issues=0
    
    # Check for required MPRIS interfaces and methods
    local required_methods=(
        "Play"
        "Pause" 
        "Stop"
        "Next"
        "Previous"
        "Seek"
        "SetPosition"
        "PlayPause"
    )
    
    local method_handler_file="${PROJECT_ROOT}/src/MethodHandler.cpp"
    
    if [ -f "$method_handler_file" ]; then
        echo "Checking required MPRIS methods implementation:" >> "${compliance_report}"
        
        for method in "${required_methods[@]}"; do
            if grep -q "handle${method}_unlocked" "$method_handler_file"; then
                echo "✓ $method method implemented" >> "${compliance_report}"
            else
                echo "✗ $method method missing" >> "${compliance_report}"
                compliance_issues=$((compliance_issues + 1))
            fi
        done
        
        echo "" >> "${compliance_report}"
        
        # Check for required properties
        local required_properties=(
            "PlaybackStatus"
            "Metadata"
            "Position"
            "CanGoNext"
            "CanGoPrevious"
            "CanPlay"
            "CanPause"
            "CanSeek"
            "CanControl"
        )
        
        local property_manager_file="${PROJECT_ROOT}/src/PropertyManager.cpp"
        
        if [ -f "$property_manager_file" ]; then
            echo "Checking required MPRIS properties implementation:" >> "${compliance_report}"
            
            for property in "${required_properties[@]}"; do
                if grep -q "$property" "$property_manager_file"; then
                    echo "✓ $property property implemented" >> "${compliance_report}"
                else
                    echo "✗ $property property missing" >> "${compliance_report}"
                    compliance_issues=$((compliance_issues + 1))
                fi
            done
        fi
    fi
    
    echo "" >> "${compliance_report}"
    echo "Total compliance issues: $compliance_issues" >> "${compliance_report}"
    
    if [ $compliance_issues -eq 0 ]; then
        echo -e "${GREEN}✓${NC} MPRIS compliance: PASS"
        log_analysis "MPRIS Compliance" "PASS" "All required methods and properties implemented"
    else
        echo -e "${RED}✗${NC} MPRIS compliance: FAIL ($compliance_issues issues)"
        log_analysis "MPRIS Compliance" "FAIL" "$compliance_issues missing methods/properties"
    fi
    
    return $compliance_issues
}

# Function to generate final report
generate_final_report() {
    echo -e "${CYAN}Generating final static analysis report...${NC}"
    
    cat >> "${REPORT_FILE}" << 'EOF'

Summary of Analysis Tools Used:
===============================

1. Threading Pattern Analysis - Custom validation of public/private lock patterns
2. Cppcheck - General static analysis with focus on threading issues
3. Clang-Tidy - Advanced static analysis with concurrency checks
4. Custom Threading Analysis - Project-specific threading safety validation
5. MPRIS Compliance - Validation against MPRIS specification requirements

EOF
    
    # Include all detailed reports
    echo "Detailed Analysis Reports:" >> "${REPORT_FILE}"
    echo "=========================" >> "${REPORT_FILE}"
    echo "" >> "${REPORT_FILE}"
    
    for report in "${ANALYSIS_DIR}"/*.txt; do
        if [ -f "$report" ] && [ "$report" != "$REPORT_FILE" ]; then
            echo "--- $(basename "$report") ---" >> "${REPORT_FILE}"
            cat "$report" >> "${REPORT_FILE}"
            echo "" >> "${REPORT_FILE}"
        fi
    done
    
    echo "Static analysis complete. Report available at: ${REPORT_FILE}"
}

# Main execution
main() {
    echo "Starting MPRIS static analysis validation..."
    echo ""
    
    local total_issues=0
    
    # Run all analysis tools
    analyze_threading_patterns
    total_issues=$((total_issues + $?))
    
    run_cppcheck
    
    run_clang_tidy
    
    run_custom_threading_analysis
    
    validate_mpris_compliance
    total_issues=$((total_issues + $?))
    
    # Generate final report
    generate_final_report
    
    echo ""
    echo -e "${BLUE}Static Analysis Summary${NC}"
    echo "======================="
    echo "Analysis directory: ${ANALYSIS_DIR}"
    echo "Report file: ${REPORT_FILE}"
    echo ""
    
    if [ $total_issues -eq 0 ]; then
        echo -e "${GREEN}✓ All static analysis checks PASSED${NC}"
        echo "MPRIS implementation meets threading safety and compliance requirements."
        exit 0
    else
        echo -e "${YELLOW}⚠ Static analysis found $total_issues issues${NC}"
        echo "Review the detailed report for specific recommendations."
        exit 1
    fi
}

# Run main function
main "$@"