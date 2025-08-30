# FLAC Demuxer Developer Guide

## Overview

This guide provides comprehensive information for developers working with the FLAC demuxer implementation in PsyMP3. It covers integration patterns, architectural decisions, thread safety considerations, and best practices for extending FLAC support.

## Architecture Overview

### Component Relationships

The FLAC demuxer operates within a multi-layered architecture:

```
Application Layer
    ↓
MediaFactory (format detection and demuxer creation)
    ↓
FLACDemuxer (container parsing and frame extraction)
    ↓
IOHandler (file/network I/O abstraction)
    ↓
FLACCodec (bitstream decoding - separate component)
```

### Separation of Concerns

The FLAC implementation is deliberately split into two distinct components:

1. **FLACDemuxer**: Handles FLAC container format parsing, metadata extraction, and frame boundary detection
2. **FLACCodec**: Handles FLAC bitstream decoding and audio sample generation

This separation enables:
- Clean architectural boundaries
- Independent testing and debugging
- Support for FLAC streams in other containers (future OGG FLAC support)
- Maintainable code with clear responsibilities

## Integration with FLACCodec

### Data Flow Between Components

```cpp
// FLACDemuxer extracts complete FLAC frames
MediaChunk chunk = demuxer->readChunk();

// MediaChunk contains:
// - Complete FLAC frame (sync + header + subframes + CRC)
// - Timestamp information (timestamp_samples)
// - Stream identification (stream_id = 0 for FLAC)
// - Frame properties (is_keyframe = true for all FLAC frames)

// FLACCodec processes the frame data
AudioSamples samples = codec->decode(chunk.data, chunk.size);
```

### Frame Data Format

The FLACDemuxer provides frames in the exact format expected by FLACCodec:

```
[FLAC Frame Structure]
┌─────────────────┬──────────────────┬─────────────────┬─────────┐
│ Sync Pattern    │ Frame Header     │ Subframe Data   │ CRC-16  │
│ (2 bytes)       │ (variable)       │ (variable)      │ (2 bytes)│
└─────────────────┴──────────────────┴─────────────────┴─────────┘
```

**Critical Integration Points:**

1. **Complete Frames**: FLACDemuxer always provides complete, valid FLAC frames
2. **Timestamp Accuracy**: Frame timestamps are calculated from sample positions
3. **Error Handling**: Corrupted frames are either skipped or marked for codec-level recovery
4. **Seeking Coordination**: Demuxer seeking requires codec reset for proper decoding

### Codec Reset Protocol

When seeking occurs, proper coordination is required:

```cpp
// Seeking workflow
bool FLACDemuxer::seekTo(uint64_t timestamp_ms) {
    // 1. Demuxer performs container-level seeking
    bool seek_success = performSeek_unlocked(timestamp_ms);
    
    // 2. Application must reset codec state
    // (This happens at the application layer, not in demuxer)
    
    return seek_success;
}

// Application-level coordination
void Player::seekTo(uint64_t timestamp_ms) {
    if (demuxer->seekTo(timestamp_ms)) {
        codec->reset();  // Clear codec internal state
        // Resume playback from new position
    }
}
```

## Container Parsing vs Bitstream Decoding

### Container Level (FLACDemuxer Responsibility)

**FLAC Container Structure:**
```
fLaC Stream Marker (4 bytes)
├── METADATA_BLOCK_STREAMINFO (mandatory)
├── METADATA_BLOCK_* (optional blocks)
└── Audio Data (FLAC frames)
```

**Container-Level Operations:**
- Stream marker validation (`fLaC`)
- Metadata block parsing and extraction
- Frame boundary detection and synchronization
- Seeking using container-level information (SEEKTABLE)
- Duration calculation from total samples
- Position tracking during playback

### Bitstream Level (FLACCodec Responsibility)

**FLAC Frame Internal Structure:**
```
Frame Header
├── Sync pattern (14 bits: 11111111111110xx)
├── Blocking strategy, block size, sample rate
├── Channel assignment, sample size
└── Frame/sample number, CRC-8

Subframe Data (per channel)
├── Subframe header (type, wasted bits, etc.)
├── Encoded audio samples
└── Entropy coding (Rice/residual coding)

Frame Footer
└── CRC-16 checksum
```

**Bitstream-Level Operations:**
- Frame header parsing and validation
- Subframe decoding (constant, verbatim, fixed, LPC)
- Entropy decoding (Rice codes, residual coding)
- Sample reconstruction and bit depth conversion
- Channel decorrelation (mid-side, left-side, right-side)
- Audio sample output generation

### Boundary Responsibilities

| Operation | Component | Rationale |
|-----------|-----------|-----------|
| Frame sync detection | FLACDemuxer | Container knows frame boundaries |
| Frame header parsing | Both | Demuxer for size, codec for decoding |
| Metadata extraction | FLACDemuxer | Container-level information |
| Sample decoding | FLACCodec | Bitstream-level operation |
| Seeking | FLACDemuxer | Uses container metadata (SEEKTABLE) |
| Error recovery | Both | Demuxer skips frames, codec handles corruption |

## Thread Safety Architecture

### Mandatory Public/Private Lock Pattern

All FLACDemuxer methods follow the established threading safety pattern:

```cpp
class FLACDemuxer : public Demuxer {
public:
    // Public methods acquire locks and call private implementations
    MediaChunk readChunk() override {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        return readChunk_unlocked();
    }
    
    bool seekTo(uint64_t timestamp_ms) override {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        return seekTo_unlocked(timestamp_ms);
    }
    
private:
    // Private methods assume locks are already held
    MediaChunk readChunk_unlocked();
    bool seekTo_unlocked(uint64_t timestamp_ms);
    
    // Internal calls use _unlocked versions to prevent deadlocks
    void internalMethod() {
        // CORRECT: Call unlocked version
        auto chunk = readChunk_unlocked();
        
        // INCORRECT: Would cause deadlock
        // auto chunk = readChunk();
    }
    
    mutable std::mutex m_state_mutex;
};
```

### Lock Acquisition Order

The FLACDemuxer follows the documented lock hierarchy:

```cpp
// Lock acquisition order (to prevent deadlocks):
// 1. IOHandler locks (if any)
// 2. FLACDemuxer::m_state_mutex
// 3. Internal data structure locks (if needed)
```

### Thread Safety Guarantees

**Safe Concurrent Operations:**
- Multiple threads can call `getStreams()` simultaneously
- Position queries (`getPosition()`, `getDuration()`) are thread-safe
- Metadata access is thread-safe after initial parsing

**Serialized Operations:**
- Frame reading (`readChunk()`) is serialized per demuxer instance
- Seeking operations are serialized and invalidate ongoing reads
- Container parsing is serialized during initialization

**Callback Safety:**
- No callbacks are invoked while holding internal locks
- All callback invocations occur after lock release

### Exception Safety

All lock acquisitions use RAII guards for exception safety:

```cpp
MediaChunk FLACDemuxer::readChunk() {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    try {
        return readChunk_unlocked();
    } catch (...) {
        // Lock automatically released by RAII guard
        throw;
    }
}
```

## Performance Considerations

### Frame Size Estimation Strategy

Based on real-world debugging, accurate frame size estimation is critical:

```cpp
uint32_t calculateFrameSize_unlocked(const FLACFrame& frame) {
    // Priority 1: Use STREAMINFO minimum frame size
    if (m_streaminfo.isValid() && m_streaminfo.min_frame_size > 0) {
        // For fixed block size streams, use minimum directly
        if (m_streaminfo.min_block_size == m_streaminfo.max_block_size) {
            Debug::log("flac", "[calculateFrameSize] Fixed block size, using min: ", 
                      m_streaminfo.min_frame_size, " bytes");
            return m_streaminfo.min_frame_size;
        }
        
        // Variable block size: conservative scaling
        uint32_t estimated = m_streaminfo.min_frame_size;
        // Apply minimal scaling based on block size ratio
        return estimated;
    }
    
    // Priority 2: Conservative fallback
    Debug::log("flac", "[calculateFrameSize] Using conservative fallback: 64 bytes");
    return 64;
}
```

**Key Performance Lessons:**
- **STREAMINFO minimum frame size** is the most accurate estimate
- **Avoid complex theoretical calculations** that often produce wrong results
- **Highly compressed streams** can have frames as small as 14 bytes
- **Conservative estimates** prevent excessive I/O operations

### I/O Optimization Patterns

```cpp
// Efficient frame boundary detection
bool findNextFrame_unlocked(FLACFrame& frame) {
    const size_t MAX_SEARCH_BYTES = 512;  // Limit search scope
    const size_t SEARCH_INCREMENT = 16;   // Use larger increments
    
    for (size_t offset = 0; offset < MAX_SEARCH_BYTES; offset += SEARCH_INCREMENT) {
        if (checkSyncPattern(offset)) {
            frame.file_offset = m_current_offset + offset;
            return true;
        }
    }
    
    // Fallback: use STREAMINFO-based positioning
    return useStreamInfoFallback_unlocked(frame);
}
```

### Memory Management

```cpp
// Efficient metadata storage
class FLACDemuxer {
private:
    // Store only essential metadata
    FLAC__StreamMetadata_StreamInfo m_streaminfo;
    std::vector<FLAC__StreamMetadata_SeekPoint> m_seektable;
    std::map<std::string, std::string> m_vorbis_comments;
    
    // Avoid storing large embedded images in memory
    struct PictureInfo {
        uint32_t type;
        std::string mime_type;
        uint64_t data_offset;  // File position, not data itself
        uint32_t data_size;
    };
    std::vector<PictureInfo> m_pictures;
};
```

## Extending FLAC Support

### Adding New Metadata Block Types

To support additional FLAC metadata blocks:

```cpp
// 1. Add new block type enum
enum FLACMetadataType {
    // ... existing types ...
    FLAC_METADATA_TYPE_NEW_BLOCK = 127  // Custom application block
};

// 2. Add parser method
bool FLACDemuxer::parseNewBlock_unlocked(const FLACMetadataBlock& block) {
    // Read block data
    std::vector<uint8_t> block_data(block.length);
    if (!m_io_handler->read(block_data.data(), block.length)) {
        return false;
    }
    
    // Parse block-specific data
    // ... implementation ...
    
    return true;
}

// 3. Integrate with metadata parsing loop
bool FLACDemuxer::parseMetadataBlocks_unlocked() {
    // ... existing parsing ...
    
    switch (block.type) {
        // ... existing cases ...
        case FLAC_METADATA_TYPE_NEW_BLOCK:
            if (!parseNewBlock_unlocked(block)) {
                Debug::log("flac", "Failed to parse new block type");
            }
            break;
    }
    
    // ... continue parsing ...
}
```

### Supporting FLAC in Other Containers

For future OGG FLAC support:

```cpp
// Abstract FLAC frame processing
class FLACFrameProcessor {
public:
    virtual ~FLACFrameProcessor() = default;
    virtual bool processFrame(const uint8_t* data, size_t size) = 0;
    virtual bool seekToSample(uint64_t sample) = 0;
};

// Native FLAC implementation
class NativeFLACDemuxer : public Demuxer, public FLACFrameProcessor {
    // Current implementation
};

// Future OGG FLAC implementation
class OggFLACDemuxer : public Demuxer, public FLACFrameProcessor {
public:
    bool processFrame(const uint8_t* data, size_t size) override {
        // Process FLAC frame within OGG packet
        return m_flac_processor.processFrame(data, size);
    }
    
private:
    FLACFrameProcessor m_flac_processor;  // Reuse FLAC logic
};
```

### Custom Seeking Strategies

To implement alternative seeking methods:

```cpp
class FLACDemuxer {
private:
    // Strategy pattern for seeking
    enum SeekStrategy {
        SEEK_STRATEGY_SEEKTABLE,
        SEEK_STRATEGY_BINARY_SEARCH,
        SEEK_STRATEGY_FRAME_INDEX,  // Future implementation
        SEEK_STRATEGY_LINEAR
    };
    
    bool seekTo_unlocked(uint64_t timestamp_ms) {
        uint64_t target_sample = timestampToSample(timestamp_ms);
        
        // Try strategies in order of preference
        if (m_seektable.size() > 0) {
            return seekWithTable_unlocked(target_sample);
        }
        
        if (m_frame_index.size() > 0) {  // Future feature
            return seekWithFrameIndex_unlocked(target_sample);
        }
        
        // Fallback to beginning for compressed streams
        return seekToBeginning_unlocked();
    }
    
    // Future frame indexing implementation
    bool buildFrameIndex_unlocked() {
        // Parse entire file once to build frame position index
        // Enables accurate seeking without SEEKTABLE
        return true;
    }
};
```

## Error Handling and Recovery

### Container-Level Error Recovery

```cpp
bool FLACDemuxer::parseContainer_unlocked() {
    try {
        // Validate stream marker
        if (!validateStreamMarker_unlocked()) {
            Debug::log("flac", "Invalid FLAC stream marker");
            return false;
        }
        
        // Parse metadata with recovery
        if (!parseMetadataBlocks_unlocked()) {
            Debug::log("flac", "Metadata parsing failed, attempting recovery");
            return attemptMetadataRecovery_unlocked();
        }
        
        return true;
    } catch (const std::exception& e) {
        Debug::log("flac", "Container parsing exception: ", e.what());
        return false;
    }
}

bool FLACDemuxer::attemptMetadataRecovery_unlocked() {
    // Try to derive STREAMINFO from first frame
    FLACFrame first_frame;
    if (findFirstFrame_unlocked(first_frame)) {
        return deriveStreamInfoFromFrame_unlocked(first_frame);
    }
    return false;
}
```

### Frame-Level Error Recovery

```cpp
MediaChunk FLACDemuxer::readChunk_unlocked() {
    while (!m_eof) {
        try {
            FLACFrame frame;
            if (findNextFrame_unlocked(frame)) {
                return readFrameData_unlocked(frame);
            }
        } catch (const FrameCorruptionException& e) {
            Debug::log("flac", "Frame corruption detected: ", e.what());
            
            // Skip corrupted frame and continue
            if (!skipCorruptedFrame_unlocked()) {
                break;  // Cannot recover
            }
            continue;  // Try next frame
        }
    }
    
    // Return empty chunk on EOF or unrecoverable error
    return MediaChunk();
}
```

## Testing and Validation

### Unit Testing Patterns

```cpp
// Test frame size estimation accuracy
TEST(FLACDemuxerTest, FrameSizeEstimation) {
    auto demuxer = createTestDemuxer("test_files/highly_compressed.flac");
    
    // Verify STREAMINFO-based estimation
    EXPECT_TRUE(demuxer->parseContainer());
    
    auto frame_size = demuxer->estimateFrameSize();
    EXPECT_GE(frame_size, 14);  // Minimum possible FLAC frame size
    EXPECT_LE(frame_size, 1024);  // Reasonable maximum for test file
}

// Test thread safety
TEST(FLACDemuxerTest, ConcurrentAccess) {
    auto demuxer = createTestDemuxer("test_files/standard.flac");
    EXPECT_TRUE(demuxer->parseContainer());
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // Multiple threads reading chunks
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&demuxer, &success_count]() {
            for (int j = 0; j < 10; ++j) {
                auto chunk = demuxer->readChunk();
                if (!chunk.data.empty()) {
                    success_count++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_GT(success_count.load(), 0);
}
```

### Integration Testing

```cpp
// Test demuxer-codec integration
TEST(FLACIntegrationTest, DemuxerCodecCoordination) {
    auto demuxer = std::make_unique<FLACDemuxer>(createFileHandler("test.flac"));
    auto codec = std::make_unique<FLACCodec>();
    
    EXPECT_TRUE(demuxer->parseContainer());
    
    // Test frame processing
    auto chunk = demuxer->readChunk();
    EXPECT_FALSE(chunk.data.empty());
    
    // Codec should accept demuxer output
    auto samples = codec->decode(chunk.data.data(), chunk.data.size());
    EXPECT_GT(samples.size(), 0);
    
    // Test seeking coordination
    EXPECT_TRUE(demuxer->seekTo(5000));  // Seek to 5 seconds
    codec->reset();  // Application must reset codec
    
    auto chunk_after_seek = demuxer->readChunk();
    EXPECT_FALSE(chunk_after_seek.data.empty());
}
```

## Debugging and Troubleshooting

### Debug Logging Strategy

All debug messages include method identification tokens:

```cpp
void FLACDemuxer::someMethod_unlocked() {
    Debug::log("flac", "[someMethod] Starting operation with parameter: ", param);
    
    if (condition) {
        Debug::log("flac", "[someMethod] Condition met, proceeding with action");
    } else {
        Debug::log("flac", "[someMethod] Condition failed, using fallback");
    }
    
    Debug::log("flac", "[someMethod] Operation completed successfully");
}
```

### Common Issues and Solutions

**Issue: Frame size estimation produces huge values**
```cpp
// Problem: Complex theoretical calculations
uint32_t bad_estimate = (channels * bits_per_sample * block_size) / 8;  // Often wrong

// Solution: Use STREAMINFO minimum
uint32_t good_estimate = m_streaminfo.min_frame_size;  // Accurate for compressed streams
```

**Issue: Seeking fails with compressed files**
```cpp
// Problem: Binary search assumes predictable frame positions
bool bad_seek = seekBinary_unlocked(target_sample);  // Fails with compression

// Solution: Use SEEKTABLE or frame indexing
bool good_seek = seekWithTable_unlocked(target_sample);  // Works with compression
```

**Issue: Thread deadlocks during seeking**
```cpp
// Problem: Public method calling public method
bool badSeekTo(uint64_t timestamp) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return seekWithTable(timestamp);  // Deadlock: seekWithTable also acquires lock
}

// Solution: Use _unlocked internal methods
bool goodSeekTo(uint64_t timestamp) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return seekWithTable_unlocked(timestamp);  // Safe: no lock acquisition
}
```

## Best Practices Summary

### Architecture
- Maintain clear separation between container parsing and bitstream decoding
- Use dependency injection for IOHandler to support different input sources
- Follow established PsyMP3 patterns for consistency

### Performance
- Prioritize STREAMINFO minimum frame size for estimation
- Limit frame boundary search scope to prevent excessive I/O
- Use conservative fallbacks when accurate estimation fails

### Thread Safety
- Always follow public/private lock pattern with `_unlocked` suffixes
- Document lock acquisition order to prevent deadlocks
- Use RAII lock guards for exception safety
- Never invoke callbacks while holding internal locks

### Error Handling
- Implement graceful degradation for corrupted metadata
- Provide recovery mechanisms for frame-level errors
- Log errors with sufficient detail for debugging
- Maintain playback capability even with partial failures

### Testing
- Test with various FLAC file types and compression levels
- Validate thread safety with concurrent access scenarios
- Verify integration with codec components
- Include performance regression tests

This developer guide provides the foundation for maintaining and extending the FLAC demuxer implementation while following established PsyMP3 architectural patterns and best practices.