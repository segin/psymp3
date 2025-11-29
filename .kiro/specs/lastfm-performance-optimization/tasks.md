# Implementation Plan

- [x] 1. Optimize MD5 Hash Function
  - [x] 1.1 Replace iostream-based hex formatting with lookup table implementation
    - Add static constexpr hex_chars lookup table
    - Use reserve(32) for output string
    - Convert bytes using bit shifting and table lookup
    - _Requirements: 1.1, 1.2_
  - [x] 1.2 Write property test for MD5 hash correctness
    - **Property 1: MD5 Hash Correctness**
    - **Validates: Requirements 1.1, 1.2**
  - [x] 1.3 Add cached password hash to avoid redundant computation
    - Store m_password_hash member variable
    - Compute once in readConfig() or on first use
    - Reuse in performHandshake() for auth token generation
    - _Requirements: 1.3_

- [x] 2. Optimize String Building for HTTP Requests
  - [x] 2.1 Refactor submitScrobble() to use string concatenation
    - Replace ostringstream with std::string
    - Use reserve(512) for POST data buffer
    - Use += operator for concatenation
    - _Requirements: 2.1, 2.4_
  - [x] 2.2 Refactor setNowPlaying() to use string concatenation
    - Same pattern as submitScrobble()
    - _Requirements: 2.1, 2.4_
  - [x] 2.3 Refactor performHandshake() URL building
    - Use string concatenation for URL construction
    - _Requirements: 2.3_
  - [x] 2.4 Write property test for URL encoding round-trip
    - **Property 2: URL Encoding Round-Trip**
    - **Validates: Requirements 2.2**

- [x] 3. Checkpoint - Verify optimizations compile and basic functionality works
  - Ensure all tests pass, ask the user if questions arise.

- [x] 4. Implement Lazy Debug Logging
  - [x] 4.1 Add DEBUG_LOG_LAZY macro to debug.h
    - Macro checks isChannelEnabled() before evaluating arguments
    - Use do-while(0) pattern for safe macro expansion
    - _Requirements: 3.1, 3.3_
  - [x] 4.2 Replace Debug::log calls in LastFM.cpp with DEBUG_LOG_LAZY
    - Update all log calls to use new macro
    - Preserve existing log messages and channels
    - _Requirements: 3.4_
  - [x] 4.3 Write property test for lazy evaluation
    - **Property 3: Debug Logging Lazy Evaluation**
    - **Validates: Requirements 3.1, 3.3**

- [x] 5. Optimize Submission Thread Behavior
  - [x] 5.1 Add exponential backoff state and logic
    - Add m_backoff_seconds member variable
    - Add resetBackoff() and increaseBackoff() methods
    - Implement backoff in submissionThreadLoop()
    - _Requirements: 4.3_
  - [x] 5.2 Verify condition variable blocking behavior
    - Review wait() predicate to ensure no spurious wakeups cause spinning
    - Add backoff wait after failed submissions
    - _Requirements: 4.1, 4.4_
  - [x] 5.3 Write property test for exponential backoff progression
    - **Property 5: Exponential Backoff Progression**
    - **Validates: Requirements 4.3**
  - [x] 5.4 Write property test for scrobble batching
    - **Property 4: Scrobble Batching Correctness**
    - **Validates: Requirements 4.2**

- [x] 6. Checkpoint - Verify thread behavior and backoff logic
  - Ensure all tests pass, ask the user if questions arise.

- [x] 7. Optimize Memory Allocations
  - [x] 7.1 Add move semantics to Scrobble queue operations
    - Use std::move when pushing to queue
    - Use emplace where possible
    - _Requirements: 5.1_
  - [x] 7.2 Optimize XML serialization buffer reuse
    - Pre-allocate string buffers in saveScrobbles()
    - Reuse buffers across iterations
    - _Requirements: 5.2_
  - [x] 7.3 Write property test for scrobble cache round-trip
    - **Property 6: Scrobble Cache Round-Trip**
    - **Validates: Requirements 5.2, 6.3, 6.4**

- [x] 8. Verify API Compatibility
  - [x] 8.1 Test configuration file backward compatibility
    - Verify existing config files parse correctly
    - Verify written config files maintain format
    - _Requirements: 6.1, 6.2_
  - [x] 8.2 Write property test for configuration round-trip
    - **Property 7: Configuration Round-Trip**
    - **Validates: Requirements 6.2**

- [ ] 9. Thread Safety Verification
  - [ ] 9.1 Review and document lock acquisition order
    - Ensure public/private lock pattern is followed
    - Add _unlocked variants where missing
    - _Requirements: 7.2_
  - [ ] 9.2 Verify graceful shutdown behavior
    - Test that pending scrobbles are saved on shutdown
    - _Requirements: 7.3_
  - [ ] 9.3 Write property test for thread-safe queue operations
    - **Property 8: Thread-Safe Queue Operations**
    - **Validates: Requirements 7.1, 7.4**

- [ ] 10. Final Checkpoint - Verify all optimizations and tests
  - Ensure all tests pass, ask the user if questions arise.
