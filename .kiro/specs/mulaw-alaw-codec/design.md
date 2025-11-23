# μ-LAW/A-LAW CODEC DESIGN

## Overview

The μ-law/A-law codec implementation provides ITU-T G.711 compliant audio decoding for telephony applications within PsyMP3 through two separate, specialized codec classes:

- **MuLawCodec**: Dedicated μ-law (G.711 μ-law) decoder for North American telephony standard
- **ALawCodec**: Dedicated A-law (G.711 A-law) decoder for European telephony standard

This separation aligns with PsyMP3's architecture where codec selection occurs at the MediaFactory level based on the codec_name field in StreamInfo. Each codec specializes in its respective format, eliminating runtime format detection and providing cleaner, more maintainable code.

The design emphasizes performance through lookup table-based conversion, standards compliance with ITU-T G.711 specifications, and seamless integration with PsyMP3's existing AudioCodec architecture. Both implementations support real-time decoding requirements for telephony applications while maintaining bit-perfect accuracy.

## Architecture

### Core Components

```
MuLawCodec (AudioCodec)
├── μ-law Conversion Table (256 entries)
├── Decoder Engine
│   ├── μ-law table-based conversion
│   ├── Multi-channel processing
│   └── Sample rate handling
└── Integration Layer
    ├── AudioCodec interface compliance
    ├── MediaChunk processing
    └── AudioFrame generation

ALawCodec (AudioCodec)
├── A-law Conversion Table (256 entries)
├── Decoder Engine
│   ├── A-law table-based conversion
│   ├── Multi-channel processing
│   └── Sample rate handling
└── Integration Layer
    ├── AudioCodec interface compliance
    ├── MediaChunk processing
    └── AudioFrame generation
```

### Design Rationale

**Lookup Table Approach**: Pre-computed conversion tables provide optimal performance for real-time telephony applications, eliminating the need for mathematical calculations during decoding. This approach is standard in telecommunications equipment and ensures consistent performance regardless of input data patterns.

**Separate Codec Classes**: Dedicated codec classes for μ-law and A-law eliminate runtime format detection and align with PsyMP3's architecture where codec selection occurs at the MediaFactory level. This provides cleaner code, better maintainability, and clearer separation of concerns.

**Shared Base Implementation**: Both codecs inherit from a common base class (SimplePCMCodec) to share common functionality while maintaining format-specific conversion logic.

**Container Agnostic Processing**: Both decoders separate container handling from audio decoding, allowing support for WAV, AU, and raw bitstreams through the same core engine. Container-specific parameters are extracted during initialization.

## Components and Interfaces

### MuLawCodec Class

```cpp
class MuLawCodec : public SimplePCMCodec {
public:
    explicit MuLawCodec(const StreamInfo& stream_info);
    
    // AudioCodec interface implementation
    bool canDecode(const StreamInfo& stream_info) const override;
    std::string getCodecName() const override { return "mulaw"; }

protected:
    // SimplePCMCodec implementation
    size_t convertSamples(const std::vector<uint8_t>& input_data, 
                         std::vector<int16_t>& output_samples) override;
    size_t getBytesPerInputSample() const override { return 1; }

private:
    // Conversion table (static, shared across instances)
    static const int16_t MULAW_TO_PCM[256];
    
    // Internal methods
    static void initializeMuLawTable();
};
```

### ALawCodec Class

```cpp
class ALawCodec : public SimplePCMCodec {
public:
    explicit ALawCodec(const StreamInfo& stream_info);
    
    // AudioCodec interface implementation
    bool canDecode(const StreamInfo& stream_info) const override;
    std::string getCodecName() const override { return "alaw"; }

protected:
    // SimplePCMCodec implementation
    size_t convertSamples(const std::vector<uint8_t>& input_data, 
                         std::vector<int16_t>& output_samples) override;
    size_t getBytesPerInputSample() const override { return 1; }

private:
    // Conversion table (static, shared across instances)
    static const int16_t ALAW_TO_PCM[256];
    
    // Internal methods
    static void initializeALawTable();
};
```

### Lookup Table Design

**μ-law Conversion Table**: 256-entry lookup table mapping 8-bit μ-law values to 16-bit signed PCM samples according to ITU-T G.711 specification. The table handles the logarithmic compression curve and sign bit encoding specific to μ-law.

**A-law Conversion Table**: 256-entry lookup table mapping 8-bit A-law values to 16-bit signed PCM samples according to ITU-T G.711 specification. The table implements the A-law compression characteristic with proper handling of the even-bit inversion.

**Memory Efficiency**: Tables are declared as static const arrays, shared across all codec instances to minimize memory usage while ensuring thread-safe read-only access.

### Codec Selection Logic

Format detection is handled at the MediaFactory level through demuxer integration:

```cpp
// MuLawCodec validation
bool MuLawCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "mulaw" || 
           stream_info.codec_name == "pcm_mulaw" ||
           stream_info.codec_name == "g711_mulaw";
}

// ALawCodec validation  
bool ALawCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "alaw" || 
           stream_info.codec_name == "pcm_alaw" ||
           stream_info.codec_name == "g711_alaw";
}
```

**Demuxer Responsibility**: Container demuxers (WAV, AU) are responsible for setting the appropriate codec_name in StreamInfo based on format tags:
- WAV format tag 0x0007 → codec_name = "mulaw"
- WAV format tag 0x0006 → codec_name = "alaw"  
- AU encoding 1 → codec_name = "mulaw"
- AU encoding 27 → codec_name = "alaw"

## Data Models

### StreamInfo Integration

The codec extracts necessary parameters from StreamInfo:
- **Sample Rate**: Defaults to 8 kHz for raw streams, uses container-specified rate otherwise
- **Channels**: Supports mono (standard) and stereo configurations
- **Format Identification**: Uses container format codes, file extensions, or MIME types
- **Bitstream Parameters**: Handles raw streams without container headers

### AudioFrame Output

Generated AudioFrames contain:
- **Sample Format**: 16-bit signed PCM in host byte order
- **Channel Layout**: Interleaved samples for multi-channel audio
- **Sample Rate**: Preserved from input or defaulted to 8 kHz
- **Frame Size**: Variable based on input chunk size

### Conversion Accuracy

**ITU-T G.711 Compliance**: Lookup tables are generated to match ITU-T G.711 specification exactly, ensuring bit-perfect accuracy for all 256 possible input values.

**Silence Handling**: Special handling for silence values (0xFF for μ-law maps to 0, 0x55 for A-law maps to -8 as closest-to-silence) ensures proper quiet periods in telephony applications.

## Error Handling

### Initialization Errors

- **Unsupported Format**: Returns false from initialize() if format cannot be determined
- **Invalid Parameters**: Validates sample rate and channel count ranges
- **Memory Allocation**: Handles table initialization failures gracefully

### Runtime Error Recovery

```cpp
std::unique_ptr<AudioFrame> MuLawALawCodec::decode(const MediaChunk& chunk) {
    if (!m_initialized) {
        Debug::log("MuLawALawCodec: Decode called on uninitialized codec");
        return nullptr;
    }
    
    if (chunk.data == nullptr || chunk.size == 0) {
        Debug::log("MuLawALawCodec: Invalid chunk data");
        return std::make_unique<AudioFrame>(m_sampleRate, m_channels, 0);
    }
    
    try {
        return processChunk(chunk.data, chunk.size);
    } catch (const std::exception& e) {
        Debug::log("MuLawALawCodec: Decode error: %s", e.what());
        return nullptr;
    }
}
```

### Robustness Features

- **Corrupted Data Tolerance**: All 8-bit values are valid inputs, preventing crashes on corrupted streams
- **Truncated Stream Handling**: Processes available data and returns partial frames
- **State Consistency**: Maintains decoder state even when errors occur
- **Graceful Degradation**: Continues operation with best-effort decoding when possible

## Testing Strategy

### Dual Testing Approach

The testing strategy employs both unit testing and property-based testing to provide comprehensive coverage:

- **Unit tests** verify specific examples, edge cases, and error conditions
- **Property tests** verify universal properties that should hold across all inputs
- Together they provide comprehensive coverage: unit tests catch concrete bugs, property tests verify general correctness

### Property-Based Testing

**Framework**: The implementation will use an appropriate property-based testing library for C++ (such as RapidCheck or similar). Each property-based test will run a minimum of 100 iterations to ensure thorough coverage of the input space.

**Property Test Requirements**:
- Each correctness property from the design document must be implemented as a property-based test
- Each test must be tagged with a comment explicitly referencing the correctness property: `// Feature: mulaw-alaw-codec, Property N: <property_text>`
- Each correctness property must be implemented by a single property-based test

**Property Test Coverage**:
1. **ITU-T G.711 Conversion Accuracy** (Property 1): Generate all 256 possible input values, verify each produces ITU-T compliant output
2. **Lookup Table Completeness** (Property 2): Generate all 256 input values, verify no crashes or invalid outputs
3. **Silence Value Handling** (Property 3): Test silence values produce expected PCM outputs
4. **Codec Selection Exclusivity** (Property 4): Generate various StreamInfo configurations, verify correct codec selection
5. **Sample Count Preservation** (Property 5): Generate random chunk sizes, verify output sample count matches input byte count
6. **Multi-channel Interleaving** (Property 6): Generate multi-channel data, verify correct interleaving pattern
7. **Thread Safety Independence** (Property 7): Run concurrent decode operations, verify no interference
8. **Error State Consistency** (Property 8): Generate error conditions followed by valid data, verify recovery
9. **Container Parameter Preservation** (Property 9): Generate StreamInfo with various parameters, verify preservation
10. **Raw Stream Defaults** (Property 10): Generate raw stream configurations, verify 8 kHz mono defaults

### Unit Testing

**Conversion Accuracy Tests**:
- Verify all 256 μ-law values convert to correct PCM samples
- Verify all 256 A-law values convert to correct PCM samples
- Compare against ITU-T reference implementations
- Test silence value handling (0xFF μ-law, 0x55 A-law)
- Test specific known input/output pairs from ITU-T test vectors

**Format Detection Tests**:
- Test WAV container format detection (format tags 0x0006, 0x0007)
- Test AU container format detection (encoding values 1, 27)
- Test file extension detection (.ul, .al files)
- Test MIME type detection (audio/basic, audio/x-alaw)
- Test codec_name variants (mulaw, pcm_mulaw, g711_mulaw, etc.)

**Error Handling Tests**:
- Test behavior with null chunk data
- Test behavior with zero-size chunks
- Test behavior with invalid StreamInfo parameters
- Test behavior with unsupported codec_name values
- Test memory allocation failure scenarios

### Integration Testing

**Container Integration**:
- Test with WAV files containing μ-law/A-law audio
- Test with AU files from Sun/NeXT systems
- Test raw bitstream processing without containers
- Verify parameter extraction from container headers
- Test seeking operations with sample-accurate positioning

**AudioCodec Interface Compliance**:
- Test initialize() with various StreamInfo configurations
- Test decode() with different chunk sizes (including VoIP-typical small packets)
- Test flush() behavior for stream completion
- Test reset() for seeking operations
- Test canDecode() with various StreamInfo configurations

**Multi-threading Tests**:
- Test concurrent codec instance creation
- Test concurrent decode operations on different instances
- Test concurrent access to shared lookup tables
- Verify thread-safe logging operations
- Test cleanup during concurrent operations

### Performance Testing

**Real-time Requirements**:
- Measure decoding performance against telephony real-time requirements
- Test with 8 kHz, 16 kHz, 32 kHz, and 48 kHz sample rates
- Verify multi-channel processing efficiency
- Benchmark against reference implementations
- Measure performance margin above real-time requirements

**Memory Usage**:
- Verify lookup table memory footprint (2KB total)
- Test concurrent codec instance memory usage
- Measure allocation overhead during operation
- Test memory efficiency with small packet sizes

**Throughput Testing**:
- Measure samples processed per second
- Test with continuous streams of varying lengths
- Verify cache efficiency with sequential access patterns
- Test performance with multiple concurrent streams

### Compliance Testing

**ITU-T G.711 Validation**:
- Process ITU-T test vectors for μ-law and A-law
- Verify bit-perfect accuracy against reference outputs
- Test edge cases and boundary conditions
- Validate signal-to-noise ratio characteristics
- Compare output against reference implementations for all 256 input values

**Standards Compliance**:
- Verify compliance with ITU-T G.711 recommendation
- Test interoperability with VoIP systems
- Validate RTP payload format compatibility
- Test with legacy telephony audio files

## Integration with PsyMP3 Architecture

### Modular Organization

Following PsyMP3's established codec architecture patterns, the μ-law and A-law codecs are organized as a PCM codec submodule:

**Directory Structure**:
```
src/codecs/pcm/           # PCM codec family implementation
  ├── Makefile.am         # Builds libpcmcodecs.a convenience library
  ├── SimplePCMCodec.cpp  # Base class for PCM-family codecs
  ├── MuLawCodec.cpp      # μ-law codec implementation
  └── ALawCodec.cpp       # A-law codec implementation

include/codecs/pcm/       # PCM codec family headers
  ├── SimplePCMCodec.h    # Base class interface
  ├── MuLawCodec.h        # μ-law codec interface
  └── ALawCodec.h         # A-law codec interface
```

**Namespace Pattern**:
```cpp
namespace PsyMP3 {
namespace Codec {
namespace PCM {
    class SimplePCMCodec : public AudioCodec { /* ... */ };
    class MuLawCodec : public SimplePCMCodec { /* ... */ };
    class ALawCodec : public SimplePCMCodec { /* ... */ };
}}}
```

**Build System Integration**:
- `src/codecs/Makefile.am` conditionally includes PCM codec subdirectory via `SUBDIRS`
- `src/codecs/pcm/Makefile.am` builds `libpcmcodecs.a` static library
- `src/Makefile.am` links the library: `psymp3_LDADD += codecs/pcm/libpcmcodecs.a`
- `configure.ac` includes codec Makefiles in `AC_CONFIG_FILES`

**Include Path Pattern**:
- Headers are included with full path: `#include "codecs/pcm/MuLawCodec.h"`
- Master header `psymp3.h` includes codec headers conditionally based on build flags
- Codec source files only include `psymp3.h` (master header rule)

**Using Declarations**:
```cpp
// In psymp3.h for backward compatibility
using PsyMP3::Codec::PCM::MuLawCodec;
using PsyMP3::Codec::PCM::ALawCodec;
```

### AudioCodec Interface Compliance

Both MuLawCodec and ALawCodec fully implement the AudioCodec interface, ensuring seamless integration with PsyMP3's media processing pipeline. The codecs register with MediaFactory using conditional compilation to handle builds where G.711 support is not required.

### MediaFactory Registration

```cpp
#ifdef ENABLE_MULAW_CODEC
void registerMuLawCodec() {
    AudioCodecFactory::registerCodec("mulaw", [](const StreamInfo& stream_info) {
        return std::make_unique<MuLawCodec>(stream_info);
    });
    
    AudioCodecFactory::registerCodec("pcm_mulaw", [](const StreamInfo& stream_info) {
        return std::make_unique<MuLawCodec>(stream_info);
    });
}
#endif

#ifdef ENABLE_ALAW_CODEC  
void registerALawCodec() {
    AudioCodecFactory::registerCodec("alaw", [](const StreamInfo& stream_info) {
        return std::make_unique<ALawCodec>(stream_info);
    });
    
    AudioCodecFactory::registerCodec("pcm_alaw", [](const StreamInfo& stream_info) {
        return std::make_unique<ALawCodec>(stream_info);
    });
}
#endif
```

### Conditional Compilation Support

Following PsyMP3's established patterns, the codec implementation uses conditional compilation guards to ensure clean builds when G.711 support is not available or desired. This maintains backward compatibility and reduces binary size for deployments that don't require telephony codec support.

### Debug Integration

The codec uses PsyMP3's Debug logging system for troubleshooting and development:
- Format detection logging
- Initialization status reporting
- Error condition reporting
- Performance metrics (when debug enabled)

### Thread Safety

Each codec instance maintains independent state, ensuring thread-safe operation in multi-threaded environments. The shared lookup tables are read-only static data, eliminating synchronization requirements while supporting concurrent decoding operations.

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: ITU-T G.711 Conversion Accuracy
*For any* 8-bit μ-law or A-law encoded value, the decoded 16-bit PCM output should match the ITU-T G.711 specification exactly for that input value.
**Validates: Requirements 1.1, 2.1, 6.1, 6.2, 6.4**

### Property 2: Lookup Table Completeness
*For any* 8-bit input value (0-255), both MuLawCodec and ALawCodec lookup tables should contain a valid 16-bit PCM output value.
**Validates: Requirements 1.7, 2.7**

### Property 3: Silence Value Handling
*For any* codec instance, decoding the silence value (0xFF for μ-law, 0x55 for A-law) should produce the specified silence PCM value (0 for μ-law, -8 for A-law).
**Validates: Requirements 1.6, 2.6, 6.6**

### Property 4: Codec Selection Exclusivity
*For any* StreamInfo with codec_name "mulaw", MuLawCodec.canDecode() should return true and ALawCodec.canDecode() should return false, and vice versa for "alaw".
**Validates: Requirements 9.6, 10.5, 10.6**

### Property 5: Sample Count Preservation
*For any* input MediaChunk with N bytes, the output AudioFrame should contain exactly N decoded PCM samples (since each input byte produces one output sample).
**Validates: Requirements 1.2, 2.2**

### Property 6: Multi-channel Interleaving Consistency
*For any* multi-channel audio stream, samples should be interleaved in the output AudioFrame such that for C channels, sample order is [Ch0_S0, Ch1_S0, ..., ChC-1_S0, Ch0_S1, Ch1_S1, ...].
**Validates: Requirements 7.6**

### Property 7: Thread Safety Independence
*For any* two codec instances operating concurrently, operations on one instance should not affect the state or output of the other instance.
**Validates: Requirements 11.1, 11.2**

### Property 8: Error State Consistency
*For any* codec instance that encounters an error during decode(), subsequent calls to decode() with valid data should still produce correct output (no persistent error state).
**Validates: Requirements 8.8**

### Property 9: Container Parameter Preservation
*For any* StreamInfo with specified sample_rate and channels, the output AudioFrame should use those exact values rather than defaults.
**Validates: Requirements 4.8, 7.5**

### Property 10: Raw Stream Default Parameters
*For any* raw bitstream without container headers (no sample_rate or channels in StreamInfo), the codec should default to 8 kHz mono.
**Validates: Requirements 3.2, 3.5, 7.7**

## Performance Considerations

### Lookup Table Optimization

**Cache Efficiency**: Tables are sized to fit in CPU cache (2KB total for both tables), ensuring fast access during high-volume decoding operations.

**Memory Access Patterns**: Sequential processing of audio samples provides optimal cache utilization and memory bandwidth usage.

**SIMD Considerations**: While the lookup table approach doesn't directly benefit from SIMD vectorization (due to the table lookup indirection), the sequential memory access pattern allows for efficient prefetching. Future optimizations could explore SIMD for multi-channel interleaving operations where beneficial, though the current scalar implementation already exceeds real-time requirements by a significant margin.

### Real-time Performance

**Latency Minimization**: Direct table lookup eliminates computational overhead, providing consistent low-latency decoding suitable for real-time telephony applications.

**Throughput Optimization**: The codec can process multiple samples per CPU cycle, easily exceeding real-time requirements for telephony sample rates.

**Packet Processing Efficiency**: Small packet sizes (typical in VoIP applications) are handled efficiently without allocation overhead, supporting real-time streaming requirements.

### Scalability

**Concurrent Processing**: Multiple codec instances can operate simultaneously without interference, supporting multi-line telephony applications and concurrent media playback.

**Resource Efficiency**: Minimal memory footprint per instance (excluding shared tables) allows for high-density codec deployment in server environments.

### Thread Safety Architecture

**Instance Independence**: Each codec instance maintains its own state (sample rate, channel count, initialization status) in instance variables, ensuring complete independence between concurrent codec instances.

**Shared Read-Only Tables**: The static const lookup tables (MULAW_TO_PCM and ALAW_TO_PCM) are shared across all instances but are read-only after initialization, eliminating the need for synchronization primitives.

**Lock-Free Operation**: The codec design avoids locks entirely by ensuring all shared data is immutable and all mutable state is instance-local. This provides optimal performance for concurrent decoding operations.

**Thread-Safe Logging**: Debug logging operations use PsyMP3's thread-safe Debug::log() facility, ensuring diagnostic output doesn't corrupt or interfere between threads.