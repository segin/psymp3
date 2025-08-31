# **FLAC CODEC DESIGN**

## **Overview**

This design document specifies the implementation of a container-agnostic FLAC audio codec for PsyMP3. The codec decodes FLAC bitstream data from any container (native FLAC, Ogg FLAC, or potentially ISO FLAC) into standard 16-bit PCM audio. This codec works with the AudioCodec architecture and receives FLAC frame data from various demuxers.

**Design Philosophy Based on Implementation Experience:**

The design incorporates critical lessons learned from extensive FLAC demuxer development:

1. **Performance-First Architecture**: Optimized for real-time decoding of high-resolution files (24-bit/96kHz+)
2. **RFC 9639 Strict Compliance**: All implementation decisions validated against the official FLAC specification
3. **Threading Safety by Design**: Follows PsyMP3's established public/private lock pattern throughout
4. **Container Independence**: Works seamlessly with any demuxer providing MediaChunk data
5. **Conditional Compilation Ready**: Designed for clean integration with build-time dependency checking
6. **Memory Efficiency**: Optimized for minimal memory usage and allocation overhead
7. **Error Resilience**: Robust handling of corrupted frames and edge cases

The design focuses on efficient, accurate FLAC decoding while maintaining compatibility with the existing FLAC implementation and providing a clean interface for the modern demuxer/codec architecture.

## **Architecture**

### **Class Structure with Threading Safety**

```cpp
/**
 * @brief Container-agnostic FLAC audio codec implementation
 * 
 * This codec decodes FLAC bitstream data from MediaChunk containers into
 * 16-bit PCM audio samples. It follows PsyMP3's threading safety patterns
 * and integrates with conditional compilation for optional FLAC support.
 * 
 * @thread_safety This class is thread-safe. All public methods follow the
 *                public/private lock pattern to prevent deadlocks.
 * 
 * Lock acquisition order (to prevent deadlocks):
 * 1. m_state_mutex (for codec state and configuration)
 * 2. m_buffer_mutex (for output buffer management)
 * 3. libFLAC internal locks (managed by libFLAC)
 */
class FLACCodec : public AudioCodec {
public:
    explicit FLACCodec(const StreamInfo& stream_info);
    ~FLACCodec() override;
    
    // AudioCodec interface implementation (thread-safe public methods)
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "flac"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
    // FLAC-specific public interface
    bool supportsSeekReset() const;
    uint64_t getCurrentSample() const;
    FLACCodecStats getStats() const;

private:
    // Thread safety - documented lock acquisition order above
    mutable std::mutex m_state_mutex;    ///< Protects codec state and configuration
    mutable std::mutex m_buffer_mutex;   ///< Protects output buffer operations
    std::atomic<bool> m_error_state{false}; ///< Thread-safe error state flag
    
    // FLAC decoder state (protected by m_state_mutex)
    std::unique_ptr<FLAC::Decoder::Stream> m_decoder;
    bool m_decoder_initialized = false;
    
    // Stream configuration (protected by m_state_mutex)
    uint32_t m_sample_rate = 0;
    uint16_t m_channels = 0;
    uint16_t m_bits_per_sample = 0;
    uint64_t m_total_samples = 0;
    
    // Decoding state (protected by m_state_mutex)
    std::atomic<uint64_t> m_current_sample{0};  ///< Atomic for fast reads
    uint32_t m_last_block_size = 0;
    bool m_stream_finished = false;
    
    // Output buffering (protected by m_buffer_mutex)
    std::vector<int16_t> m_output_buffer;
    size_t m_buffer_read_position = 0;
    static constexpr size_t MAX_BUFFER_SAMPLES = 48000 * 2 * 4; // 4 seconds stereo
    
    // Performance optimization state (protected by m_state_mutex)
    std::vector<uint8_t> m_input_buffer;     ///< Reusable input buffer
    std::vector<FLAC__int32> m_decode_buffer; ///< Reusable decode buffer
    size_t m_frames_decoded = 0;
    size_t m_samples_decoded = 0;
    
    // Thread-safe private implementations (assume locks are held)
    bool initialize_unlocked();
    AudioFrame decode_unlocked(const MediaChunk& chunk);
    AudioFrame flush_unlocked();
    void reset_unlocked();
    bool canDecode_unlocked(const StreamInfo& stream_info) const;
    
    // libFLAC integration methods (assume appropriate locks are held)
    bool initializeLibFLAC();
    void cleanupLibFLAC();
    bool processFrameData(const uint8_t* data, size_t size);
    
    // Bit depth conversion methods (assume m_buffer_mutex is held)
    void convertSamples(const FLAC__int32* const buffer[], uint32_t block_size);
    int16_t convert8BitTo16Bit(FLAC__int32 sample);
    int16_t convert24BitTo16Bit(FLAC__int32 sample);
    int16_t convert32BitTo16Bit(FLAC__int32 sample);
    
    // Channel processing methods (assume m_buffer_mutex is held)
    void processChannelAssignment(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processIndependentChannels(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processLeftSideStereo(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processRightSideStereo(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    void processMidSideStereo(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
    
    // Error handling methods (assume appropriate locks are held)
    void handleDecoderError(FLAC__StreamDecoderErrorStatus status);
    bool recoverFromError();
    void resetDecoderState();
    AudioFrame createSilenceFrame(uint32_t block_size);
    
    // Memory management methods (assume appropriate locks are held)
    void optimizeBufferSizes();
    void ensureBufferCapacity(size_t required_samples);
    void freeUnusedMemory();
    
    // Performance monitoring (assume m_state_mutex is held)
    void updatePerformanceStats(uint32_t block_size);
    void logPerformanceMetrics() const;
};

/**
 * @brief FLAC codec performance and debugging statistics
 */
struct FLACCodecStats {
    size_t frames_decoded = 0;
    size_t samples_decoded = 0;
    size_t total_bytes_processed = 0;
    size_t conversion_operations = 0;
    size_t error_count = 0;
    double average_frame_size = 0.0;
    double decode_efficiency = 0.0;  // samples per second
    size_t memory_usage_bytes = 0;
};
```

### **Supporting Components**

```cpp
/**
 * @brief libFLAC stream decoder wrapper with callback integration
 * 
 * This class wraps libFLAC's stream decoder and provides the callback
 * interface for integrating with the FLACCodec. It handles the low-level
 * FLAC decoding operations while the parent codec manages higher-level
 * concerns like threading and output formatting.
 */
class FLACStreamDecoder : public FLAC::Decoder::Stream {
public:
    explicit FLACStreamDecoder(FLACCodec* parent);
    virtual ~FLACStreamDecoder();
    
protected:
    // libFLAC decoder callbacks (called by libFLAC during decoding)
    
    /**
     * @brief Read callback - provides FLAC frame data to libFLAC
     * @param buffer Buffer to fill with FLAC data
     * @param bytes Pointer to number of bytes to read/actually read
     * @return Read status indicating success, EOF, or error
     */
    FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], size_t *bytes) override;
    
    /**
     * @brief Write callback - receives decoded PCM samples from libFLAC
     * @param frame FLAC frame information (block size, channels, etc.)
     * @param buffer Array of decoded sample buffers (one per channel)
     * @return Write status indicating success or error
     */
    FLAC__StreamDecoderWriteStatus write_callback(const FLAC__Frame *frame, 
                                                  const FLAC__int32 * const buffer[]) override;
    
    /**
     * @brief Metadata callback - receives FLAC metadata during decoding
     * @param metadata FLAC metadata block (usually STREAMINFO)
     */
    void metadata_callback(const FLAC__StreamMetadata *metadata) override;
    
    /**
     * @brief Error callback - handles libFLAC decoding errors
     * @param status Error status from libFLAC decoder
     */
    void error_callback(FLAC__StreamDecoderErrorStatus status) override;
    
public:
    // Data feeding interface for parent codec
    bool feedData(const uint8_t* data, size_t size);
    void clearInputBuffer();
    size_t getInputBufferSize() const;
    
private:
    FLACCodec* m_parent;                    ///< Parent codec for callbacks
    std::vector<uint8_t> m_input_buffer;    ///< Input data buffer for libFLAC
    size_t m_buffer_position = 0;           ///< Current read position in buffer
    mutable std::mutex m_input_mutex;       ///< Thread safety for input buffer
    
    // Performance optimization
    static constexpr size_t INPUT_BUFFER_SIZE = 64 * 1024; ///< 64KB input buffer
    
    // Error handling state
    bool m_error_occurred = false;
    FLAC__StreamDecoderErrorStatus m_last_error = FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC;
};

/**
 * @brief FLAC frame information extracted during decoding
 * 
 * This structure contains the essential information about a FLAC frame
 * that's needed for proper decoding and output formatting.
 */
struct FLACFrameInfo {
    uint32_t block_size = 0;         ///< Number of samples in this frame
    uint32_t sample_rate = 0;        ///< Sample rate for this frame
    uint16_t channels = 0;           ///< Number of channels in this frame
    uint16_t bits_per_sample = 0;    ///< Bits per sample in this frame
    uint64_t sample_number = 0;      ///< Starting sample number for this frame
    uint8_t channel_assignment = 0;  ///< Channel assignment mode (independent, left-side, etc.)
    bool variable_block_size = false; ///< True if using variable block size strategy
    
    /**
     * @brief Check if frame information is valid
     */
    bool isValid() const {
        return block_size > 0 && sample_rate > 0 && channels > 0 && 
               bits_per_sample >= 4 && bits_per_sample <= 32;
    }
    
    /**
     * @brief Get frame duration in milliseconds
     */
    uint64_t getDurationMs() const {
        if (sample_rate == 0 || block_size == 0) return 0;
        return (static_cast<uint64_t>(block_size) * 1000) / sample_rate;
    }
    
    /**
     * @brief Get expected output sample count for 16-bit conversion
     */
    size_t getOutputSampleCount() const {
        return static_cast<size_t>(block_size) * channels;
    }
};

/**
 * @brief Conditional compilation support for FLAC codec
 * 
 * This namespace provides compile-time detection and registration
 * of FLAC codec support based on library availability.
 */
namespace FLACCodecSupport {
    
#ifdef HAVE_FLAC
    /**
     * @brief Check if FLAC codec is available at compile time
     */
    constexpr bool isAvailable() { return true; }
    
    /**
     * @brief Register FLAC codec with MediaFactory (conditional compilation)
     */
    void registerCodec();
    
    /**
     * @brief Create FLAC codec instance (conditional compilation)
     */
    std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info);
    
#else
    /**
     * @brief FLAC codec not available - compile-time disabled
     */
    constexpr bool isAvailable() { return false; }
    
    /**
     * @brief No-op registration when FLAC not available
     */
    inline void registerCodec() { /* No FLAC support */ }
    
    /**
     * @brief Return nullptr when FLAC not available
     */
    inline std::unique_ptr<AudioCodec> createCodec(const StreamInfo&) { return nullptr; }
    
#endif // HAVE_FLAC
    
} // namespace FLACCodecSupport
```

## **Components and Interfaces**

### **1. Initialization Component**

**Purpose**: Configure codec from StreamInfo and initialize FLAC decoder with performance optimization

**Key Methods**:
- `bool initialize()`: Thread-safe main initialization entry point
- `bool initialize_unlocked()`: Internal initialization implementation
- `bool configureFromStreamInfo_unlocked()`: Extract parameters from StreamInfo
- `bool initializeFLACDecoder_unlocked()`: Set up libFLAC decoder
- `bool validateConfiguration_unlocked()`: Verify codec parameters against RFC 9639

**Initialization Process with Performance Optimization**:
1. **Thread-safe parameter extraction** from StreamInfo with validation
2. **RFC 9639 compliance validation** against FLAC specification limits
3. **Optimized libFLAC decoder initialization** with performance settings
4. **Pre-allocated buffer setup** to minimize runtime allocations
5. **Bit depth conversion configuration** with optimized algorithms
6. **Performance monitoring initialization** for debugging and optimization

**Parameter Extraction with RFC 9639 Validation**:
```cpp
bool FLACCodec::configureFromStreamInfo_unlocked(const StreamInfo& stream_info) {
    // Extract basic parameters
    m_sample_rate = stream_info.sample_rate;
    m_channels = stream_info.channels;
    m_bits_per_sample = stream_info.bits_per_sample;
    m_total_samples = stream_info.duration_samples;
    
    // RFC 9639 compliance validation
    if (m_sample_rate < 1 || m_sample_rate > 655350) {
        Debug::log("flac_codec", "[configureFromStreamInfo] Invalid sample rate: ", m_sample_rate);
        return false;
    }
    
    if (m_channels < 1 || m_channels > 8) {
        Debug::log("flac_codec", "[configureFromStreamInfo] Invalid channel count: ", m_channels);
        return false;
    }
    
    if (m_bits_per_sample < 4 || m_bits_per_sample > 32) {
        Debug::log("flac_codec", "[configureFromStreamInfo] Invalid bit depth: ", m_bits_per_sample);
        return false;
    }
    
    // Performance optimization: Pre-calculate buffer sizes
    size_t max_block_size = 65535; // RFC 9639 maximum
    size_t required_buffer_size = max_block_size * m_channels;
    
    // Pre-allocate buffers to avoid runtime allocations
    m_output_buffer.reserve(required_buffer_size);
    m_decode_buffer.reserve(required_buffer_size);
    m_input_buffer.reserve(64 * 1024); // 64KB input buffer
    
    Debug::log("flac_codec", "[configureFromStreamInfo] Configured: ", 
              m_sample_rate, "Hz, ", m_channels, " channels, ", 
              m_bits_per_sample, " bits, ", m_total_samples, " samples");
    
    return true;
}
```

**libFLAC Initialization with Performance Settings**:
```cpp
bool FLACCodec::initializeFLACDecoder_unlocked() {
    try {
        // Create optimized libFLAC decoder
        m_decoder = std::make_unique<FLACStreamDecoder>(this);
        
        if (!m_decoder) {
            Debug::log("flac_codec", "[initializeFLACDecoder] Failed to create decoder");
            return false;
        }
        
        // Configure decoder for optimal performance
        m_decoder->set_md5_checking(false);  // Disable MD5 for performance
        
        // Initialize decoder state
        FLAC__StreamDecoderInitStatus init_status = m_decoder->init();
        if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
            Debug::log("flac_codec", "[initializeFLACDecoder] Init failed: ", init_status);
            return false;
        }
        
        m_decoder_initialized = true;
        Debug::log("flac_codec", "[initializeFLACDecoder] Decoder initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[initializeFLACDecoder] Exception: ", e.what());
        return false;
    }
}
```

### **2. Frame Decoding Component**

**Purpose**: Decode individual FLAC frames into PCM samples with high-performance optimization

**Key Methods**:
- `AudioFrame decode(const MediaChunk& chunk)`: Thread-safe main decoding interface
- `AudioFrame decode_unlocked(const MediaChunk& chunk)`: Internal decoding implementation
- `bool processFrameData_unlocked(const uint8_t* data, size_t size)`: Process single frame efficiently
- `bool feedDataToDecoder_unlocked(const uint8_t* data, size_t size)`: Feed data to libFLAC
- `AudioFrame extractDecodedSamples_unlocked()`: Get decoded samples from buffer

**High-Performance Decoding Process**:
1. **Thread-safe MediaChunk reception** with complete FLAC frame validation
2. **Optimized frame data feeding** to libFLAC decoder with minimal copying
3. **Efficient libFLAC callback handling** for decoded samples
4. **Optimized bit depth conversion** using fast algorithms
5. **Memory-efficient AudioFrame creation** with pre-allocated buffers
6. **Performance monitoring** for optimization and debugging

**Optimized Frame Processing Flow**:
```
MediaChunk → Validation → libFLAC Decoder → Callback Processing → Bit Depth Conversion → AudioFrame
     ↓              ↓              ↓                ↓                    ↓              ↓
Thread-Safe    RFC 9639      Performance      Channel           Optimized        Minimal
Validation    Compliance     Optimized        Processing        Algorithms       Allocation
```

**Performance-Optimized Decoding Implementation**:
```cpp
AudioFrame FLACCodec::decode_unlocked(const MediaChunk& chunk) {
    // Performance monitoring
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Validate input chunk
    if (chunk.data.empty()) {
        Debug::log("flac_codec", "[decode] Empty chunk received");
        return AudioFrame(); // Empty frame
    }
    
    // Ensure decoder is ready
    if (!m_decoder_initialized || !m_decoder) {
        Debug::log("flac_codec", "[decode] Decoder not initialized");
        setErrorState_unlocked(true);
        return createSilenceFrame_unlocked(1024); // Fallback silence
    }
    
    try {
        // Clear previous output buffer efficiently
        {
            std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
            m_output_buffer.clear();
            m_buffer_read_position = 0;
        }
        
        // Feed frame data to libFLAC decoder
        if (!m_decoder->feedData(chunk.data.data(), chunk.data.size())) {
            Debug::log("flac_codec", "[decode] Failed to feed data to decoder");
            return handleDecodingError_unlocked(chunk);
        }
        
        // Process the frame through libFLAC
        if (!m_decoder->process_single()) {
            Debug::log("flac_codec", "[decode] libFLAC processing failed");
            return handleDecodingError_unlocked(chunk);
        }
        
        // Extract decoded samples
        AudioFrame result = extractDecodedSamples_unlocked();
        
        // Update performance statistics
        updatePerformanceStats_unlocked(result.sample_count);
        
        // Performance logging (debug builds only)
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        if (duration.count() > 1000) { // Log if decoding takes >1ms
            Debug::log("flac_codec", "[decode] Frame decoding took ", duration.count(), " μs");
        }
        
        return result;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[decode] Exception during decoding: ", e.what());
        setErrorState_unlocked(true);
        return createSilenceFrame_unlocked(1024);
    }
}

AudioFrame FLACCodec::extractDecodedSamples_unlocked() {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    if (m_output_buffer.empty()) {
        Debug::log("flac_codec", "[extractDecodedSamples] No samples in buffer");
        return AudioFrame();
    }
    
    // Create AudioFrame with optimal memory usage
    AudioFrame frame;
    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.sample_count = m_output_buffer.size() / m_channels;
    frame.timestamp_samples = m_current_sample.load();
    
    // Efficient sample data transfer
    frame.samples.reserve(m_output_buffer.size());
    frame.samples.assign(m_output_buffer.begin(), m_output_buffer.end());
    
    // Update position tracking
    m_current_sample.fetch_add(frame.sample_count);
    
    Debug::log("flac_codec", "[extractDecodedSamples] Extracted ", 
              frame.sample_count, " samples at ", frame.timestamp_samples);
    
    return frame;
}
```

### **3. Bit Depth Conversion Component**

**Purpose**: Convert FLAC samples to consistent 16-bit PCM output with optimized algorithms

**Key Methods**:
- `void convertSamples_unlocked(const FLAC__int32* const buffer[], uint32_t block_size)`: Main conversion entry point
- `int16_t convert8BitTo16Bit(FLAC__int32 sample)`: Optimized 8-bit upscaling
- `int16_t convert24BitTo16Bit(FLAC__int32 sample)`: Optimized 24-bit downscaling with dithering
- `int16_t convert32BitTo16Bit(FLAC__int32 sample)`: Optimized 32-bit downscaling
- `void convertSamplesVectorized(const FLAC__int32* input, int16_t* output, size_t count, int source_bits)`: SIMD-optimized batch conversion

**High-Performance Conversion Algorithms**:

**Optimized 8-bit to 16-bit** (upscaling with proper scaling):
```cpp
inline int16_t FLACCodec::convert8BitTo16Bit(FLAC__int32 sample) {
    // Scale 8-bit (-128 to 127) to 16-bit (-32768 to 32767)
    // Use bit shifting for maximum performance
    return static_cast<int16_t>(sample << 8);
}
```

**Optimized 24-bit to 16-bit** (downscaling with optional dithering):
```cpp
inline int16_t FLACCodec::convert24BitTo16Bit(FLAC__int32 sample) {
    // High-quality downscaling with optional triangular dithering
    #ifdef ENABLE_DITHERING
        // Add triangular dither for better quality
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        static thread_local std::uniform_int_distribution<int> dither(-128, 127);
        
        int32_t dithered = sample + dither(gen);
        return static_cast<int16_t>(std::clamp(dithered >> 8, -32768, 32767));
    #else
        // Simple truncation for maximum performance
        return static_cast<int16_t>(sample >> 8);
    #endif
}
```

**Optimized 32-bit to 16-bit** (downscaling with overflow protection):
```cpp
inline int16_t FLACCodec::convert32BitTo16Bit(FLAC__int32 sample) {
    // Scale down from 32-bit to 16-bit range with overflow protection
    // Use arithmetic right shift to preserve sign
    int32_t scaled = sample >> 16;
    
    // Clamp to 16-bit range to prevent overflow
    return static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
}
```

**Vectorized Batch Conversion** (SIMD optimization where available):
```cpp
void FLACCodec::convertSamplesVectorized(const FLAC__int32* input, int16_t* output, 
                                        size_t count, int source_bits) {
    #ifdef HAVE_SSE2
        // SSE2-optimized conversion for x86/x64
        if (source_bits == 24 && count >= 8) {
            convertSamples24BitSSE2(input, output, count);
            return;
        }
    #endif
    
    #ifdef HAVE_NEON
        // NEON-optimized conversion for ARM
        if (source_bits == 24 && count >= 8) {
            convertSamples24BitNEON(input, output, count);
            return;
        }
    #endif
    
    // Fallback to scalar conversion
    convertSamplesScalar(input, output, count, source_bits);
}
```

**Main Conversion Entry Point with Performance Optimization**:
```cpp
void FLACCodec::convertSamples_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Ensure output buffer has sufficient capacity
    size_t required_samples = static_cast<size_t>(block_size) * m_channels;
    
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        
        // Resize buffer efficiently
        if (m_output_buffer.capacity() < required_samples) {
            m_output_buffer.reserve(required_samples * 2); // Over-allocate for future frames
        }
        m_output_buffer.resize(required_samples);
    }
    
    // Perform optimized conversion based on bit depth
    switch (m_bits_per_sample) {
        case 8:
            convertSamples8Bit_unlocked(buffer, block_size);
            break;
        case 16:
            convertSamples16Bit_unlocked(buffer, block_size); // Direct copy
            break;
        case 24:
            convertSamples24Bit_unlocked(buffer, block_size);
            break;
        case 32:
            convertSamples32Bit_unlocked(buffer, block_size);
            break;
        default:
            // Generic conversion for unusual bit depths
            convertSamplesGeneric_unlocked(buffer, block_size);
            break;
    }
    
    // Update conversion statistics
    m_conversion_operations++;
}

void FLACCodec::convertSamples24Bit_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    // Optimized 24-bit to 16-bit conversion with interleaving
    for (uint32_t sample = 0; sample < block_size; ++sample) {
        for (uint16_t channel = 0; channel < m_channels; ++channel) {
            size_t output_index = sample * m_channels + channel;
            m_output_buffer[output_index] = convert24BitTo16Bit(buffer[channel][sample]);
        }
    }
}
```

**Performance Characteristics**:
- **8-bit conversion**: Single bit shift operation (fastest)
- **16-bit conversion**: Direct memory copy (no conversion needed)
- **24-bit conversion**: Single bit shift with optional dithering
- **32-bit conversion**: Bit shift with overflow protection
- **SIMD optimization**: 4-8x speedup for batch conversions on supported hardware
- **Memory efficiency**: Pre-allocated buffers minimize allocation overhead

### **4. Channel Processing Component**

**Purpose**: Handle various FLAC channel configurations and stereo modes

**Key Methods**:
- `void processChannelAssignment(const FLAC__Frame* frame, const FLAC__int32* const buffer[])`
- `void processIndependentChannels(const FLAC__Frame* frame, const FLAC__int32* const buffer[])`
- `void processLeftSideStereo(const FLAC__Frame* frame, const FLAC__int32* const buffer[])`
- `void processRightSideStereo(const FLAC__Frame* frame, const FLAC__int32* const buffer[])`
- `void processMidSideStereo(const FLAC__Frame* frame, const FLAC__int32* const buffer[])`

**Channel Assignment Types**:
- **Independent**: Each channel decoded separately
- **Left-Side**: Left channel + (Left-Right) difference
- **Right-Side**: (Left+Right) sum + Right channel  
- **Mid-Side**: (Left+Right) sum + (Left-Right) difference

**Stereo Reconstruction**:
```cpp
// Left-Side stereo reconstruction
void processLeftSideStereo(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    for (uint32_t i = 0; i < frame->header.blocksize; i++) {
        FLAC__int32 left = buffer[0][i];
        FLAC__int32 side = buffer[1][i];
        FLAC__int32 right = left - side;
        
        m_output_buffer.push_back(convertSample(left));
        m_output_buffer.push_back(convertSample(right));
    }
}
```

### **5. Threading and Buffering Component**

**Purpose**: Manage asynchronous decoding and output buffering

**Key Methods**:
- `void startDecoderThread()`: Start background decoding thread
- `void stopDecoderThread()`: Stop and cleanup decoder thread
- `void decoderThreadLoop()`: Main decoder thread function
- `void manageOutputBuffer()`: Handle output buffer flow control

**Threading Model**:
- **Main Thread**: Receives MediaChunks, returns AudioFrames
- **Decoder Thread**: Processes FLAC frames asynchronously
- **Synchronization**: Mutex and condition variables for buffer access

**Buffer Management**:
```cpp
// Output buffer with flow control
constexpr size_t MAX_BUFFER_SAMPLES = 48000 * 2 * 4; // 4 seconds stereo

void manageOutputBuffer() {
    std::unique_lock<std::mutex> lock(m_buffer_mutex);
    m_buffer_cv.wait(lock, [this] {
        return m_output_buffer.size() < MAX_BUFFER_SAMPLES || !m_decoding_active;
    });
}
```

### **6. Error Handling Component**

**Purpose**: Handle FLAC decoding errors and recovery

**Key Methods**:
- `void handleDecoderError(FLAC__StreamDecoderErrorStatus status)`
- `bool recoverFromError()`
- `void resetDecoderState()`
- `bool validateFrameData(const MediaChunk& chunk)`

**Error Recovery Strategies**:
- **Frame CRC errors**: Log warning, use decoded data
- **Lost sync**: Reset decoder, search for next frame
- **Invalid parameters**: Skip frame, continue with next
- **Memory errors**: Return error, stop decoding

## **Data Models**

### **Codec State Machine**

```
[Uninitialized] → [Initializing] → [Ready] → [Decoding] → [Flushing] → [Reset]
                                    ↓           ↑
                                [Error] → [Recovering]
```

### **Frame Processing Pipeline**

```
MediaChunk → Input Queue → FLAC Decoder → Sample Conversion → Output Buffer → AudioFrame
```

### **Threading Model**

```
Main Thread: decode() → Input Queue → [Decoder Thread] → Output Buffer → AudioFrame
                                           ↓
                                    libFLAC Processing
```

## **Error Handling**

### **Initialization Errors with Recovery**
- **Invalid StreamInfo**: Validate against RFC 9639, return false from initialize()
- **libFLAC init failure**: Clean up resources, log detailed error, return false
- **Memory allocation**: Handle gracefully with fallback buffers, return error
- **Missing dependencies**: Conditional compilation handles missing libFLAC gracefully
- **Configuration validation**: Ensure all parameters meet FLAC specification requirements

### **Decoding Errors with Resilience**
- **Corrupted frames**: Skip frame, output silence for that period, continue decoding
- **CRC mismatches**: Log warning with frame details, use available data if possible
- **Invalid frame headers**: Reset decoder state, attempt resynchronization
- **Buffer overflow**: Apply backpressure, resize buffers if within limits
- **libFLAC errors**: Map libFLAC error codes to meaningful error messages
- **Sync loss**: Implement frame boundary recovery using sync pattern detection

### **Threading Errors with Deadlock Prevention**
- **Lock acquisition**: Follow documented lock order, use RAII guards exclusively
- **Exception safety**: Ensure locks are released via RAII even during exceptions
- **Resource cleanup**: Proper cleanup on destruction with thread coordination
- **Error state management**: Thread-safe error state using atomic variables
- **Callback safety**: Never invoke callbacks while holding internal locks

### **Performance Error Handling**
- **Real-time constraints**: Detect when decoding falls behind real-time requirements
- **Memory pressure**: Monitor memory usage and free unused buffers
- **CPU overload**: Implement adaptive quality reduction if performance degrades
- **I/O errors**: Handle network stream interruptions and file access failures

**Error Recovery Implementation**:
```cpp
AudioFrame FLACCodec::handleDecodingError_unlocked(const MediaChunk& chunk) {
    m_error_count++;
    
    // Attempt decoder recovery
    if (m_error_count < MAX_CONSECUTIVE_ERRORS) {
        Debug::log("flac_codec", "[handleDecodingError] Attempting decoder recovery, error count: ", m_error_count);
        
        if (recoverFromError_unlocked()) {
            // Reset error count on successful recovery
            m_error_count = 0;
            
            // Retry decoding the frame
            return decode_unlocked(chunk);
        }
    }
    
    // Recovery failed or too many errors - return silence
    Debug::log("flac_codec", "[handleDecodingError] Recovery failed, returning silence frame");
    setErrorState_unlocked(true);
    
    // Estimate frame size for silence generation
    uint32_t estimated_block_size = estimateBlockSize_unlocked(chunk);
    return createSilenceFrame_unlocked(estimated_block_size);
}

bool FLACCodec::recoverFromError_unlocked() {
    try {
        // Reset libFLAC decoder state
        if (m_decoder && m_decoder_initialized) {
            if (!m_decoder->reset()) {
                Debug::log("flac_codec", "[recoverFromError] libFLAC reset failed");
                return false;
            }
        }
        
        // Clear internal buffers
        {
            std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
            m_output_buffer.clear();
            m_buffer_read_position = 0;
        }
        
        // Reset position tracking
        // Note: Don't reset m_current_sample as it tracks playback position
        
        Debug::log("flac_codec", "[recoverFromError] Decoder recovery successful");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[recoverFromError] Exception during recovery: ", e.what());
        return false;
    }
}

AudioFrame FLACCodec::createSilenceFrame_unlocked(uint32_t block_size) {
    AudioFrame frame;
    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.sample_count = block_size;
    frame.timestamp_samples = m_current_sample.load();
    
    // Create silence samples (all zeros)
    size_t total_samples = static_cast<size_t>(block_size) * m_channels;
    frame.samples.assign(total_samples, 0);
    
    // Update position tracking
    m_current_sample.fetch_add(block_size);
    
    Debug::log("flac_codec", "[createSilenceFrame] Created silence frame: ", 
              block_size, " samples, ", total_samples, " total samples");
    
    return frame;
}
```

## **Performance Considerations**

### **Memory Usage Optimization**
- **Pre-allocated buffers**: Reserve maximum expected buffer sizes during initialization
- **Buffer reuse**: Reuse buffers across frames to minimize allocation overhead
- **Bounded memory growth**: Implement limits to prevent memory exhaustion
- **libFLAC integration**: Let libFLAC manage its internal buffers efficiently
- **Memory monitoring**: Track memory usage and free unused buffers proactively

### **CPU Efficiency Optimization**
- **Optimized bit depth conversion**: Use bit shifting and SIMD instructions where available
- **Efficient channel processing**: Minimize branching in channel reconstruction loops
- **Threading overhead reduction**: Use atomic variables for frequently accessed data
- **libFLAC optimization**: Configure libFLAC for maximum performance (disable MD5 checking)
- **Vectorized operations**: Use SIMD instructions for batch sample conversion
- **Cache-friendly data layout**: Structure data to maximize CPU cache efficiency

### **Real-Time Performance Requirements**
Based on implementation experience with high-resolution files:

**Performance Targets**:
- **44.1kHz/16-bit**: <0.5% CPU usage on modern hardware
- **96kHz/24-bit**: <2% CPU usage on modern hardware  
- **192kHz/32-bit**: <5% CPU usage on modern hardware
- **Frame processing**: <100μs per frame for real-time playback
- **Memory allocation**: Zero allocations during steady-state decoding

**Optimization Strategies**:
```cpp
// Performance-critical decoding loop optimization
void FLACCodec::optimizeDecodingPerformance_unlocked() {
    // Pre-allocate all buffers to maximum expected sizes
    size_t max_frame_samples = 65535 * m_channels; // RFC 9639 maximum
    
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        m_output_buffer.reserve(max_frame_samples);
        m_decode_buffer.reserve(max_frame_samples);
    }
    
    m_input_buffer.reserve(64 * 1024); // 64KB input buffer
    
    // Configure libFLAC for maximum performance
    if (m_decoder) {
        m_decoder->set_md5_checking(false);  // Disable MD5 for performance
        // Additional libFLAC performance settings...
    }
    
    Debug::log("flac_codec", "[optimizeDecodingPerformance] Performance optimization applied");
}

// Efficient sample conversion with SIMD support
void FLACCodec::convertSamplesOptimized_unlocked(const FLAC__int32* const buffer[], 
                                                uint32_t block_size) {
    // Use vectorized conversion for large blocks
    if (block_size >= 64 && m_channels <= 2) {
        convertSamplesVectorized_unlocked(buffer, block_size);
    } else {
        // Use scalar conversion for small blocks or multi-channel
        convertSamplesScalar_unlocked(buffer, block_size);
    }
}
```

### **I/O Efficiency and Streaming**
- **Frame-based processing**: Process complete FLAC frames to minimize overhead
- **Efficient buffer management**: Reuse buffers and minimize memory copying
- **Network stream optimization**: Handle network latency and buffering efficiently
- **Cache-conscious design**: Structure data access patterns for CPU cache efficiency
- **Minimal system calls**: Batch operations to reduce system call overhead

### **Threading Performance**
- **Lock-free fast paths**: Use atomic variables for frequently read data
- **Minimal lock contention**: Keep critical sections as short as possible
- **RAII lock guards**: Ensure exception safety without performance penalty
- **Thread-local storage**: Use thread-local variables for per-thread state
- **Lock hierarchy**: Follow documented lock order to prevent deadlocks

### **Performance Monitoring and Debugging**
```cpp
struct FLACCodecPerformanceMetrics {
    std::atomic<size_t> frames_decoded{0};
    std::atomic<size_t> samples_decoded{0};
    std::atomic<size_t> total_decode_time_us{0};
    std::atomic<size_t> conversion_operations{0};
    std::atomic<size_t> buffer_reallocations{0};
    std::atomic<size_t> error_recoveries{0};
    
    double getAverageDecodeTimeUs() const {
        size_t frames = frames_decoded.load();
        return frames > 0 ? static_cast<double>(total_decode_time_us.load()) / frames : 0.0;
    }
    
    double getSamplesPerSecond() const {
        size_t time_us = total_decode_time_us.load();
        size_t samples = samples_decoded.load();
        return time_us > 0 ? (static_cast<double>(samples) * 1000000.0) / time_us : 0.0;
    }
};
```

### **Benchmarking and Validation**
- **Real-time performance tests**: Validate performance with actual high-resolution files
- **Memory usage profiling**: Monitor memory usage patterns during extended playback
- **CPU usage measurement**: Ensure CPU usage stays within acceptable limits
- **Latency measurement**: Measure end-to-end latency from MediaChunk to AudioFrame
- **Regression testing**: Automated performance regression detection

## **Integration Points**

### **With Demuxers**
- Accept MediaChunk data from any demuxer (FLACDemuxer, OggDemuxer)
- Handle frame data regardless of container format
- Process timestamp information for accurate positioning
- Support seeking through reset() method

### **With AudioCodec Architecture**
- Implement all AudioCodec interface methods
- Return consistent AudioFrame format
- Handle initialization from StreamInfo
- Provide codec capability reporting

### **With Existing FLAC Implementation**
- Maintain compatibility with current FLAC file support
- Provide equivalent or better performance
- Support all previously working FLAC files
- Integrate with DemuxedStream bridge

### **With Threading System**
- Use PsyMP3's thread naming conventions
- Integrate with system thread management
- Handle thread-safe logging and error reporting
- Coordinate with player thread synchronization

This design provides efficient, accurate FLAC decoding that works with any container format while maintaining compatibility with existing functionality and integrating cleanly with the modern codec architecture.