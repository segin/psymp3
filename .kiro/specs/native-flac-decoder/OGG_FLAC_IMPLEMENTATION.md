# Ogg FLAC Support Implementation Summary

## Task 15: Implement Ogg FLAC Support

### Requirements (Requirement 49)

The native FLAC decoder must support Ogg FLAC streams with the following capabilities:

1. ✅ Parse Ogg FLAC header packet
2. ✅ Verify 0x7F 0x46 0x4C 0x41 0x43 signature  
3. ✅ Parse version number and header count
4. ✅ Parse metadata from header packets
5. ✅ Decode FLAC frames from audio packets
6. ✅ Handle granule position for sample numbering
7. ✅ Support chained Ogg streams

### Implementation Architecture

The Ogg FLAC support is implemented across two components following the separation of concerns principle:

#### 1. OggDemuxer (Container Layer)
**Location**: `src/demuxers/ogg/OggDemuxer.cpp`

The OggDemuxer handles all Ogg container-specific operations:

- **Header Parsing** (`parseFLACHeaders`): Parses Ogg FLAC identification header
  - Verifies "\x7FFLAC" signature (5 bytes)
  - Reads major/minor version (should be 1.0)
  - Reads header packet count (big-endian)
  - Validates native FLAC signature "fLaC" at offset 9
  - Parses STREAMINFO metadata block
  - Extracts sample rate, channels, bits per sample, total samples

- **Codec Identification** (`identifyCodec`): Detects FLAC streams in Ogg containers
  - Checks for "\x7FFLAC" signature in first packet
  - Returns "flac" codec name for proper codec selection

- **Granule Position Handling** (`granuleToMs`, `msToGranule`):
  - Converts between granule positions and milliseconds
  - FLAC granule positions are sample-based (like Vorbis)
  - Formula: `timestamp_ms = (granule * 1000) / sample_rate`

- **Stream Chaining**: Handles chained Ogg streams
  - Detects stream changes via serial number
  - Resets stream state for new logical streams
  - Maintains separate state per stream

#### 2. Native FLAC Codec (Codec Layer)
**Location**: `src/codecs/flac/NativeFLACCodec.cpp`

The native FLAC codec is **container-agnostic** (Requirement 15):

- **Container Independence**: Codec does not depend on container format
  - Receives FLAC frames via MediaChunk from any demuxer
  - Decodes frames identically regardless of source (native FLAC, Ogg FLAC, etc.)
  - No Ogg-specific code in codec layer

- **Streamable Subset Support** (Requirement 19):
  - Handles frames without STREAMINFO references
  - Extracts sample rate from frame header when needed
  - Extracts bit depth from frame header when needed
  - Supports mid-stream synchronization

#### 3. MetadataParser Enhancements
**Location**: `src/codecs/flac/MetadataParser.cpp`

Added Ogg FLAC header parsing capabilities:

- **`parseOggFLACHeader`**: Parses Ogg FLAC identification header
  - Verifies "\x7FFLAC" signature (Requirements 49.2)
  - Reads version number (Requirements 49.3)
  - Reads header packet count (Requirements 49.4)
  - Returns parsed values for validation

- **`skipOggFLACSignature`**: Skips Ogg FLAC header to access native FLAC data
  - Skips 9 bytes: signature (5) + version (2) + header count (2)
  - Positions bitstream at native FLAC metadata blocks

### Data Flow

```
Ogg FLAC File
    ↓
OggDemuxer
    ├─ Parses Ogg pages
    ├─ Identifies FLAC codec via "\x7FFLAC" signature
    ├─ Parses Ogg FLAC header packet
    ├─ Extracts STREAMINFO metadata
    ├─ Extracts FLAC frames from audio packets
    └─ Converts granule positions to timestamps
    ↓
MediaChunk (FLAC frame data)
    ↓
Native FLAC Codec
    ├─ Decodes FLAC frame (container-agnostic)
    ├─ Parses frame header
    ├─ Decodes subframes
    ├─ Applies channel decorrelation
    ├─ Reconstructs PCM samples
    └─ Returns AudioFrame
    ↓
Audio Output (16-bit PCM)
```

### Ogg FLAC Header Format

```
Ogg FLAC Identification Header (first packet):
┌─────────────────────────────────────────────────────────┐
│ Offset │ Size │ Description                              │
├────────┼──────┼──────────────────────────────────────────┤
│   0    │  1   │ 0x7F (signature byte)                    │
│   1    │  4   │ "FLAC" (ASCII)                           │
│   5    │  1   │ Major version (1)                        │
│   6    │  1   │ Minor version (0)                        │
│   7    │  2   │ Number of header packets (big-endian)    │
│   9    │  4   │ "fLaC" (native FLAC signature)           │
│  13    │  4   │ STREAMINFO metadata block header         │
│  17    │ 34   │ STREAMINFO data                          │
└─────────────────────────────────────────────────────────┘

Subsequent packets contain:
- Additional metadata blocks (SEEKTABLE, VORBIS_COMMENT, etc.)
- FLAC audio frames (identical to native FLAC format)
```

### Testing

Existing tests verify Ogg FLAC support:

1. **`test_codec_detection.cpp`**: Tests Ogg FLAC codec identification
2. **`test_flac_codec_container_agnostic.cpp`**: Tests container-agnostic decoding
3. **`test_flac_codec_demuxer_integration.cpp`**: Tests Ogg FLAC integration
4. **`test_oggdemuxer_comprehensive.cpp`**: Tests Ogg FLAC demuxing
5. **`test_time_conversion.cpp`**: Tests granule position conversion

New test created:
- **`test_native_flac_ogg_support.cpp`**: Tests Ogg FLAC header parsing in MetadataParser

### Compliance

The implementation complies with:

- **RFC 9639**: FLAC specification (frame format, metadata, decoding)
- **RFC 3533**: Ogg container format (page structure, packet boundaries)
- **Ogg FLAC Mapping**: Xiph.Org Ogg FLAC specification

### Key Design Decisions

1. **Separation of Concerns**: Container parsing (OggDemuxer) vs codec decoding (FLACCodec)
2. **Container Agnostic Codec**: Codec works with any container (native FLAC, Ogg, ISO)
3. **Reuse Existing Infrastructure**: Leverages existing OggDemuxer for Ogg parsing
4. **Minimal Codec Changes**: Codec remains simple and focused on FLAC decoding

### Verification

To verify Ogg FLAC support works:

```bash
# Play Ogg FLAC file
./src/psymp3 file.oga

# With debug logging
./src/psymp3 --debug=ogg,flac_codec --logfile=ogg_flac.log file.oga

# Check logs for:
# - "Identified as FLAC" (codec detection)
# - "Ogg FLAC identification header found" (header parsing)
# - "Parsed STREAMINFO" (metadata extraction)
# - Frame decoding messages (audio decoding)
```

### Conclusion

Ogg FLAC support is **fully implemented** and **operational**:

- ✅ OggDemuxer handles Ogg container parsing
- ✅ Native FLAC codec decodes FLAC frames container-agnostically
- ✅ MetadataParser can parse Ogg FLAC headers if needed
- ✅ Granule position handling for seeking and timestamps
- ✅ Chained stream support
- ✅ Existing tests verify functionality

The implementation follows the principle of separation of concerns, with the demuxer handling container-specific operations and the codec remaining container-agnostic. This design allows the native FLAC codec to work seamlessly with any container format (native FLAC, Ogg FLAC, ISO FLAC, etc.) without modification.
