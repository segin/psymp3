# **IOHANDLER SUBSYSTEM IMPLEMENTATION PLAN**

## **Implementation Tasks**

- [x] 1. Set Up Directory Structure and Build System
  - Create modular directory structure (src/io/, include/io/, src/io/file/, src/io/http/)
  - Set up Makefile.am files for each subdirectory building convenience libraries
  - Configure namespace organization (PsyMP3::IO::, PsyMP3::IO::File::, PsyMP3::IO::HTTP::)
  - Update configure.ac with new Makefile locations
  - Add backward compatibility using declarations in psymp3.h
  - _Requirements: 10.8_

- [x] 2. Create Base IOHandler Interface
  - Implement IOHandler base class in PsyMP3::IO namespace with virtual interface methods
  - Add default implementations that return safe values for non-functional state
  - Create proper virtual destructor for polymorphic cleanup
  - Add protected member variables for common state tracking (m_closed, m_eof, m_position)
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8_

- [x] 2.1 Define Core I/O Interface Methods
  - Implement read() method with fread-like semantics (size, count parameters)
  - Add seek() method supporting SEEK_SET, SEEK_CUR, SEEK_END positioning modes
  - Create tell() method returning current byte offset with off_t for large file support
  - Add close() method for resource cleanup with standard return codes
  - Implement eof() method returning boolean end-of-stream condition
  - Create getFileSize() method returning total size in bytes or -1 if unknown
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7_

- [x] 3. Implement FileIOHandler for Local Files
  - Create FileIOHandler class in PsyMP3::IO::File namespace inheriting from IOHandler
  - Add constructor accepting TagLib::String path with Unicode support
  - Implement public/private lock pattern with _unlocked methods for thread safety
  - Add mutex member variable for synchronization
  - Implement all IOHandler interface methods using standard C file operations
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 2.10, 2.11, 9.1_

- [x] 3.1 Implement Cross-Platform File Opening
  - Add Windows-specific file opening using _wfopen for Unicode support
  - Implement Unix/Linux file opening using fopen with proper encoding handling
  - Create proper error handling with InvalidMediaException for file open failures
  - Add file handle validation and initialization
  - Store file path for debugging purposes
  - _Requirements: 2.1, 2.2, 2.4, 5.1, 5.2, 5.3, 5.4_

- [x] 3.2 Implement Thread-Safe File I/O Operations
  - Create public read() method acquiring lock and calling read_unlocked()
  - Implement read_unlocked() using standard C fread() for maximum compatibility
  - Add public seek() acquiring lock and calling seek_unlocked()
  - Implement seek_unlocked() and tell_unlocked() using 64-bit operations (fseeko/ftello)
  - Create close() method with double-closing prevention and handle nullification
  - Implement eof() method using standard C feof() function
  - _Requirements: 2.5, 2.6, 2.7, 2.10, 9.1_

- [x] 3.3 Add Large File Support
  - Implement getFileSize() using fstat() system call for accurate size reporting
  - Use 64-bit file operations on all platforms for files larger than 2GB
  - Add platform-specific implementations (_fstat64 on Windows, fstat on Unix)
  - Ensure proper handling of very large files without integer overflow
  - Protect file size queries with mutex for thread safety
  - _Requirements: 2.8, 2.9, 5.1, 5.2, 5.4, 9.1_

- [x] 3.4 Add Debug Logging and Error Handling
  - Implement comprehensive debug logging using PsyMP3 Debug system with "file" category
  - Add error condition logging with detailed context information
  - Create proper exception handling with descriptive error messages
  - Add validation for file operations and parameter checking
  - Ensure thread-safe logging operations
  - _Requirements: 2.11, 7.1, 10.3, 9.8_

- [x] 4. Create HTTPClient Foundation
  - Implement HTTPClient class in PsyMP3::IO::HTTP namespace with static methods
  - Add support for GET, POST, HEAD, and range request methods
  - Create Response struct for returning HTTP response data
  - Implement thread-safe design using per-request libcurl handles
  - Add comprehensive SSL/TLS support for HTTPS connections
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 4.8, 4.9, 4.10, 4.11, 4.12, 9.3_

- [x] 4.1 Implement Core HTTP Methods
  - Create static get() method for standard HTTP GET requests with headers and timeout
  - Add static post() method for HTTP POST with data, content type, and custom headers
  - Implement static head() method for metadata-only requests without response body
  - Create static getRange() method for HTTP range requests with start/end byte specification
  - _Requirements: 4.1, 4.6_

- [x] 4.2 Add libcurl Integration with Thread Safety
  - Implement performRequest() method using libcurl for robust HTTP operations
  - Create per-request CURL handle initialization for thread safety
  - Add proper libcurl initialization and cleanup with RAII management
  - Create response parsing for status codes, headers, and body content
  - Implement proper error handling for network and HTTP failures
  - Ensure no shared state between concurrent requests
  - _Requirements: 4.4, 4.7, 4.8, 4.11, 4.12, 9.3_

- [x] 4.3 Implement SSL/TLS and Advanced Features
  - Add HTTPS support with proper certificate validation using libcurl
  - Implement automatic redirect following for 3xx responses
  - Create URL encoding functionality for safe parameter transmission
  - Add URL parsing for host, port, path, and protocol component extraction
  - Configure libcurl for thread-safe operation (CURL_GLOBAL_ALL)
  - _Requirements: 4.2, 4.3, 4.5, 4.9, 4.10, 9.3_

- [x] 5. Implement HTTPIOHandler for Network Streams
  - Create HTTPIOHandler class in PsyMP3::IO::HTTP namespace inheriting from IOHandler
  - Add constructors for URL with and without explicit content length
  - Implement public/private lock pattern with _unlocked methods for thread safety
  - Add mutex member variable for synchronization
  - Implement HTTP streaming with range request support for seeking
  - Add efficient buffering system for network data
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 3.9, 3.10, 3.11, 3.12, 9.2_

- [x] 5.1 Implement Thread-Safe HTTP Initialization
  - Create public initialize() method acquiring lock and calling initialize_unlocked()
  - Implement initialize_unlocked() performing HEAD request for metadata discovery
  - Extract Content-Length header for total file size determination
  - Parse Content-Type header and normalize MIME type information
  - Detect Accept-Ranges header for range request capability
  - Protect initialization state with mutex
  - _Requirements: 3.1, 3.2, 3.4, 9.2_

- [x] 5.2 Add Thread-Safe HTTP Buffering System
  - Implement efficient buffering with configurable buffer size (default 64KB)
  - Create fillBuffer_unlocked() method using HTTP range requests for data retrieval
  - Add readFromBuffer_unlocked() method for efficient data extraction from buffer
  - Implement isPositionBuffered_unlocked() for buffer hit detection and optimization
  - Protect all buffer state (m_buffer, m_buffer_offset, m_buffer_start_position) with mutex
  - _Requirements: 3.5, 3.8, 3.9, 6.2, 9.2_

- [x] 5.3 Implement Thread-Safe HTTP I/O Operations
  - Create public read() method acquiring lock and calling read_unlocked()
  - Implement read_unlocked() with intelligent buffering and range request management
  - Add public seek() method acquiring lock and calling seek_unlocked()
  - Implement seek_unlocked() using HTTP range requests when supported by server
  - Create tell() method returning current logical position in stream
  - Implement eof() method detecting end-of-stream from content length or server response
  - _Requirements: 3.6, 3.7, 3.10, 3.11, 3.12, 9.2_

- [x] 5.4 Add HTTP Debug Logging and Error Handling
  - Implement comprehensive debug logging using PsyMP3 Debug system with "http" category
  - Add network error handling distinguishing temporary vs permanent failures
  - Create proper HTTP status code handling and error reporting
  - Implement timeout handling for network operations without hanging
  - Ensure thread-safe logging operations
  - _Requirements: 3.10, 3.11, 7.2, 7.3, 7.4, 10.3, 9.8_

- [x] 6. Ensure Cross-Platform Compatibility
  - Verify platform-specific file operations work correctly on Windows and Unix systems
  - Test proper Unicode filename support with platform-appropriate APIs
  - Validate consistent error handling across different operating systems
  - Ensure network operations work consistently across platform network stacks
  - Test on Windows, Linux, macOS, and BSD variants
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8_

- [x] 6.1 Validate Windows-Specific Implementation
  - Test _wfopen() for Unicode filename support on Windows
  - Verify _fstat64() for large file support on Windows
  - Validate Windows-specific error code handling and translation
  - Test Winsock integration for network operations via libcurl
  - _Requirements: 5.1, 5.2, 5.4, 5.6_

- [x] 6.2 Validate Unix/Linux Platform Implementation
  - Test fopen() with proper encoding handling for Unix systems
  - Verify fstat() for file size determination on Unix platforms
  - Validate POSIX-compliant error handling and reporting
  - Test BSD socket integration for network operations via libcurl
  - _Requirements: 5.2, 5.3, 5.4, 5.6_

- [x] 6.3 Ensure Cross-Platform Consistency
  - Test consistent path handling for different path separator conventions
  - Verify uniform error reporting across platforms
  - Validate consistent large file support using appropriate 64-bit operations
  - Ensure network operations behave identically across platforms
  - _Requirements: 5.3, 5.5, 5.7, 5.8_

- [x] 7. Optimize Performance and Efficiency
  - Profile and optimize I/O operations to ensure no impact on playback quality
  - Tune buffering strategies for different I/O sources based on benchmarks
  - Optimize memory-efficient resource management and allocation
  - Validate performance for common media streaming access patterns
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8_

- [x] 7.1 Optimize File I/O Performance
  - Profile and minimize system calls through efficient buffering in FileIOHandler
  - Benchmark read-ahead optimization for sequential file access patterns
  - Test efficient seeking strategies for large files
  - Consider memory-mapped file access for very large files (optional enhancement)
  - Measure lock overhead and optimize if necessary
  - _Requirements: 6.1, 6.3, 6.7, 6.8_

- [x] 7.2 Enhance Network I/O Performance
  - Implement and tune intelligent read-ahead buffering for HTTP streams
  - Verify connection reuse and keep-alive support through libcurl
  - Test adaptive buffer sizing based on network conditions
  - Optimize HTTP requests through efficient range request batching
  - Minimize lock contention during network operations
  - _Requirements: 6.2, 6.4, 6.5, 6.6_

- [x] 7.3 Optimize Memory Management
  - Verify bounded buffers prevent excessive memory usage
  - Profile buffer allocation and reuse strategies for efficiency
  - Consider memory pool allocation for frequently used buffer sizes
  - Ensure proper cleanup and resource management in all code paths
  - Test memory usage under various workloads
  - _Requirements: 6.6, 6.7, 6.8, 8.1, 8.2, 8.6_

- [x] 8. Implement Comprehensive Error Handling
  - Add comprehensive error handling for all I/O operations
  - Create graceful handling of network issues and file problems
  - Implement proper error reporting using PsyMP3 exception hierarchy
  - Add recovery mechanisms for temporary failures where appropriate
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 10.2_

- [x] 8.1 Implement File Operation Error Handling
  - Add specific error messages for different file operation failures
  - Implement proper handling of permission denied and file not found errors
  - Create timeout handling for network file systems and slow storage
  - Add validation for file parameters and operation preconditions
  - Use InvalidMediaException for file opening failures
  - _Requirements: 2.4, 7.1, 7.5, 7.7, 7.8, 10.2_

- [x] 8.2 Implement Network Error Handling
  - Add distinction between temporary and permanent network failures
  - Implement proper HTTP status code handling and error reporting
  - Create timeout handling for network operations without hanging
  - Consider retry mechanisms for transient network errors
  - Provide detailed error messages from libcurl
  - _Requirements: 3.10, 3.11, 7.2, 7.3, 7.4, 7.6_

- [x] 8.3 Ensure Resource Management Error Handling
  - Implement proper cleanup for memory allocation failures
  - Add handling for resource exhaustion scenarios
  - Create safe error propagation without resource leaks
  - Ensure destructors handle cleanup even in error conditions
  - Use RAII throughout for automatic resource management
  - _Requirements: 7.6, 7.8, 8.1, 8.2, 8.5, 8.7, 8.8_

- [x] 9. Ensure Memory Management and Resource Safety
  - Implement proper memory management for long-running media player
  - Apply RAII principles throughout for automatic resource cleanup
  - Create bounded resource usage to prevent memory leaks
  - Ensure thread-safe resource management with proper synchronization
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8_

- [x] 9.1 Implement RAII Resource Management
  - Use RAII for FILE* handles in FileIOHandler (closed in destructor)
  - Use RAII for CURL* handles in HTTPClient (per-request cleanup)
  - Ensure proper initialization of all member variables in constructors
  - Add exception-safe resource management in all operations using RAII lock guards
  - Create proper cleanup in destructors for all allocated resources
  - _Requirements: 8.1, 8.2, 8.5, 8.8_

- [x] 9.2 Validate Memory Leak Prevention
  - Verify bounded caches and buffers prevent unbounded growth
  - Test proper cleanup of HTTP client resources and connections
  - Monitor memory usage and validate limits for large operations
  - Ensure no memory leaks in error paths and exception handling
  - Run memory leak detection tools (valgrind, sanitizers)
  - _Requirements: 8.3, 8.4, 8.6, 8.7_

- [x] 10. Complete PsyMP3 Integration
  - Integrate IOHandler subsystem with PsyMP3's existing codebase and conventions
  - Add consistent error reporting using PsyMP3's exception hierarchy (InvalidMediaException)
  - Implement proper logging integration with PsyMP3's Debug system ("io", "http", "file" categories)
  - Ensure API consistency across all IOHandler implementations
  - Add backward compatibility using declarations in psymp3.h
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8_

- [x] 10.1 Integrate with Demuxer Architecture
  - Update demuxer implementations to use IOHandler interface
  - Verify all demuxers work with FileIOHandler for local files
  - Test demuxers with HTTPIOHandler for network streams
  - Ensure proper error propagation through demuxer chain
  - Validate TagLib::String parameter compatibility for file paths
  - _Requirements: 10.1, 10.5_

- [x] 10.2 Integrate with MediaFactory and URI Handling
  - Integrate with PsyMP3's URI parsing and handling components
  - Create factory methods for IOHandler creation based on URI schemes (file://, http://, https://)
  - Add support for PsyMP3's configuration system for I/O parameters
  - Ensure consistent resource management patterns with PsyMP3 conventions
  - Test automatic handler selection based on URI
  - _Requirements: 10.4, 10.6, 10.7, 10.8_

- [x] 11. Create Comprehensive Testing Suite
  - Implement unit tests for all IOHandler interface methods and implementations
  - Add integration tests with demuxers and various file types
  - Create thread safety tests validating concurrent access patterns
  - Add performance benchmarks and regression tests
  - Test error handling and recovery scenarios thoroughly
  - _Requirements: All requirements validation_

- [x] 11.1 Implement FileIOHandler Unit Tests
  - Test basic operations: read, seek, tell, eof, getFileSize, close
  - Test with various file types, sizes (including >2GB), and access patterns
  - Validate Unicode filename handling on all platforms
  - Test error conditions: file not found, permission denied, invalid operations
  - Test edge cases: empty files, single-byte files, boundary conditions
  - Verify thread safety with concurrent access from multiple threads
  - _Requirements: 1.1-1.8, 2.1-2.11, 5.1-5.8, 9.1_

- [x] 11.2 Implement HTTPIOHandler Unit Tests
  - Test initialization with HEAD request and metadata extraction
  - Test sequential reading with efficient buffering
  - Test seeking with HTTP range requests
  - Test non-seekable streams and proper error handling
  - Test buffer management, position tracking, and EOF detection
  - Test network errors: timeouts, connection failures, HTTP errors
  - Verify thread safety with concurrent access from multiple threads
  - _Requirements: 3.1-3.12, 9.2_

- [x] 11.3 Implement HTTPClient Unit Tests
  - Test GET, POST, HEAD methods with various parameters
  - Test HTTPS support and SSL/TLS connections
  - Test range requests with partial content retrieval
  - Test error handling: network failures, HTTP status codes, timeouts
  - Test URL operations: encoding, parsing, validation
  - Test header management: custom headers, response header parsing
  - Verify thread safety with concurrent requests from multiple threads
  - _Requirements: 4.1-4.12, 9.3_

- [x] 11.4 Add Integration Tests
  - Test IOHandler integration with FLAC, Ogg, and ISO demuxers
  - Test with real media files of various formats and sizes
  - Verify performance with large files and high-bitrate network streams
  - Test error handling and recovery across component boundaries
  - Validate MediaFactory URI-based handler selection
  - Test with PsyMP3 Debug logging system integration
  - _Requirements: 10.1-10.8_

- [x] 11.5 Create Thread Safety Tests
  - Test concurrent reads from same FileIOHandler instance
  - Test concurrent seeks and reads on HTTPIOHandler
  - Test simultaneous HTTP requests from multiple threads
  - Stress test with high concurrency levels
  - Verify no deadlocks with public/private lock pattern
  - Test exception safety with concurrent access
  - _Requirements: 9.1-9.8_

- [x] 11.6 Create Performance and Stress Tests
  - Benchmark FileIOHandler throughput vs. direct fread operations
  - Benchmark HTTPIOHandler performance with various buffer sizes
  - Measure seek operation latency for local and network sources
  - Test memory usage under various workloads
  - Stress test with large files (>4GB)
  - Stress test with rapid seeking patterns
  - Test extended operation duration for memory leak detection
  - _Requirements: 6.1-6.8, 8.1-8.8_

- [x] 12. Create Documentation
  - Add comprehensive inline documentation for all public interfaces
  - Create API documentation for IOHandler classes and methods
  - Write developer guide for extending the IOHandler system
  - Document integration patterns and best practices
  - Ensure code follows PsyMP3 style guidelines and conventions
  - _Requirements: 10.7, 10.8_

- [x] 12.1 Write API Documentation
  - Document all IOHandler classes, methods, and usage patterns
  - Add code examples for common I/O operations and error handling
  - Explain cross-platform considerations and limitations
  - Document thread safety requirements using public/private lock pattern
  - Document lock acquisition order to prevent deadlocks
  - Include namespace organization and backward compatibility notes
  - _Requirements: 10.7, 10.8, 9.1-9.8_

- [x] 12.2 Write Developer Guide
  - Create guide for implementing new IOHandler types
  - Document HTTPClient usage patterns and best practices
  - Explain performance optimization opportunities and techniques
  - Provide troubleshooting guide for common I/O issues
  - Document build system integration for new I/O handlers
  - Include examples of extending the subsystem
  - _Requirements: 10.7, 10.8, 6.1-6.8, 7.1-7.8_

- [x] 13. Validate System Integration and Performance
  - Ensure IOHandler system integrates properly with existing PsyMP3 functionality
  - Validate performance meets or exceeds requirements
  - Test all current file types and network streams work correctly
  - Ensure memory usage and resource consumption remain reasonable
  - Verify no regressions in existing functionality
  - _Requirements: 10.1-10.8, 6.1-6.8_

- [x] 13.1 Validate Backward Compatibility
  - Verify all currently supported file formats work with FileIOHandler
  - Test existing network streaming functionality with HTTPIOHandler
  - Validate metadata extraction and seeking behavior remain consistent
  - Ensure no regression in audio quality or playback performance
  - Test with existing PsyMP3 test suite
  - _Requirements: 10.1, 10.2, 10.5, 10.8_

- [x] 13.2 Validate Performance Requirements
  - Benchmark FileIOHandler and HTTPIOHandler against requirements
  - Measure memory usage and ensure it meets efficiency requirements
  - Test with various file sizes, network conditions, and usage patterns
  - Validate that thread safety doesn't significantly impact performance
  - Compare against baseline measurements
  - Ensure smooth audio playback with no interruptions
  - _Requirements: 6.1-6.8, 8.1-8.8, 10.1-10.8_