# **OPUS CODEC DESIGN**

## **Overview**

This design document specifies the implementation of a container-agnostic Opus audio codec for PsyMP3. The codec decodes Opus bitstream data from any container (primarily Ogg Opus) into standard 16-bit PCM audio. This codec works with the AudioCodec architecture and receives Opus packet data from various demuxers.

The design focuses on efficient, accurate Opus decoding using libopus while maintaining compatibility with the existing Opus implementation and providing a clean interface for the modern demuxer/codec architecture.

## **Architecture**

### **Class Structure**

```cpp
class OpusCodec : public AudioCodec {
private:
    // libopus decoder state
    OpusDecoder* m_opus_decoder = nullptr;
    
    // Stream configuration
    uint32_t m_sample_rate = 48000;  // Opus always outputs at 48kHz
    uint16_t m_channels = 0;
    uint16_t m_pre_skip = 0;
    int16_t m_output_gain = 0;
    
    // Header processing state
    int m_header_packets_received = 0;
    bool m_decoder_initialized = false;
    
    // Decoding buffers
    std::vector<int16_t> m_output_buffer;
    std::vector<float> m_float_buffer;  // For float decoding if needed
    
    // Position tracking
    std::atomic<uint64_t> m_samples_decoded{0};
    std::atomic<uint64_t> m_samples_to_skip{0};
    
    // Error handling
    std::atomic<bool> m_error_state{false};
    std::string m_last_error;
    
public:
    explicit OpusCodec(const StreamInfo& stream_info);
    ~OpusCodec() override;
    
    // AudioCodec interface implementation
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "opus"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    // Header processing
    bool processHeaderPacket(const std::vector<uint8_t>& packet_data);
    bool processIdentificationHeader(const std::vector<uint8_t>& packet_data);
    bool processCommentHeader(const std::vector<uint8_t>& packet_data);
    
    // Audio decoding
    AudioFrame decodeAudioPacket(const std::vector<uint8_t>& packet_data);
    void applyPreSkip(AudioFrame& frame);
    void applyOutputGain(AudioFrame& frame);
    
    // Utility methods
    bool validateOpusPacket(const std::vector<uint8_t>& packet_data);
    void handleDecoderError(int opus_error);
    void resetDecoderState();
};
```

### **Supporting Structures**

```cpp
struct OpusHeader {
    uint8_t version;
    uint8_t channel_count;
    uint16_t pre_skip;
    uint32_t input_sample_rate;
    int16_t output_gain;
    uint8_t channel_mapping_family;
    
    // Channel mapping (for multichannel)
    uint8_t stream_count;
    uint8_t coupled_stream_count;
    std::vector<uint8_t> channel_mapping;
    
    bool isValid() const;
    static OpusHeader parseFromPacket(const std::vector<uint8_t>& packet_data);
};

struct OpusComments {
    std::string vendor_string;
    std::vector<std::pair<std::string, std::string>> user_comments;
    
    static OpusComments parseFromPacket(const std::vector<uint8_t>& packet_data);
};
```

## **Components and Interfaces**

### **1. Initialization Component**

**Purpose**: Configure codec from StreamInfo and initialize libopus decoder

**Key Methods**:
- `bool initialize()`: Main initialization entry point
- `bool configureFromStreamInfo()`: Extract parameters from StreamInfo
- `bool initializeOpusDecoder()`: Set up libopus decoder
- `bool validateConfiguration()`: Verify codec parameters

**Initialization Process**:
1. Extract Opus parameters from StreamInfo
2. Validate parameters against Opus specification
3. Initialize libopus decoder with proper channel configuration
4. Set up internal buffers for decoding
5. Configure pre-skip and gain parameters

### **2. Header Processing Component**

**Purpose**: Process Opus identification and comment headers

**Key Methods**:
- `bool processHeaderPacket(const std::vector<uint8_t>& packet_data)`: Route header processing
- `bool processIdentificationHeader(const std::vector<uint8_t>& packet_data)`: Parse ID header
- `bool processCommentHeader(const std::vector<uint8_t>& packet_data)`: Parse comment header
- `bool validateHeaderSequence()`: Ensure proper header order

**Header Processing Flow**:
1. **Identification Header**: Extract channel count, pre-skip, gain, sample rate
2. **Comment Header**: Extract metadata (handled by demuxer, validated by codec)
3. **Decoder Initialization**: Create libopus decoder after ID header

### **3. Audio Decoding Component**

**Purpose**: Decode Opus audio packets into PCM samples

**Key Methods**:
- `AudioFrame decodeAudioPacket(const std::vector<uint8_t>& packet_data)`: Main decoding
- `int decodeOpusFrame(const uint8_t* data, int len, int16_t* pcm, int frame_size)`: libopus call
- `bool validateOpusPacket(const std::vector<uint8_t>& packet_data)`: Packet validation
- `void handleDecoderError(int opus_error)`: Error handling

**Decoding Process**:
1. Validate incoming Opus packet
2. Call opus_decode() with packet data
3. Handle decoder errors and recovery
4. Apply pre-skip and gain adjustments
5. Return AudioFrame with decoded samples

### **4. Pre-skip and Gain Processing Component**

**Purpose**: Handle Opus pre-skip and output gain requirements

**Key Methods**:
- `void applyPreSkip(AudioFrame& frame)`: Remove initial samples
- `void applyOutputGain(AudioFrame& frame)`: Apply gain adjustment
- `int16_t applyGainToSample(int16_t sample, int16_t gain_q8)`: Per-sample gain
- `void updateSkipCounter(size_t samples_processed)`: Track skipped samples

### **5. Multi-Channel Support Component**

**Purpose**: Handle various Opus channel configurations

**Key Methods**:
- `bool configureChannelMapping(const OpusHeader& header)`: Set up channel mapping
- `bool initializeMultiStreamDecoder(const OpusHeader& header)`: Multi-stream setup
- `void processChannelMapping(AudioFrame& frame)`: Apply channel order
- `bool validateChannelConfiguration(uint8_t channels, uint8_t mapping_family)`: Validation

**Channel Mapping Support**:
- **Family 0**: Mono/Stereo (1-2 channels)
- **Family 1**: Surround sound (1-8 channels, Vorbis order)
- **Family 255**: Custom mapping (application-defined)

### **6. Error Handling Component**

**Purpose**: Handle libopus errors and provide recovery

**Key Methods**:
- `void handleDecoderError(int opus_error)`: Process libopus error codes
- `bool recoverFromError()`: Attempt error recovery
- `void resetDecoderState()`: Reset decoder after errors
- `std::string getErrorString(int opus_error)`: Convert error codes to strings

**Error Recovery Strategies**:
- **OPUS_BAD_ARG**: Log error, skip packet
- **OPUS_BUFFER_TOO_SMALL**: Increase buffer size, retry
- **OPUS_INTERNAL_ERROR**: Reset decoder, continue
- **OPUS_INVALID_PACKET**: Skip packet, output silence
- **OPUS_UNIMPLEMENTED**: Report unsupported feature
- **OPUS_INVALID_STATE**: Reset decoder state
- **OPUS_ALLOC_FAIL**: Report memory error, stop decoding

## **Data Models**

### **Codec State Machine**

```
[Uninitialized] → [WaitingForHeaders] → [ProcessingHeaders] → [Ready] → [Decoding] → [Flushing] → [Reset]
                                                ↓                           ↑
                                            [Error] → [Recovering]
```

### **Header Processing Pipeline**

```
MediaChunk → Header Detection → ID Header Processing → Comment Header Processing → Decoder Initialization → Ready
```

### **Audio Processing Pipeline**

```
MediaChunk → Packet Validation → libopus Decoding → Pre-skip Application → Gain Application → AudioFrame
```

## **Correctness Properties**

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Header Parsing Round-Trip Consistency

*For any* valid OpusHead header bytes with channel count, pre-skip, and output gain values, parsing the header and extracting these fields should produce values identical to the original encoded values.

**Validates: Requirements 1.2, 16.1, 16.2, 16.3**

### Property 2: Pre-skip Sample Removal Accuracy

*For any* valid pre-skip value N and decoded audio stream, exactly N samples should be discarded from the beginning of the decoded output before any samples are returned to the caller.

**Validates: Requirements 1.7, 5.1**

### Property 3: Output Gain Application Correctness

*For any* output gain value G (in Q7.8 format) and input sample S, the output sample should equal S * pow(10, G/(20.0*256)), clamped to the valid 16-bit range.

**Validates: Requirements 5.2, 13.4**

### Property 4: Header Sequence Validation

*For any* sequence of header packets, the codec should accept identification header followed by comment header, and reject any other ordering (comment before identification, duplicate headers, or missing headers).

**Validates: Requirements 1.1, 16.7**

### Property 5: Corrupted Packet Resilience

*For any* corrupted or invalid Opus packet, the codec should not crash, should skip the packet, and should output silence or use PLC for the affected frame duration while continuing to process subsequent packets.

**Validates: Requirements 1.8, 8.2, 8.3**

### Property 6: Invalid Header Rejection

*For any* invalid OpusHead header (wrong signature, unsupported version, invalid channel count, or malformed structure), the codec should reject initialization and report an appropriate error.

**Validates: Requirements 8.1, 16.8**

### Property 7: Reset State Consistency

*For any* codec instance that has processed headers and audio packets, calling reset() should restore the pre-skip counter to the original header value and clear internal buffers, allowing the codec to correctly process packets from any stream position.

**Validates: Requirements 15.1, 15.4**

### Property 8: Thread Instance Independence

*For any* two codec instances decoding the same or different streams concurrently, the decoded output from each instance should be identical to what would be produced by sequential decoding of the same input.

**Validates: Requirements 10.1, 10.2**

### Property 9: Variable Frame Size Support

*For any* valid Opus packet with frame duration D (where D is 2.5, 5, 10, 20, 40, or 60 ms), the decoded output should contain exactly D * 48 samples per channel.

**Validates: Requirements 1.5, 3.6**

### Property 10: TOC Byte Parsing Correctness

*For any* valid TOC byte, parsing should correctly extract the configuration (bits 3-7), stereo flag (bit 2), and frame count code (bits 0-1) per RFC 6716 Section 3.1.

**Validates: Requirements 17.1, 17.2, 17.3, 17.4**

### Property 11: End Trimming Accuracy

*For any* end-of-stream with specified end position P, the total output samples should equal P minus the pre-skip value, with excess samples from the final packet discarded.

**Validates: Requirements 18.1, 18.2, 18.3**

### Property 12: PLC Sample Count Consistency

*For any* missing packet with expected duration D, invoking packet loss concealment should produce exactly D * 48 samples per channel, maintaining correct timing.

**Validates: Requirements 20.1, 20.4**

### Property 13: Comment Header Structure Validation

*For any* OpusTags header, the codec should validate that vendor string length and comment entry lengths do not exceed packet boundaries, rejecting malformed headers.

**Validates: Requirements 1.3, 14.2, 16.5, 16.6**

## **Error Handling**

### **Initialization Errors**
- **Invalid StreamInfo**: Return false from initialize()
- **libopus init failure**: Clean up resources, return false
- **Memory allocation**: Handle gracefully, return error
- **Invalid headers**: Reject initialization, log specific errors

### **Decoding Errors**
- **Corrupted packets**: Skip packet, output silence for that period
- **Invalid packet format**: Log warning, continue with next packet
- **Decoder errors**: Reset decoder state, attempt recovery
- **Buffer overflow**: Increase buffer size, retry operation

### **Header Processing Errors**
- **Missing headers**: Wait for required headers, timeout if not received
- **Invalid header format**: Reject stream, report format error
- **Unsupported features**: Report specific unsupported feature
- **Header sequence errors**: Reset header processing, start over

## **Performance Considerations**

### **Memory Usage**
- **Output buffering**: Use appropriately sized buffers for maximum frame size
- **Input processing**: Process packets immediately, minimal buffering
- **libopus overhead**: Let libopus manage its internal state efficiently
- **Channel mapping**: Optimize for common mono/stereo cases

### **CPU Efficiency**
- **libopus optimization**: Use libopus built-in optimizations (SIMD, etc.)
- **Gain processing**: Efficient fixed-point gain calculations
- **Pre-skip handling**: Minimize data copying during skip operations
- **Error handling**: Fast error detection and recovery paths

### **Decoding Efficiency**
- **Frame size handling**: Support variable frame sizes efficiently
- **Mode switching**: Handle SILK/CELT/Hybrid mode changes seamlessly
- **Bandwidth adaptation**: Efficient handling of bandwidth changes
- **Quality scaling**: Support different complexity settings

## **Integration Points**

### **With Demuxers**
- Accept MediaChunk data from any demuxer (OggDemuxer primarily)
- Handle packet data regardless of container format
- Process timestamp information for accurate positioning
- Support seeking through reset() method

### **With AudioCodec Architecture**
- Implement all AudioCodec interface methods consistently
- Return standard AudioFrame format (16-bit PCM at 48kHz)
- Handle initialization from StreamInfo parameters
- Provide codec capability reporting

### **With Existing Opus Implementation**
- Maintain compatibility with current Opus file support
- Provide equivalent or better performance than opusfile-based implementation
- Support all previously working Opus files
- Integrate with DemuxedStream bridge seamlessly

### **With libopus Library**
- Use libopus 1.3+ features and optimizations
- Handle libopus API correctly and efficiently
- Manage libopus decoder lifecycle properly
- Utilize libopus error reporting and recovery

## **Testing Strategy**

### **Property-Based Testing**

The implementation will use **RapidCheck** as the property-based testing library, consistent with other PsyMP3 codec tests.

**Configuration:**
- Minimum 100 iterations per property test
- Each property test must be tagged with: `**Feature: opus-codec, Property {number}: {property_text}**`

**Property Test Coverage:**
1. Header parsing round-trip tests with generated OpusHead bytes
2. Pre-skip removal tests with various pre-skip values (0 to 65535)
3. Output gain application tests with Q7.8 gain values (-32768 to 32767)
4. Header sequence validation with permutations of header packets
5. Corrupted packet handling with bit-flipped and truncated packets
6. Reset state consistency tests with decode-reset-decode sequences
7. Thread independence tests with concurrent codec instances
8. TOC byte parsing tests with all valid configuration values
9. End trimming tests with various end positions
10. PLC sample count tests with simulated packet loss

### **Unit Testing**

Unit tests complement property tests by verifying specific examples and edge cases:

- **Header Parsing**: Test with known-good OpusHead/OpusTags packets
- **Mode Detection**: Test SILK, CELT, and Hybrid mode identification
- **Channel Configurations**: Test mono, stereo, and surround configurations
- **Error Conditions**: Test specific error codes and recovery paths
- **Integration**: Test with real Opus files from various encoders

### **Integration Testing**

- Test OpusCodec with OggDemuxer using real Opus files
- Test seeking behavior with reset() and pre-roll
- Test MediaChunk to AudioFrame conversion pipeline
- Test DemuxedStream bridge integration

This design provides efficient, accurate Opus decoding that works with any container format while maintaining compatibility with existing functionality and integrating cleanly with the modern codec architecture.