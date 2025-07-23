# **DEMUXER ARCHITECTURE IMPLEMENTATION PLAN**

## **Implementation Tasks**

- [x] 1. Create Base Demuxer Interface and Data Structures
  - Implement Demuxer base class with pure virtual interface methods
  - Create StreamInfo and MediaChunk data structures with comprehensive fields
  - Add template helper methods for endianness handling (readLE, readBE, readFourCC)
  - Implement constructor accepting unique_ptr<IOHandler> parameter
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 1.10_

- [x] 1.1 Define Core Data Structures
  - Create StreamInfo structure with all required fields for audio/video/subtitle streams
  - Implement MediaChunk structure with data, timing, and metadata fields
  - Add proper constructors, copy/move semantics, and validation methods
  - Ensure structures are efficiently copyable and memory-aligned
  - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9, 2.10_

- [x] 1.2 Implement Base Demuxer Class
  - Create Demuxer base class with pure virtual interface methods
  - Add protected member variables for common demuxer state
  - Implement template helper methods for endianness conversion
  - Add proper virtual destructor and resource management
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 1.10_

- [x] 2. Implement DemuxerFactory System
  - Create DemuxerFactory class with static factory methods
  - Implement probeFormat() method for magic byte detection
  - Add createDemuxer() methods with and without file path hints
  - Create format signature database for automatic detection
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 7.10_

- [x] 2.1 Add Format Detection Logic
  - Implement magic byte detection for all supported container formats
  - Add format signature database with priority-based matching
  - Create probeFormat() method that examines file headers
  - Handle ambiguous cases and provide fallback detection methods
  - _Requirements: 7.1, 7.2, 7.3, 7.9, 7.10_

- [x] 2.2 Implement Demuxer Creation Logic
  - Create factory methods that return appropriate demuxer instances
  - Add error handling for unsupported formats and initialization failures
  - Implement file path hint processing for raw audio format detection
  - Add proper resource management and cleanup for failed creations
  - _Requirements: 7.4, 7.5, 7.6, 7.7, 7.8_

- [x] 3. Enhance Existing Demuxer Implementations
  - Update OggDemuxer to fully comply with new interface requirements
  - Enhance ChunkDemuxer with improved RIFF/AIFF support
  - Complete ISODemuxer implementation for MP4/M4A files
  - Improve RawAudioDemuxer with better format detection
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 3.8, 3.9, 3.10_

- [x] 3.1 Complete OggDemuxer Integration
  - Ensure OggDemuxer implements all required Demuxer interface methods
  - Add proper StreamInfo population with Ogg-specific metadata
  - Implement MediaChunk creation with granule position information
  - Add comprehensive error handling and recovery mechanisms
  - _Requirements: 3.1, 3.2, 3.3, 3.10_

- [x] 3.2 Enhance ChunkDemuxer Capabilities
  - Complete RIFF/WAV and AIFF format support in ChunkDemuxer
  - Add proper endianness handling for different chunk formats
  - Implement comprehensive metadata extraction from chunk headers
  - Add support for various audio codecs within chunk containers
  - _Requirements: 3.4, 3.5, 3.6, 3.10_

- [ ] 3.3 Finalize ISODemuxer Implementation
  - Complete MP4/M4A container parsing with full box hierarchy support
  - Implement sample table processing for accurate seeking
  - Add support for multiple tracks and codec detection
  - Create efficient seeking using sample-to-chunk mapping
  - _Requirements: 3.7, 3.8, 3.9, 3.10_

- [x] 4. Create MediaFactory System
  - Implement MediaFactory class with comprehensive format registration
  - Add ContentInfo structure for format detection results
  - Create MediaFormat structure for format capability description
  - Implement extensible format registration and detection system
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7, 8.8, 8.9, 8.10_

- [x] 4.1 Implement Format Registration System
  - Create MediaFormat structure with comprehensive format metadata
  - Add format registration methods for dynamic format addition
  - Implement priority-based format matching and selection
  - Create lookup tables for efficient extension and MIME type mapping
  - _Requirements: 8.1, 8.2, 8.3, 8.8_

- [x] 4.2 Add Content Detection Pipeline
  - Implement multi-stage content detection (extension, MIME, magic bytes)
  - Create ContentInfo structure with confidence scoring
  - Add content analysis methods for advanced format detection
  - Implement fallback detection strategies for ambiguous cases
  - _Requirements: 8.4, 8.5, 8.6, 8.7, 8.9, 8.10_

- [x] 4.3 Create Stream Factory System
  - Implement createStream() methods with automatic format detection
  - Add createStreamWithMimeType() for HTTP streaming with MIME hints
  - Create analyzeContent() methods for format analysis without stream creation
  - Add proper error handling and unsupported format reporting
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7_

- [x] 5. Implement DemuxedStream Bridge
  - Create DemuxedStream class implementing legacy Stream interface
  - Add demuxer and codec integration for complete audio pipeline
  - Implement buffering system for efficient chunk-to-PCM conversion
  - Create position tracking and seeking coordination between components
  - _Requirements: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6, 9.7, 9.8, 9.9, 9.10_

- [x] 5.1 Implement Stream Interface Bridge
  - Create DemuxedStream class inheriting from Stream base class
  - Implement all required Stream interface methods (getData, seekTo, eof, getLength)
  - Add proper initialization with demuxer and codec creation
  - Ensure backward compatibility with existing Stream-based code
  - _Requirements: 9.1, 9.2, 9.9, 9.10_

- [x] 5.2 Add Demuxer-Codec Integration
  - Implement automatic demuxer selection based on file format
  - Add codec selection and initialization based on stream information
  - Create MediaChunk to AudioFrame conversion pipeline
  - Handle multiple streams and stream switching capabilities
  - _Requirements: 9.3, 9.4, 9.5, 9.6_

- [x] 5.3 Implement Buffering and Position Tracking
  - Create efficient buffering system for MediaChunk and AudioFrame data
  - Add position tracking based on consumed samples rather than packet timestamps
  - Implement seeking coordination between demuxer and codec components
  - Handle EOF detection and stream completion properly
  - _Requirements: 9.7, 9.8, 9.9, 9.10_

- [ ] 6. Add Performance Optimizations
  - Implement efficient memory management with bounded buffers
  - Add I/O optimization for both local files and network streams
  - Create CPU-efficient parsing and processing algorithms
  - Implement scalable architecture for large files and multiple streams
  - _Requirements: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8_

- [ ] 6.1 Optimize Memory Usage
  - Implement streaming architecture that processes data incrementally
  - Add bounded buffer management to prevent memory exhaustion
  - Create efficient resource pooling and reuse strategies
  - Use smart pointers and RAII for automatic resource management
  - _Requirements: 10.1, 10.2, 10.6, 10.8_

- [ ] 6.2 Enhance I/O Performance
  - Optimize for sequential access patterns in media files
  - Add read-ahead buffering for network streams
  - Implement efficient seeking strategies for different container formats
  - Create cache management for frequently accessed metadata
  - _Requirements: 10.3, 10.4, 10.7, 10.8_

- [ ] 6.3 Improve CPU Efficiency
  - Optimize format detection with fast magic byte matching
  - Create efficient parsing algorithms for common container formats
  - Add threading support where beneficial for performance
  - Optimize common code paths and handle edge cases separately
  - _Requirements: 10.5, 10.6, 10.7, 10.8_

- [ ] 7. Implement Error Handling and Robustness
  - Add comprehensive error handling for all demuxer operations
  - Implement graceful degradation for corrupted or unusual files
  - Create recovery mechanisms for various error conditions
  - Add proper resource cleanup in all error paths
  - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7, 11.8_

- [ ] 7.1 Add Container-Level Error Handling
  - Handle format detection failures with appropriate fallbacks
  - Implement container parsing error recovery and continuation
  - Add I/O error propagation and handling throughout the system
  - Create memory allocation failure handling with proper cleanup
  - _Requirements: 11.1, 11.2, 11.3, 11.8_

- [ ] 7.2 Implement Runtime Error Recovery
  - Add corrupted data handling with section skipping and recovery
  - Implement seeking error handling with range clamping and approximation
  - Create stream error isolation to prevent affecting other streams
  - Add threading error handling with proper synchronization
  - _Requirements: 11.4, 11.5, 11.6, 11.7_

- [ ] 8. Add Thread Safety and Concurrency Support
  - Implement proper synchronization for shared demuxer state
  - Add thread-safe factory operations and format registration
  - Create safe concurrent access patterns for demuxer instances
  - Handle multi-threaded cleanup and resource management
  - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8_

- [ ] 8.1 Implement Demuxer Thread Safety
  - Add synchronization for shared state in demuxer instances
  - Implement thread-safe I/O operations through IOHandler coordination
  - Create atomic operations for position and state tracking
  - Handle concurrent seeking and reading operations safely
  - _Requirements: 12.1, 12.2, 12.3, 12.7_

- [ ] 8.2 Add Factory Thread Safety
  - Implement thread-safe format registration and lookup operations
  - Add proper synchronization for factory method calls
  - Create thread-safe error state propagation across components
  - Handle concurrent factory operations and resource sharing
  - _Requirements: 12.4, 12.5, 12.6, 12.8_

- [ ] 9. Create Extensibility and Plugin Support
  - Implement dynamic format registration system for plugins
  - Add custom demuxer and content detector support
  - Create extensible metadata handling for format-specific information
  - Design stable ABI for external plugin modules
  - _Requirements: 13.1, 13.2, 13.3, 13.4, 13.5, 13.6, 13.7, 13.8, 13.9, 13.10_

- [ ] 9.1 Implement Plugin Architecture
  - Create dynamic format registration system for runtime plugin loading
  - Add custom demuxer implementation support through base interface
  - Implement pluggable content detection algorithms
  - Design stable ABI for external format support modules
  - _Requirements: 13.1, 13.2, 13.3, 13.8_

- [ ] 9.2 Add Extensibility Features
  - Create custom stream factory function support
  - Add extensible IOHandler implementation registration
  - Implement format-specific metadata extension mechanisms
  - Create runtime configuration system for demuxer behavior
  - _Requirements: 13.4, 13.5, 13.6, 13.7, 13.9, 13.10_

- [ ] 10. Ensure Integration and API Consistency
  - Complete integration with IOHandler subsystem
  - Add comprehensive PsyMP3 error reporting and logging integration
  - Implement URI parsing and handling integration
  - Ensure consistent API patterns across all demuxer components
  - _Requirements: 14.1, 14.2, 14.3, 14.4, 14.5, 14.6, 14.7, 14.8, 14.9, 14.10_

- [ ] 10.1 Complete IOHandler Integration
  - Ensure all demuxers use IOHandler interface exclusively for I/O operations
  - Test integration with both FileIOHandler and HTTPIOHandler implementations
  - Add proper error propagation from IOHandler to demuxer layers
  - Validate large file support and network streaming capabilities
  - _Requirements: 14.1, 14.4, 14.7_

- [ ] 10.2 Add PsyMP3 System Integration
  - Integrate with PsyMP3 Debug logging system using appropriate categories
  - Use PsyMP3 exception hierarchy for consistent error reporting
  - Add integration with URI parsing and handling components
  - Ensure compatibility with TagLib::String parameters where needed
  - _Requirements: 14.2, 14.3, 14.5, 14.6_

- [ ] 10.3 Ensure API Consistency
  - Validate consistent method signatures and return types across demuxers
  - Add comprehensive parameter validation and error checking
  - Implement consistent resource management patterns throughout
  - Create uniform configuration and settings handling
  - _Requirements: 14.8, 14.9, 14.10_

- [ ] 11. Create Comprehensive Testing Suite
  - Implement unit tests for all major demuxer architecture components
  - Add integration tests with various file formats and I/O sources
  - Create performance benchmarks and regression tests
  - Test error handling and recovery scenarios thoroughly
  - _Requirements: All requirements validation_

- [ ] 11.1 Implement Unit Tests
  - Test base Demuxer interface and data structure functionality
  - Verify DemuxerFactory format detection and demuxer creation
  - Test MediaFactory registration and content detection systems
  - Validate DemuxedStream bridge functionality and compatibility
  - _Requirements: 1.1-1.10, 7.1-7.10, 8.1-8.10, 9.1-9.10_

- [ ] 11.2 Add Integration Tests
  - Test demuxer implementations with various container formats
  - Verify IOHandler integration with different I/O sources
  - Test multi-threaded scenarios and concurrent access patterns
  - Validate error handling and recovery across component boundaries
  - _Requirements: 3.1-3.10, 10.1-10.8, 11.1-11.8, 12.1-12.8_

- [ ] 11.3 Create Performance and Regression Tests
  - Benchmark demuxer performance with large files and network streams
  - Test memory usage and resource management under various conditions
  - Create regression tests for previously fixed issues and edge cases
  - Validate scalability with multiple concurrent streams and operations
  - _Requirements: 10.1-10.8, 11.1-11.8, 13.1-13.10, 14.1-14.10_

- [ ] 12. Documentation and Code Quality
  - Add comprehensive inline documentation for all public interfaces
  - Create developer documentation for extending the demuxer architecture
  - Document integration patterns and best practices
  - Ensure code follows PsyMP3 style guidelines and conventions
  - _Requirements: 14.8, 14.9, 14.10_

- [ ] 12.1 Create API Documentation
  - Document all public classes, methods, and data structures
  - Add usage examples and integration patterns
  - Explain error handling and recovery mechanisms
  - Document thread safety considerations and limitations
  - _Requirements: 14.8, 14.9, 14.10_

- [ ] 12.2 Add Developer Guide
  - Create guide for implementing new demuxer types
  - Document plugin development and format registration
  - Explain architecture design decisions and trade-offs
  - Provide troubleshooting guide for common integration issues
  - _Requirements: 13.1-13.10, 14.1-14.10_

- [ ] 13. Validate Backward Compatibility
  - Ensure existing PsyMP3 functionality continues to work unchanged
  - Test DemuxedStream bridge with all current audio file types
  - Validate performance meets or exceeds current implementation
  - Verify metadata extraction and seeking behavior consistency
  - _Requirements: 9.1-9.10, 14.1-14.10_

- [ ] 13.1 Test Legacy Compatibility
  - Verify all currently supported file formats continue to work
  - Test existing Stream interface usage patterns through DemuxedStream
  - Validate metadata extraction matches current implementation results
  - Ensure seeking accuracy and performance meet current standards
  - _Requirements: 9.1, 9.2, 9.9, 9.10_

- [ ] 13.2 Performance Validation
  - Benchmark new architecture against current implementation
  - Measure memory usage and ensure reasonable resource consumption
  - Test with large files and various network conditions
  - Validate that new features don't impact existing performance
  - _Requirements: 10.1-10.8, 14.1-14.10_