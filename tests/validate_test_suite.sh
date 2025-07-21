#!/bin/bash

# Comprehensive test suite validation script
# Tests the test harness functionality and backward compatibility

set -e

echo "=== PsyMP3 Test Suite Validation ==="
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Detect platform
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
elif [[ "$OSTYPE" == "cygwin" ]]; then
    PLATFORM="Windows (Cygwin)"
elif [[ "$OSTYPE" == "msys" ]]; then
    PLATFORM="Windows (MSYS)"
elif [[ "$OSTYPE" == "win32" ]]; then
    PLATFORM="Windows"
else
    PLATFORM="Unknown"
fi

echo -e "${BLUE}Detected platform: $PLATFORM${NC}"
echo

run_test() {
    local test_name="$1"
    local test_command="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -n "Testing $test_name... "
    
    if eval "$test_command" > /dev/null 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
}

run_test_with_output() {
    local test_name="$1"
    local test_command="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo "Testing $test_name..."
    
    if eval "$test_command"; then
        echo -e "${GREEN}PASS${NC}: $test_name"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        echo -e "${RED}FAIL${NC}: $test_name"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
}

skip_test() {
    local test_name="$1"
    local reason="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
    echo -e "Testing $test_name... ${YELLOW}SKIP${NC} ($reason)"
}

echo -e "${BLUE}1. Testing build system integration...${NC}"

# Test that make check target exists
run_test "make check target availability" "make -n check"

# Test that test harness builds
run_test "test harness compilation" "make test-harness"

# Test that individual tests build
run_test "individual test compilation" "make test_rect_containment"

# Test autotools integration
run_test "autotools dependency tracking" "grep -q 'AUTOMAKE_OPTIONS.*subdir-objects' Makefile.am"

# Test that test directory is included in build
run_test "tests directory in build" "grep -q 'SUBDIRS.*tests' ../Makefile.am"

echo

echo -e "${BLUE}2. Testing test harness functionality...${NC}"

# Test basic harness execution
run_test "basic harness execution" "./test-harness --list"

# Test verbose mode
run_test "verbose mode" "./test-harness --verbose --filter nonexistent"

# Test filtering
run_test "test filtering" "./test-harness --filter rect --list"

# Test different output formats
run_test "XML output format" "./test-harness --output-format xml --filter nonexistent > test_results.xml && [ -s test_results.xml ]"
run_test "JSON output format" "./test-harness --output-format json --filter nonexistent > test_results.json && [ -s test_results.json ]"

# Clean up output files
rm -f test_results.xml test_results.json

# Test parallel execution
run_test "parallel execution" "./test-harness --parallel --filter rect"

echo

echo -e "${BLUE}3. Testing backward compatibility...${NC}"

# Test individual test execution
if [ -f "./test_rect_containment" ]; then
    run_test "individual test execution" "./test_rect_containment"
else
    skip_test "individual test execution" "test_rect_containment not found"
fi

# Test that old test files still work
found_old_tests=false
for test_file in test_rect_*; do
    if [ -x "$test_file" ] && [ "$test_file" != "test_rect.cpp" ]; then
        found_old_tests=true
        run_test "backward compatibility: $test_file" "./$test_file"
    fi
done

if [ "$found_old_tests" = false ]; then
    skip_test "backward compatibility tests" "no executable test files found"
fi

# Create a legacy-style test
cat > test_legacy_style.cpp << 'EOF'
#include <iostream>
int main() {
    std::cout << "Legacy test running...\n";
    return 0;
}
EOF

# Compile and test legacy style
if g++ -o test_legacy_style test_legacy_style.cpp -I../include 2>/dev/null; then
    run_test "legacy test compilation" "[ -x test_legacy_style ]"
    run_test "legacy test execution" "./test_legacy_style"
    
    # Test harness can run legacy tests
    run_test "harness with legacy tests" "./test-harness --filter legacy"
else
    skip_test "legacy test compilation" "compiler error"
fi

# Clean up
rm -f test_legacy_style test_legacy_style.cpp

echo

echo -e "${BLUE}4. Testing error handling...${NC}"

# Test missing executable handling
run_test "missing executable handling" "./test-harness --filter nonexistent_test_12345"

# Test invalid arguments
run_test "invalid argument handling" "! ./test-harness --invalid-option"

# Create a crashing test
cat > test_crash.cpp << 'EOF'
#include <iostream>
int main() {
    std::cout << "About to crash...\n";
    int* ptr = nullptr;
    *ptr = 42; // This will crash
    return 0;
}
EOF

# Compile and test crash handling
if g++ -o test_crash test_crash.cpp 2>/dev/null; then
    run_test "crash handling" "./test-harness --filter crash; [ $? -ne 0 ]"
else
    skip_test "crash handling" "compiler error"
fi

# Clean up
rm -f test_crash test_crash.cpp

# Create a timeout test
cat > test_timeout.cpp << 'EOF'
#include <iostream>
#include <thread>
#include <chrono>
int main() {
    std::cout << "Starting long operation...\n";
    std::this_thread::sleep_for(std::chrono::seconds(30));
    return 0;
}
EOF

# Compile and test timeout handling
if g++ -o test_timeout test_timeout.cpp -pthread 2>/dev/null; then
    run_test "timeout handling" "timeout 5s ./test-harness --timeout 2 --filter timeout; [ $? -ne 0 ]"
else
    skip_test "timeout handling" "compiler error"
fi

# Clean up
rm -f test_timeout test_timeout.cpp

echo

echo -e "${BLUE}5. Testing performance and resource usage...${NC}"

# Create a performance test
cat > test_performance.cpp << 'EOF'
#include <iostream>
#include <chrono>
int main() {
    auto start = std::chrono::high_resolution_clock::now();
    // Do some work
    for (int i = 0; i < 1000000; i++) {
        volatile int x = i * i;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Performance test completed in " << duration.count() << "ms\n";
    return 0;
}
EOF

# Compile and test performance monitoring
if g++ -o test_performance test_performance.cpp 2>/dev/null; then
    run_test "performance monitoring" "./test-harness --verbose --filter performance | grep -q 'ms'"
else
    skip_test "performance monitoring" "compiler error"
fi

# Clean up
rm -f test_performance test_performance.cpp

echo

echo -e "${BLUE}6. Testing cross-platform compatibility...${NC}"

# Test on current platform
run_test "current platform execution" "./test-harness --filter rect"

# Test file system operations
run_test "test discovery file operations" "[ -n \"$(./test-harness --list | grep -c test_)\" ]"

# Platform-specific tests
case "$PLATFORM" in
    Linux)
        run_test "Linux-specific file paths" "find . -name 'test_*' | grep -q test_"
        ;;
    macOS)
        run_test "macOS-specific file paths" "find . -name 'test_*' | grep -q test_"
        ;;
    Windows*)
        run_test "Windows-specific file paths" "dir /b test_* 2>/dev/null | grep -q test_ || find . -name 'test_*' | grep -q test_"
        ;;
    *)
        skip_test "platform-specific tests" "unknown platform"
        ;;
esac

echo

echo -e "${BLUE}7. Testing integration with autotools...${NC}"

# Test autotools regeneration
if [ -f "../generate-configure.sh" ]; then
    if [ -x "../generate-configure.sh" ]; then
        run_test "autotools regeneration" "cd .. && ./generate-configure.sh > /dev/null 2>&1"
    else
        skip_test "autotools regeneration" "generate-configure.sh not executable"
    fi
else
    skip_test "autotools regeneration" "generate-configure.sh not found"
fi

# Test configure script
if [ -f "../configure" ]; then
    run_test "configure script" "cd .. && ./configure --help | grep -q test"
else
    skip_test "configure script" "configure not found"
fi

echo

echo -e "${BLUE}8. Testing comprehensive test execution...${NC}"

# Run the test harness integration test
if [ -f "./test_harness_integration" ] || make test_harness_integration > /dev/null 2>&1; then
    run_test_with_output "test harness integration test" "./test_harness_integration"
else
    skip_test "test harness integration test" "compilation failed"
fi

# Run the full test suite
run_test_with_output "full test suite execution" "./test-harness --verbose"

echo

echo -e "${BLUE}=== Validation Summary ===${NC}"
echo "Total tests: $TOTAL_TESTS"
echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed: ${RED}$FAILED_TESTS${NC}"
echo -e "Skipped: ${YELLOW}$SKIPPED_TESTS${NC}"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "\n${GREEN}All validation tests passed!${NC}"
    echo "The test harness is working correctly and maintains backward compatibility."
    exit 0
else
    echo -e "\n${RED}Some validation tests failed.${NC}"
    echo "Please review the failures above and fix any issues."
    exit 1
fi