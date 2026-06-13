# Demuxer Architecture Test Suite

This directory contains comprehensive tests for the PsyMP3 demuxer architecture, implementing the testing requirements from the demuxer architecture specification.

## Test Organization

### Unit Tests

#### `test_demuxer_unit.cpp`
Tests the base Demuxer interface and data structures:
- StreamInfo data structure validation
- MediaChunk data structure validation  
- Base Demuxer interface functionality
- Error handling and recovery
- Thread safety of base components

#### `test_demuxer_factory_unit.cpp`
Tests the DemuxerFactory format detection and creation:
- Format signature detection
- Magic byte recognition for RIFF, Ogg, AIFF, MP4, FLAC
- Format detection with file path hints
- Demuxer creation for supported formats
- Format signature registration
- Thread safety of factory operations

#### `test_media_factory_unit.cpp`
Tests the MediaFactory stream creation system:
- MediaFormat and ContentInfo data structures
- URI and path utilities (extension extraction, HTTP detection)
- MIME type utilities and conversions
- Format support queries
- Format registration and unregistration
- Content detection and analysis

#### `test_demuxed_stream_unit.cpp`
Tests the DemuxedStream bridge functionality:
- Stream initialization with demuxer/codec integration
- Stream information access and metadata
- Chunk reading and audio frame decoding
- Seeking functionality
- Error handling in stream processing
- Multi-stream switching capabilities

### Integration Tests

#### `test_demuxer_integration.cpp`
Tests demuxer implementations with real container formats:
- RIFF/WAV demuxer with generated test data
- Ogg demuxer with multi-page structure
- AIFF demuxer with big-endian format
- MP4 demuxer with box structure
- FLAC demuxer with metadata blocks
- Multi-threaded demuxer access
- Error recovery scenarios
- IOHandler integration with different sources

#### `test_media_factory_integration.cpp`
Tests MediaFactory with complete workflow:
- Stream creation with various formats
- MIME type hint processing
- Content analysis and format detection
- Format registration lifecycle
- HTTP streaming support
- Error handling and exception scenarios
- Thread safety under concurrent access
- ContentInfo-based stream creation

### Performance and Regression Tests

#### `test_demuxer_performance.cpp`
Tests performance characteristics and prevents regressions:
- Parsing performance with large files (30+ seconds of audio)
- Chunk reading throughput and I/O efficiency
- Seeking performance across file positions
- Memory usage and buffer pool efficiency
- Concurrent access performance with multiple threads
- Scalability with multiple simultaneous demuxers
- Regression tests for known edge cases

## Test Data Generation

The test suite includes sophisticated test data generators that create valid container files:

- **RIFF/WAV**: Complete headers with PCM audio data
- **Ogg**: Multi-page structure with Vorbis identification
- **AIFF**: Big-endian format with proper chunk structure
- **MP4**: Basic box structure with ftyp and mdat boxes
- **FLAC**: Metadata blocks with STREAMINFO

## Running Tests

### Individual Tests
```bash
# Build and run specific test
make test_demuxer_unit && ./test_demuxer_unit
make test_demuxer_integration && ./test_demuxer_integration
```

### Complete Test Suite
```bash
# Run all demuxer architecture tests
./run_demuxer_tests.sh
```

### Integration with Build System
```bash
# Run all tests including demuxer tests
make check
```

## Performance Benchmarks

The performance tests establish baseline expectations:

- **Parsing**: Large files should parse within 1-2 seconds
- **Chunk Reading**: Minimum 100 chunks/second, 1 MB/second throughput
- **Seeking**: Average seek time under 50ms
- **Memory Usage**: Buffer pool should stay under 10MB during normal operation
- **Concurrency**: No significant performance degradation with 4 concurrent threads

## Test Framework

All tests use the custom `TestFramework` with features:
- Comprehensive assertion macros (`ASSERT_TRUE`, `ASSERT_EQUALS`, etc.)
- Test case lifecycle management (setUp/tearDown)
- Performance timing utilities
- Thread-safe test execution
- Detailed failure reporting

## Mock Objects

The test suite includes sophisticated mock implementations:
- `MockIOHandler`: Configurable I/O simulation with failure injection
- `MockDemuxer`: Base demuxer functionality testing
- `MockAudioCodec`: Audio decoding simulation
- `PerformanceIOHandler`: I/O operation counting and metrics

## Coverage

The test suite validates all requirements from the demuxer architecture specification:

### Requirements 1.1-1.10 (Base Demuxer Interface)
- ✅ Interface consistency and method signatures
- ✅ Error handling and recovery mechanisms
- ✅ Thread safety and concurrent access
- ✅ Resource management and cleanup

### Requirements 7.1-7.10 (DemuxerFactory)
- ✅ Format detection by magic bytes and extensions
- ✅ Demuxer creation and registration
- ✅ Priority-based format selection
- ✅ Thread-safe factory operations

### Requirements 8.1-8.10 (MediaFactory)
- ✅ Stream creation and content analysis
- ✅ MIME type handling and HTTP streaming
- ✅ Format registration and plugin architecture
- ✅ Error handling and exception management

### Requirements 9.1-9.10 (DemuxedStream Bridge)
- ✅ Stream interface compatibility
- ✅ Demuxer/codec integration
- ✅ Metadata access and stream switching
- ✅ Performance and memory efficiency

### Requirements 10.1-10.8 (Multi-threading)
- ✅ Thread-safe state access
- ✅ Concurrent chunk reading
- ✅ Atomic operations and synchronization
- ✅ Deadlock prevention

### Requirements 11.1-11.8 (Error Handling)
- ✅ Graceful error recovery
- ✅ Comprehensive error reporting
- ✅ Resource cleanup on failure
- ✅ Validation and boundary checking

### Requirements 12.1-12.8 (I/O Integration)
- ✅ IOHandler abstraction compatibility
- ✅ File and network I/O support
- ✅ Seeking and positioning
- ✅ EOF and error state handling

### Requirements 13.1-13.10 (Performance)
- ✅ Parsing and processing benchmarks
- ✅ Memory usage optimization
- ✅ I/O efficiency metrics
- ✅ Scalability validation

### Requirements 14.1-14.10 (Validation)
- ✅ Input validation and sanitization
- ✅ Format compliance checking
- ✅ Boundary condition testing
- ✅ Regression prevention

## Maintenance

When adding new demuxer implementations:

1. Add format detection tests to `test_demuxer_factory_unit.cpp`
2. Add integration tests with real data to `test_demuxer_integration.cpp`
3. Update performance benchmarks in `test_demuxer_performance.cpp`
4. Add test data generator for the new format
5. Update this README with coverage information

## Dependencies

The test suite requires:
- TestFramework (libtest_utilities.a)
- Core demuxer architecture components
- Standard C++17 features (threading, chrono, atomic)
- POSIX-compatible system for threading tests

## Known Limitations

- Network streaming tests use mock data (no actual HTTP requests)
- Some format-specific tests use simplified/minimal test data
- Performance benchmarks are system-dependent
- Thread safety tests are probabilistic (may not catch all race conditions)

## Future Enhancements

- Add fuzzing tests for robustness
- Implement property-based testing
- Add memory leak detection
- Expand codec integration testing
- Add real-world file format validation