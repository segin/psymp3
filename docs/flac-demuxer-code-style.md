# FLAC Demuxer Code Style Guide

## Overview

This document defines the coding standards and style guidelines for the FLAC demuxer implementation in PsyMP3. Following these guidelines ensures consistency with the existing codebase and maintainability.

## General Principles

### 1. Consistency with PsyMP3 Codebase

The FLAC demuxer follows established PsyMP3 patterns:

- **Header inclusion**: Only include `psymp3.h` in `.cpp` files
- **Naming conventions**: Match existing PsyMP3 class and method naming
- **Error handling**: Use PsyMP3 Debug logging system
- **Memory management**: Follow RAII principles and use smart pointers
- **Threading**: Implement mandatory public/private lock pattern

### 2. Code Organization

```cpp
// File structure for FLACDemuxer.cpp
#include "psymp3.h"  // Only include master header

// Constructor implementation
FLACDemuxer::FLACDemuxer(std::unique_ptr<IOHandler> handler)
    : Demuxer(std::move(handler))
{
    // Initialization code
}

// Public interface methods (with locking)
bool FLACDemuxer::parseContainer() {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return parseContainer_unlocked();
}

// Private implementation methods (assume locks held)
bool FLACDemuxer::parseContainer_unlocked() {
    // Actual implementation
}
```

## Naming Conventions

### Class and Type Names

```cpp
// Classes: PascalCase
class FLACDemuxer : public Demuxer {
    // ...
};

// Enums: PascalCase with descriptive prefix
enum class FLACMetadataType : uint8_t {
    STREAMINFO = 0,
    PADDING = 1,
    // ...
};

// Structs: PascalCase
struct FLACStreamInfo {
    uint16_t min_block_size;
    // ...
};
```

### Member Variables

```cpp
class FLACDemuxer : public Demuxer {
private:
    // Member variables: m_ prefix + snake_case
    bool m_container_parsed = false;
    uint64_t m_file_size = 0;
    std::mutex m_state_mutex;
    
    // Atomic members: m_ prefix + snake_case
    std::atomic<uint64_t> m_current_sample{0};
    std::atomic<bool> m_error_state{false};
    
    // Constants: ALL_CAPS with descriptive names
    static constexpr size_t MAX_SEEK_TABLE_ENTRIES = 10000;
    static constexpr size_t FRAME_BUFFER_SIZE = 64 * 1024;
};
```

### Method Names

```cpp
class FLACDemuxer : public Demuxer {
public:
    // Public methods: camelCase (following Demuxer interface)
    bool parseContainer() override;
    MediaChunk readChunk() override;
    bool seekTo(uint64_t timestamp_ms) override;
    
    // Additional public methods: camelCase
    uint64_t getCurrentSample() const;
    
private:
    // Private unlocked methods: camelCase + _unlocked suffix
    bool parseContainer_unlocked();
    MediaChunk readChunk_unlocked();
    bool seekTo_unlocked(uint64_t timestamp_ms);
    
    // Helper methods: camelCase (no _unlocked if not corresponding to public method)
    bool parseMetadataBlocks();
    bool findNextFrame(FLACFrame& frame);
    void initializeBuffers();
};
```

### Variable Names

```cpp
void FLACDemuxer::someMethod() {
    // Local variables: snake_case
    uint64_t target_sample = 0;
    bool parse_success = false;
    std::vector<uint8_t> frame_data;
    
    // Loop variables: single letter or descriptive
    for (size_t i = 0; i < count; ++i) {
        // ...
    }
    
    for (const auto& seek_point : m_seektable) {
        // ...
    }
}
```

## Code Formatting

### Indentation and Spacing

```cpp
// 4 spaces for indentation (no tabs)
class FLACDemuxer : public Demuxer {
public:
    bool parseContainer() override {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        
        if (!m_handler) {
            Debug::log(Debug::ERROR, "No IOHandler available");
            return false;
        }
        
        return parseContainer_unlocked();
    }
    
private:
    bool parseContainer_unlocked() {
        // Validate fLaC stream marker
        uint8_t marker[4];
        if (m_handler->read(marker, 4) != 4) {
            return false;
        }
        
        if (std::memcmp(marker, "fLaC", 4) != 0) {
            Debug::log(Debug::ERROR, "Invalid FLAC stream marker");
            return false;
        }
        
        return parseMetadataBlocks();
    }
};
```

### Braces and Line Breaks

```cpp
// Opening brace on same line for methods and control structures
if (condition) {
    // Code here
} else if (other_condition) {
    // Code here
} else {
    // Code here
}

// Opening brace on new line for classes and namespaces
class FLACDemuxer : public Demuxer
{
    // Alternative style - choose one and be consistent
};

// Preferred style (matches existing PsyMP3 code)
class FLACDemuxer : public Demuxer {
    // Code here
};
```

### Function Parameters

```cpp
// Short parameter lists: single line
bool seekTo(uint64_t timestamp_ms);

// Long parameter lists: one parameter per line, aligned
bool complexMethod(
    const std::string& filename,
    uint32_t sample_rate,
    uint8_t channels,
    const std::vector<uint8_t>& metadata
);

// Function calls: similar rules
auto result = complexFunction(
    parameter1,
    parameter2,
    parameter3
);
```

## Documentation Standards

### Header File Documentation

```cpp
/**
 * @brief Brief description of the class or method
 * 
 * Detailed description explaining:
 * - What the class/method does
 * - How it fits into the larger system
 * - Important usage notes or limitations
 * - Thread safety guarantees
 * 
 * @param parameter_name Description of parameter
 * @return Description of return value
 * @throws ExceptionType When this exception is thrown
 * 
 * @thread_safety Thread-safe/Not thread-safe with explanation
 * 
 * @example
 * ```cpp
 * // Usage example
 * auto demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
 * if (demuxer->parseContainer()) {
 *     // Success
 * }
 * ```
 */
class FLACDemuxer : public Demuxer {
    /**
     * @brief Parse FLAC container headers and identify streams
     * 
     * This method parses the FLAC container format, including the fLaC stream
     * marker and all metadata blocks. It extracts essential information needed
     * for playback and seeking operations.
     * 
     * The parsing process:
     * 1. Validates fLaC stream marker
     * 2. Parses all metadata blocks (STREAMINFO, SEEKTABLE, etc.)
     * 3. Locates the start of audio frame data
     * 4. Prepares internal state for streaming operations
     * 
     * @return true if container was successfully parsed, false on error
     * 
     * @thread_safety Thread-safe. Can be called concurrently with other methods.
     * 
     * @note This method must be called before any streaming operations.
     *       Calling it multiple times is safe but unnecessary.
     */
    bool parseContainer() override;
};
```

### Implementation File Documentation

```cpp
// Implementation files: Focus on complex algorithms and non-obvious code
bool FLACDemuxer::seekWithTable_unlocked(uint64_t target_sample) {
    // Binary search through seek table to find closest seek point
    // before target sample position
    size_t left = 0;
    size_t right = m_seektable.size();
    size_t best_index = 0;
    
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        
        if (m_seektable[mid].sample_number <= target_sample) {
            best_index = mid;
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    // Seek to the file position specified in the seek table
    const auto& seek_point = m_seektable[best_index];
    uint64_t file_position = m_audio_data_offset + seek_point.stream_offset;
    
    if (!m_handler->seek(file_position)) {
        Debug::log(Debug::ERROR, "Failed to seek to position from seek table");
        return false;
    }
    
    // Update position tracking
    m_current_offset = file_position;
    m_current_sample.store(seek_point.sample_number);
    
    // If we're not exactly at the target, parse frames forward
    if (seek_point.sample_number < target_sample) {
        return seekLinear_unlocked(target_sample);
    }
    
    return true;
}
```

### Inline Comments

```cpp
bool FLACDemuxer::parseFrameHeader_unlocked(FLACFrame& frame) {
    // Read frame header - minimum 4 bytes for sync code and basic info
    uint8_t header_buffer[16]; // Maximum possible header size
    if (m_handler->read(header_buffer, 4) != 4) {
        return false;
    }
    
    // Validate sync code (14 bits: 11111111111110xx)
    uint16_t sync = (header_buffer[0] << 8) | header_buffer[1];
    if ((sync & 0xFFFC) != 0xFFF8) {
        return false; // Invalid sync code
    }
    
    // Parse blocking strategy (1 bit)
    frame.variable_block_size = (header_buffer[1] & 0x01) != 0;
    
    // Parse block size (4 bits) - may need additional bytes
    uint8_t block_size_code = (header_buffer[2] >> 4) & 0x0F;
    frame.block_size = decodeBlockSize(block_size_code, header_buffer, 4);
    
    // Continue parsing other header fields...
    return true;
}
```

## Error Handling Patterns

### Consistent Error Reporting

```cpp
// Use PsyMP3 Debug logging system consistently
bool FLACDemuxer::parseStreamInfoBlock_unlocked(const FLACMetadataBlock& block) {
    if (block.length != 34) {
        Debug::log(Debug::ERROR, 
                   "Invalid STREAMINFO block size: " + std::to_string(block.length));
        return false;
    }
    
    std::vector<uint8_t> data(34);
    if (m_handler->read(data.data(), 34) != 34) {
        Debug::log(Debug::ERROR, "Failed to read STREAMINFO block data");
        return false;
    }
    
    // Parse STREAMINFO data...
    Debug::log(Debug::INFO, 
               "Parsed STREAMINFO: " + std::to_string(m_streaminfo.sample_rate) + 
               "Hz, " + std::to_string(m_streaminfo.channels) + " channels");
    
    return true;
}
```

### Exception Safety

```cpp
// Use RAII and proper exception handling
MediaChunk FLACDemuxer::readChunk_unlocked() {
    try {
        // Allocate frame buffer using RAII
        std::vector<uint8_t> frame_buffer;
        frame_buffer.reserve(FRAME_BUFFER_SIZE);
        
        FLACFrame frame;
        if (!findNextFrame_unlocked(frame)) {
            return MediaChunk{}; // Return empty chunk on failure
        }
        
        // Read frame data with proper error handling
        if (!readFrameData_unlocked(frame, frame_buffer)) {
            Debug::log(Debug::WARNING, "Failed to read frame data, skipping");
            return readChunk_unlocked(); // Try next frame
        }
        
        // Create MediaChunk with move semantics to avoid copying
        MediaChunk chunk;
        chunk.data = std::move(frame_buffer);
        chunk.timestamp_samples = frame.sample_offset;
        chunk.stream_id = 1;
        chunk.is_keyframe = true;
        
        return chunk;
        
    } catch (const std::exception& e) {
        Debug::log(Debug::ERROR, "Exception in readChunk: " + std::string(e.what()));
        setErrorState(true);
        return MediaChunk{};
    }
}
```

## Threading Patterns

### Public/Private Lock Pattern Implementation

```cpp
class FLACDemuxer : public Demuxer {
public:
    // Public methods: acquire locks and delegate to private methods
    bool seekTo(uint64_t timestamp_ms) override {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        return seekTo_unlocked(timestamp_ms);
    }
    
    MediaChunk readChunk() override {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        return readChunk_unlocked();
    }
    
    std::vector<StreamInfo> getStreams() const override {
        std::lock_guard<std::mutex> lock(m_metadata_mutex);
        return getStreams_unlocked();
    }
    
private:
    // Private methods: assume appropriate locks are held
    bool seekTo_unlocked(uint64_t timestamp_ms) {
        // Implementation assumes m_state_mutex is held
        uint64_t target_sample = msToSamples(timestamp_ms);
        
        // Safe to call other _unlocked methods
        if (m_seektable.empty()) {
            return seekBinary_unlocked(target_sample);
        } else {
            return seekWithTable_unlocked(target_sample);
        }
    }
    
    MediaChunk readChunk_unlocked() {
        // Implementation assumes m_state_mutex is held
        // Can safely access and modify state variables
        FLACFrame frame;
        if (!findNextFrame_unlocked(frame)) {
            return MediaChunk{};
        }
        
        // Update position tracking
        updatePositionTracking_unlocked(frame.sample_offset);
        
        return createMediaChunk_unlocked(frame);
    }
    
    std::vector<StreamInfo> getStreams_unlocked() const {
        // Implementation assumes m_metadata_mutex is held
        // Can safely access metadata
        if (!m_container_parsed) {
            return {};
        }
        
        StreamInfo info;
        populateStreamInfo_unlocked(info);
        return {info};
    }
};
```

### Lock Ordering Documentation

```cpp
class FLACDemuxer : public Demuxer {
private:
    // CRITICAL: Lock acquisition order (to prevent deadlocks):
    // 1. m_state_mutex (for container state and position tracking)
    // 2. m_metadata_mutex (for metadata access)
    // 3. IOHandler locks (managed by IOHandler implementation)
    mutable std::mutex m_state_mutex;
    mutable std::mutex m_metadata_mutex;
    
    // Example of correct lock ordering
    void complexOperation_unlocked() {
        // This method assumes m_state_mutex is already held
        
        // Need to access metadata - acquire second lock
        std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
        
        // Now safe to access both state and metadata
        updateStateFromMetadata_unlocked();
    }
    
    // NEVER do this - wrong lock order causes deadlocks
    void badMethod() {
        std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex); // WRONG ORDER!
        std::lock_guard<std::mutex> state_lock(m_state_mutex);       // DEADLOCK RISK!
    }
};
```

## Memory Management

### RAII and Smart Pointers

```cpp
class FLACDemuxer : public Demuxer {
private:
    // Use smart pointers for automatic memory management
    std::unique_ptr<IOHandler> m_handler; // Inherited from base class
    
    // Use containers for automatic cleanup
    std::vector<FLACSeekPoint> m_seektable;
    std::map<std::string, std::string> m_vorbis_comments;
    std::vector<FLACPicture> m_pictures;
    
    // Use RAII for temporary allocations
    void processLargeData() {
        // Automatic cleanup when scope exits
        std::vector<uint8_t> temp_buffer(LARGE_BUFFER_SIZE);
        
        // Process data...
        
        // Buffer automatically freed when method exits
    }
    
    // Proper resource management in constructors
    void initializeBuffers() {
        try {
            m_frame_buffer.reserve(FRAME_BUFFER_SIZE);
            m_sync_buffer.reserve(SYNC_SEARCH_BUFFER_SIZE);
            m_readahead_buffer.reserve(1024 * 1024); // 1MB for network streams
        } catch (const std::bad_alloc& e) {
            Debug::log(Debug::ERROR, "Failed to allocate buffers: " + std::string(e.what()));
            throw; // Re-throw to indicate construction failure
        }
    }
};
```

### Memory Usage Tracking

```cpp
class FLACDemuxer : public Demuxer {
private:
    size_t m_memory_usage_bytes = 0; // Track memory usage
    
    void updateMemoryUsage() {
        size_t usage = 0;
        
        // Calculate memory usage of major components
        usage += m_seektable.size() * sizeof(FLACSeekPoint);
        usage += m_frame_buffer.capacity();
        usage += m_sync_buffer.capacity();
        usage += m_readahead_buffer.capacity();
        
        // Add metadata memory usage
        for (const auto& [key, value] : m_vorbis_comments) {
            usage += key.size() + value.size();
        }
        
        for (const auto& picture : m_pictures) {
            usage += picture.cached_data.capacity();
        }
        
        m_memory_usage_bytes = usage;
        
        // Log if memory usage is high
        if (usage > 10 * 1024 * 1024) { // 10MB threshold
            Debug::log(Debug::WARNING, 
                       "High memory usage: " + std::to_string(usage / 1024) + " KB");
        }
    }
};
```

## Performance Considerations

### Efficient Algorithms

```cpp
// Use efficient algorithms for common operations
size_t FLACDemuxer::findSeekPointIndex_unlocked(uint64_t target_sample) const {
    // Binary search for O(log n) performance
    auto it = std::lower_bound(
        m_seektable.begin(), 
        m_seektable.end(),
        target_sample,
        [](const FLACSeekPoint& point, uint64_t sample) {
            return point.sample_number < sample;
        }
    );
    
    // Return index of closest seek point before target
    if (it != m_seektable.begin()) {
        --it;
    }
    
    return std::distance(m_seektable.begin(), it);
}
```

### Buffer Reuse

```cpp
class FLACDemuxer : public Demuxer {
private:
    // Reusable buffers to minimize allocations
    mutable std::vector<uint8_t> m_frame_buffer;
    mutable std::vector<uint8_t> m_sync_buffer;
    
    bool readFrameData_unlocked(const FLACFrame& frame, std::vector<uint8_t>& data) {
        // Reuse existing buffer capacity when possible
        if (m_frame_buffer.capacity() < frame.frame_size) {
            m_frame_buffer.reserve(frame.frame_size);
        }
        
        m_frame_buffer.resize(frame.frame_size);
        
        if (m_handler->read(m_frame_buffer.data(), frame.frame_size) != frame.frame_size) {
            return false;
        }
        
        // Move data to output (efficient, no copying)
        data = std::move(m_frame_buffer);
        
        // Prepare buffer for next use
        m_frame_buffer.clear();
        
        return true;
    }
};
```

## Testing Integration

### Unit Test Structure

```cpp
// Test file: tests/test_flac_demuxer_unit.cpp
#include "test_framework.h"

class FLACDemuxerTest : public TestCase {
private:
    std::unique_ptr<FLACDemuxer> createTestDemuxer(const std::string& test_file) {
        auto handler = std::make_unique<FileIOHandler>("tests/data/" + test_file);
        return std::make_unique<FLACDemuxer>(std::move(handler));
    }
    
public:
    void testContainerParsing() {
        auto demuxer = createTestDemuxer("test.flac");
        
        TEST_ASSERT(demuxer->parseContainer(), "Container parsing should succeed");
        
        auto streams = demuxer->getStreams();
        TEST_ASSERT(!streams.empty(), "Should have at least one stream");
        
        const auto& stream = streams[0];
        TEST_ASSERT(stream.sample_rate > 0, "Sample rate should be valid");
        TEST_ASSERT(stream.channels > 0, "Channel count should be valid");
    }
    
    void testSeeking() {
        auto demuxer = createTestDemuxer("test_with_seektable.flac");
        demuxer->parseContainer();
        
        uint64_t duration = demuxer->getDuration();
        uint64_t target = duration / 2; // Seek to middle
        
        TEST_ASSERT(demuxer->seekTo(target), "Seeking should succeed");
        
        uint64_t position = demuxer->getPosition();
        TEST_ASSERT(std::abs(static_cast<int64_t>(position - target)) < 1000,
                   "Seek position should be within 1 second of target");
    }
    
    void testThreadSafety() {
        auto demuxer = createTestDemuxer("test.flac");
        demuxer->parseContainer();
        
        std::atomic<int> error_count{0};
        std::vector<std::thread> threads;
        
        // Multiple threads performing different operations
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&demuxer, &error_count]() {
                try {
                    for (int j = 0; j < 100; ++j) {
                        auto chunk = demuxer->readChunk();
                        auto position = demuxer->getPosition();
                        std::this_thread::yield();
                    }
                } catch (...) {
                    error_count.fetch_add(1);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        TEST_ASSERT(error_count.load() == 0, "No thread safety errors should occur");
    }
};

// Register test
REGISTER_TEST(FLACDemuxerTest);
```

This code style guide ensures that the FLAC demuxer implementation maintains consistency with PsyMP3 conventions while following modern C++ best practices for maintainability, performance, and thread safety.