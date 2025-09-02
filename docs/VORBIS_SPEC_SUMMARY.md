# Vorbis I Specification - Summary

## Overview
The Vorbis I specification defines a general-purpose perceptual audio codec for variable bitrate audio compression. Vorbis is designed for high-quality audio compression and is commonly used in Ogg containers.

## Key Features
- **Variable bitrate (VBR)** compression
- **Sample rates**: Typically 8 kHz to 192 kHz
- **Channels**: 1 to 255 channels supported
- **Bitrates**: ~45 kb/s to 500+ kb/s
- **Lossless mode**: Not supported (lossy compression only)

## Packet Structure

### Vorbis Packet Types
1. **Identification Header**: Stream parameters and codec info
2. **Comment Header**: Metadata and tags
3. **Setup Header**: Codebook and configuration data
4. **Audio Packets**: Compressed audio data

### Identification Header
```
- Packet type: 0x01
- Vorbis signature: "vorbis"
- Version: 0x00000000
- Channels: 1-255
- Sample rate: Hz
- Bitrate maximum/nominal/minimum
- Blocksize 0/1: Power of 2 values
- Framing flag: Must be 1
```

### Comment Header  
```
- Packet type: 0x03
- Vorbis signature: "vorbis"
- Vendor string length + vendor string
- User comment list length
- User comments (field=value format)
- Framing flag: Must be 1
```

### Setup Header
```
- Packet type: 0x05
- Vorbis signature: "vorbis"
- Codebook configuration
- Time domain transforms
- Floor configurations
- Residue configurations
- Mapping configurations
- Mode configurations
- Framing flag: Must be 1
```

## Audio Encoding Process

### Block Structure
- **Short blocks**: Typically 256 samples
- **Long blocks**: Typically 2048 samples
- **Overlapping**: 50% overlap between blocks
- **Windowing**: Modified Discrete Cosine Transform (MDCT)

### Encoding Pipeline
1. **Windowing**: Apply window function to audio blocks
2. **MDCT**: Transform to frequency domain
3. **Floor**: Encode spectral envelope
4. **Residue**: Encode spectral details
5. **Bitpacking**: Pack into Vorbis packets

## Implementation Guidelines

### Header Processing
1. Parse identification header for stream parameters
2. Process comment header for metadata
3. Parse setup header for decoder configuration
4. Validate all headers before processing audio

### Audio Decoding
1. Unpack packet header and mode
2. Decode floor (spectral envelope)
3. Decode residue (spectral details)
4. Apply inverse MDCT
5. Apply windowing and overlap-add

### Error Handling
- Validate packet headers and framing flags
- Handle corrupted packets gracefully
- Implement proper error recovery

## Ogg Encapsulation Specifics

### Granule Position
- Represents PCM sample position
- Monotonically increasing
- Used for seeking and timing

### Page Structure
- Headers typically on separate pages
- Audio packets can span multiple pages
- BOS (Beginning of Stream) flag on first page
- EOS (End of Stream) flag on last page

## Critical Implementation Details

### Sample Rate and Channels
- Must match identification header values
- No mid-stream changes allowed
- Channel mapping follows Vorbis specification

### Blocksize Handling
- Two blocksizes defined in identification header
- Block type determined by packet mode
- Proper overlap-add required for reconstruction

### Seeking
- Granule position enables sample-accurate seeking
- Bisection search recommended
- May require decoding multiple packets for accuracy

### Memory Management
- Codebooks can be large (setup header)
- Audio blocks require overlap buffers
- Efficient memory allocation critical

## Quality and Bitrate

### Quality Levels
- **-1 to 10**: Quality scale (higher = better quality)
- **Quality 3**: ~112 kb/s stereo (good quality)
- **Quality 5**: ~160 kb/s stereo (high quality)
- **Quality 8**: ~256 kb/s stereo (very high quality)

### Bitrate Management
- **VBR**: Variable bitrate (recommended)
- **ABR**: Average bitrate targeting
- **CBR**: Constant bitrate (not recommended)

## Common Pitfalls
- Not validating framing flags
- Incorrect overlap-add implementation
- Improper granule position handling
- Not handling variable block sizes
- Ignoring setup header validation
- Incorrect channel mapping

## Reference
Full specification: `docs/vorbis-spec.html`
Ogg container: `docs/rfc3533.txt`