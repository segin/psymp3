# Implementation Plan

- [x] 1. Create threading safety analysis and documentation framework
  - Create static analysis script to identify classes with mutex usage and public lock-acquiring methods
  - Document current lock acquisition patterns and identify potential deadlock scenarios
  - Create baseline threading safety test framework for validation
  - _Requirements: 1.1, 1.3, 5.1_

- [x] 2. Refactor Audio class threading safety
- [x] 2.1 Implement private unlocked methods for Audio class
  - Create `isFinished_unlocked()`, `setStream_unlocked()`, `resetBuffer_unlocked()`, and `getBufferLatencyMs_unlocked()` methods
  - Modify existing public methods to call private unlocked versions after acquiring appropriate locks
  - Ensure proper lock acquisition order between `m_stream_mutex` and `m_buffer_mutex`
  - _Requirements: 1.1, 1.2, 3.1_

- [x] 2.2 Update Audio class internal method calls to use unlocked versions
  - Modify `decoderThreadLoop()` to use unlocked methods when locks are already held
  - Update `callback()` function to use unlocked buffer access methods
  - Ensure `setStream()` uses unlocked methods for internal operations
  - _Requirements: 1.2, 3.3_

- [x] 2.3 Create comprehensive thread safety tests for Audio class
  - Write unit tests for concurrent access to Audio public methods
  - Create deadlock prevention tests that would fail with old implementation
  - Implement stress tests with multiple threads performing audio operations
  - _Requirements: 3.3, 5.4_

- [-] 3. Refactor IOHandler class threading safety
- [x] 3.1 Implement private unlocked methods for IOHandler base class
  - Create `read_unlocked()`, `seek_unlocked()`, `tell_unlocked()`, and `close_unlocked()` methods
  - Implement `getMemoryStats_unlocked()` and other static method unlocked versions
  - Modify public methods to acquire locks and call private unlocked implementations
  - _Requirements: 1.1, 1.2, 4.1_

- [x] 3.2 Update IOHandler memory management to use unlocked patterns
  - Modify `updateMemoryUsage()`, `checkMemoryLimits()`, and `performMemoryOptimization()` to have unlocked versions
  - Ensure memory tracking methods don't cause deadlocks when called from I/O operations
  - Update error handling methods to use proper lock patterns
  - _Requirements: 1.2, 3.1, 4.1_

- [x] 3.3 Refactor FileIOHandler and HTTPIOHandler derived classes
  - Update derived class implementations to use base class unlocked methods
  - Ensure derived classes follow the same public/private lock pattern
  - Modify any derived class specific locking to be compatible with base class pattern
  - _Requirements: 1.1, 1.2, 4.1_

- [x] 3.4 Create IOHandler subsystem thread safety tests
  - Write tests for concurrent file and HTTP I/O operations
  - Create tests that verify memory management doesn't cause deadlocks during I/O
  - Implement integration tests for IOHandler with other threaded components
  - _Requirements: 3.3, 5.4_

- [x] 4. Refactor MemoryPoolManager class threading safety
- [x] 4.1 Implement private unlocked methods for MemoryPoolManager
  - Create `allocateBuffer_unlocked()`, `releaseBuffer_unlocked()`, and `getMemoryStats_unlocked()` methods
  - Implement `optimizeMemoryUsage_unlocked()` and other management method unlocked versions
  - Modify public methods to acquire `m_mutex` and call private unlocked implementations
  - _Requirements: 1.1, 1.2, 4.2_

- [x] 4.2 Implement callback safety for MemoryPoolManager
  - Create `notifyPressureCallbacks_unlocked()` that can be called without holding internal locks
  - Implement callback queuing system to prevent reentrancy issues
  - Modify pressure callback registration/unregistration to be thread-safe
  - _Requirements: 1.2, 3.1, 4.2_

- [x] 4.3 Update MemoryPoolManager integration with MemoryTracker
  - Ensure MemoryTracker callbacks don't cause deadlocks in MemoryPoolManager
  - Modify callback handling to use unlocked methods where appropriate
  - Test integration between memory tracking and pool management under high concurrency
  - _Requirements: 1.2, 3.3, 4.2_

- [x] 4.4 Create MemoryPoolManager thread safety tests
  - Write tests for concurrent buffer allocation and release operations
  - Create tests that verify callback system doesn't cause deadlocks
  - Implement stress tests for memory pressure scenarios with multiple threads
  - _Requirements: 3.3, 5.4_

- [x] 5. Refactor Surface class SDL locking patterns
- [x] 5.1 Implement private unlocked methods for Surface class
  - Create unlocked versions of drawing methods: `put_pixel_unlocked()`, `hline_unlocked()`, `vline_unlocked()`
  - Implement `line_unlocked()`, `fill_unlocked()`, and other graphics operation unlocked methods
  - Modify public methods to handle SDL surface locking and call private unlocked implementations
  - _Requirements: 1.1, 1.2, 4.3_

- [x] 5.2 Update Surface class internal method calls
  - Modify complex drawing operations to use unlocked methods when SDL surface is already locked
  - Ensure proper SDL lock acquisition order and exception safety
  - Update any Surface methods that call other Surface methods to use unlocked versions
  - _Requirements: 1.2, 3.1_

- [x] 5.3 Create Surface class thread safety tests
  - Write tests for concurrent drawing operations on different surfaces
  - Create tests that verify SDL locking doesn't cause deadlocks
  - Implement performance tests to ensure graphics operations aren't significantly impacted
  - _Requirements: 3.3, 5.4_

- [x] 6. Update project steering documents with threading guidelines
- [x] 6.1 Create comprehensive threading safety guidelines document
  - Write detailed guidelines for implementing public/private lock patterns
  - Document lock acquisition order requirements and naming conventions
  - Create examples of correct and incorrect threading patterns
  - _Requirements: 2.1, 2.2, 2.3_

- [x] 6.2 Update development standards with threading requirements
  - Modify existing development standards to include threading safety requirements
  - Add code review checklist items for threading safety patterns
  - Include threading safety as a requirement for new code contributions
  - _Requirements: 2.1, 2.3, 5.2_

- [x] 6.3 Create static analysis rules for threading safety
  - Implement scripts or tools to detect public methods that acquire locks without private counterparts
  - Create build system integration to check for threading anti-patterns
  - Document how to use and maintain the threading safety analysis tools
  - _Requirements: 2.3, 5.1, 5.3_

- [ ] 7. Comprehensive integration testing and validation
- [x] 7.1 Create system-wide threading safety integration tests
  - Write tests that exercise multiple threaded components simultaneously
  - Create scenarios that test audio playback, I/O operations, and memory management concurrently
  - Implement long-running stress tests to detect race conditions and deadlocks
  - _Requirements: 3.3, 5.4_

- [x] 7.2 Performance regression testing for threading changes
  - Benchmark critical paths before and after threading safety refactoring
  - Ensure lock overhead doesn't significantly impact audio playback performance
  - Validate that I/O operations maintain acceptable performance under concurrent access
  - _Requirements: 5.4_

- [x] 7.3 Update build system to include threading safety validation
  - Integrate threading safety tests into the regular build process
  - Add threading safety checks to the continuous integration pipeline
  - Create documentation for maintaining threading safety as the codebase evolves
  - _Requirements: 2.3, 5.3_