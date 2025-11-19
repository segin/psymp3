# Native FLAC Decoder Design

## Overview

This document defines the design for a native FLAC audio decoder implementation that eliminates the dependency on libFLAC. The native decoder implements the complete FLAC decoding pipeline from bitstream parsing to PCM sample reconstruction, following RFC 9639 specifications. The implementation provides the same `FLACCodec` interface as the current libFLAC wrapper, allowing build-time selection between implementations via configure flags.

### Design Goals

1. **RFC 9639 Compliance**: Strict adherence to FLAC specification
2. **Zero External Dependencies**: Pure C++ implementation without libFLAC
3. **Drop-in Replacement**: Same `FLACCodec` interface as libFLAC wrapper
4. **High Performance**: Comparable or better performance than libFLAC
5. **Thread Safety**: Follow PsyMP3's public/private lock pattern
6. **Container Agnostic**: Support native FLAC and Ogg FLAC
7. **Security**: Robust against malicious input and DoS attacks
8. **Maintainability**: Clear, well-documented code structure

## Architecture

### Component Hierarchy

```
FLACCodec (AudioCodec interface)
    ├── BitstreamReader (bit-level reading)
    ├── FrameParser (frame structure parsing)
    │   ├── FrameHeaderParser
    │   └── FrameFooterParser
    ├── SubframeDecoder (channel decoding)
    │   ├── ConstantSubframeDecoder
    │   ├── VerbatimSubframeDecoder
    │   ├── FixedPredictorDecoder
    │   └── LPCPredictorDecoder
    ├── ResidualDecoder (entropy decoding)
    │   ├── RiceDecoder
    │   └── PartitionDecoder
    ├── ChannelDecorrelator (stereo processing)
    ├── SampleReconstructor (PCM output)
    ├── CRCValidator (integrity checking)
    ├── MetadataParser (metadata extraction)
    └── BitDepthConverter (format conversion)
```

### Class Structure

#### Core Decoder Class

```cpp
class NativeFLACDecoder {
public:
    // AudioCodec interface implementation
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    bool canDecode(const StreamInfo& stream_info) const override;
    
    // FLAC-specific interface
    bool supportsSeekReset() const;
    uint64_t getCurrentSample() const;
    FLACCodecStats getStats() const;
    
private:
    // Component instances
    std::unique_ptr<BitstreamReader> m_bitstream_reader;
    std::unique_ptr<FrameParser> m_frame_parser;
    std::unique_ptr<SubframeDecoder> m_subframe_decoder;
    std::unique_ptr<ResidualDecoder> m_residual_decoder;
    std::unique_ptr<ChannelDecorrelator> m_channel_decorrelator;
    std::unique_ptr<SampleReconstructor> m_sample_reconstructor;
    std::unique_ptr<CRCValidator> m_crc_validator;
    std::unique_ptr<MetadataParser> m_metadata_parser;
    std::unique_ptr<BitDepthConverter> m_bit_depth_converter;
    
    // State management
    DecoderState m_state;
    StreamInfo m_stream_info;
    FLACStreamInfo m_flac_stream_info;
    
    // Buffers
    std::vector<uint8_t> m_input_buffer;
    std::vector<int32_t> m_decode_buffer[FLAC_MAX_CHANNELS];
    std::vector<int16_t> m_output_buffer;
    
    // Threading
    mutable std::mutex m_state_mutex;
    mutable std::mutex m_decoder_mutex;
    mutable std::mutex m_buffer_mutex;
    
    // Statistics
    FLACCodecStats m_stats;
    std::atomic<uint64_t> m_current_sample;
};
```

## Components and Interfaces

### 1. BitstreamReader

**Purpose**: Efficient bit-level reading from byte-aligned input stream.

**Interface**:
```cpp
class BitstreamReader {
public:
    BitstreamReader();
    
    // Input management
    void feedData(const uint8_t* data, size_t size);
    void clearBuffer();
    size_t getAvailableBits() const;
    
    // Bit reading
    bool readBits(uint32_t& value, uint32_t bit_count);
    bool readBitsSigned(int32_t& value, uint32_t bit_count);
    bool readUnary(uint32_t& value);
    bool readUTF8(uint64_t& value);
    bool readRiceCode(int32_t& value, uint32_t rice_param);
    
    // Alignment
    bool alignToByte();
    bool isAligned() const;
    
    // Position tracking
    uint64_t getBitPosition() const;
    void resetPosition();
    
private:
    std::vector<uint8_t> m_buffer;
    size_t m_byte_position;
    uint32_t m_bit_position;  // 0-7 within current byte
    
    // Bit buffer for efficient reading
    uint64_t m_bit_cache;
    uint32_t m_cache_bits;
    
    void refillCache();
};
```

**Key Design Decisions**:
- Use 64-bit cache for efficient multi-bit reads
- Big-endian bit ordering per RFC 9639
- Automatic cache refill when needed
- Track position for seeking and error recovery

### 2. FrameParser

**Purpose**: Parse FLAC frame structure including header and footer.

**Interface**:
```cpp
struct FrameHeader {
    uint32_t block_size;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bit_depth;
    ChannelAssignment channel_assignment;
    bool is_variable_block_size;
    uint64_t coded_number;  // frame or sample number
    uint8_t crc8;
};

struct FrameFooter {
    uint16_t crc16;
};

class FrameParser {
public:
    FrameParser(BitstreamReader* reader, CRCValidator* crc);
    
    // Frame parsing
    bool findSync();
    bool parseFrameHeader(FrameHeader& header);
    bool parseFrameFooter(FrameFooter& footer);
    bool validateFrame(const FrameHeader& header, const FrameFooter& footer);
    
private:
    BitstreamReader* m_reader;
    CRCValidator* m_crc;
    
    bool parseBlockSize(FrameHeader& header);
    bool parseSampleRate(FrameHeader& header);
    bool parseChannels(FrameHeader& header);
    bool parseBitDepth(FrameHeader& header);
    bool parseCodedNumber(FrameHeader& header);
};
```

**Key Design Decisions**:
- Separate sync finding from header parsing
- Validate CRC-8 immediately after header
- Support both fixed and variable block size streams
- Handle uncommon block sizes and sample rates

### 3. SubframeDecoder

**Purpose**: Decode individual channel subframes using various prediction methods.

**Interface**:
```cpp
enum class SubframeType {
    CONSTANT,
    VERBATIM,
    FIXED,
    LPC
};

struct SubframeHeader {
    SubframeType type;
    uint32_t predictor_order;  // for FIXED and LPC
    uint32_t wasted_bits;
    uint32_t bit_depth;
};

class SubframeDecoder {
public:
    SubframeDecoder(BitstreamReader* reader, ResidualDecoder* residual);
    
    // Subframe decoding
    bool decodeSubframe(int32_t* output, uint32_t block_size, 
                       uint32_t bit_depth, bool is_side_channel);
    
private:
    BitstreamReader* m_reader;
    ResidualDecoder* m_residual;
    
    bool parseSubframeHeader(SubframeHeader& header, uint32_t bit_depth);
    bool decodeConstant(int32_t* output, uint32_t block_size, const SubframeHeader& header);
    bool decodeVerbatim(int32_t* output, uint32_t block_size, const SubframeHeader& header);
    bool decodeFixed(int32_t* output, uint32_t block_size, const SubframeHeader& header);
    bool decodeLPC(int32_t* output, uint32_t block_size, const SubframeHeader& header);
    
    // Prediction helpers
    void applyFixedPredictor(int32_t* samples, const int32_t* residuals,
                            uint32_t count, uint32_t order);
    void applyLPCPredictor(int32_t* samples, const int32_t* residuals,
                          const int32_t* coeffs, uint32_t count, 
                          uint32_t order, uint32_t shift);
};
```

**Key Design Decisions**:
- Separate decoder for each subframe type
- In-place sample reconstruction where possible
- Efficient fixed predictor implementation
- 64-bit arithmetic for LPC to prevent overflow

### 4. ResidualDecoder

**Purpose**: Decode Rice/Golomb entropy-coded residuals.

**Interface**:
```cpp
enum class CodingMethod {
    RICE_4BIT,
    RICE_5BIT
};

struct PartitionInfo {
    uint32_t rice_parameter;
    bool is_escaped;
    uint32_t escape_bits;
    uint32_t sample_count;
};

class ResidualDecoder {
public:
    ResidualDecoder(BitstreamReader* reader);
    
    // Residual decoding
    bool decodeResidual(int32_t* output, uint32_t block_size, 
                       uint32_t predictor_order);
    
private:
    BitstreamReader* m_reader;
    
    bool parseResidualHeader(CodingMethod& method, uint32_t& partition_order);
    bool decodePartition(int32_t* output, const PartitionInfo& info);
    bool decodeRiceCode(int32_t& value, uint32_t rice_param);
    bool decodeEscapedPartition(int32_t* output, const PartitionInfo& info);
    
    // Zigzag encoding/decoding
    inline int32_t unfoldSigned(uint32_t folded) {
        return (folded & 1) ? -static_cast<int32_t>((folded + 1) >> 1)
                            : static_cast<int32_t>(folded >> 1);
    }
};
```

**Key Design Decisions**:
- Support both 4-bit and 5-bit Rice parameters
- Handle escaped partitions with unencoded samples
- Efficient unary decoding using bit manipulation
- Validate residual values fit in 32-bit range

### 5. ChannelDecorrelator

**Purpose**: Undo stereo decorrelation (left-side, right-side, mid-side).

**Interface**:
```cpp
enum class ChannelAssignment {
    INDEPENDENT,
    LEFT_SIDE,
    RIGHT_SIDE,
    MID_SIDE
};

class ChannelDecorrelator {
public:
    ChannelDecorrelator();
    
    // Decorrelation
    void decorrelate(int32_t** channels, uint32_t block_size,
                    ChannelAssignment assignment);
    
private:
    void decorrelateLeftSide(int32_t* left, int32_t* side, uint32_t count);
    void decorrelateRightSide(int32_t* side, int32_t* right, uint32_t count);
    void decorrelateMidSide(int32_t* mid, int32_t* side, uint32_t count);
};
```

**Key Design Decisions**:
- In-place decorrelation to minimize memory copies
- Proper handling of mid-side odd sample reconstruction
- Side channel has extra bit of precision

### 6. SampleReconstructor

**Purpose**: Convert decoded samples to output PCM format with bit depth conversion.

**Interface**:
```cpp
class SampleReconstructor {
public:
    SampleReconstructor();
    
    // Sample reconstruction
    void reconstructSamples(int16_t* output, int32_t** channels,
                          uint32_t block_size, uint32_t channel_count,
                          uint32_t source_bit_depth);
    
private:
    // Bit depth conversion
    int16_t convertTo16Bit(int32_t sample, uint32_t source_bit_depth);
    
    // Conversion helpers
    int16_t upscale8To16(int32_t sample);
    int16_t downscale24To16(int32_t sample);
    int16_t downscale32To16(int32_t sample);
};
```

**Key Design Decisions**:
- Interleave channels during reconstruction
- Proper rounding for downscaling
- Prevent clipping during conversion
- Optimize for common 16-bit case

### 7. CRCValidator

**Purpose**: Validate frame integrity using CRC-8 and CRC-16.

**Interface**:
```cpp
class CRCValidator {
public:
    CRCValidator();
    
    // CRC computation
    uint8_t computeCRC8(const uint8_t* data, size_t length);
    uint16_t computeCRC16(const uint8_t* data, size_t length);
    
    // Incremental CRC
    void resetCRC8();
    void resetCRC16();
    void updateCRC8(uint8_t byte);
    void updateCRC16(uint8_t byte);
    uint8_t getCRC8() const;
    uint16_t getCRC16() const;
    
private:
    // Lookup tables for performance
    static const uint8_t CRC8_TABLE[256];
    static const uint16_t CRC16_TABLE[256];
    
    uint8_t m_crc8;
    uint16_t m_crc16;
    
    static void initializeTables();
};
```

**Key Design Decisions**:
- Precomputed lookup tables for performance
- CRC-8 polynomial: 0x07 (x^8 + x^2 + x^1 + x^0)
- CRC-16 polynomial: 0x8005 (x^16 + x^15 + x^2 + x^0)
- Support incremental CRC for streaming

### 8. MetadataParser

**Purpose**: Parse FLAC metadata blocks (STREAMINFO, seek table, Vorbis comments, etc.).

**Interface**:
```cpp
enum class MetadataType {
    STREAMINFO = 0,
    PADDING = 1,
    APPLICATION = 2,
    SEEKTABLE = 3,
    VORBIS_COMMENT = 4,
    CUESHEET = 5,
    PICTURE = 6
};

struct StreamInfoMetadata {
    uint32_t min_block_size;
    uint32_t max_block_size;
    uint32_t min_frame_size;
    uint32_t max_frame_size;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
    uint64_t total_samples;
    uint8_t md5_sum[16];
};

class MetadataParser {
public:
    MetadataParser(BitstreamReader* reader);
    
    // Metadata parsing
    bool parseMetadataBlock(MetadataType& type, bool& is_last);
    bool parseStreamInfo(StreamInfoMetadata& info);
    bool parseSeekTable(std::vector<SeekPoint>& points);
    bool parseVorbisComment(VorbisComment& comment);
    bool skipMetadataBlock(uint32_t length);
    
private:
    BitstreamReader* m_reader;
};
```

**Key Design Decisions**:
- Parse STREAMINFO immediately (mandatory)
- Optional parsing of other metadata types
- Skip unknown metadata blocks gracefully
- Store seek table for efficient seeking

## Data Models

### Decoder State

```cpp
enum class DecoderState {
    UNINITIALIZED,
    INITIALIZED,
    DECODING,
    ERROR,
    END_OF_STREAM
};

struct FLACStreamInfo {
    StreamInfoMetadata streaminfo;
    std::vector<SeekPoint> seek_table;
    VorbisComment vorbis_comment;
    bool has_streaminfo;
    bool has_seek_table;
    bool has_vorbis_comment;
};
```

### Frame State

```cpp
struct FrameState {
    FrameHeader header;
    uint32_t samples_decoded;
    uint64_t frame_start_position;
    bool is_valid;
};
```

### Buffer Management

```cpp
struct BufferConfig {
    static constexpr size_t MAX_BLOCK_SIZE = 65535;
    static constexpr size_t MAX_CHANNELS = 8;
    static constexpr size_t INPUT_BUFFER_SIZE = 64 * 1024;  // 64KB
    static constexpr size_t DECODE_BUFFER_SIZE = MAX_BLOCK_SIZE * MAX_CHANNELS;
};
```

## Error Handling

### Error Types

```cpp
enum class FLACError {
    NONE,
    INVALID_SYNC,
    INVALID_HEADER,
    INVALID_SUBFRAME,
    INVALID_RESIDUAL,
    CRC_MISMATCH,
    BUFFER_UNDERFLOW,
    MEMORY_ALLOCATION,
    UNSUPPORTED_FEATURE,
    CORRUPTED_DATA
};

class FLACException : public std::runtime_error {
public:
    FLACException(FLACError error, const std::string& message);
    FLACError getError() const { return m_error; }
private:
    FLACError m_error;
};
```

### Error Recovery Strategy

1. **Sync Loss**: Search for next frame sync pattern
2. **Header CRC Fail**: Skip frame, continue with next
3. **Frame CRC Fail**: Log warning, may use decoded data (RFC allows)
4. **Subframe Error**: Output silence for affected channel
5. **Memory Error**: Return error, clean up resources
6. **Unrecoverable**: Transition to ERROR state

## Testing Strategy

### Unit Tests

1. **BitstreamReader Tests**
   - Bit reading accuracy
   - Unary decoding
   - UTF-8 number decoding
   - Buffer management

2. **FrameParser Tests**
   - Sync pattern detection
   - Header parsing
   - CRC validation
   - Edge cases (uncommon sizes/rates)

3. **SubframeDecoder Tests**
   - Each subframe type
   - Wasted bits handling
   - Predictor accuracy

4. **ResidualDecoder Tests**
   - Rice code decoding
   - Partition handling
   - Escaped partitions
   - Zigzag encoding

5. **ChannelDecorrelator Tests**
   - Each stereo mode
   - Mid-side odd samples
   - Multi-channel

### Integration Tests

1. **Complete Frame Decoding**
   - Decode RFC example files
   - Verify MD5 checksums
   - Compare with libFLAC output

2. **Container Support**
   - Native FLAC files
   - Ogg FLAC streams
   - Various encoders

3. **Edge Cases**
   - Variable block sizes
   - Uncommon bit depths
   - Multi-channel audio
   - Highly compressed frames

### Performance Tests

1. **Benchmark against libFLAC**
   - 44.1kHz/16-bit (CD quality)
   - 96kHz/24-bit (high-res)
   - Various compression levels

2. **Memory Usage**
   - Peak memory consumption
   - Buffer allocation patterns

3. **Threading**
   - Concurrent decoder instances
   - Lock contention

## Performance Optimization

### Critical Path Optimizations

1. **Bitstream Reading**
   - 64-bit cache for multi-bit reads
   - Minimize buffer refills
   - Inline hot functions

2. **Rice Decoding**
   - Optimized unary decoding (CLZ instruction)
   - Batch processing where possible
   - Minimize branches

3. **Prediction**
   - SIMD for LPC (future enhancement)
   - Loop unrolling for fixed predictors
   - Efficient coefficient application

4. **Memory Access**
   - Cache-friendly data layout
   - Minimize allocations in decode path
   - Reuse buffers

### Memory Optimization

1. **Buffer Pooling**
   - Reuse decode buffers across frames
   - Pre-allocate based on max block size

2. **Lazy Allocation**
   - Allocate metadata structures only when needed
   - Skip unused metadata types

## Security Considerations

### Input Validation

1. **Bounds Checking**
   - All array accesses validated
   - Buffer overflow prevention
   - Integer overflow checks

2. **Resource Limits**
   - Maximum block size enforcement
   - Maximum partition order limits
   - Timeout for sync search

3. **Malicious Input Protection**
   - Validate all header fields
   - Reject forbidden patterns
   - Limit recursion depth

### DoS Prevention

1. **Excessive Resource Usage**
   - Limit memory allocation
   - Timeout long operations
   - Detect infinite loops

2. **Fuzz Testing**
   - Regular fuzzing with AFL/libFuzzer
   - Sanitizer builds (ASAN, UBSAN)
   - Corpus of malformed files

## Build System Integration

### Configure Options

```bash
# Enable native FLAC decoder
./configure --enable-native-flac

# Disable native FLAC decoder (use libFLAC)
./configure --disable-native-flac

# Auto-detect (prefer native if available)
./configure
```

### Conditional Compilation

```cpp
// In FLACCodec.h
#ifdef HAVE_NATIVE_FLAC
    #include "NativeFLACDecoder.h"
    using FLACCodecImpl = NativeFLACDecoder;
#elif defined(HAVE_FLAC)
    #include "LibFLACWrapper.h"
    using FLACCodecImpl = LibFLACWrapper;
#else
    #error "No FLAC decoder implementation available"
#endif

// FLACCodec is typedef to selected implementation
using FLACCodec = FLACCodecImpl;
```

### Build Dependencies

**Native Decoder**:
- No external dependencies
- C++17 standard library only

**LibFLAC Wrapper** (fallback):
- libFLAC development headers
- libFLAC library

## Migration Path

### Phase 1: Core Implementation
- Bitstream reader
- Frame parser
- Basic subframe decoders
- Simple residual decoder

### Phase 2: Complete Decoding
- All subframe types
- Full residual decoding
- Channel decorrelation
- Bit depth conversion

### Phase 3: Metadata and Features
- Metadata parsing
- Seek table support
- Ogg FLAC support
- MD5 validation

### Phase 4: Optimization
- Performance tuning
- Memory optimization
- SIMD enhancements
- Profiling and benchmarking

### Phase 5: Hardening
- Security audit
- Fuzz testing
- Edge case handling
- Documentation

## Compatibility Matrix

| Feature | Native Decoder | LibFLAC Wrapper |
|---------|---------------|-----------------|
| Native FLAC | ✓ | ✓ |
| Ogg FLAC | ✓ | ✓ |
| All bit depths (4-32) | ✓ | ✓ |
| Variable block size | ✓ | ✓ |
| Multi-channel (1-8) | ✓ | ✓ |
| Seek table | ✓ | ✓ |
| Metadata parsing | ✓ | ✓ |
| MD5 validation | ✓ | ✓ |
| CRC validation | ✓ | ✓ |
| External dependencies | None | libFLAC |
| Binary size | Smaller | Larger |
| Performance | Comparable | Reference |

## Success Criteria

1. **Correctness**: Pass all RFC 9639 compliance tests
2. **Compatibility**: Decode all test FLAC files identically to libFLAC
3. **Performance**: Within 10% of libFLAC performance
4. **Memory**: Use ≤ 2x memory of libFLAC
5. **Security**: Pass fuzzing with no crashes
6. **Integration**: Drop-in replacement for existing FLACCodec
7. **Build**: Clean builds with and without native decoder

## Future Enhancements

1. **SIMD Optimization**: Use SSE/AVX for prediction and residual decoding
2. **Multi-threading**: Parallel subframe decoding for multi-channel
3. **Encoding Support**: Add FLAC encoding capability
4. **Hardware Acceleration**: GPU-accelerated decoding
5. **Streaming Optimization**: Zero-copy paths for network streams
