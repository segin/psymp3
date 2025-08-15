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

### Unit Testing

**Conversion Accuracy Tests**:
- Verify all 256 μ-law values convert to correct PCM samples
- Verify all 256 A-law values convert to correct PCM samples
- Compare against ITU-T reference implementations
- Test silence value handling (0xFF μ-law, 0x55 A-law)

**Format Detection Tests**:
- Test WAV container format detection (format tags 0x0006, 0x0007)
- Test AU container format detection (encoding values 1, 27)
- Test file extension detection (.ul, .al files)
- Test MIME type detection (audio/basic, audio/x-alaw)

### Integration Testing

**Container Integration**:
- Test with WAV files containing μ-law/A-law audio
- Test with AU files from Sun/NeXT systems
- Test raw bitstream processing without containers
- Verify parameter extraction from container headers

**AudioCodec Interface Compliance**:
- Test initialize() with various StreamInfo configurations
- Test decode() with different chunk sizes
- Test flush() behavior for stream completion
- Test reset() for seeking operations

### Performance Testing

**Real-time Requirements**:
- Measure decoding performance against telephony real-time requirements
- Test with 8 kHz, 16 kHz, and higher sample rates
- Verify multi-channel processing efficiency
- Benchmark against reference implementations

**Memory Usage**:
- Verify lookup table memory footprint
- Test concurrent codec instance memory usage
- Measure allocation overhead during operation

### Compliance Testing

**ITU-T G.711 Validation**:
- Process ITU-T test vectors for μ-law and A-law
- Verify bit-perfect accuracy against reference outputs
- Test edge cases and boundary conditions
- Validate signal-to-noise ratio characteristics

## Integration with PsyMP3 Architecture

### AudioCodec Interface Compliance

The MuLawALawCodec fully implements the AudioCodec interface, ensuring seamless integration with PsyMP3's media processing pipeline. The codec registers with MediaFactory using conditional compilation to handle builds where G.711 support is not required.

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

## Performance Considerations

### Lookup Table Optimization

**Cache Efficiency**: Tables are sized to fit in CPU cache (2KB total for both tables), ensuring fast access during high-volume decoding operations.

**Memory Access Patterns**: Sequential processing of audio samples provides optimal cache utilization and memory bandwidth usage.

### Real-time Performance

**Latency Minimization**: Direct table lookup eliminates computational overhead, providing consistent low-latency decoding suitable for real-time telephony applications.

**Throughput Optimization**: The codec can process multiple samples per CPU cycle, easily exceeding real-time requirements for telephony sample rates.

### Scalability

**Concurrent Processing**: Multiple codec instances can operate simultaneously without interference, supporting multi-line telephony applications and concurrent media playback.

**Resource Efficiency**: Minimal memory footprint per instance (excluding shared tables) allows for high-density codec deployment in server environments.