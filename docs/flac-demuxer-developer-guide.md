# FLAC Demuxer Developer Guide

## Overview

This guide provides detailed information for developers working with or extending the FLAC demuxer implementation in PsyMP3. It covers the architecture, integration patterns, thread safety considerations, and best practices for maintaining and extending FLAC support.

## Architecture Overview

### Separation of Concerns

The FLAC implementation in PsyMP3 follows a clean separation between container parsing and bitstream decoding:

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   FLAC File     │───▶│  FLACDemuxer     │───▶│   FLACCodec     │
│   (.flac)       │    │  (Container)     │    │  (Bitstream)    │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                              │                         │
                              ▼                         ▼
                       ┌──────────────┐         ┌──────────────┐
                       │  Metadata    │         │  PCM Audio   │
                       │  StreamInfo  │         │  Samples     │
                       └──────────────┘         └──────────────┘
```

**FLACDemuxer Responsibilities:**
- Parse FLAC container format (fLaC marker, metadata blocks)
- Extract metadata (STREAMINFO, VORBIS_COMMENT, SEEKTABLE, etc.)
- Locate and extract complete FLAC frames
- Handle seeking within the container
- Provide frame data to codec

**FLACCodec Responsibilities:**
- Decode FLAC bitstream data within frames
- Handle FLAC audio compression algorithms
- Convert compressed data to PCM samples
- Manage decoder state and error recovery

### Container vs. Bitstream

Understanding the distinction between FLAC container and bitstream is crucial:

#### FLAC Container Format
```
File Structure:
┌─────────────────────────────────────────────────────────────┐
│ fLaC (4 bytes) - Stream Marker                             │
├─────────────────────────────────────────────────────────────┤
│ METADATA_BLOCK_STREAMINFO (mandatory, always first)        │
├─────────────────────────────────────────────────────────────┤
│ METADATA_BLOCK_* (optional blocks: SEEKTABLE, COMMENT,     │
│                   PICTURE, etc.)                           │
├─────────────────────────────────────────────────────────────┤
│ FLAC_FRAME_1 (complete frame with header + data + CRC)     │
├─────────────────────────────────────────────────────────────┤
│ FLAC_FRAME_2                                               │
├─────────────────────────────────────────────────────────────┤
│ ...                                                         │
├─────────────────────────────────────────────────────────────┤
│ FLAC_FRAME_N (last frame)                                  │
└─────────────────────────────────────────────────────────────┘
```

#### FLAC Frame Bitstream
```
Frame Structure (handled by FLACCodec):
┌─────────────────────────────────────────────────────────────┐
│ Frame Header (sync + blocking + sample rate + channels)    │
├─────────────────────────────────────────────────────────────┤
│ Subframe 1 (compressed audio data for channel 1)          │
├─────────────────────────────────────────────────────────────┤
│ Subframe 2 (compressed audio data for channel 2)          │
├─────────────────────────────────────────────────────────────┤
│ ...                                                         │
├─────────────────────────────────────────────────────────────┤
│ Frame Footer (CRC-16)                                      │
└─────────────────────────────────────────────────────────────┘
```

## Integration with FLACCodec

### Data Flow

The integration between FLACDemuxer and FLACCodec follows this pattern:

```cpp
// 1. Demuxer extracts complete FLAC frames
MediaChunk frame_chunk = demuxer->readChunk();

// 2. Frame data includes complete FLAC frame
// frame_chunk.data contains:
// - Frame header (sync code, parameters)
// - All subframes (compressed audio)
// - Frame footer (CRC)

// 3. Codec decodes the frame bitstream
std::vector<int32_t> pcm_samples = codec->decode(frame_chunk.data);

// 4. Audio output receives PCM samples
audio_output->play(pcm_samples);
```

### MediaChunk Format

The FLACDemuxer provides frame data in MediaChunk format:

```cpp
struct MediaChunk {
    std::vector<uint8_t> data;    // Complete FLAC frame (header + subframes + CRC)
    uint64_t timestamp_samples;   // Sample position in stream
    uint32_t stream_id;          // Always 1 for FLAC
    bool is_keyframe;            // Always true (FLAC frames are independent)
    
    // Additional metadata for codec
    uint32_t sample_rate;        // From frame header or STREAMINFO
    uint8_t channels;            // Channel configuration
    uint8_t bits_per_sample;     // Bit depth
};
```

### Codec Integration Example

```cpp
class FLACPlaybackEngine {
private:
    std::unique_ptr<FLACDemuxer> m_demuxer;
    std::unique_ptr<FLACCodec> m_codec;
    
public:
    bool initialize(const std::string& filename) {
        // Create demuxer
        auto handler = std::make_unique<FileIOHandler>(filename);
        m_demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        
        if (!m_demuxer->parseContainer()) {
            return false;
        }
        
        // Get stream information for codec configuration
        auto streams = m_demuxer->getStreams();
        if (streams.empty()) {
            return false;
        }
        
        const auto& stream = streams[0];
        
        // Create and configure codec
        m_codec = std::make_unique<FLACCodec>();
        FLACCodec::Config config;
        config.sample_rate = stream.sample_rate;
        config.channels = stream.channels;
        config.bits_per_sample = stream.bits_per_sample;
        
        return m_codec->configure(config);
    }
    
    std::vector<int32_t> readAudioSamples() {
        if (m_demuxer->isEOF()) {
            return {};
        }
        
        // Get next frame from demuxer
        MediaChunk chunk = m_demuxer->readChunk();
        if (chunk.data.empty()) {
            return {};
        }
        
        // Decode frame with codec
        return m_codec->decode(chunk.data);
    }
    
    bool seekTo(uint64_t timestamp_ms) {
        // Demuxer handles seeking within container
        if (!m_demuxer->seekTo(timestamp_ms)) {
            return false;
        }
        
        // Reset codec state after seek
        m_codec->reset();
        return true;
    }
};
```

### Error Coordination

The demuxer and codec coordinate error handling:

```cpp
// Demuxer error handling
MediaChunk FLACDemuxer::readChunk() {
    try {
        FLACFrame frame;
        if (!findNextFrame(frame)) {
            // Frame sync lost - attempt recovery
            if (handleLostFrameSync()) {
                return readChunk(); // Retry after recovery
            }
            return MediaChunk{}; // Return empty chunk on failure
        }
        
        std::vector<uint8_t> frame_data;
        if (!readFrameData(frame, frame_data)) {
            // Frame reading failed - skip this frame
            return readChunk(); // Try next frame
        }
        
        // Validate frame CRC if possible
        if (!validateFrameCRC(frame, frame_data)) {
            Debug::log(Debug::WARNING, "FLAC frame CRC mismatch");
            // Continue anyway - codec may handle it
        }
        
        return createMediaChunk(frame, frame_data);
        
    } catch (const std::exception& e) {
        Debug::log(Debug::ERROR, "FLAC demuxer error: " + std::string(e.what()));
        setErrorState(true);
        return MediaChunk{};
    }
}

// Codec error handling
std::vector<int32_t> FLACCodec::decode(const std::vector<uint8_t>& frame_data) {
    try {
        return decodeFrame(frame_data);
    } catch (const FLACDecodeError& e) {
        Debug::log(Debug::WARNING, "FLAC decode error: " + std::string(e.what()));
        // Return silence for this frame
        return createSilenceFrame();
    }
}
```

## Thread Safety Implementation

### Public/Private Lock Pattern

The FLACDemuxer follows PsyMP3's mandatory threading safety pattern:

```cpp
class FLACDemuxer : public Demuxer {
private:
    // Lock acquisition order (to prevent deadlocks):
    // 1. m_state_mutex (for container state and position tracking)
    // 2. m_metadata_mutex (for metadata access)
    // 3. IOHandler locks (managed by IOHandler implementation)
    mutable std::mutex m_state_mutex;
    mutable std::mutex m_metadata_mutex;
    
public:
    // Public methods acquire locks and call private implementations
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
    // Private methods assume locks are already held
    bool seekTo_unlocked(uint64_t timestamp_ms) {
        // Implementation assumes m_state_mutex is held
        uint64_t target_sample = msToSamples(timestamp_ms);
        
        // Can safely call other _unlocked methods
        if (m_seektable.empty()) {
            return seekBinary_unlocked(target_sample);
        } else {
            return seekWithTable_unlocked(target_sample);
        }
    }
    
    MediaChunk readChunk_unlocked() {
        // Implementation assumes m_state_mutex is held
        FLACFrame frame;
        if (!findNextFrame_unlocked(frame)) {
            return MediaChunk{};
        }
        
        std::vector<uint8_t> frame_data;
        if (!readFrameData_unlocked(frame, frame_data)) {
            return MediaChunk{};
        }
        
        return createMediaChunk_unlocked(frame, frame_data);
    }
    
    std::vector<StreamInfo> getStreams_unlocked() const {
        // Implementation assumes m_metadata_mutex is held
        if (!m_container_parsed) {
            return {};
        }
        
        StreamInfo info;
        populateStreamInfo_unlocked(info);
        return {info};
    }
};
```

### Lock Ordering and Deadlock Prevention

**Critical Rule**: Always acquire locks in the documented order:

```cpp
// CORRECT: Acquire locks in documented order
void complexOperation() {
    std::lock_guard<std::mutex> state_lock(m_state_mutex);    // First
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex); // Second
    
    // Safe to access both state and metadata
    updatePositionFromMetadata_unlocked();
}

// INCORRECT: Wrong lock order - DEADLOCK RISK
void badOperation() {
    std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex); // Wrong order!
    std::lock_guard<std::mutex> state_lock(m_state_mutex);       // DEADLOCK RISK
}
```

### Atomic Operations for Performance

Frequently accessed data uses atomic operations to avoid locking overhead:

```cpp
class FLACDemuxer : public Demuxer {
private:
    // Fast lock-free access for position tracking
    std::atomic<uint64_t> m_current_sample{0};
    std::atomic<bool> m_error_state{false};
    
public:
    // Fast position access without locking
    uint64_t getCurrentSample() const {
        return m_current_sample.load(std::memory_order_acquire);
    }
    
    uint64_t getPosition() const {
        uint64_t sample = m_current_sample.load(std::memory_order_acquire);
        return samplesToMs(sample);
    }
    
private:
    void updatePositionTracking_unlocked(uint64_t new_sample) {
        // Update atomic variable (assumes state lock is held for consistency)
        m_current_sample.store(new_sample, std::memory_order_release);
    }
};
```

### Thread Safety Testing

Comprehensive thread safety testing is essential:

```cpp
// Thread safety test framework
class ThreadSafetyTester {
private:
    std::unique_ptr<FLACDemuxer> m_demuxer;
    std::atomic<bool> m_stop_flag{false};
    std::atomic<int> m_error_count{0};
    
public:
    void runConcurrencyTest(const std::string& filename) {
        // Initialize demuxer
        auto handler = std::make_unique<FileIOHandler>(filename);
        m_demuxer = std::make_unique<FLACDemuxer>(std::move(handler));
        m_demuxer->parseContainer();
        
        std::vector<std::thread> threads;
        
        // Reader thread - continuous frame reading
        threads.emplace_back([this]() {
            while (!m_stop_flag.load()) {
                try {
                    MediaChunk chunk = m_demuxer->readChunk();
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                } catch (...) {
                    m_error_count.fetch_add(1);
                }
            }
        });
        
        // Seeker thread - random seeking
        threads.emplace_back([this]() {
            uint64_t duration = m_demuxer->getDuration();
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint64_t> dist(0, duration);
            
            while (!m_stop_flag.load()) {
                try {
                    m_demuxer->seekTo(dist(gen));
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } catch (...) {
                    m_error_count.fetch_add(1);
                }
            }
        });
        
        // Position monitor thread - frequent position queries
        threads.emplace_back([this]() {
            while (!m_stop_flag.load()) {
                try {
                    uint64_t pos = m_demuxer->getPosition();
                    uint64_t sample = m_demuxer->getCurrentSample();
                    auto streams = m_demuxer->getStreams();
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                } catch (...) {
                    m_error_count.fetch_add(1);
                }
            }
        });
        
        // Run test for specified duration
        std::this_thread::sleep_for(std::chrono::seconds(30));
        m_stop_flag.store(true);
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "Thread safety test completed. Errors: " 
                  << m_error_count.load() << std::endl;
    }
};
```

## Extending FLAC Support

### Adding New Metadata Block Types

To add support for new FLAC metadata block types:

```cpp
// 1. Add new metadata type to enum
enum class FLACMetadataType : uint8_t {
    // ... existing types ...
    CUSTOM_BLOCK = 127,  // Custom application block
};

// 2. Add data structure for new block type
struct FLACCustomBlock {
    std::string application_id;
    std::vector<uint8_t> custom_data;
    
    bool isValid() const {
        return !application_id.empty() && !custom_data.empty();
    }
};

// 3. Add storage to demuxer class
class FLACDemuxer : public Demuxer {
private:
    std::vector<FLACCustomBlock> m_custom_blocks; // Protected by m_metadata_mutex
    
    // 4. Add parsing method
    bool parseCustomBlock(const FLACMetadataBlock& block) {
        if (block.length < 4) {
            return false; // Invalid block
        }
        
        std::vector<uint8_t> block_data(block.length);
        if (m_handler->read(block_data.data(), block.length) != block.length) {
            return false;
        }
        
        FLACCustomBlock custom_block;
        // Parse application ID and custom data
        custom_block.application_id = std::string(
            reinterpret_cast<const char*>(block_data.data()), 4);
        custom_block.custom_data.assign(
            block_data.begin() + 4, block_data.end());
        
        if (custom_block.isValid()) {
            m_custom_blocks.push_back(std::move(custom_block));
        }
        
        return true;
    }
    
    // 5. Update main parsing loop
    bool parseMetadataBlocks() {
        // ... existing parsing code ...
        
        switch (block.type) {
            // ... existing cases ...
            case FLACMetadataType::CUSTOM_BLOCK:
                if (!parseCustomBlock(block)) {
                    Debug::log(Debug::WARNING, "Failed to parse custom block");
                }
                break;
        }
        
        // ... rest of parsing code ...
    }
    
public:
    // 6. Add public accessor method
    std::vector<FLACCustomBlock> getCustomBlocks() const {
        std::lock_guard<std::mutex> lock(m_metadata_mutex);
        return m_custom_blocks;
    }
};
```

### Supporting Alternative Container Formats

The demuxer architecture supports extending to other FLAC container formats:

```cpp
// Base class for FLAC container parsers
class FLACContainerParser {
public:
    virtual ~FLACContainerParser() = default;
    virtual bool parseContainer() = 0;
    virtual bool findNextFrame(FLACFrame& frame) = 0;
    virtual bool readFrameData(const FLACFrame& frame, std::vector<uint8_t>& data) = 0;
    virtual bool seekTo(uint64_t sample_position) = 0;
};

// Native FLAC container parser
class NativeFLACParser : public FLACContainerParser {
    // Current FLACDemuxer implementation
};

// Ogg FLAC container parser
class OggFLACParser : public FLACContainerParser {
public:
    bool parseContainer() override {
        // Parse Ogg container structure
        // Extract FLAC metadata from Ogg comments
        // Locate FLAC frames within Ogg pages
    }
    
    bool findNextFrame(FLACFrame& frame) override {
        // Find next Ogg page containing FLAC data
        // Extract FLAC frame from Ogg page
    }
    
    // ... other methods ...
};

// Factory for creating appropriate parser
class FLACParserFactory {
public:
    static std::unique_ptr<FLACContainerParser> createParser(
        IOHandler* handler) {
        
        // Detect container format
        uint8_t header[4];
        if (handler->read(header, 4) != 4) {
            return nullptr;
        }
        handler->seek(0); // Reset position
        
        if (std::memcmp(header, "fLaC", 4) == 0) {
            return std::make_unique<NativeFLACParser>();
        } else if (std::memcmp(header, "OggS", 4) == 0) {
            return std::make_unique<OggFLACParser>();
        }
        
        return nullptr; // Unknown format
    }
};
```

### Performance Optimization Extensions

#### Custom Seeking Strategies

```cpp
// Pluggable seeking strategy interface
class FLACSeekStrategy {
public:
    virtual ~FLACSeekStrategy() = default;
    virtual bool seekTo(FLACDemuxer* demuxer, uint64_t target_sample) = 0;
    virtual std::string getName() const = 0;
};

// Machine learning-based seeking
class MLSeekStrategy : public FLACSeekStrategy {
private:
    struct SeekPredictionModel {
        std::vector<double> weights;
        double bias;
    };
    
    SeekPredictionModel m_model;
    std::vector<std::pair<uint64_t, uint64_t>> m_training_data; // sample -> file_offset
    
public:
    bool seekTo(FLACDemuxer* demuxer, uint64_t target_sample) override {
        // Use ML model to predict file offset
        double predicted_offset = predictFileOffset(target_sample);
        
        // Seek to predicted position and refine
        return seekAndRefine(demuxer, target_sample, 
                           static_cast<uint64_t>(predicted_offset));
    }
    
    std::string getName() const override {
        return "ML-based seeking";
    }
    
private:
    double predictFileOffset(uint64_t sample) {
        // Simple linear model (could be more sophisticated)
        double normalized_sample = static_cast<double>(sample) / 1000000.0;
        return m_model.weights[0] * normalized_sample + m_model.bias;
    }
    
    void updateModel(uint64_t sample, uint64_t actual_offset) {
        m_training_data.emplace_back(sample, actual_offset);
        if (m_training_data.size() > 100) {
            retrainModel();
        }
    }
    
    void retrainModel() {
        // Simple linear regression update
        // In practice, could use more sophisticated ML algorithms
    }
};

// Integration with demuxer
class FLACDemuxer : public Demuxer {
private:
    std::unique_ptr<FLACSeekStrategy> m_seek_strategy;
    
public:
    void setSeekStrategy(std::unique_ptr<FLACSeekStrategy> strategy) {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_seek_strategy = std::move(strategy);
    }
    
private:
    bool seekTo_unlocked(uint64_t timestamp_ms) {
        uint64_t target_sample = msToSamples(timestamp_ms);
        
        if (m_seek_strategy) {
            return m_seek_strategy->seekTo(this, target_sample);
        }
        
        // Fall back to default seeking
        return seekWithDefaultStrategy_unlocked(target_sample);
    }
};
```

#### Memory Pool Integration

```cpp
// Integration with PsyMP3 memory management
class FLACDemuxer : public Demuxer {
private:
    // Use memory pools for frequent allocations
    std::unique_ptr<MemoryPool> m_frame_buffer_pool;
    std::unique_ptr<MemoryPool> m_metadata_pool;
    
    void initializeMemoryPools() {
        // Create pools for common allocation sizes
        m_frame_buffer_pool = std::make_unique<MemoryPool>(
            FRAME_BUFFER_SIZE, 10); // 10 buffers of FRAME_BUFFER_SIZE
        m_metadata_pool = std::make_unique<MemoryPool>(
            4096, 5); // 5 buffers of 4KB for metadata
    }
    
    std::vector<uint8_t> allocateFrameBuffer() {
        auto buffer = m_frame_buffer_pool->allocate();
        if (buffer) {
            return std::vector<uint8_t>(
                static_cast<uint8_t*>(buffer), 
                static_cast<uint8_t*>(buffer) + FRAME_BUFFER_SIZE);
        }
        
        // Fall back to regular allocation
        return std::vector<uint8_t>(FRAME_BUFFER_SIZE);
    }
    
    void releaseFrameBuffer(std::vector<uint8_t>& buffer) {
        if (buffer.size() == FRAME_BUFFER_SIZE) {
            m_frame_buffer_pool->deallocate(buffer.data());
            buffer.clear(); // Don't double-free
        }
        // Regular vector destructor handles other cases
    }
};
```

## Best Practices

### Code Organization

1. **Separate concerns clearly**: Keep container parsing separate from bitstream decoding
2. **Use RAII consistently**: Ensure proper resource cleanup in all paths
3. **Follow the public/private lock pattern**: Prevent deadlocks with consistent locking
4. **Minimize lock scope**: Hold locks only as long as necessary
5. **Use atomic operations for frequently accessed data**: Avoid locking overhead where possible

### Error Handling

1. **Fail gracefully**: Continue playback when possible, even with errors
2. **Log errors appropriately**: Use PsyMP3 debug logging system
3. **Provide meaningful error messages**: Help users and developers diagnose issues
4. **Test error paths**: Ensure error handling code is tested and works correctly

### Performance

1. **Profile before optimizing**: Measure actual performance bottlenecks
2. **Use appropriate buffer sizes**: Balance memory usage and I/O efficiency
3. **Minimize allocations**: Reuse buffers and use memory pools where appropriate
4. **Optimize for common cases**: Fast path for typical usage patterns

### Testing

1. **Test with diverse files**: Various encoders, bit depths, sample rates
2. **Test edge cases**: Very large files, minimal metadata, corrupted data
3. **Test thread safety**: Concurrent access patterns and stress testing
4. **Test performance**: Ensure acceptable performance characteristics
5. **Test integration**: Verify proper integration with codec and other components

### Documentation

1. **Document thread safety**: Clearly specify thread safety guarantees
2. **Document lock ordering**: Prevent deadlocks with clear lock acquisition order
3. **Document performance characteristics**: Help users understand trade-offs
4. **Provide usage examples**: Show proper integration patterns
5. **Keep documentation current**: Update documentation with code changes

## Future Enhancements

### Potential Improvements

1. **Advanced seeking algorithms**: Machine learning-based position prediction
2. **Streaming optimizations**: Better support for network streams and live streams
3. **Metadata caching**: Persistent metadata cache for frequently accessed files
4. **Parallel processing**: Multi-threaded frame parsing for very large files
5. **Hardware acceleration**: GPU-based frame parsing for high-throughput scenarios

### Extension Points

1. **Custom metadata handlers**: Plugin system for application-specific metadata
2. **Alternative container formats**: Support for FLAC in other containers
3. **Streaming protocols**: Support for specialized streaming protocols
4. **Quality analysis**: Real-time audio quality metrics and analysis
5. **Adaptive buffering**: Dynamic buffer sizing based on network conditions

This developer guide provides the foundation for understanding, maintaining, and extending the FLAC demuxer implementation in PsyMP3. Following these patterns and best practices will ensure robust, performant, and maintainable FLAC support.