# FLAC Test Suite Documentation

## Overview

The PsyMP3 FLAC test suite has been enhanced to validate all FLAC functionality against real test data files located in `tests/data/`. This ensures comprehensive testing with actual FLAC files rather than just synthetic data.

## Test Data Files

The test suite expects FLAC test files in the `tests/data/` directory:

- `11 life goes by.flac`
- `RADIO GA GA.flac`
- `11 Everlong.flac`

These files are used by all FLAC tests to validate real-world functionality.

## Test Categories

### 1. Core Validation Tests

#### `test_flac_test_data_validation`
- **Purpose**: Validates basic accessibility and format compliance of test data files
- **Coverage**: File existence, FLAC signature validation, basic demuxer initialization
- **Requirements**: Tests all available FLAC files in `tests/data/`

#### `test_flac_rfc_compliance`
- **Purpose**: Validates compliance with RFC 9639 FLAC specification
- **Coverage**: FLAC signature, STREAMINFO block, frame structure, sync patterns
- **Requirements**: Ensures all test files conform to FLAC standard

#### `test_flac_comprehensive_validation`
- **Purpose**: Comprehensive functionality testing with real files
- **Coverage**: Demuxer functionality, metadata extraction, frame reading, seeking, performance, error recovery, thread safety, memory usage
- **Requirements**: Tests all core FLAC functionality against real data

### 2. Quality and Performance Tests

#### `test_flac_quality_validation`
- **Purpose**: Quality validation and accuracy testing
- **Coverage**: Bit-perfect validation, signal-to-noise ratio, harmonic distortion, conversion quality
- **Enhancement**: Now includes validation against real test files in addition to synthetic tests

#### `test_flac_performance_with_real_files`
- **Purpose**: Performance testing with real FLAC files
- **Coverage**: Parsing performance, frame reading speed, seeking performance
- **Enhancement**: Updated to use `FLACTestDataUtils` for consistent test file access

### 3. Integration and Threading Tests

#### `test_flac_codec_integration`
- **Purpose**: Integration testing between FLACCodec and DemuxedStream
- **Enhancement**: Now includes `flac_test_data_utils.h` for real file testing

#### `test_flac_codec_threading_safety`
- **Purpose**: Thread safety validation following PsyMP3 threading patterns
- **Enhancement**: Enhanced with test data utilities for real-world threading scenarios

## Test Utilities

### `flac_test_data_utils.h`

Common utility class providing:

- **`getTestFiles()`**: Returns list of expected test file paths
- **`findAvailableTestFile()`**: Finds first available test file
- **`getAvailableTestFiles()`**: Returns all existing test files
- **`fileExists()`**: Checks file existence
- **`getFileSize()`**: Returns file size in bytes
- **`printTestFileInfo()`**: Prints test file information for debugging
- **`validateTestDataAvailable()`**: Validates at least one test file exists

### Usage Pattern

All FLAC tests should follow this pattern:

```cpp
#include "psymp3.h"
#include "flac_test_data_utils.h"

bool testFunction() {
    // Print test file information
    FLACTestDataUtils::printTestFileInfo("Test Name");
    
    // Validate test data is available
    if (!FLACTestDataUtils::validateTestDataAvailable("Test Name")) {
        return false;
    }
    
    // Get available test files
    auto testFiles = FLACTestDataUtils::getAvailableTestFiles();
    
    // Test with each file
    for (const auto& file : testFiles) {
        // Test implementation
    }
}
```

## Running Tests

### Individual Tests

Run individual tests directly:
```bash
./test_flac_test_data_validation
./test_flac_rfc_compliance
./test_flac_comprehensive_validation
```

### Test Suite Runner

Use the comprehensive test runner:
```bash
./run_flac_tests.sh
```

The runner will:
- Check for test data availability
- Run tests in order of importance
- Provide comprehensive results summary
- Exit with appropriate status codes

### Build Requirements

All FLAC tests require:
- FLAC support enabled (`HAVE_FLAC`)
- Test data files in `tests/data/`
- Standard PsyMP3 build dependencies

## Test File Requirements

### Minimum Requirements

- At least one valid FLAC file in `tests/data/`
- Files must have valid FLAC signature (`fLaC`)
- Files must be readable and non-empty

### Recommended Test Files

- Various sample rates (44.1kHz, 48kHz, 96kHz)
- Different bit depths (16-bit, 24-bit)
- Different channel configurations (mono, stereo)
- Various compression levels
- Files with different metadata configurations

## Error Handling

### Missing Test Data

When no test data is available:
- Tests will report the missing files
- Tests will exit gracefully with appropriate error codes
- Clear error messages indicate expected file locations

### Corrupted Test Data

When test data is corrupted:
- Tests validate FLAC signatures
- Tests check file accessibility
- Tests report specific validation failures

## Integration with Build System

### Makefile Integration

All new tests are integrated into `tests/Makefile.am`:
- Conditional compilation based on `HAVE_FLAC`
- Proper dependency linking
- Standard build flags and libraries

### Autotools Integration

Tests integrate with the standard autotools build process:
- `make check` runs all tests including FLAC tests
- Parallel build support
- Proper dependency tracking

## Threading Safety Compliance

All FLAC tests follow PsyMP3 threading safety guidelines:
- Public/private lock pattern implementation
- RAII lock guards usage
- Documented lock acquisition order
- Exception safety guarantees

## Performance Considerations

### Test Performance

- Tests limit frame reading for performance (typically 10-100 frames)
- Performance thresholds are reasonable for CI environments
- Memory usage is monitored and controlled

### Real File Testing

- Tests work with files of various sizes
- Seeking tests adapt to file length
- Performance tests provide meaningful metrics

## Future Enhancements

### Planned Improvements

1. **Automated Test Data Generation**: Generate test files with specific characteristics
2. **Fuzzing Integration**: Add fuzzing tests with malformed FLAC data
3. **Benchmark Comparisons**: Compare performance against reference implementations
4. **Metadata Validation**: Enhanced metadata block validation
5. **Error Injection**: Systematic error injection testing

### Test Coverage Goals

- 100% code coverage for FLAC demuxer
- All RFC 9639 specification requirements
- All error conditions and edge cases
- Performance regression detection
- Memory leak detection

## Troubleshooting

### Common Issues

1. **Missing Test Files**: Ensure FLAC files exist in `tests/data/`
2. **Build Failures**: Check FLAC library dependencies
3. **Permission Issues**: Ensure test files are readable
4. **Performance Issues**: Check system resources during testing

### Debug Information

All tests provide detailed debug output:
- File existence and size information
- Test progress and results
- Error messages with context
- Performance metrics and timing

## Compliance and Standards

### RFC 9639 Compliance

Tests validate compliance with FLAC specification:
- File format structure
- Metadata block requirements
- Frame format validation
- Sync pattern verification

### PsyMP3 Standards

Tests follow PsyMP3 development standards:
- Threading safety patterns
- Error handling conventions
- Memory management practices
- Code organization principles