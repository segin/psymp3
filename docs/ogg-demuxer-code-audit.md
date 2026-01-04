# Ogg Demuxer Code Audit Report

**Date**: 2026-01-02  
**Auditor**: AI Code Analysis  
**Scope**: Complete audit of Ogg demuxer implementation against RFC 3533, codec-specific RFCs, and `libogg` API patterns

## Executive Summary

This audit examined the PsyMP3 Ogg demuxer implementation across all components: `OggSyncManager`, `OggStreamManager`, `OggDemuxer`, `OggSeekingEngine`, and codec header parsers (Vorbis, Opus, FLAC, Speex). The analysis identified **1 critical memory safety issue**, **2 high-priority logic errors**, and several medium/low-priority compliance and robustness concerns.

## Audit Scope

### Files Analyzed
- [`src/demuxer/ogg/OggSyncManager.cpp`](file:///home/segin/psymp3/src/demuxer/ogg/OggSyncManager.cpp)
- [`src/demuxer/ogg/OggStreamManager.cpp`](file:///home/segin/psymp3/src/demuxer/ogg/OggStreamManager.cpp)
- [`src/demuxer/ogg/OggDemuxer.cpp`](file:///home/segin/psymp3/src/demuxer/ogg/OggDemuxer.cpp)
- [`src/demuxer/ogg/OggSeekingEngine.cpp`](file:///home/segin/psymp3/src/demuxer/ogg/OggSeekingEngine.cpp)
- [`src/demuxer/ogg/VorbisHeaderParser.cpp`](file:///home/segin/psymp3/src/demuxer/ogg/VorbisHeaderParser.cpp)
- [`src/demuxer/ogg/OpusHeaderParser.cpp`](file:///home/segin/psymp3/src/demuxer/ogg/OpusHeaderParser.cpp)
- [`src/demuxer/ogg/FLACHeaderParser.cpp`](file:///home/segin/psymp3/src/demuxer/ogg/FLACHeaderParser.cpp)
- [`src/demuxer/ogg/SpeexHeaderParser.cpp`](file:///home/segin/psymp3/src/demuxer/ogg/SpeexHeaderParser.cpp)
- [`src/demuxer/ogg/CodecHeaderParser.cpp`](file:///home/segin/psymp3/src/demuxer/ogg/CodecHeaderParser.cpp)

### Standards Referenced
- **RFC 3533**: The Ogg Encapsulation Format Version 0
- **RFC 7845**: Ogg Encapsulation for the Opus Audio Codec
- **RFC 9639 Section 10.1**: FLAC-in-Ogg encapsulation
- **Vorbis I Specification**: Vorbis codec header and packet handling
- **libogg API Documentation**: Reference implementation patterns

---

## 1. Critical Issues (Memory Safety)

### Issue 1.1: Dangerous Page Pointer Lifetime in `OggSyncManager`

**Location**: [`OggSyncManager.cpp:95-128`](file:///home/segin/psymp3/src/demuxer/ogg/OggSyncManager.cpp#L95-L128)

**Severity**: 🔴 **CRITICAL** - Risk of use-after-free and memory corruption

#### Problem Description

The `getPrevPage()` and `getPrevPageSerial()` functions perform shallow copies of `ogg_page` structures:

```cpp
// Line 117 in getPrevPage()
*page = temp_page; // Shallow copy of struct
```

The `ogg_page` struct contains raw pointers (`header` and `body`) that point directly into the internal buffer managed by `ogg_sync_state`. According to the `libogg` API contract, these pointers are **only valid until the next call** to:
- `ogg_sync_buffer()`
- `ogg_sync_wrote()`
- `ogg_sync_reset()`
- `ogg_sync_clear()`

#### Why This is Critical

1. **Immediate Invalidation**: The very next line after the shallow copy often calls `reset()` or seeks to a different position, which invalidates the pointers.

2. **Use-After-Free**: When the caller attempts to access `page->header` or `page->body`, they are dereferencing freed/reused memory.

3. **Seeking Context**: These functions are specifically used for seeking operations where the sync state is guaranteed to be modified immediately after.

#### Evidence from Code

```cpp
int OggSyncManager::getPrevPage(ogg_page* page) {
    // ... scanning logic ...
    if (found_any) {
        return 1;  // Caller now has page with invalid pointers
    }
}
// Caller will likely call reset() or seek() next, invalidating the page
```

#### RFC 3533 Compliance

RFC 3533 does not mandate specific memory management, but the implementation violates the `libogg` API contract which is the de facto standard for Ogg handling.

#### Recommended Fix

**Option 1**: Deep copy the page data
```cpp
struct SafeOggPage {
    std::vector<uint8_t> header_data;
    std::vector<uint8_t> body_data;
    ogg_page page;  // Pointers redirected to our buffers
};
```

**Option 2**: Refactor to process pages immediately within the lock
```cpp
// Instead of returning a page, return the file offset where it was found
int64_t OggSyncManager::findPrevPageOffset(long serial = -1);
```

---

## 2. High Priority Issues (Logic Errors)

### Issue 2.1: Broken Seeking in Multiplexed Ogg Files

**Location**: [`OggSeekingEngine.cpp:111-145`](file:///home/segin/psymp3/src/demuxer/ogg/OggSeekingEngine.cpp#L111-L145)

**Severity**: 🟠 **HIGH** - Seeking will fail or produce incorrect results in multiplexed streams

#### Problem Description

The `bisectForward()` function lacks a serial number check when comparing granule positions:

```cpp
// Line 124-137
if (m_sync.getNextPage(&page) != 1) {
    // ...
}

int64_t gp = ogg_page_granulepos(&page);
if (gp < 0) {
    // ...
}

if (gp < target_granule) {  // BUG: No serial number check!
    begin = mid;
} else {
    end = mid;
}
```

#### Why This is High Priority

1. **Multiplexed Streams**: RFC 3533 explicitly supports "grouping" (concurrent multiplexing) where pages from different logical streams are interleaved.

2. **Invalid Comparison**: Comparing the granule position of stream A against a target granule for stream B is mathematically meaningless. Granule positions are codec-specific and stream-specific.

3. **Bisection Failure**: The algorithm will converge on the wrong file offset or enter an infinite loop when it encounters pages from other streams.

#### RFC 3533 Compliance

RFC 3533 Section 4 states:
> "Pages of all logical bitstreams are concurrently interleaved, but they need not be in a regular order"

The seeking implementation must check `ogg_page_serialno(&page)` and skip pages that don't match the target stream.

#### Recommended Fix

```cpp
bool OggSeekingEngine::bisectForward(int64_t target_granule, int64_t begin, int64_t end) {
    const int MAX_ITERATIONS = 50;
    int iterations = 0;
    int target_serial = m_stream.getSerialNumber();  // Add this
    
    while (end - begin > 8192 && iterations < MAX_ITERATIONS) {
        int64_t mid = begin + (end - begin) / 2;
        m_sync.seek(mid);
        
        ogg_page page;
        // Find the first page for our target stream
        bool found_target_stream = false;
        while (m_sync.getNextPage(&page) == 1) {
            if (ogg_page_serialno(&page) == target_serial) {
                found_target_stream = true;
                break;
            }
        }
        
        if (!found_target_stream) {
            begin = mid + 1;
            iterations++;
            continue;
        }
        
        int64_t gp = ogg_page_granulepos(&page);
        // ... rest of logic
    }
}
```

### Issue 2.2: Ignored Data Gaps in Stream Layer

**Location**: [`OggStreamManager.cpp:33-40`](file:///home/segin/psymp3/src/demuxer/ogg/OggStreamManager.cpp#L33-L40)

**Severity**: 🟠 **HIGH** - Silent data loss and potential A/V desync

#### Problem Description

The `getPacket()` function only checks for success (`1`) from `ogg_stream_packetout()`:

```cpp
int OggStreamManager::getPacket(ogg_packet* packet) {
    if (!m_initialized) return -1;
    int result = ogg_stream_packetout(&m_stream_state, packet);
    if (result == 1 && packet->granulepos >= 0) {
        m_last_granule = packet->granulepos;
    }
    return result;  // Returns -1 to caller, but doesn't log it
}
```

According to `libogg` documentation, `ogg_stream_packetout()` returns:
- `1`: Packet successfully extracted
- `0`: Need more data
- **`-1`: Lost sync / data gap detected**

#### Why This is High Priority

1. **Silent Failures**: Data gaps are not logged or reported, making debugging impossible.

2. **A/V Desync**: In multiplexed audio/video files, a gap in one stream can cause the streams to drift out of sync.

3. **Corruption Propagation**: The caller has no way to know that the stream state is corrupted and may continue processing invalid data.

#### RFC 3533 Compliance

RFC 3533 Section 6 states:
> "Missing pages can be detected via sequence numbers"

The implementation should detect and handle these gaps appropriately.

#### Recommended Fix

```cpp
int OggStreamManager::getPacket(ogg_packet* packet) {
    if (!m_initialized) return -1;
    int result = ogg_stream_packetout(&m_stream_state, packet);
    
    if (result == -1) {
        Debug::log("ogg", "OggStreamManager: Data gap detected in stream ", m_serial_no);
        // Optionally: increment a gap counter, notify error handler, etc.
    }
    
    if (result == 1 && packet->granulepos >= 0) {
        m_last_granule = packet->granulepos;
    }
    return result;
}
```

---

## 3. Medium Priority Issues (Compliance & Robustness)

### Issue 3.1: Missing Framing Bit Validation in Vorbis Parser

**Location**: [`VorbisHeaderParser.cpp:99-157`](file:///home/segin/psymp3/src/demuxer/ogg/VorbisHeaderParser.cpp#L99-L157)

**Severity**: 🟡 **MEDIUM** - Spec non-compliance

#### Problem Description

The Vorbis I specification requires a framing bit check at the end of identification and comment headers. The current implementation only checks the framing bit for the identification header (line 132) but not for the comment or setup headers.

#### Vorbis I Specification Requirement

From the Vorbis I spec:
> "Each header packet begins with the same header fields... and ends with a framing bit of '1'"

#### Recommended Fix

Add framing bit validation to comment and setup header parsing:

```cpp
case 3: // Comment Header
    if (m_headers_count != 1) return false;
    
    if (packet->bytes > 7) {
        parseVorbisComment(data + 7, packet->bytes - 7);
    }
    
    // Check framing bit (last byte, bit 0)
    if (!(data[packet->bytes - 1] & 1)) return false;
    
    m_headers_count = 2;
    return true;
```

### Issue 3.2: Resource Exhaustion in Comment Parsing

**Location**: [`VorbisHeaderParser.cpp:21-97`](file:///home/segin/psymp3/src/demuxer/ogg/VorbisHeaderParser.cpp#L21-L97), [`OpusHeaderParser.cpp:22-97`](file:///home/segin/psymp3/src/demuxer/ogg/OpusHeaderParser.cpp#L22-L97)

**Severity**: 🟡 **MEDIUM** - Potential DoS vector

#### Problem Description

The comment parsers read `vendor_length` and `comment_count` from untrusted input without sanity checks:

```cpp
uint32_t vendor_length = data[offset] | (data[offset+1] << 8) | 
                         (data[offset+2] << 16) | (data[offset+3] << 24);
// No check if vendor_length > reasonable limit
```

A malicious file could specify a vendor length of 4GB, causing massive memory allocation.

#### Recommended Fix

```cpp
const uint32_t MAX_VENDOR_LENGTH = 1024 * 1024;  // 1MB
const uint32_t MAX_COMMENT_COUNT = 10000;
const uint32_t MAX_COMMENT_LENGTH = 1024 * 1024;  // 1MB per comment

if (vendor_length > MAX_VENDOR_LENGTH) {
    Debug::log("ogg", "VorbisHeaderParser: vendor_length too large: ", vendor_length);
    return false;
}
```

### Issue 3.3: Primary Stream Selection Logic

**Location**: [`OggDemuxer.cpp:92-99`](file:///home/segin/psymp3/src/demuxer/ogg/OggDemuxer.cpp#L92-L99)

**Severity**: 🟡 **MEDIUM** - Suboptimal user experience

#### Problem Description

The demuxer always selects the first BOS page as the primary stream:

```cpp
// Set primary stream to first audio stream
if (!m_has_primary_serial) {
    m_primary_serial = serial;
    m_has_primary_serial = true;
}
```

In files with multiple audio tracks (e.g., multilingual audio) or metadata-only streams, this may not be the desired default.

#### Recommended Enhancement

Implement a priority system:
1. First audio stream with language tag matching user preference
2. First audio stream marked as "default"
3. First audio stream (current behavior)

---

## 4. Low Priority Issues (Minor Improvements)

### Issue 4.1: Limited Search Range in Duration Calculation

**Location**: [`OggSeekingEngine.cpp:58-87`](file:///home/segin/psymp3/src/demuxer/ogg/OggSeekingEngine.cpp#L58-L87)

**Severity**: 🟢 **LOW** - Edge case handling

#### Problem Description

`getLastGranule()` only searches the last 64KB of the file:

```cpp
int64_t search_pos = file_size - 65536; // Last 64KB
```

While this is standard practice and works for 99.9% of files, exceptionally large pages or late-stream metadata could cause this to miss the true last granule.

#### Recommended Enhancement

If no valid granule is found in the last 64KB, expand the search window progressively (128KB, 256KB, etc.) up to a reasonable maximum.

### Issue 4.2: Const-Correctness Violations

**Location**: [`OggSyncManager.cpp:158-165`](file:///home/segin/psymp3/src/demuxer/ogg/OggSyncManager.cpp#L158-L165)

**Severity**: 🟢 **LOW** - Code quality

#### Problem Description

`getFileSize()` is marked `const` but uses `const_cast` to modify the IOHandler:

```cpp
int64_t OggSyncManager::getFileSize() const {
    long current = m_io_handler->tell();
    const_cast<PsyMP3::IO::IOHandler*>(m_io_handler)->seek(0, SEEK_END);
    // ...
}
```

This violates the const contract and can lead to subtle bugs.

#### Recommended Fix

Either:
1. Remove `const` from the function signature
2. Cache the file size in the constructor

---

## 5. Positive Findings

### Correct Implementations

1. **`ogg_sync_pageseek` Usage**: Correctly tracks logical offset accounting for skipped bytes during sync errors ([OggSyncManager.cpp:55-70](file:///home/segin/psymp3/src/demuxer/ogg/OggSyncManager.cpp#L55-L70))

2. **Granule Arithmetic**: Safe overflow/underflow handling in `safeGranuleAdd()` and `safeGranuleSub()` ([OggSeekingEngine.cpp:20-40](file:///home/segin/psymp3/src/demuxer/ogg/OggSeekingEngine.cpp#L20-L40))

3. **Codec Detection**: Proper signature validation for all supported codecs ([CodecHeaderParser.cpp:17-49](file:///home/segin/psymp3/src/demuxer/ogg/CodecHeaderParser.cpp#L17-L49))

4. **Thread Safety**: Consistent use of `std::recursive_mutex` in `OggDemuxer` with public/private lock pattern

---

## Summary of Recommendations

| Priority | Issue | Recommended Action |
|----------|-------|-------------------|
| 🔴 Critical | Page pointer lifetime | Implement deep copy or refactor to immediate processing |
| 🟠 High | Multiplexed seeking | Add serial number check in bisection loop |
| 🟠 High | Data gap handling | Log and handle `-1` return from `ogg_stream_packetout()` |
| 🟡 Medium | Framing bit validation | Add checks to Vorbis comment/setup headers |
| 🟡 Medium | Resource limits | Add sanity checks to comment parsing |
| 🟡 Medium | Primary stream selection | Implement priority-based selection |
| 🟢 Low | Duration search range | Progressive window expansion |
| 🟢 Low | Const-correctness | Remove `const_cast` or cache file size |

---

## Conclusion

The Ogg demuxer implementation demonstrates good understanding of the RFC 3533 specification and `libogg` API in most areas. However, the **critical memory safety issue** in `OggSyncManager` and the **broken seeking logic** for multiplexed streams require immediate attention. Addressing these issues will significantly improve the robustness and correctness of the Ogg demuxer.

The implementation would benefit from:
1. Comprehensive unit tests for multiplexed Ogg files
2. Fuzz testing with malformed/malicious Ogg files
3. Integration tests with reference implementations (libvorbisfile, libopusfile)
4. Memory sanitizer runs to catch use-after-free bugs

---

**Report Generated**: 2026-01-02  
**Next Review**: After critical issues are resolved
