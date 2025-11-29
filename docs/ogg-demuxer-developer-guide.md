# OggDemuxer Developer Guide

## Overview

The OggDemuxer is a comprehensive implementation of the Ogg container format demuxer for PsyMP3. It follows the exact behavior patterns of the reference implementations (libvorbisfile and libopusfile) to ensure maximum compatibility and correctness.

### RFC Compliance

The OggDemuxer is compliant with the following specifications:

- **RFC 3533**: The Ogg Encapsulation Format Version 0 - Core container format
- **RFC 7845**: Ogg Encapsulation for the Opus Audio Codec - Opus-specific handling
- **RFC 9639 Section 10.1**: FLAC-in-Ogg encapsulation - FLAC audio in Ogg containers
- **Vorbis I Specification**: Vorbis codec header and packet handling

### Supported Codecs

| Codec | File Extensions | Signature | RFC/Spec |
|-------|-----------------|-----------|----------|
| Vorbis | .ogg | `\x01vorbis` | Vorbis I Spec |
| Opus | .opus, .oga | `OpusHead` | RFC 7845 |
| FLAC | .oga | `\x7fFLAC` | RFC 9639 §10.1 |
| Speex | .spx | `Speex   ` | Speex Spec |
| Theora | .ogv | `\x80theora` | Theora Spec |

## Architecture

### Core Components

The OggDemuxer is built around several key components:

1. **Container Parsing**: Uses libogg for robust Ogg page and packet parsing
2. **Codec Detection**: Identifies audio codecs from packet signatures
3. **Seeking System**: Implements bisection search for efficient timestamp-based seeking
4. **Granule Position Arithmetic**: Safe handling of wraparound granule position encoding
5. **Page Extraction**: Reference-pattern page discovery and backward scanning
6. **Data Streaming**: Efficient packet buffering and delivery
7. **FLAC-in-Ogg Handler**: RFC 9639 Section 10.1 compliant FLAC encapsulation

### Threading Safety

The OggDemuxer follows PsyMP3's public/private lock pattern:

- **Public methods**: Acquire locks and call private `_unlocked` implementations
- **Private methods**: Perform work without acquiring locks
- **Lock order**: `m_ogg_state_mutex` → `m_packet_queue_mutex` → `m_page_cache_mutex` → `m_seek_hints_mutex`

### Lock Acquisition Order

To prevent deadlocks, locks must always be acquired in this order:

1. `m_ogg_state_mutex` - Protects streams, ogg_streams, sync_state, seeking state
2. `m_packet_queue_mutex` - Protects packet queues in streams
3. `m_page_cache_mutex` - Protects page cache for seeking optimization
4. `m_seek_hints_mutex` - Protects seek hint cache

## Extending Codec Support

### Adding a New Codec

To add support for a new codec in Ogg containers:

#### 1. Codec Detection

Add codec signature detection in `identifyCodec()`:

```cpp
std::string OggDemuxer::identifyCodec(const std::vector<uint8_t>& packet_data) {
    // Existing codec checks...
    
    // Add your codec signature check
    if (hasSignature(packet_data, "YourCodecSignature")) {
        return "your_codec_name";
    }
    
    return ""; // Unknown codec
}
```

#### 2. Header Parsing

Implement header parsing function following the pattern:

```cpp
bool OggDemuxer::parseYourCodecHeaders(OggStream& stream, const OggPacket& packet) {
    try {
        // Validate packet size and signature
        if (packet.data.size() < minimum_header_size) {
            return false;
        }
        
        if (!hasSignature(packet.data, "YourCodecSignature")) {
            return false;
        }
        
        // Parse header based on packet type
        if (stream.header_packets.empty()) {
            // Identification header
            return parseYourCodecIdentificationHeader(stream, packet);
        } else if (stream.header_packets.size() == 1) {
            // Comment header (if applicable)
            return parseYourCodecCommentHeader(stream, packet);
        } else {
            // Additional headers (if applicable)
            return parseYourCodecSetupHeader(stream, packet);
        }
        
    } catch (const std::exception& e) {
        Debug::log("ogg", "Exception parsing YourCodec headers: ", e.what());
        return false;
    }
}
```

#### 3. Time Conversion

Add granule-to-time conversion logic in `granuleToMs()` and `msToGranule()`:

```cpp
uint64_t OggDemuxer::granuleToMs(uint64_t granule, uint32_t stream_id) const {
    // Existing codec conversions...
    
    if (stream.codec_name == "your_codec_name") {
        // Implement your codec's granule-to-time conversion
        // Example for sample-based codecs:
        if (stream.sample_rate > 0) {
            return (granule * 1000) / stream.sample_rate;
        }
    }
    
    return 0;
}
```

#### 4. Integration with processPages()

Add your codec to the header parsing switch in `processPages()`:

```cpp
// In processPages() method
if (stream.codec_name == "your_codec_name") {
    is_header_packet = parseYourCodecHeaders(stream, ogg_packet);
} else
```

#### 5. Conditional Compilation

Add conditional compilation support:

```cpp
// In header file
#ifdef HAVE_YOUR_CODEC
    bool parseYourCodecHeaders(OggStream& stream, const OggPacket& packet);
#endif

// In implementation
#ifdef HAVE_YOUR_CODEC
if (stream.codec_name == "your_codec_name") {
    is_header_packet = parseYourCodecHeaders(stream, ogg_packet);
} else
#endif
```

#### 6. Build System Integration

Update `configure.ac` to detect your codec:

```bash
# Check for your codec library
PKG_CHECK_MODULES([YOUR_CODEC], [your-codec-lib >= 1.0], [
    have_your_codec=yes
    AC_DEFINE([HAVE_YOUR_CODEC], [1], [Define to 1 if your codec is available])
], [
    have_your_codec=no
])

# Update OggDemuxer availability check
if test "x$have_vorbis" = "xyes" || test "x$have_opus" = "xyes" || test "x$have_your_codec" = "xyes"; then
  have_oggdemuxer=yes
elif test "x$have_flac" = "xyes" && test "x$have_ogg" = "xyes"; then
  have_oggdemuxer=yes
fi
```

### Codec-Specific Considerations

#### Sample-Based Codecs (like Vorbis, FLAC)

For codecs where granule position represents sample count:

```cpp
// Simple conversion
uint64_t timestamp_ms = (granule * 1000) / sample_rate;
uint64_t granule = (timestamp_ms * sample_rate) / 1000;
```

#### Frame-Based Codecs (like Opus)

For codecs with fixed frame sizes and pre-skip:

```cpp
// Account for pre-skip and frame rate
uint64_t samples = granule - pre_skip;
uint64_t timestamp_ms = (samples * 1000) / 48000; // Opus uses 48kHz granule rate
```

#### Variable Frame Size Codecs

For codecs with variable frame sizes, you may need to:

1. Parse frame headers to determine sample counts
2. Maintain running totals of samples processed
3. Use packet-level granule position tracking

### Error Handling Patterns

Follow reference implementation error handling:

```cpp
bool OggDemuxer::parseYourCodecHeaders(OggStream& stream, const OggPacket& packet) {
    try {
        // Parsing logic here
        
    } catch (const std::bad_alloc& e) {
        // Memory allocation failure
        reportError("Memory", "Memory allocation failed parsing YourCodec headers", 
                   OggErrorCodes::DEMUX_EFAULT);
        return false;
    } catch (const std::exception& e) {
        // Malformed data - parse what's possible and continue
        Debug::log("ogg", "Exception parsing YourCodec headers: ", e.what());
        reportError("Format", "Malformed YourCodec header data", 
                   OggErrorCodes::DEMUX_EBADHEADER);
        return false;
    }
}
```

## FLAC-in-Ogg Support (RFC 9639 Section 10.1)

The OggDemuxer provides full support for FLAC audio encapsulated in Ogg containers per RFC 9639 Section 10.1. This is distinct from native FLAC files (.flac) which use the FLACDemuxer.

### File Extensions

- `.oga` - Ogg Audio (may contain FLAC, Vorbis, Opus, or Speex)
- `.ogg` - Generic Ogg (typically Vorbis, but may contain FLAC)

### FLAC-in-Ogg Identification Header Structure

The first packet in a FLAC-in-Ogg stream is exactly 51 bytes:

```
Offset  Size  Description
------  ----  -----------
0       5     Signature: "\x7fFLAC" (0x7F 0x46 0x4C 0x41 0x43)
5       1     Mapping version major (0x01 for version 1.0)
6       1     Mapping version minor (0x00 for version 1.0)
7       2     Header packet count (big-endian, 0 = unknown)
9       4     fLaC signature (0x66 0x4C 0x61 0x43)
13      4     Metadata block header for STREAMINFO
17      34    STREAMINFO metadata block
```

### First Page Requirements

Per RFC 9639 Section 10.1, the first page of a FLAC-in-Ogg stream:
- MUST be exactly 79 bytes (27-byte header + 1 lacing value + 51-byte packet)
- MUST have the BOS (Beginning of Stream) flag set
- MUST have granule position 0

### STREAMINFO Extraction

The STREAMINFO block (34 bytes) contains essential audio parameters:

```cpp
// STREAMINFO bit layout (RFC 9639)
// Bits 0-15:   Minimum block size (samples)
// Bits 16-31:  Maximum block size (samples)
// Bits 32-55:  Minimum frame size (bytes, 24 bits)
// Bits 56-79:  Maximum frame size (bytes, 24 bits)
// Bits 80-99:  Sample rate (20 bits)
// Bits 100-102: Number of channels minus 1 (3 bits)
// Bits 103-107: Bits per sample minus 1 (5 bits)
// Bits 108-143: Total samples (36 bits)
// Bits 144-271: MD5 signature (128 bits)
```

### Granule Position Handling

FLAC-in-Ogg uses sample-based granule positions:

| Page Type | Granule Position |
|-----------|------------------|
| Header pages | 0 |
| Audio pages with completed packet | Sample count at end of last completed packet |
| Audio pages without completed packet | 0xFFFFFFFFFFFFFFFF |

```cpp
// Check for special "no packet" granule
bool OggDemuxer::isFlacOggNoPacketGranule(uint64_t granule_position) {
    return granule_position == 0xFFFFFFFFFFFFFFFFULL;
}

// Convert granule to samples (direct mapping for FLAC)
uint64_t OggDemuxer::flacOggGranuleToSamples(uint64_t granule_position, 
                                              const OggStream& stream) const {
    if (isFlacOggNoPacketGranule(granule_position)) {
        return 0;
    }
    return granule_position;  // Direct sample count
}
```

### Audio Packet Structure

Each audio packet in a FLAC-in-Ogg stream contains exactly one FLAC frame:
- No additional encapsulation
- Frame header starts immediately at packet offset 0
- Frame CRC is validated by the FLAC codec, not the demuxer

### Chaining Support

Per RFC 9639 Section 10.1, audio property changes require a new stream:
- Current stream ends with EOS page
- New stream begins with BOS page
- New identification header with updated STREAMINFO

```cpp
bool OggDemuxer::flacOggRequiresChaining(const OggStream& new_stream_info, 
                                          const OggStream& current_stream) const {
    return new_stream_info.sample_rate != current_stream.sample_rate ||
           new_stream_info.channels != current_stream.channels ||
           new_stream_info.bits_per_sample != current_stream.bits_per_sample;
}
```

### Version Handling

The OggDemuxer supports FLAC-in-Ogg mapping version 1.0 (0x01 0x00):

```cpp
bool OggDemuxer::handleFlacOggVersionMismatch(uint8_t major_version, 
                                               uint8_t minor_version) {
    if (major_version == 1 && minor_version == 0) {
        return true;  // Fully supported
    }
    if (major_version == 1) {
        // Minor version mismatch - attempt parsing with warning
        Debug::log("ogg", "FLAC-in-Ogg: Unknown minor version %d.%d, attempting parse",
                   major_version, minor_version);
        return true;
    }
    // Major version mismatch - unsupported
    Debug::log("ogg", "FLAC-in-Ogg: Unsupported major version %d.%d",
               major_version, minor_version);
    return false;
}
```

### Metadata Handling

FLAC-in-Ogg streams typically include Vorbis comment metadata:
- First header packet after identification SHOULD be Vorbis comment
- Uses same format as Ogg Vorbis comment header
- Provides artist, title, album, and other metadata

### Duration Calculation

Duration is calculated from STREAMINFO or last granule position:

```cpp
// Prefer STREAMINFO total_samples if available
if (stream.total_samples > 0 && stream.sample_rate > 0) {
    duration_ms = (stream.total_samples * 1000) / stream.sample_rate;
} else {
    // Fall back to last granule position
    uint64_t last_granule = getLastGranulePosition();
    if (last_granule > 0 && stream.sample_rate > 0) {
        duration_ms = (last_granule * 1000) / stream.sample_rate;
    }
}
```

### Seeking in FLAC-in-Ogg

Seeking uses the same bisection algorithm as other Ogg codecs:
1. Convert target timestamp to granule position
2. Bisect file to find page with target granule
3. Reset stream state and continue from found position

Since FLAC frames are variable-length, seeking lands on the nearest frame boundary.

## Testing New Codecs

### Unit Tests

Create comprehensive unit tests for your codec:

```cpp
// tests/test_your_codec_ogg.cpp
#include "test_framework.h"

void test_your_codec_identification() {
    // Test codec signature detection
    std::vector<uint8_t> header_data = {/* your codec signature */};
    OggDemuxer demuxer(/* test IOHandler */);
    
    std::string codec = demuxer.identifyCodec(header_data);
    ASSERT_EQ(codec, "your_codec_name");
}

void test_your_codec_header_parsing() {
    // Test header parsing with valid data
    // Test error handling with malformed data
    // Test all header types (ID, comment, setup)
}

void test_your_codec_time_conversion() {
    // Test granule-to-time conversion accuracy
    // Test edge cases (zero, maximum values)
    // Test pre-skip handling if applicable
}
```

### Integration Tests

Test with real Ogg files containing your codec:

```cpp
void test_your_codec_real_file() {
    auto handler = std::make_unique<FileIOHandler>("test_files/sample.your_codec.ogg");
    OggDemuxer demuxer(std::move(handler));
    
    ASSERT_TRUE(demuxer.parseContainer());
    
    auto streams = demuxer.getStreams();
    ASSERT_FALSE(streams.empty());
    ASSERT_EQ(streams[0].codec_name, "your_codec_name");
    
    // Test seeking, duration calculation, packet reading
}
```

## Performance Considerations

### Memory Usage

- Implement bounded packet queues to prevent memory exhaustion
- Use minimal copying in packet processing
- Clean up codec-specific resources properly

### I/O Efficiency

- Minimize file seeks during header parsing
- Use efficient granule position caching for seeking
- Implement read-ahead buffering for network streams

### CPU Efficiency

- Optimize header parsing for common cases
- Use efficient signature matching algorithms
- Avoid unnecessary data copying

## Debugging and Troubleshooting

### Debug Logging

Use PsyMP3's debug logging system:

```cpp
Debug::log("ogg", "YourCodec: Parsing identification header, size=", packet.data.size());
Debug::log("ogg", "YourCodec: Sample rate=", sample_rate, ", channels=", channels);
```

### Common Issues

1. **Signature Detection Failures**: Verify packet signatures match specification
2. **Header Parsing Errors**: Check endianness and field offsets
3. **Time Conversion Issues**: Verify granule position interpretation
4. **Seeking Problems**: Ensure granule comparison works correctly
5. **Memory Leaks**: Use RAII and proper cleanup in error paths

### Validation Tools

Use reference implementations to validate behavior:

```bash
# Compare with ogginfo
ogginfo your_test_file.ogg

# Compare seeking behavior with reference players
# Test with various file types and encoders
```

## Reference Implementation Compliance

Always verify that your codec implementation matches reference behavior:

1. **Header Structure**: Follow official codec specifications exactly
2. **Granule Position**: Use the same interpretation as reference implementations
3. **Error Handling**: Return the same error codes for the same conditions
4. **Edge Cases**: Handle boundary conditions the same way

## Contributing

When contributing codec support:

1. Follow PsyMP3 coding standards and threading patterns
2. Include comprehensive unit and integration tests
3. Update build system configuration
4. Add documentation for the new codec
5. Test with various file types and encoders
6. Verify performance characteristics

The OggDemuxer architecture is designed to be extensible while maintaining strict compatibility with reference implementations. Following these patterns ensures that new codecs integrate seamlessly with the existing system.