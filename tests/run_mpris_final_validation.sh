#!/bin/bash
#
# run_mpris_final_validation.sh - Comprehensive MPRIS final validation and performance optimization
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
BOLD='\033[1m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(dirname "$0")"
PROJECT_ROOT="${SCRIPT_DIR}/.."
VALIDATION_DIR="/tmp/mpris_final_validation_$$"
FINAL_REPORT="${VALIDATION_DIR}/final_validation_report.html"

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Create validation directory
mkdir -p "${VALIDATION_DIR}"

echo -e "${BOLD}${BLUE}MPRIS Final Validation and Performance Optimization${NC}"
echo "===================================================="
echo "Validation directory: ${VALIDATION_DIR}"
echo "Final report: ${FINAL_REPORT}"
echo "Timestamp: $(date)"
echo ""

# Function to log test results
log_test_result() {
    local test_name="$1"
    local result="$2"
    local details="$3"
    local duration="$4"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    case "$result" in
        "PASS")
            PASSED_TESTS=$((PASSED_TESTS + 1))
            echo -e "${GREEN}✓${NC} $test_name: PASS${details:+ ($details)}"
            ;;
        "FAIL")
            FAILED_TESTS=$((FAILED_TESTS + 1))
            echo -e "${RED}✗${NC} $test_name: FAIL${details:+ ($details)}"
            ;;
        "SKIP")
            SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
            echo -e "${YELLOW}⚠${NC} $test_name: SKIP${details:+ ($details)}"
            ;;
    esac
    
    # Log to results file for report generation
    echo "$test_name|$result|$details|$duration" >> "${VALIDATION_DIR}/test_results.log"
}

# Function to run a test with timeout and logging
run_validation_test() {
    local test_name="$1"
    local test_command="$2"
    local timeout_seconds="${3:-300}" # 5 minute default timeout
    local description="$4"
    
    echo -e "${CYAN}Running: $test_name${NC}"
    if [ -n "$description" ]; then
        echo "Description: $description"
    fi
    
    local start_time=$(date +%s)
    local test_log="${VALIDATION_DIR}/${test_name// /_}.log"
    local test_result="FAIL"
    local test_details=""
    
    if timeout "${timeout_seconds}s" bash -c "$test_command" > "$test_log" 2>&1; then
        test_result="PASS"
        test_details="Completed successfully"
    else
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            test_result="FAIL"
            test_details="Timed out after ${timeout_seconds}s"
        else
            test_result="FAIL"
            test_details="Exit code: $exit_code"
        fi
    fi
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    log_test_result "$test_name" "$test_result" "$test_details" "${duration}s"
    echo ""
    
    return $([ "$test_result" = "PASS" ] && echo 0 || echo 1)
}

# Function to build all validation tests
build_validation_tests() {
    echo -e "${CYAN}Building validation test suite...${NC}"
    
    cd "${PROJECT_ROOT}"
    
    local test_binaries=(
        "test_mpris_performance_profiler"
        "test_mpris_regression_validation"
        "test_mpris_integration"
        "test_mpris_spec_compliance"
        "test_mpris_concurrent_clients"
        "test_mpris_reconnection_behavior"
        "test_mpris_memory_validation"
    )
    
    local build_success=true
    local build_log="${VALIDATION_DIR}/build.log"
    
    echo "Building validation tests..." > "$build_log"
    
    for binary in "${test_binaries[@]}"; do
        echo "Building $binary..." >> "$build_log"
        
        if make -j$(nproc) "$binary" >> "$build_log" 2>&1; then
            echo "✓ $binary built successfully" >> "$build_log"
        else
            echo "✗ $binary build failed" >> "$build_log"
            build_success=false
        fi
    done
    
    if [ "$build_success" = true ]; then
        log_test_result "Build Validation Tests" "PASS" "All test binaries built"
    else
        log_test_result "Build Validation Tests" "FAIL" "Some test binaries failed to build"
    fi
    
    echo ""
    return $([ "$build_success" = true ] && echo 0 || echo 1)
}

# Task 15.1: Profile lock contention and optimize critical paths
run_performance_profiling() {
    echo -e "${BOLD}${BLUE}Task 15.1: Performance Profiling and Lock Contention Analysis${NC}"
    echo "============================================================="
    
    # Run performance profiler
    run_validation_test "Lock Contention Profiling" \
        "${PROJECT_ROOT}/tests/test_mpris_performance_profiler" \
        300 \
        "Profile lock contention and optimize critical paths"
    
    # Check if performance data was generated
    if [ -f "mpris_performance_profile.csv" ]; then
        log_test_result "Performance Data Generation" "PASS" "CSV data file created"
        mv "mpris_performance_profile.csv" "${VALIDATION_DIR}/"
    else
        log_test_result "Performance Data Generation" "FAIL" "No CSV data file generated"
    fi
    
    if [ -f "mpris_performance_report.txt" ]; then
        log_test_result "Performance Report Generation" "PASS" "Report file created"
        mv "mpris_performance_report.txt" "${VALIDATION_DIR}/"
    else
        log_test_result "Performance Report Generation" "FAIL" "No report file generated"
    fi
}

# Task 15.2: Validate threading safety with static analysis tools
run_static_analysis() {
    echo -e "${BOLD}${BLUE}Task 15.2: Static Analysis and Threading Safety Validation${NC}"
    echo "=========================================================="
    
    # Run static analysis script
    run_validation_test "Static Analysis Validation" \
        "${PROJECT_ROOT}/tests/run_mpris_static_analysis.sh" \
        600 \
        "Validate threading safety with static analysis tools"
    
    # Check for static analysis reports
    local analysis_reports=(
        "/tmp/mpris_static_analysis_*/static_analysis_report.txt"
        "/tmp/mpris_static_analysis_*/threading_pattern_analysis.txt"
        "/tmp/mpris_static_analysis_*/cppcheck_report.txt"
        "/tmp/mpris_static_analysis_*/clang_tidy_report.txt"
    )
    
    for report_pattern in "${analysis_reports[@]}"; do
        for report_file in $report_pattern; do
            if [ -f "$report_file" ]; then
                local report_name=$(basename "$report_file")
                cp "$report_file" "${VALIDATION_DIR}/"
                log_test_result "Static Analysis Report: $report_name" "PASS" "Report generated"
            fi
        done
    done
}

# Task 15.3: Perform final integration testing with complete PsyMP3 application
run_integration_testing() {
    echo -e "${BOLD}${BLUE}Task 15.3: Final Integration Testing${NC}"
    echo "===================================="
    
    # Run comprehensive integration tests
    run_validation_test "MPRIS Integration Testing" \
        "${PROJECT_ROOT}/tests/run_mpris_integration_tests.sh" \
        900 \
        "Perform final integration testing with complete PsyMP3 application"
    
    # Run basic integration tests as fallback
    run_validation_test "Basic Integration Testing" \
        "${PROJECT_ROOT}/tests/run_basic_integration_tests.sh" \
        300 \
        "Basic integration testing with available components"
    
    # Check for integration test reports
    local integration_reports=(
        "/tmp/mpris_integration_tests_*/integration_test_report.html"
        "/tmp/mpris_integration_tests_*/integration_results.log"
    )
    
    for report_pattern in "${integration_reports[@]}"; do
        for report_file in $report_pattern; do
            if [ -f "$report_file" ]; then
                local report_name=$(basename "$report_file")
                cp "$report_file" "${VALIDATION_DIR}/"
                log_test_result "Integration Report: $report_name" "PASS" "Report generated"
            fi
        done
    done
}

# Task 15.4: Benchmark performance impact compared to old implementation
run_performance_benchmarking() {
    echo -e "${BOLD}${BLUE}Task 15.4: Performance Benchmarking${NC}"
    echo "===================================="
    
    # Run regression validation (includes performance comparison)
    run_validation_test "Performance Regression Testing" \
        "${PROJECT_ROOT}/tests/test_mpris_regression_validation" \
        600 \
        "Benchmark performance impact compared to old implementation"
    
    # Check for performance benchmark reports
    if [ -f "mpris_regression_report.txt" ]; then
        log_test_result "Performance Benchmark Report" "PASS" "Regression report generated"
        mv "mpris_regression_report.txt" "${VALIDATION_DIR}/"
    else
        log_test_result "Performance Benchmark Report" "FAIL" "No regression report generated"
    fi
}

# Task 15.5: Verify no regressions in Player functionality
run_regression_validation() {
    echo -e "${BOLD}${BLUE}Task 15.5: Player Functionality Regression Validation${NC}"
    echo "====================================================="
    
    # Test MPRIS-Player integration
    run_validation_test "Player Integration Validation" \
        "${PROJECT_ROOT}/tests/test_mpris_integration" \
        300 \
        "Verify no regressions in Player functionality"
    
    # Test specific Player method compatibility
    if [ -f "${PROJECT_ROOT}/tests/test_player_compatibility.cpp" ]; then
        run_validation_test "Player Method Compatibility" \
            "${PROJECT_ROOT}/tests/test_player_compatibility" \
            300 \
            "Verify Player method compatibility"
    else
        log_test_result "Player Method Compatibility" "SKIP" "Test not available"
    fi
}

# Task 15.6: Create final test report and validation summary
generate_final_validation_report() {
    echo -e "${BOLD}${BLUE}Task 15.6: Final Test Report and Validation Summary${NC}"
    echo "=================================================="
    
    local report_generation_start=$(date +%s)
    
    # Generate comprehensive HTML report
    cat > "${FINAL_REPORT}" << EOF
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>MPRIS Final Validation Report</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            line-height: 1.6;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background-color: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 0 20px rgba(0,0,0,0.1);
        }
        .header {
            text-align: center;
            border-bottom: 3px solid #007acc;
            padding-bottom: 20px;
            margin-bottom: 30px;
        }
        .header h1 {
            color: #007acc;
            margin: 0;
            font-size: 2.5em;
        }
        .summary {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        .summary-card {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 10px;
            text-align: center;
        }
        .summary-card h3 {
            margin: 0 0 10px 0;
            font-size: 2em;
        }
        .summary-card p {
            margin: 0;
            opacity: 0.9;
        }
        .section {
            margin-bottom: 40px;
        }
        .section h2 {
            color: #333;
            border-left: 4px solid #007acc;
            padding-left: 15px;
            margin-bottom: 20px;
        }
        .test-results {
            overflow-x: auto;
        }
        .test-table {
            width: 100%;
            border-collapse: collapse;
            margin-bottom: 20px;
        }
        .test-table th,
        .test-table td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }
        .test-table th {
            background-color: #f8f9fa;
            font-weight: 600;
            color: #333;
        }
        .test-table tr:hover {
            background-color: #f8f9fa;
        }
        .status-pass {
            color: #28a745;
            font-weight: bold;
        }
        .status-fail {
            color: #dc3545;
            font-weight: bold;
        }
        .status-skip {
            color: #ffc107;
            font-weight: bold;
        }
        .recommendations {
            background-color: #e7f3ff;
            border-left: 4px solid #007acc;
            padding: 20px;
            border-radius: 5px;
        }
        .footer {
            text-align: center;
            margin-top: 40px;
            padding-top: 20px;
            border-top: 1px solid #ddd;
            color: #666;
        }
        .chart-container {
            margin: 20px 0;
            text-align: center;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>MPRIS Final Validation Report</h1>
            <p>Task 15: Final validation and performance optimization</p>
            <p>Generated: $(date)</p>
        </div>

        <div class="summary">
            <div class="summary-card">
                <h3>$TOTAL_TESTS</h3>
                <p>Total Tests</p>
            </div>
            <div class="summary-card">
                <h3>$PASSED_TESTS</h3>
                <p>Passed</p>
            </div>
            <div class="summary-card">
                <h3>$FAILED_TESTS</h3>
                <p>Failed</p>
            </div>
            <div class="summary-card">
                <h3>$SKIPPED_TESTS</h3>
                <p>Skipped</p>
            </div>
        </div>

        <div class="section">
            <h2>Executive Summary</h2>
            <p>This report presents the results of the comprehensive final validation for the MPRIS refactor project. 
            The validation covers performance profiling, static analysis, integration testing, benchmarking, and regression validation.</p>
            
            <div class="chart-container">
                <p><strong>Success Rate: $(( (PASSED_TESTS * 100) / (TOTAL_TESTS > 0 ? TOTAL_TESTS : 1) ))%</strong></p>
            </div>
        </div>

        <div class="section">
            <h2>Test Results</h2>
            <div class="test-results">
                <table class="test-table">
                    <thead>
                        <tr>
                            <th>Test Name</th>
                            <th>Status</th>
                            <th>Details</th>
                            <th>Duration</th>
                        </tr>
                    </thead>
                    <tbody>
EOF

    # Add test results to HTML report
    if [ -f "${VALIDATION_DIR}/test_results.log" ]; then
        while IFS='|' read -r test_name result details duration; do
            local status_class=""
            case "$result" in
                "PASS") status_class="status-pass" ;;
                "FAIL") status_class="status-fail" ;;
                "SKIP") status_class="status-skip" ;;
            esac
            
            cat >> "${FINAL_REPORT}" << EOF
                        <tr>
                            <td>$test_name</td>
                            <td class="$status_class">$result</td>
                            <td>$details</td>
                            <td>$duration</td>
                        </tr>
EOF
        done < "${VALIDATION_DIR}/test_results.log"
    fi

    cat >> "${FINAL_REPORT}" << EOF
                    </tbody>
                </table>
            </div>
        </div>

        <div class="section">
            <h2>Task Completion Status</h2>
            <ul>
                <li><strong>Task 15.1 - Performance Profiling:</strong> $([ -f "${VALIDATION_DIR}/mpris_performance_report.txt" ] && echo "✓ Completed" || echo "✗ Incomplete")</li>
                <li><strong>Task 15.2 - Static Analysis:</strong> $([ -f "${VALIDATION_DIR}/static_analysis_report.txt" ] && echo "✓ Completed" || echo "✗ Incomplete")</li>
                <li><strong>Task 15.3 - Integration Testing:</strong> $([ -f "${VALIDATION_DIR}/integration_results.log" ] && echo "✓ Completed" || echo "✗ Incomplete")</li>
                <li><strong>Task 15.4 - Performance Benchmarking:</strong> $([ -f "${VALIDATION_DIR}/mpris_regression_report.txt" ] && echo "✓ Completed" || echo "✗ Incomplete")</li>
                <li><strong>Task 15.5 - Regression Validation:</strong> $([ $FAILED_TESTS -eq 0 ] && echo "✓ Completed" || echo "⚠ Issues Found")</li>
                <li><strong>Task 15.6 - Final Report:</strong> ✓ Completed</li>
            </ul>
        </div>

        <div class="section">
            <h2>Performance Analysis</h2>
EOF

    # Include performance data if available
    if [ -f "${VALIDATION_DIR}/mpris_performance_report.txt" ]; then
        echo "            <pre>" >> "${FINAL_REPORT}"
        head -50 "${VALIDATION_DIR}/mpris_performance_report.txt" >> "${FINAL_REPORT}"
        echo "            </pre>" >> "${FINAL_REPORT}"
    else
        echo "            <p>Performance analysis data not available.</p>" >> "${FINAL_REPORT}"
    fi

    cat >> "${FINAL_REPORT}" << EOF
        </div>

        <div class="section">
            <h2>Recommendations</h2>
            <div class="recommendations">
EOF

    # Generate recommendations based on test results
    if [ $FAILED_TESTS -eq 0 ]; then
        cat >> "${FINAL_REPORT}" << EOF
                <h3>✓ Ready for Production</h3>
                <p>All validation tests have passed successfully. The MPRIS refactor is ready for production deployment.</p>
                <ul>
                    <li>Performance improvements have been validated</li>
                    <li>Threading safety patterns are correctly implemented</li>
                    <li>No regressions detected in Player functionality</li>
                    <li>Integration testing completed successfully</li>
                </ul>
EOF
    else
        cat >> "${FINAL_REPORT}" << EOF
                <h3>⚠ Issues Require Attention</h3>
                <p>$FAILED_TESTS test(s) failed and require attention before production deployment.</p>
                <ul>
                    <li>Review failed test logs for specific issues</li>
                    <li>Address any performance regressions identified</li>
                    <li>Verify threading safety in failed components</li>
                    <li>Re-run validation after fixes are applied</li>
                </ul>
EOF
    fi

    cat >> "${FINAL_REPORT}" << EOF
            </div>
        </div>

        <div class="section">
            <h2>Detailed Reports</h2>
            <p>The following detailed reports are available in the validation directory:</p>
            <ul>
EOF

    # List all available reports
    for report_file in "${VALIDATION_DIR}"/*.{txt,log,csv}; do
        if [ -f "$report_file" ]; then
            local report_name=$(basename "$report_file")
            echo "                <li><code>$report_name</code></li>" >> "${FINAL_REPORT}"
        fi
    done

    cat >> "${FINAL_REPORT}" << EOF
            </ul>
        </div>

        <div class="footer">
            <p>MPRIS Final Validation Report - Generated by PsyMP3 Test Suite</p>
            <p>Validation Directory: $VALIDATION_DIR</p>
        </div>
    </div>
</body>
</html>
EOF

    local report_generation_end=$(date +%s)
    local report_duration=$((report_generation_end - report_generation_start))
    
    log_test_result "Final Validation Report Generation" "PASS" "HTML report created" "${report_duration}s"
    
    echo "Final validation report generated: ${FINAL_REPORT}"
}

# Function to cleanup and preserve results
cleanup_and_preserve() {
    echo -e "${CYAN}Preserving validation results...${NC}"
    
    # Create a permanent results directory
    local results_dir="${PROJECT_ROOT}/validation_results_$(date +%Y%m%d_%H%M%S)"
    mkdir -p "$results_dir"
    
    # Copy all validation results
    cp -r "${VALIDATION_DIR}"/* "$results_dir/" 2>/dev/null || true
    
    echo "Validation results preserved in: $results_dir"
    
    # Clean up temporary directory
    rm -rf "${VALIDATION_DIR}"
}

# Main execution function
main() {
    echo "Starting MPRIS final validation and performance optimization..."
    echo ""
    
    # Initialize results log
    echo "# MPRIS Final Validation Test Results" > "${VALIDATION_DIR}/test_results.log"
    echo "# Format: TestName|Result|Details|Duration" >> "${VALIDATION_DIR}/test_results.log"
    
    # Build validation tests
    build_validation_tests
    
    # Execute all validation tasks
    run_performance_profiling
    run_static_analysis
    run_integration_testing
    run_performance_benchmarking
    run_regression_validation
    
    # Generate final report
    generate_final_validation_report
    
    # Print final summary
    echo ""
    echo -e "${BOLD}${BLUE}Final Validation Summary${NC}"
    echo "========================"
    echo "Total tests: $TOTAL_TESTS"
    echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed: ${RED}$FAILED_TESTS${NC}"
    echo -e "Skipped: ${YELLOW}$SKIPPED_TESTS${NC}"
    echo ""
    echo "Success rate: $(( (PASSED_TESTS * 100) / (TOTAL_TESTS > 0 ? TOTAL_TESTS : 1) ))%"
    echo ""
    
    # Final result determination
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${BOLD}${GREEN}✓ FINAL VALIDATION PASSED${NC}"
        echo "MPRIS refactor is ready for production deployment."
        echo "All performance optimization and validation requirements have been met."
        
        cleanup_and_preserve
        exit 0
    else
        echo -e "${BOLD}${RED}✗ FINAL VALIDATION FAILED${NC}"
        echo "$FAILED_TESTS test(s) failed and require attention."
        echo "Review the detailed reports before proceeding with deployment."
        
        cleanup_and_preserve
        exit 1
    fi
}

# Execute main function
main "$@"