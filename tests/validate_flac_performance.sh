#!/bin/bash
#
# validate_flac_performance.sh - Validate FLAC demuxer performance and compatibility
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
TEST_DIR="$(dirname "$0")"
BUILD_DIR="${TEST_DIR}/.."
TEST_FLAC_FILE="/mnt/8TB-3/music/almost monday/DIVE/11 life goes by.flac"

echo -e "${BLUE}FLAC Demuxer Performance Validation${NC}"
echo "===================================="
echo

# Function to check if FLAC support is enabled
check_flac_support() {
    echo "Checking FLAC support..."
    
    if ! grep -q "HAVE_FLAC" "${BUILD_DIR}/include/config.h" 2>/dev/null; then
        echo -e "${RED}FLAC support not enabled in build configuration${NC}"
        return 1
    fi
    
    echo -e "${GREEN}FLAC support is enabled${NC}"
    return 0
}

# Function to check if test file exists
check_test_file() {
    echo "Checking test file availability..."
    echo "Test file: ${TEST_FLAC_FILE}"
    
    if [ ! -f "${TEST_FLAC_FILE}" ]; then
        echo -e "${YELLOW}Test file not found - real file tests will be skipped${NC}"
        return 1
    fi
    
    # Get file info
    local file_size=$(stat -c%s "${TEST_FLAC_FILE}" 2>/dev/null || echo "unknown")
    echo "File size: ${file_size} bytes"
    
    echo -e "${GREEN}Test file is available${NC}"
    return 0
}

# Function to build FLAC tests
build_flac_tests() {
    echo "Building FLAC compatibility tests..."
    
    local tests_to_build=(
        "test_flac_demuxer_simple"
        "test_flac_real_file"
    )
    
    cd "${BUILD_DIR}"
    
    for test in "${tests_to_build[@]}"; do
        echo "Building ${test}..."
        if make -C tests "${test}" > /dev/null 2>&1; then
            echo -e "${GREEN}✓ ${test} built successfully${NC}"
        else
            echo -e "${RED}✗ ${test} build failed${NC}"
            echo "Attempting verbose build:"
            make -C tests "${test}"
            return 1
        fi
    done
    
    return 0
}

# Function to run simple FLAC test
run_simple_test() {
    echo "Running simple FLAC demuxer test..."
    
    local test_executable="${BUILD_DIR}/tests/test_flac_demuxer_simple"
    
    if [ ! -x "${test_executable}" ]; then
        echo -e "${RED}Simple test executable not found or not executable${NC}"
        return 1
    fi
    
    echo "Executing: ${test_executable}"
    
    # Capture output and return code
    local output_file=$(mktemp)
    local start_time=$(date +%s.%N)
    
    if "${test_executable}" > "${output_file}" 2>&1; then
        local end_time=$(date +%s.%N)
        local duration=$(echo "$end_time - $start_time" | bc -l 2>/dev/null || echo "0")
        
        echo -e "${GREEN}Simple test PASSED${NC} (${duration}s)"
        
        # Show test summary
        if grep -q "Tests run:" "${output_file}"; then
            grep "Tests run:\|Tests passed:\|Tests failed:" "${output_file}"
        fi
        
        rm -f "${output_file}"
        return 0
    else
        local end_time=$(date +%s.%N)
        local duration=$(echo "$end_time - $start_time" | bc -l 2>/dev/null || echo "0")
        
        echo -e "${RED}Simple test FAILED${NC} (${duration}s)"
        
        # Show failure details
        echo "Test output:"
        cat "${output_file}"
        
        rm -f "${output_file}"
        return 1
    fi
}

# Function to run real file test
run_real_file_test() {
    echo "Running real FLAC file test..."
    
    if [ ! -f "${TEST_FLAC_FILE}" ]; then
        echo -e "${YELLOW}Test file not available - skipping real file test${NC}"
        return 0
    fi
    
    local test_executable="${BUILD_DIR}/tests/test_flac_real_file"
    
    if [ ! -x "${test_executable}" ]; then
        echo -e "${RED}Real file test executable not found or not executable${NC}"
        return 1
    fi
    
    echo "Executing: ${test_executable}"
    echo "Test file: ${TEST_FLAC_FILE}"
    
    # Capture output and return code
    local output_file=$(mktemp)
    local start_time=$(date +%s.%N)
    
    if "${test_executable}" > "${output_file}" 2>&1; then
        local end_time=$(date +%s.%N)
        local duration=$(echo "$end_time - $start_time" | bc -l 2>/dev/null || echo "0")
        
        echo -e "${GREEN}Real file test PASSED${NC} (${duration}s)"
        
        # Show performance metrics if available
        if grep -q "Parse time:\|Seek time:\|Average" "${output_file}"; then
            echo "Performance metrics:"
            grep -E "Parse time:|Seek time:|Average|Duration:" "${output_file}" | sed 's/^/  /'
        fi
        
        rm -f "${output_file}"
        return 0
    else
        local end_time=$(date +%s.%N)
        local duration=$(echo "$end_time - $start_time" | bc -l 2>/dev/null || echo "0")
        
        echo -e "${RED}Real file test FAILED${NC} (${duration}s)"
        
        # Show failure details
        echo "Test output:"
        cat "${output_file}"
        
        rm -f "${output_file}"
        return 1
    fi
}

# Function to analyze FLACDemuxer implementation status
analyze_implementation() {
    echo "Analyzing FLACDemuxer implementation..."
    
    local flac_demuxer_src="${BUILD_DIR}/src/FLACDemuxer.cpp"
    local flac_demuxer_hdr="${BUILD_DIR}/include/FLACDemuxer.h"
    
    if [ ! -f "${flac_demuxer_src}" ]; then
        echo -e "${RED}FLACDemuxer.cpp not found${NC}"
        return 1
    fi
    
    if [ ! -f "${flac_demuxer_hdr}" ]; then
        echo -e "${RED}FLACDemuxer.h not found${NC}"
        return 1
    fi
    
    # Check if files compile
    echo "Checking if FLACDemuxer compiles..."
    cd "${BUILD_DIR}"
    
    if make -C src FLACDemuxer.o > /dev/null 2>&1; then
        echo -e "${GREEN}✓ FLACDemuxer compiles successfully${NC}"
    else
        echo -e "${RED}✗ FLACDemuxer compilation failed${NC}"
        echo "Compilation errors:"
        make -C src FLACDemuxer.o
        return 1
    fi
    
    # Get implementation statistics
    local src_lines=$(wc -l < "${flac_demuxer_src}")
    local hdr_lines=$(wc -l < "${flac_demuxer_hdr}")
    
    echo "Implementation statistics:"
    echo "  Source lines: ${src_lines}"
    echo "  Header lines: ${hdr_lines}"
    echo "  Total lines: $((src_lines + hdr_lines))"
    
    # Check for key methods
    echo "Checking key method implementations:"
    
    local methods=(
        "parseContainer"
        "getStreams"
        "readChunk"
        "seekTo"
        "getDuration"
    )
    
    for method in "${methods[@]}"; do
        if grep -q "${method}" "${flac_demuxer_src}"; then
            echo -e "  ${GREEN}✓ ${method}${NC}"
        else
            echo -e "  ${RED}✗ ${method}${NC}"
        fi
    done
    
    return 0
}

# Function to provide recommendations
provide_recommendations() {
    echo
    echo "Recommendations:"
    echo "================"
    
    echo "1. ${YELLOW}Debug FLACDemuxer Implementation${NC}"
    echo "   - The test failures suggest issues with the FLACDemuxer parsing logic"
    echo "   - Focus on parseContainer() method and FLAC format handling"
    echo "   - Add debug logging to identify where parsing fails"
    
    echo
    echo "2. ${YELLOW}Investigate Resource Deadlock${NC}"
    echo "   - Real file tests show 'Resource deadlock avoided' errors"
    echo "   - Check for threading issues or recursive locking"
    echo "   - Review IOHandler usage and file access patterns"
    
    echo
    echo "3. ${YELLOW}Validate FLAC Format Handling${NC}"
    echo "   - Ensure mock FLAC data matches actual FLAC specification"
    echo "   - Test with minimal real FLAC files to isolate issues"
    echo "   - Compare with reference FLAC implementations"
    
    echo
    echo "4. ${YELLOW}Simplify Testing Approach${NC}"
    echo "   - Start with basic container recognition tests"
    echo "   - Gradually add complexity (metadata, seeking, etc.)"
    echo "   - Use existing FLAC implementation as reference"
    
    echo
    echo "Next Steps:"
    echo "==========="
    echo "• Fix FLACDemuxer parseContainer() implementation"
    echo "• Resolve threading/locking issues"
    echo "• Add comprehensive error logging"
    echo "• Test with various FLAC file types"
    echo "• Validate against FLAC specification"
}

# Main execution
main() {
    local overall_success=true
    
    # Check prerequisites
    if ! check_flac_support; then
        echo -e "${RED}FLAC support not available - cannot run tests${NC}"
        exit 1
    fi
    
    check_test_file
    local test_file_available=$?
    
    # Build tests
    if ! build_flac_tests; then
        echo -e "${RED}Failed to build FLAC tests${NC}"
        overall_success=false
    fi
    
    # Run tests
    echo
    echo "Running FLAC compatibility tests..."
    echo "=================================="
    
    if ! run_simple_test; then
        overall_success=false
    fi
    
    echo
    
    if [ $test_file_available -eq 0 ]; then
        if ! run_real_file_test; then
            overall_success=false
        fi
    fi
    
    echo
    
    # Analyze implementation
    if ! analyze_implementation; then
        overall_success=false
    fi
    
    # Provide recommendations
    provide_recommendations
    
    # Final summary
    echo
    echo "Validation Summary:"
    echo "=================="
    
    if [ "$overall_success" = true ]; then
        echo -e "${GREEN}FLAC demuxer validation completed successfully${NC}"
        echo "The testing framework is ready and functional"
        return 0
    else
        echo -e "${RED}FLAC demuxer validation found issues${NC}"
        echo "Implementation needs fixes before full compatibility"
        return 1
    fi
}

# Run main function with all arguments
main "$@"