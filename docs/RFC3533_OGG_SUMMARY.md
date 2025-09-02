# RFC 3533: The Ogg Encapsulation Format Version 0 - Summary

## Overview
RFC 3533 defines the Ogg bitstream format, a general-purpose container format for multimedia content. This is the foundational specification for the Ogg container format used by Vorbis, Opus, FLAC, and other codecs.

## Key Concepts

### Bitstream Structure
- **Pages**: Fundamental unit of the Ogg bitstream
- **Packets**: Logical units of data from codecs, can span multiple pages
- **Streams**: Logical bitstreams identified by unique serial numbers

### Page Format
```
Page Header (27+ bytes):
- Capture pattern: "OggS" (0x4f676753)
- Version: 0x00
- Header type flags
- Granule position (8 bytes)
- Bitstream serial number (4 bytes)
- Page sequence number (4 bytes)
- CRC checksum (4 bytes)
- Page segments count (1 byte)
- Segment table (variable length)
```

### Header Type Flags
- **0x01**: Continuation of packet from previous page
- **0x02**: First page of logical bitstream (BOS - Beginning of Stream)
- **0x04**: Last page of logical bitstream (EOS - End of Stream)

### Granule Position
- Codec-specific position marker
- For audio: typically sample position
- Value of -1 indicates no packets finish on this page

## Implementation Guidelines

### Page Parsing
1. Verify capture pattern "OggS"
2. Check version field (must be 0)
3. Validate CRC checksum
4. Parse segment table to determine packet boundaries
5. Handle packet continuation across pages

### Stream Multiplexing
- Multiple logical streams can be multiplexed in single Ogg file
- Each stream has unique serial number
- Pages from different streams can be interleaved

### Error Recovery
- CRC validation for corruption detection
- Page boundaries allow recovery from errors
- Missing pages can be detected via sequence numbers

## Critical Implementation Details

### Packet Reconstruction
- Packets may span multiple pages
- Use continuation flag and segment table to reconstruct packets
- Handle incomplete packets at stream boundaries

### Seeking
- Pages are self-contained units for seeking
- Granule position enables time-based seeking
- Bisection search recommended for efficient seeking

### Memory Management
- Page size is variable (up to 65,307 bytes theoretical maximum)
- Typical page size: 4-8KB for optimal performance
- Buffer management critical for streaming applications

## Common Pitfalls
- Assuming fixed page sizes
- Not handling packet continuation properly
- Ignoring CRC validation
- Incorrect granule position interpretation
- Not handling multiplexed streams

## Reference
Full specification: `docs/rfc3533.txt`