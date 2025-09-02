# RFC and Specification Reference Index

This document provides a comprehensive index of all audio codec and container format specifications available in the PsyMP3 project documentation.

## Container Formats

### Ogg Encapsulation Format
- **RFC 3533**: The Ogg Encapsulation Format Version 0
  - File: `docs/rfc3533.txt`
  - Summary: `docs/RFC3533_OGG_SUMMARY.md`
  - Description: Defines the fundamental Ogg bitstream format, page structure, and stream multiplexing

- **RFC 3534**: The application/ogg Media Type
  - File: `docs/rfc3534.txt`
  - Description: Defines the MIME media type for Ogg files

## Audio Codecs

### FLAC (Free Lossless Audio Codec)
- **RFC 9639**: The Free Lossless Audio Codec (FLAC) Format
  - File: `docs/rfc9639.txt`
  - Summary: `docs/RFC9639_FLAC_SUMMARY.md`
  - Description: Complete specification for FLAC lossless audio compression format

### Opus Interactive Audio Codec
- **RFC 6716**: Definition of the Opus Audio Codec
  - File: `docs/rfc6716.txt`
  - Summary: `docs/RFC6716_OPUS_SUMMARY.md`
  - Description: Core Opus codec specification including SILK and CELT modes

- **RFC 7845**: Ogg Encapsulation for the Opus Audio Codec
  - File: `docs/rfc7845.txt`
  - Description: Opus-specific requirements for Ogg container encapsulation

### Vorbis Audio Codec
- **Vorbis I Specification**: Official Vorbis audio codec specification
  - File: `docs/vorbis-spec.html`
  - Summary: `docs/VORBIS_SPEC_SUMMARY.md`
  - Description: Complete specification for Vorbis lossy audio compression format

## Usage Guidelines

### When to Consult These Documents

#### During Development
- **Always** reference the appropriate RFC/specification when implementing codec or container features
- **Validate** implementation decisions against the official specifications
- **Use** the summary documents for quick reference during development

#### During Debugging
- **Check** specification compliance when encountering format-related issues
- **Verify** packet structure and bit-level encoding against official documentation
- **Reference** error handling requirements from specifications

#### During Code Review
- **Ensure** all format-specific code follows specification requirements
- **Validate** that edge cases are handled according to official standards
- **Check** that implementation matches specification recommendations

### Implementation Priority Order

1. **Container Format**: Start with Ogg specification (RFC 3533) for demuxer implementation
2. **Codec Headers**: Implement codec-specific header parsing per individual specifications
3. **Audio Processing**: Follow codec-specific audio decoding processes
4. **Error Handling**: Implement robust error recovery as specified in each format
5. **Optimization**: Apply performance optimizations while maintaining specification compliance

### Cross-Reference Requirements

#### Ogg + Vorbis
- Must follow both RFC 3533 (Ogg) and Vorbis I specification
- Granule position calculation specific to Vorbis
- Header packet ordering requirements

#### Ogg + Opus
- Must follow RFC 3533 (Ogg), RFC 6716 (Opus), and RFC 7845 (Ogg+Opus)
- Pre-skip and output gain handling
- 48 kHz granule position requirements

#### Ogg + FLAC
- Must follow RFC 3533 (Ogg) and RFC 9639 (FLAC)
- FLAC-specific Ogg mapping requirements
- Metadata block handling in Ogg context

## Specification Compliance Checklist

### Container Format (Ogg)
- [ ] Proper "OggS" capture pattern detection
- [ ] CRC validation for all pages
- [ ] Correct packet reconstruction across page boundaries
- [ ] Stream multiplexing support
- [ ] Granule position handling per codec requirements

### FLAC Codec
- [ ] Sync pattern detection (0xFF followed by 0xF8-0xFF)
- [ ] Metadata block parsing and validation
- [ ] Variable frame size handling
- [ ] Sample rate and bit depth support
- [ ] Error recovery for corrupted streams

### Vorbis Codec
- [ ] Three-header validation (identification, comment, setup)
- [ ] Framing flag verification
- [ ] Variable block size overlap-add
- [ ] Codebook and configuration parsing
- [ ] Quality level support

### Opus Codec
- [ ] TOC byte parsing (configuration, stereo, frame count)
- [ ] Mode detection (SILK, CELT, hybrid)
- [ ] Sample rate handling (internal 48 kHz)
- [ ] Pre-skip compensation
- [ ] Packet loss concealment

## Quick Reference Links

- **Ogg Container**: `docs/RFC3533_OGG_SUMMARY.md`
- **FLAC Codec**: `docs/RFC9639_FLAC_SUMMARY.md`
- **Vorbis Codec**: `docs/VORBIS_SPEC_SUMMARY.md`
- **Opus Codec**: `docs/RFC6716_OPUS_SUMMARY.md`

## Maintenance Notes

- Keep this index updated when new specifications are added
- Update summary documents when specifications are revised
- Ensure all implementation code references appropriate specifications
- Validate specification compliance during code reviews