# Test Harness Integration Design Document

## Overview

This design document outlines the architecture for integrating the existing standalone test files in PsyMP3 into a unified test harness. The solution will refactor the current GNU Autotools build system, clean up dependencies, create a comprehensive test framework, and provide detailed reporting capabilities. The design preserves existing test functionality while providing a modern, maintainable testing infrastructure.

## Architecture

### High-Level Architecture

The test harness follows a modular architecture with clear separation of concerns:

```
Test Harness Architecture
├── Test Discovery Engine
│   ├── File System Scanner
│   ├── Test Metadata Parser
│   └── Dependency Resolver
├── Test Execution Engine  
│   ├── Process Manager
│   ├── Timeout Handler
│   └── Output Capture
├── Test Framework Core
│   ├── Common Test Utilities
│   ├── Assertion Framework
│   └── Test Result Aggregator
├── Reporting System
│   ├── Console Reporter
│   ├── XML Reporter
│   └── JSON Reporter
└── Build System Integration
    ├── Autotools Configuration
    ├── Makefile.am Generation
    └── Dependency Management
```

### Component Interaction Flow

1. **Discovery Phase**: Test Discovery Engine scans the tests/ directory and identifies test files
2. **Build Phase**: Build System Integration ensures all test executables are compiled
3. **Execution Phase**: Test Execution Engine runs tests and captures output
4. **Reporting Phase**: Reporting System processes results and generates reports
##
 Components and Interfaces

### Test Discovery Engine

**Purpose**: Automatically discover and catalog test files and their metadata.

**Interface**:
```cpp
class TestDiscovery {
public:
    struct TestInfo {
        std::string name;
        std::string executable_path;
        std::vector<std::string> dependencies;
        std::chrono::milliseconds timeout;
        std::string description;
    };
    
    std::vector<TestInfo> discoverTests(const std::string& test_directory);
    bool isTestFile(const std::string& filename);
    TestInfo parseTestMetadata(const std::string& filepath);
};
```

**Implementation Details**:
- Scans tests/ directory for files matching pattern `test_*.cpp`
- Parses source files for special comments containing test metadata
- Resolves dependencies based on linked libraries and object files
- Supports configurable timeout values per test

### Test Execution Engine

**Purpose**: Execute individual tests and manage their lifecycle.

**Interface**:
```cpp
class TestExecutor {
public:
    struct ExecutionResult {
        std::string test_name;
        int exit_code;
        std::chrono::milliseconds execution_time;
        std::string stdout_output;
        std::string stderr_output;
        bool timed_out;
        std::string error_message;
    };
    
    ExecutionResult executeTest(const TestInfo& test);
    void setGlobalTimeout(std::chrono::milliseconds timeout);
    void enableParallelExecution(bool enable);
};
```

**Implementation Details**:
- Uses process spawning to execute test executables
- Implements timeout mechanism with SIGTERM/SIGKILL escalation
- Captures both stdout and stderr streams separately
- Supports parallel execution for independent tests
- Handles process crashes and abnormal terminations gracefully##
# Test Framework Core

**Purpose**: Provide common utilities and standardized test structure.

**Interface**:
```cpp
// Common test utilities header
namespace TestFramework {
    class TestCase {
    public:
        TestCase(const std::string& name);
        virtual ~TestCase() = default;
        
        void run();
        const std::string& getName() const;
        bool hasPassed() const;
        const std::vector<std::string>& getFailures() const;
        
    protected:
        virtual void runTest() = 0;
        void assert_true(bool condition, const std::string& message);
        void assert_equals(const auto& expected, const auto& actual, const std::string& message);
        void assert_not_null(const void* ptr, const std::string& message);
    };
    
    class TestSuite {
    public:
        void addTest(std::unique_ptr<TestCase> test);
        void runAll();
        void printResults();
        int getFailureCount() const;
    };
}
```

**Implementation Details**:
- Provides standardized assertion macros with descriptive failure messages
- Implements test case lifecycle management
- Supports test grouping and organization
- Includes common utilities for Rect testing and other components

### Reporting System

**Purpose**: Generate comprehensive test reports in multiple formats.

**Interface**:
```cpp
class TestReporter {
public:
    virtual ~TestReporter() = default;
    virtual void reportStart(const std::vector<TestInfo>& tests) = 0;
    virtual void reportTestResult(const ExecutionResult& result) = 0;
    virtual void reportSummary(const TestSummary& summary) = 0;
};

class ConsoleReporter : public TestReporter { /* ... */ };
class XMLReporter : public TestReporter { /* ... */ };
class JSONReporter : public TestReporter { /* ... */ };
```

**Implementation Details**:
- Console reporter provides real-time progress and colored output
- XML reporter generates JUnit-compatible XML for CI integration
- JSON reporter provides structured data for custom tooling
- Supports verbose and quiet modes
- Includes performance metrics and timing information### Buil
d System Integration

**Purpose**: Integrate test harness with GNU Autotools build system.

**Key Files**:
- `configure.ac`: Updated with test-specific configuration
- `tests/Makefile.am`: New autotools makefile for test directory
- `Makefile.am`: Updated to include tests subdirectory

**Configuration Changes**:
```autoconf
# In configure.ac
AC_CONFIG_FILES([Makefile src/Makefile res/Makefile tests/Makefile])

# Add test harness executable
AC_ARG_ENABLE([test-harness],
  [AS_HELP_STRING([--enable-test-harness], [Build test harness @<:@default=yes@:>@])],
  [enable_test_harness=$enableval],
  [enable_test_harness=yes])

AM_CONDITIONAL([BUILD_TEST_HARNESS], [test "x$enable_test_harness" = "xyes"])
```

## Data Models

### Test Metadata Model

```cpp
struct TestMetadata {
    std::string name;                    // Human-readable test name
    std::string description;             // Test description
    std::vector<std::string> tags;       // Test categorization tags
    std::chrono::milliseconds timeout;   // Maximum execution time
    std::vector<std::string> dependencies; // Required object files/libraries
    bool parallel_safe;                  // Can run in parallel with other tests
    std::string author;                  // Test author
    std::string created_date;            // Creation date
};
```

### Test Result Model

```cpp
struct TestResult {
    std::string test_name;
    TestStatus status;                   // PASSED, FAILED, TIMEOUT, ERROR
    std::chrono::milliseconds duration;
    std::string failure_message;
    std::vector<std::string> assertions; // Individual assertion results
    std::string stdout_capture;
    std::string stderr_capture;
    int exit_code;
};

struct TestSummary {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int timeout_tests;
    int error_tests;
    std::chrono::milliseconds total_duration;
    std::vector<TestResult> results;
};
```## 
Error Handling

### Test Execution Errors

1. **Compilation Failures**: 
   - Capture compiler output and report as build errors
   - Continue with other tests that compile successfully
   - Provide clear error messages with file and line information

2. **Runtime Crashes**:
   - Detect segmentation faults and other signals
   - Capture core dump information if available
   - Report crash location using debugging symbols

3. **Timeout Handling**:
   - Implement graceful termination with SIGTERM
   - Escalate to SIGKILL if process doesn't respond
   - Report timeout duration and expected vs actual execution time

4. **Resource Exhaustion**:
   - Monitor memory usage during test execution
   - Detect and report excessive resource consumption
   - Implement resource limits to prevent system impact

### Build System Error Handling

1. **Dependency Resolution**:
   - Detect missing dependencies and provide clear error messages
   - Suggest installation commands for missing packages
   - Validate version requirements and compatibility

2. **Configuration Errors**:
   - Validate autotools configuration during generation
   - Provide helpful error messages for common configuration issues
   - Support fallback configurations for optional dependencies

## Testing Strategy

### Unit Testing for Test Harness

The test harness itself will be tested using a bootstrap approach:

1. **Core Component Tests**: Unit tests for discovery, execution, and reporting components
2. **Integration Tests**: End-to-end tests using sample test files
3. **Mock Test Scenarios**: Synthetic tests that simulate various failure conditions
4. **Performance Tests**: Validate harness performance with large test suites

### Test Refactoring Strategy

Existing tests will be refactored in phases:

1. **Phase 1**: Extract common utilities and create shared test framework
2. **Phase 2**: Standardize assertion patterns and error reporting
3. **Phase 3**: Add metadata annotations and improve test organization
4. **Phase 4**: Optimize test execution and add performance benchmarks

### Validation Approach

1. **Backward Compatibility**: Ensure all existing tests continue to pass
2. **Feature Parity**: Verify new harness provides same functionality as individual test execution
3. **Performance Validation**: Confirm harness doesn't significantly impact test execution time
4. **Cross-Platform Testing**: Validate on Linux, Windows, and macOS (where supported)## B
uild System Refactoring

### Dependency Cleanup

**Removed Dependencies**:
- `opusfile`: Not directly used in core functionality
- `vorbisfile`: Redundant with existing vorbis support

**Optimized Dependencies**:
- Make external codec libraries optional (FLAC++, libmpg123, vorbis, opus)
- Keep internal codec support mandatory (WAV, alaw/ulaw)
- Conditional compilation for optional codec features
- Improved pkg-config detection with fallbacks

### Makefile.am Structure

```makefile
# tests/Makefile.am
SUBDIRS = .

# Test harness executable
if BUILD_TEST_HARNESS
bin_PROGRAMS = test-harness
test_harness_SOURCES = test_harness.cpp test_discovery.cpp test_executor.cpp test_reporter.cpp
test_harness_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/include
test_harness_LDADD = $(top_builddir)/src/libpsymp3_core.la
endif

# Individual test executables
check_PROGRAMS = test_rect_area_validation test_rect_containment test_rect_intersection test_rect_union

# Test execution target
check-local: $(check_PROGRAMS)
if BUILD_TEST_HARNESS
	./test-harness --verbose
else
	@echo "Running individual tests..."
	@for test in $(check_PROGRAMS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done
endif

CLEANFILES = $(check_PROGRAMS)
```

### Configure.ac Updates

```autoconf
# Remove unused dependencies
# PKG_CHECK_MODULES([VORBIS], [vorbisfile])  # Removed
# PKG_CHECK_MODULES([OPUS], [opusfile])     # Removed

# Make external codecs optional
AC_ARG_ENABLE([flac],
  [AS_HELP_STRING([--enable-flac], [Enable FLAC support @<:@default=yes@:>@])],
  [enable_flac=$enableval], [enable_flac=yes])

AC_ARG_ENABLE([mp3],
  [AS_HELP_STRING([--enable-mp3], [Enable MP3 support @<:@default=yes@:>@])],
  [enable_mp3=$enableval], [enable_mp3=yes])

AC_ARG_ENABLE([vorbis],
  [AS_HELP_STRING([--enable-vorbis], [Enable Vorbis support @<:@default=yes@:>@])],
  [enable_vorbis=$enableval], [enable_vorbis=yes])

AC_ARG_ENABLE([opus],
  [AS_HELP_STRING([--enable-opus], [Enable Opus support @<:@default=yes@:>@])],
  [enable_opus=$enableval], [enable_opus=yes])

# Check for optional codec libraries
if test "x$enable_flac" = "xyes"; then
  PKG_CHECK_MODULES([FLAC], [flac++], [have_flac=yes], [have_flac=no])
  if test "x$have_flac" = "xyes"; then
    AC_DEFINE([HAVE_FLAC], [1], [Define to 1 if FLAC support is enabled])
  fi
fi

if test "x$enable_mp3" = "xyes"; then
  PKG_CHECK_MODULES([LIBMPG123], [libmpg123 >= 1.8], [have_mp3=yes], [have_mp3=no])
  if test "x$have_mp3" = "xyes"; then
    AC_DEFINE([HAVE_MP3], [1], [Define to 1 if MP3 support is enabled])
  fi
fi

AM_CONDITIONAL([HAVE_FLAC], [test "x$have_flac" = "xyes"])
AM_CONDITIONAL([HAVE_MP3], [test "x$have_mp3" = "xyes"])
```

This design provides a comprehensive, maintainable, and extensible test harness that integrates seamlessly with the existing PsyMP3 codebase while modernizing the testing infrastructure.