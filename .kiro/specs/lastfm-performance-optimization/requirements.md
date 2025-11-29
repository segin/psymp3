# Requirements Document

## Introduction

This specification addresses critical performance issues identified in the Last.fm scrobbling system through callgrind profiling. The profiling revealed that approximately 25% of total CPU time during audio playback is consumed by iostream operations, string formatting, and MD5 hashing in the Last.fm submission thread. The root causes include:

1. **Excessive MD5 hash computations** - MD5 is computed per-scrobble using expensive iostream-based hex formatting
2. **Inefficient string building** - Heavy use of `std::ostringstream` for POST data construction
3. **Redundant debug logging** - Debug channel checks and string formatting occur even when logging is disabled
4. **Tight submission loop** - The submission thread may spin unnecessarily when scrobbles are queued

The goal is to reduce Last.fm-related CPU overhead by 90%+ while maintaining full functionality and API compatibility.

## Glossary

- **Scrobble**: A record of a track that has been played, submitted to Last.fm for tracking listening history
- **Last.fm Submissions API 1.2**: The legacy audioscrobbler protocol used for submitting scrobbles
- **MD5 Hash**: A cryptographic hash function used for authentication tokens in the Last.fm API
- **Session Key**: An authentication token obtained through handshake with Last.fm servers
- **Now Playing**: A real-time notification to Last.fm of the currently playing track
- **Submission Thread**: A background thread responsible for batch-submitting cached scrobbles
- **Handshake**: The authentication process to obtain session keys and submission URLs

## Requirements

### Requirement 1: Optimize MD5 Hash Computation

**User Story:** As a user, I want audio playback to remain smooth during scrobbling, so that Last.fm integration does not degrade my listening experience.

#### Acceptance Criteria

1. WHEN the system computes an MD5 hash THEN the system SHALL use a pre-allocated buffer for hex conversion instead of iostream formatting
2. WHEN the system formats MD5 hash output THEN the system SHALL use a lookup table for byte-to-hex conversion
3. WHEN the system computes authentication tokens THEN the system SHALL cache the password hash to avoid redundant computation
4. WHEN the system generates MusicBrainz ID fallbacks THEN the system SHALL compute the hash only once per scrobble submission

### Requirement 2: Optimize String Building for HTTP Requests

**User Story:** As a user, I want scrobble submissions to use minimal CPU resources, so that playback performance is not affected.

#### Acceptance Criteria

1. WHEN the system builds POST data for scrobble submission THEN the system SHALL use string concatenation with reserve() instead of ostringstream
2. WHEN the system URL-encodes strings THEN the system SHALL pre-calculate the output size and reserve capacity
3. WHEN the system builds handshake URLs THEN the system SHALL use efficient string concatenation patterns
4. WHEN the system constructs HTTP request bodies THEN the system SHALL avoid temporary string allocations where possible

### Requirement 3: Optimize Debug Logging

**User Story:** As a developer, I want debug logging to have zero overhead when disabled, so that release builds perform optimally.

#### Acceptance Criteria

1. WHEN debug logging is disabled for a channel THEN the system SHALL not evaluate log message arguments
2. WHEN the system checks if a debug channel is enabled THEN the system SHALL use O(1) lookup complexity
3. WHEN the system formats debug messages THEN the system SHALL defer string construction until after channel check
4. WHEN the lastfm debug channel is disabled THEN the system SHALL skip all string formatting operations in LastFM methods

### Requirement 4: Optimize Submission Thread Behavior

**User Story:** As a user, I want the scrobble submission thread to be efficient, so that it does not consume CPU when idle.

#### Acceptance Criteria

1. WHEN the submission thread has no work THEN the system SHALL block on condition variable without spinning
2. WHEN the submission thread processes scrobbles THEN the system SHALL batch multiple scrobbles efficiently
3. WHEN the submission thread encounters network errors THEN the system SHALL implement exponential backoff
4. WHEN the submission thread is idle THEN the system SHALL consume zero CPU cycles

### Requirement 5: Reduce Memory Allocations

**User Story:** As a user, I want the scrobbling system to minimize memory churn, so that garbage collection pauses do not affect playback.

#### Acceptance Criteria

1. WHEN the system creates Scrobble objects THEN the system SHALL use move semantics where possible
2. WHEN the system serializes scrobbles to XML THEN the system SHALL reuse string buffers
3. WHEN the system parses XML responses THEN the system SHALL avoid unnecessary string copies
4. WHEN the system manages the scrobble queue THEN the system SHALL minimize allocations during enqueue/dequeue

### Requirement 6: Maintain API Compatibility

**User Story:** As a user, I want my existing Last.fm configuration to continue working, so that I do not need to reconfigure after the update.

#### Acceptance Criteria

1. WHEN the system reads existing configuration files THEN the system SHALL parse all existing configuration keys
2. WHEN the system writes configuration files THEN the system SHALL maintain the existing file format
3. WHEN the system loads cached scrobbles THEN the system SHALL parse the existing XML cache format
4. WHEN the system saves cached scrobbles THEN the system SHALL write compatible XML format

### Requirement 7: Thread Safety

**User Story:** As a user, I want scrobbling to work reliably in a multi-threaded environment, so that no scrobbles are lost.

#### Acceptance Criteria

1. WHEN multiple threads access the scrobble queue THEN the system SHALL use proper synchronization
2. WHEN the submission thread accesses shared state THEN the system SHALL follow the public/private lock pattern
3. WHEN the system shuts down THEN the system SHALL gracefully stop the submission thread and save pending scrobbles
4. WHEN the system modifies session state THEN the system SHALL protect against race conditions
