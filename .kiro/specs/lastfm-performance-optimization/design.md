# Design Document: Last.fm Performance Optimization

## Overview

This design addresses critical performance issues in the Last.fm scrobbling system identified through callgrind profiling. The profiling revealed that ~25% of CPU time during audio playback was consumed by iostream operations, string formatting, and MD5 hashing in the Last.fm code paths.

### Key Optimizations

1. **MD5 Hash Function** - Replace iostream-based hex formatting with lookup table
2. **String Building** - Use `std::string::reserve()` and `+=` instead of `std::ostringstream`
3. **Debug Logging** - Implement lazy evaluation macros to avoid argument evaluation when disabled
4. **Submission Thread** - Add exponential backoff and ensure proper blocking behavior

## Architecture

The optimized Last.fm system maintains the same external interface while replacing internal implementations with more efficient alternatives.

```
┌─────────────────────────────────────────────────────────────┐
│                      LastFM Class                           │
├─────────────────────────────────────────────────────────────┤
│  Public API (unchanged)                                     │
│  ├── setNowPlaying(track)                                   │
│  ├── unsetNowPlaying()                                      │
│  ├── scrobbleTrack(track)                                   │
│  ├── getCachedScrobbleCount()                               │
│  └── forceSubmission()                                      │
├─────────────────────────────────────────────────────────────┤
│  Optimized Internals                                        │
│  ├── md5Hash() - Lookup table hex conversion                │
│  ├── buildPostData() - String reserve + concatenation       │
│  ├── submissionThreadLoop() - Exponential backoff           │
│  └── Lazy debug logging macros                              │
└─────────────────────────────────────────────────────────────┘
```

## Components and Interfaces

### 1. Optimized MD5 Hash Function

Replace the current iostream-based implementation:

```cpp
// BEFORE (slow - uses ostringstream)
std::string LastFM::md5Hash(const std::string& input) {
    // ... compute hash ...
    std::ostringstream hexHash;
    for (unsigned int i = 0; i < hash_len; i++) {
        hexHash << std::hex << std::setw(2) << std::setfill('0') 
                << static_cast<unsigned int>(hash[i]);
    }
    return hexHash.str();
}

// AFTER (fast - uses lookup table)
std::string LastFM::md5Hash(const std::string& input) {
    static constexpr char hex_chars[] = "0123456789abcdef";
    // ... compute hash ...
    std::string result;
    result.reserve(32);  // MD5 is always 16 bytes = 32 hex chars
    for (unsigned int i = 0; i < hash_len; i++) {
        result += hex_chars[(hash[i] >> 4) & 0x0F];
        result += hex_chars[hash[i] & 0x0F];
    }
    return result;
}
```

### 2. Optimized String Building

Replace ostringstream with direct string operations:

```cpp
// BEFORE (slow)
std::ostringstream postData;
postData << "s=" << HTTPClient::urlEncode(m_session_key);
postData << "&a[0]=" << HTTPClient::urlEncode(artist);
// ... more fields ...
return postData.str();

// AFTER (fast)
std::string postData;
postData.reserve(512);  // Pre-allocate reasonable size
postData += "s=";
postData += HTTPClient::urlEncode(m_session_key);
postData += "&a[0]=";
postData += HTTPClient::urlEncode(artist);
// ... more fields ...
return postData;
```

### 3. Lazy Debug Logging

Add a macro that prevents argument evaluation when logging is disabled:

```cpp
// In debug.h - add lazy evaluation macro
#define DEBUG_LOG_LAZY(channel, ...) \
    do { \
        if (Debug::isChannelEnabled(channel)) { \
            Debug::log(channel, __VA_ARGS__); \
        } \
    } while(0)

// Usage in LastFM.cpp
DEBUG_LOG_LAZY("lastfm", "Scrobble submitted: ", artist, " - ", title);
```

### 4. Exponential Backoff for Submission Thread

Add backoff state and logic:

```cpp
class LastFM {
private:
    // Backoff state
    int m_backoff_seconds = 0;
    static constexpr int MAX_BACKOFF_SECONDS = 3600;  // 1 hour max
    
    void resetBackoff() { m_backoff_seconds = 0; }
    void increaseBackoff() {
        if (m_backoff_seconds == 0) {
            m_backoff_seconds = 60;  // Start at 1 minute
        } else {
            m_backoff_seconds = std::min(m_backoff_seconds * 2, MAX_BACKOFF_SECONDS);
        }
    }
};
```

## Data Models

### Scrobble Class (unchanged)

The Scrobble class interface remains unchanged. Internal optimizations use move semantics where possible.

### Configuration State

```cpp
// Cached password hash to avoid recomputation
std::string m_password_hash;  // Computed once, reused for auth tokens
```

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: MD5 Hash Correctness

*For any* input string, the md5Hash function SHALL produce the same 32-character lowercase hexadecimal output as the reference OpenSSL MD5 implementation.

**Validates: Requirements 1.1, 1.2**

### Property 2: URL Encoding Round-Trip

*For any* string containing ASCII characters, URL encoding followed by URL decoding SHALL produce the original string.

**Validates: Requirements 2.2**

### Property 3: Debug Logging Lazy Evaluation

*For any* disabled debug channel, log statement arguments SHALL NOT be evaluated (no side effects from argument expressions).

**Validates: Requirements 3.1, 3.3**

### Property 4: Scrobble Batching Correctness

*For any* set of N scrobbles in the queue where N > batch_size, the submission thread SHALL submit exactly batch_size scrobbles per batch until fewer than batch_size remain.

**Validates: Requirements 4.2**

### Property 5: Exponential Backoff Progression

*For any* sequence of K consecutive network failures, the backoff delay SHALL be min(initial_delay * 2^(K-1), max_delay) seconds.

**Validates: Requirements 4.3**

### Property 6: Scrobble Cache Round-Trip

*For any* list of Scrobble objects, saving to XML cache and loading back SHALL produce an equivalent list of Scrobble objects.

**Validates: Requirements 5.2, 6.3, 6.4**

### Property 7: Configuration Round-Trip

*For any* valid configuration state (username, password, session_key, URLs), writing to config file and reading back SHALL produce the same configuration values.

**Validates: Requirements 6.2**

### Property 8: Thread-Safe Queue Operations

*For any* sequence of concurrent enqueue and dequeue operations from multiple threads, the queue SHALL maintain consistency (no lost items, no duplicates, no corruption).

**Validates: Requirements 7.1, 7.4**

## Error Handling

### Network Errors

- Implement exponential backoff starting at 60 seconds, doubling up to 1 hour max
- Reset backoff on successful submission
- Log errors only when debug channel is enabled

### Authentication Errors

- Clear session state on BADAUTH response
- Attempt re-handshake on next submission
- Limit handshake attempts to prevent infinite loops

### XML Parsing Errors

- Log parsing errors and skip malformed entries
- Continue processing remaining valid entries
- Preserve unparseable data in cache for manual recovery

## Testing Strategy

### Dual Testing Approach

This implementation uses both unit tests and property-based tests:

- **Unit tests** verify specific examples and edge cases
- **Property-based tests** verify universal properties across random inputs

### Property-Based Testing Framework

Use **RapidCheck** for C++ property-based testing, consistent with other PsyMP3 tests.

### Test Configuration

- Minimum 100 iterations per property test
- Each property test tagged with: `**Feature: lastfm-performance-optimization, Property {N}: {description}**`

### Unit Test Coverage

1. MD5 hash against known test vectors (RFC 1321)
2. URL encoding special characters
3. Configuration file parsing
4. XML cache format compatibility
5. Backoff timing calculations

### Property Test Coverage

1. MD5 hash correctness for random inputs
2. URL encoding round-trip for random strings
3. Scrobble cache round-trip for random scrobble lists
4. Configuration round-trip for random config values
5. Thread-safe queue operations under concurrent load

### Performance Benchmarks

Before/after comparison using callgrind:
- MD5 hash function: Target 10x improvement
- POST data building: Target 5x improvement
- Overall Last.fm CPU usage: Target 90% reduction
