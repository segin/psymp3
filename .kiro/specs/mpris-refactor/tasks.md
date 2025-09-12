# Implementation Plan

- [x] 1. Create foundational types and RAII wrappers
  - Create MPRISTypes.h with enumerations, DBusVariant, and MPRISMetadata structures
  - Implement RAII deleters for D-Bus resources (DBusConnectionDeleter, DBusMessageDeleter)
  - Create Result<T> template class for error handling
  - Write unit tests for type conversions and RAII resource management
  - _Requirements: 3.1, 3.2, 3.3, 6.2_

- [x] 2. Implement D-Bus Connection Manager
  - Create DBusConnectionManager class with public/private lock pattern
  - Implement connection establishment with proper error handling
  - Add automatic reconnection logic with exponential backoff
  - Implement connection state monitoring and cleanup
  - Write unit tests with mock D-Bus connections
  - _Requirements: 1.1, 1.2, 3.1, 6.1_

- [x] 3. Create Property Manager for thread-safe state caching
  - Implement PropertyManager class following threading safety guidelines
  - Add metadata caching with thread-safe getters and setters
  - Implement playback status tracking with atomic updates
  - Add position tracking with timestamp-based interpolation
  - Create property-to-DBus conversion methods
  - Write unit tests for concurrent property access
  - _Requirements: 2.1, 2.2, 2.4, 4.1, 4.2_

- [x] 4. Implement Method Handler for D-Bus message processing
  - Create MethodHandler class with public/private lock pattern
  - Implement individual method handlers (Play, Pause, Stop, Next, Previous)
  - Add seeking method handlers (Seek, SetPosition) with proper validation
  - Implement property getter/setter handlers with error responses
  - Add comprehensive input validation and error handling
  - Write unit tests for each method handler with edge cases
  - _Requirements: 2.3, 4.3, 4.4, 6.2_

- [x] 5. Create Signal Emitter for asynchronous property notifications
  - Implement SignalEmitter class with worker thread for non-blocking operation
  - Add PropertiesChanged signal emission with batching support
  - Implement Seeked signal for position change notifications
  - Create signal queue with proper synchronization and bounded size
  - Add signal emission error handling and retry logic
  - Write unit tests for signal emission and threading behavior
  - _Requirements: 2.3, 2.4, 4.1_

- [x] 6. Implement main MPRIS Manager coordinator
  - Create MPRISManager class integrating all components
  - Implement initialization and shutdown with proper resource cleanup
  - Add public API methods following threading safety pattern
  - Implement component coordination and error propagation
  - Add connection loss handling and automatic recovery
  - Write integration tests with all components working together
  - _Requirements: 1.1, 1.3, 2.1, 2.2, 3.3_

- [x] 7. Create comprehensive error handling system
  - Implement MPRISError exception hierarchy with categorization
  - Add error logging with configurable detail levels
  - Create error recovery strategies for different failure types
  - Implement graceful degradation when D-Bus is unavailable
  - Add error reporting to Player for user notification
  - Write unit tests for error scenarios and recovery paths
  - _Requirements: 1.1, 6.1, 6.2, 6.3, 7.1, 7.2_

- [x] 8. Integrate new MPRIS system with Player class
  - Replace old MPRIS class usage in Player constructor and destructor
  - Update Player method calls to use new MPRISManager API
  - Ensure proper lock acquisition order between Player and MPRIS mutexes
  - Add MPRIS status updates for all Player state changes
  - Implement callback safety to prevent deadlocks
  - Write integration tests with real Player instance
  - _Requirements: 2.2, 2.3, 4.1, 4.3_
  
- [x] 9. Create mock framework for testing
  - Implement MockDBusConnection class for unit testing
  - Create MockPlayer class for MPRIS testing isolation
  - Add test utilities for threading safety validation
  - Implement test fixtures for different MPRIS scenarios
  - Create performance benchmarking tests for lock contention
  - Write stress tests for high-concurrency scenarios
  - _Requirements: 5.1, 5.2, 5.3, 5.4_

- [x] 10. Add comprehensive unit tests for all components
  - Write DBusConnectionManager tests with connection failure scenarios
  - Create PropertyManager tests with concurrent access patterns
  - Implement MethodHandler tests with malformed message handling
  - Add SignalEmitter tests with queue overflow and threading validation
  - Create MPRISManager tests with component failure scenarios
  - Write threading safety tests following project test framework
  - _Requirements: 5.1, 5.2, 5.4, 6.2_

- [x] 11. Implement logging and debugging infrastructure
  - Add configurable logging levels for MPRIS operations
  - Implement D-Bus message tracing for debugging
  - Create performance metrics collection for lock contention
  - Add connection state monitoring and reporting
  - Implement debug mode with detailed state dumping
  - Write tests for logging configuration and output
  - _Requirements: 7.1, 7.2, 7.3, 7.4_

- [x] 12. Create build system integration and conditional compilation
  - Update Makefile.am to include new MPRIS source files
  - Ensure proper conditional compilation with HAVE_DBUS guards
  - Add new header files to include directory structure
  - Update configure.ac if needed for additional D-Bus requirements
  - Verify clean builds with and without D-Bus support
  - Test build system changes on different platforms
  - _Requirements: 5.3_

- [x] 13. Perform integration testing and validation
  - Test MPRIS functionality with real D-Bus clients (media players, desktop environments)
  - Validate MPRIS specification compliance with standard test tools
  - Perform stress testing with multiple concurrent D-Bus clients
  - Test reconnection behavior with D-Bus service restarts
  - Validate memory usage and leak detection under load
  - Create automated integration test suite
  - _Requirements: 1.2, 4.1, 4.2, 4.3, 4.4, 6.1_

- [-] 14. Update documentation and cleanup legacy code
  - Remove old MPRIS implementation files (mpris.h, mpris.cpp)
  - Update Player class documentation for new MPRIS integration
  - Create developer documentation for MPRIS architecture
  - Add troubleshooting guide for MPRIS issues
  - Update build documentation for MPRIS dependencies
  - Create migration notes for any API changes
  - _Requirements: 7.1, 7.4_

- [ ] 15. Final validation and performance optimization
  - Profile lock contention and optimize critical paths
  - Validate threading safety with static analysis tools
  - Perform final integration testing with complete PsyMP3 application
  - Benchmark performance impact compared to old implementation
  - Verify no regressions in Player functionality
  - Create final test report and validation summary
  - _Requirements: 1.4, 2.4, 6.4_