# Vorbis I Specification Summary

## Document Information

- **Source**: Vorbis I Specification by Xiph.Org Foundation (July 4, 2020)
- **Full Specification**: `docs/vorbis-spec.html`
- **Purpose**: Quick reference for PsyMP3 VorbisCodec implementation

## Overview

Vorbis is a general-purpose perceptual audio codec using the Modified Discrete Cosine Transform (MDCT). It is designed for variable bitrate (VBR) compression with high encoder flexibility.

### Key Characteristics

| Parameter | Range |
|-----------|-------|
| Sample Rate | 8 kHz - 192 kHz |
| Channels | 1 - 255 |
| Bitrate | ~45 kb/s - 500+ kb/s |
| Quality Levels | -1 to 10 |
| Compression | Lossy only |

## Packet Structure

Vorbis streams consist of three mandatory header packets followed by audio packets.

### Header Packet Types

| Type | Byte | Signature | Purpose |
|------|------|-----------|---------|
| Identification | 0x01 | "vorbis" | Stream parameters |
| Comment | 0x03 | "vorbis" | Metadata/tags |
| Setup | 0x05 | "vorbis" | Codebook configuration |

### Common Header Format

All header packets begin with:
1. **Packet type** (8 bits): 0x01, 0x03, or 0x05
2. **Signature** (48 bits): ASCII "vorbis"

## Identification Header (Type 0x01)

The identification header contains essential stream parameters.

### Field Layout

| Field | Bits | Description |
|-------|------|-------------|
| packet_type | 8 | Must be 0x01 |
| signature | 48 | ASCII "vorbis" |
| vorbis_version | 32 | Must be 0x00000000 |
| audio_channels | 8 | 1-255 channels |
| audio_sample_rate | 32 | Hz (must be > 0) |
| bitrate_maximum | 32 | Upper bitrate bound (-1 = unset) |
| bitrate_nominal | 32 | Target/average bitrate (-1 = unset) |
| bitrate_minimum | 32 | Lower bitrate bound (-1 = unset) |
| blocksize_0 | 4 | Short block exponent (6-13) |
| blocksize_1 | 4 | Long block exponent (6-13) |
| framing_flag | 1 | Must be 1 |

### Block Size Constraints

- `blocksize_0` and `blocksize_1` are stored as exponents (actual size = 2^exponent)
- Valid exponent range: 6-13 (block sizes 64-8192)
- Constraint: `blocksize_0 <= blocksize_1`
- Typical values: blocksize_0=8 (256 samples), blocksize_1=11 (2048 samples)

### Validation Rules

1. `vorbis_version` must equal 0
2. `audio_channels` must be > 0
3. `audio_sample_rate` must be > 0
4. `blocksize_0` must be in range [6, 13]
5. `blocksize_1` must be in range [6, 13]
6. `blocksize_0` must be <= `blocksize_1`
7. `framing_flag` must be 1

## Comment Header (Type 0x03)

The comment header contains metadata in Vorbis Comment format.

### Structure

| Field | Type | Description |
|-------|------|-------------|
| packet_type | 8 bits | Must be 0x03 |
| signature | 48 bits | ASCII "vorbis" |
| vendor_length | 32 bits | Length of vendor string |
| vendor_string | variable | UTF-8 encoder identification |
| user_comment_list_length | 32 bits | Number of comments |
| user_comments | variable | Array of field=value pairs |
| framing_flag | 1 bit | Must be 1 |

### Comment Format

- Comments are UTF-8 encoded strings
- Format: `FIELDNAME=value`
- Field names are case-insensitive ASCII
- Common fields: TITLE, ARTIST, ALBUM, TRACKNUMBER, DATE, GENRE

## Setup Header (Type 0x05)

The setup header contains the complete codec configuration including codebooks.

### Contents

1. **Codebooks**: Huffman/VQ codebook definitions
2. **Time Domain Transforms**: Reserved (always 0 in Vorbis I)
3. **Floors**: Spectral envelope configurations (type 0 or 1)
4. **Residues**: Spectral detail configurations (type 0, 1, or 2)
5. **Mappings**: Channel coupling and submap definitions
6. **Modes**: Frame encoding mode configurations

### Size Considerations

- Setup headers are typically 2-8 KB
- Recommended maximum: 4 KB for streaming
- Size is unbounded in specification

## Audio Packet Decode

### Decode Pipeline

```
Packet → Mode Select → Floor Decode → Residue Decode → 
Inverse Coupling → Dot Product → Inverse MDCT → Overlap-Add → PCM Output
```

### Packet Structure

1. **Packet type bit** (1 bit): Must be 0 for audio packets
2. **Mode number** (variable bits): Selects decode mode
3. **Window flags** (if applicable): For block size transitions
4. **Floor data**: Per-channel spectral envelope
5. **Residue data**: Per-channel spectral detail

### Block Size Selection

- Mode configuration determines block size (short or long)
- Window shape depends on previous and next block sizes
- Four window shapes: short-short, short-long, long-short, long-long

## Overlap-Add Reconstruction

### Window Function

Vorbis uses a sine window:
```
window[i] = sin(π/2 * sin²(π * (i + 0.5) / n))
```

### Overlap Processing

1. Apply window to current block
2. Add overlapping portion from previous block
3. Output non-overlapping samples
4. Store overlap for next block

### First Block Handling

- First audio block produces no output (establishes overlap buffer)
- Decoder must handle this gracefully

## Channel Mapping

### Standard Channel Orders

| Channels | Order |
|----------|-------|
| 1 (Mono) | M |
| 2 (Stereo) | L, R |
| 3 | L, C, R |
| 4 | FL, FR, RL, RR |
| 5 | FL, C, FR, RL, RR |
| 6 (5.1) | FL, C, FR, RL, RR, LFE |
| 7 | FL, C, FR, SL, SR, RC, LFE |
| 8 (7.1) | FL, C, FR, SL, SR, RL, RR, LFE |

### Channel Coupling

- Stereo and multi-channel may use magnitude/angle coupling
- Coupling is decoded via inverse coupling step
- Improves compression efficiency for correlated channels

## libvorbis API Reference

### Initialization Functions

| Function | Purpose |
|----------|---------|
| `vorbis_info_init()` | Initialize vorbis_info structure |
| `vorbis_comment_init()` | Initialize vorbis_comment structure |
| `vorbis_synthesis_init()` | Initialize DSP state after headers |
| `vorbis_block_init()` | Initialize block structure |

### Header Processing

| Function | Purpose |
|----------|---------|
| `vorbis_synthesis_headerin()` | Process header packet |
| `vorbis_synthesis_idheader()` | Check if packet is ID header |

### Audio Decoding

| Function | Purpose |
|----------|---------|
| `vorbis_synthesis()` | Decode packet into block |
| `vorbis_synthesis_blockin()` | Submit block to DSP state |
| `vorbis_synthesis_pcmout()` | Get decoded PCM samples |
| `vorbis_synthesis_read()` | Consume decoded samples |
| `vorbis_synthesis_lapout()` | Get samples with lap info |

### State Management

| Function | Purpose |
|----------|---------|
| `vorbis_synthesis_restart()` | Reset for seeking |
| `vorbis_synthesis_halfrate()` | Enable half-rate decode |
| `vorbis_synthesis_halfrate_p()` | Query half-rate status |

### Cleanup Functions

| Function | Purpose |
|----------|---------|
| `vorbis_block_clear()` | Free block structure |
| `vorbis_dsp_clear()` | Free DSP state |
| `vorbis_comment_clear()` | Free comment structure |
| `vorbis_info_clear()` | Free info structure |

### Query Functions

| Function | Purpose |
|----------|---------|
| `vorbis_info_blocksize()` | Get block size (0=short, 1=long) |
| `vorbis_bitrate()` | Get current bitrate |
| `vorbis_comment_query()` | Query comment by tag |
| `vorbis_comment_query_count()` | Count comments with tag |

## Error Codes

| Code | Value | Meaning |
|------|-------|---------|
| OV_FALSE | -1 | Not true / no data |
| OV_EOF | -2 | End of file |
| OV_HOLE | -3 | Data interruption |
| OV_EREAD | -128 | Read error |
| OV_EFAULT | -129 | Internal logic fault |
| OV_EIMPL | -130 | Unimplemented feature |
| OV_EINVAL | -131 | Invalid argument |
| OV_ENOTVORBIS | -132 | Not Vorbis data |
| OV_EBADHEADER | -133 | Invalid header |
| OV_EVERSION | -134 | Version mismatch |
| OV_ENOTAUDIO | -135 | Not an audio packet |
| OV_EBADPACKET | -136 | Bad/corrupt packet |
| OV_EBADLINK | -137 | Invalid stream link |
| OV_ENOSEEK | -138 | Stream not seekable |

## Quality and Bitrate

### Quality Level Mapping (Approximate)

| Quality | Bitrate (Stereo) | Use Case |
|---------|------------------|----------|
| -1 | ~45 kb/s | Low bandwidth |
| 0 | ~64 kb/s | Speech |
| 3 | ~112 kb/s | Good quality |
| 5 | ~160 kb/s | High quality |
| 6 | ~192 kb/s | Very high quality |
| 8 | ~256 kb/s | Near-transparent |
| 10 | ~500 kb/s | Maximum quality |

### Bitrate Modes

- **VBR (Variable)**: Quality-based, recommended
- **ABR (Average)**: Target average bitrate
- **CBR (Constant)**: Fixed bitrate, not recommended

## Ogg Encapsulation

### Granule Position

- Represents absolute PCM sample position
- Used for seeking and timing
- Monotonically increasing within stream

### Page Requirements

1. Each header packet on separate page
2. First page has BOS (Beginning of Stream) flag
3. Last page has EOS (End of Stream) flag
4. Audio packets may span pages

### Seeking

- Use granule position for sample-accurate seeking
- Bisection search recommended for efficiency
- May require decoding multiple packets for accuracy

## Implementation Notes

### Decoder Memory

- Codebooks require significant memory (from setup header)
- Overlap buffer: 2 × max_block_size × channels × sizeof(float)
- Working memory for MDCT and floor/residue decode

### Performance Considerations

- MDCT is primary computational cost
- Floor type 1 is faster than floor type 0
- Consider SIMD optimization for MDCT and windowing

### Common Pitfalls

1. Not validating framing flags in headers
2. Incorrect overlap-add implementation
3. Improper handling of first block (no output)
4. Not handling variable block sizes correctly
5. Ignoring channel coupling in multi-channel
6. Not resetting state properly for seeking

## References

- Full Specification: `docs/vorbis-spec.html`
- Ogg Container: `docs/rfc3533.txt` (RFC 3533)
- Ogg Media Type: `docs/rfc3534.txt` (RFC 3534)
- libvorbis Source: https://github.com/xiph/vorbis

