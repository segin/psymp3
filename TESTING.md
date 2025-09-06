# PsyMP3 Testing Guide

This document provides comprehensive information about testing PsyMP3, including the test harness capabilities and individual test execution methods.

## Test Harness Overview

PsyMP3 includes a unified test harness that provides automatic test discovery, execution, and reporting. The test harness is built when you configure with `--enable-test-harness` (enabled by default).

## Running Tests

### Quick Start

**Run all tests:**
```bash
make check
```

This will build all test executables and run them through the test harness, providing a comprehensive report of test results.

### Test Harness Options

**Basic Usage:**
```bash
cd tests && ./test-harness                    # Run all tests
cd tests && ./test-harness -v                # Run with verbose output
cd tests && ./test-harness -l                # List available tests
cd tests && ./test-harness -q                # Quiet mode (summary only)
```

**Filtering and Selection:**
```bash
cd tests && ./test-harness -f "*rect*"       # Run only rectangle tests
cd tests && ./test-harness -f "*containment*" # Run containment tests
cd tests && ./test-harness -s                # Stop on first failure
```

**Parallel Execution:**
```bash
cd tests && ./test-harness -p                # Run tests in parallel
cd tests && ./test-harness -p -j 8           # Use 8 parallel processes
```

**Output Formats:**
```bash
cd tests && ./test-harness -o xml > results.xml    # XML output for CI
cd tests && ./test-harness -o json > results.json  # JSON output
cd tests && ./test-harness -o console              # Console output (default)
```

**Performance Analysis:**
```bash
cd tests && ./test-harness --track-performance     # Enable performance tracking
cd tests && ./test-harness --show-performance      # Show performance report
cd tests && ./test-harness --show-detailed-perf    # Detailed performance metrics
cd tests && ./test-harness --analyze-trends        # Analyze performance trends
```

**Advanced Options:**
```bash
cd tests && ./test-harness -t 60               # Set 60-second timeout per test
cd tests && ./test-harness -d /path/to/tests   # Specify test directory
```

## Individual Test Execution

If you need to run specific tests individually:

```bash
cd tests
make test_rect_containment
./test_rect_containment
```

### Available Test Executables

- `test_rect_area_validation`
- `test_rect_containment`
- `test_rect_intersection`
- `test_rect_union`
- `test_rect_centering`
- `test_rect_centering_overflow`
- `test_rect_expansion`
- `test_rect_transformation`
- `test_rect_normalization`
- `test_rect_modern_cpp`

## Test Harness Features

The unified test harness provides:

- **Automatic Discovery**: Finds and runs all available tests
- **Standardized Reporting**: Consistent output format across all tests
- **Performance Tracking**: Monitor test execution times and analyze trends
- **Multiple Output Formats**: Support for console, XML, and JSON output for CI integration
- **Parallel Execution**: Run tests concurrently for faster completion
- **Error Handling**: Robust timeout management and failure reporting
- **Filtering**: Run specific subsets of tests based on name patterns
- **Verbose Modes**: Control output detail level from quiet summaries to verbose debugging

## Continuous Integration

For CI environments, use:

```bash
# Generate XML report for Jenkins/similar
make check && cd tests && ./test-harness -o xml > test-results.xml

# Generate JSON report for custom processing
make check && cd tests && ./test-harness -o json > test-results.json

# Parallel execution with timeout for faster CI builds
make check && cd tests && ./test-harness -p -t 30
```

## Troubleshooting

### Test Failures

1. **Individual Test Debugging**: Run the specific failing test individually with verbose output
2. **Performance Issues**: Use `--track-performance` to identify slow tests
3. **Timeout Issues**: Increase timeout with `-t <seconds>` for slower systems

### Build Issues

If tests fail to build:

1. Ensure all dependencies are installed
2. Check that `--enable-test-harness` was used during configure
3. Verify that `make` completed successfully before running tests

## Contributing Tests

When adding new tests:

1. Place test files in the `tests/` directory
2. Follow the naming convention: `test_<component>.cpp`
3. Ensure tests are compilable independently
4. Add appropriate build rules to `tests/Makefile.am`
5. Test both individual execution and harness integration