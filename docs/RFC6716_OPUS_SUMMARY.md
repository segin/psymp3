# RFC 6716: Definition of the Opus Audio Codec - Summary

## Overview
RFC 6716 defines the Opus audio codec, a versatile codec designed for interactive speech and audio transmission over the Internet. Opus combines SILK (for speech) and CELT (for music) technologies.

## Key Features
- **Bitrates**: 6 kb/s to 510 kb/s
- **Sample rates**: 8, 12, 16, 24, and 48 kHz
- **Frame sizes**: 2.5, 5, 10, 20, 40, or 60 ms
- **Channels**: Mono and stereo
- **Low latency**: Algorithmic delay of 22.5 ms at 48 kHz

## Packet Structure

### Opus Packet Format
```
TOC (Table of Contents) - 1 byte:
- Configuration (5 bits): Determines mode, bandwidth, frame size
- Stereo flag (1 bit): 0=mono, 1=stereo  
- Frame count code (2 bits): Number of frames in packet

Frame data follows TOC byte
```

### Configuration Values (5-bit field)
- **0-15**: SILK-only modes (NB, MB, WB)
- **16-19**: Hybrid SILK/CELT modes
- **20-31**: CELT-only modes

## Audio Modes

### SILK Mode (Speech)
- Optimized for speech content
- Lower bitrates (6-40 kb/s typical)
- Narrowband (8 kHz), Mediumband (12 kHz), Wideband (16 kHz)

### CELT Mode (Music)
- Optimized for music and general audio
- Higher bitrates (64-510 kb/s)
- Fullband (48 kHz) support
- Lower algorithmic delay

### Hybrid Mode
- Combines SILK for low frequencies, CELT for high frequencies
- Balances speech optimization with music quality

## Frame Sizes and Bitrates

### Supported Frame Durations
- **2.5 ms**: Ultra-low latency applications
- **5 ms**: Low latency applications  
- **10 ms**: Default for most applications
- **20 ms**: Standard frame size
- **40 ms**: Higher efficiency applications
- **60 ms**: Maximum efficiency

### Bitrate Recommendations
- **Speech**: 8-32 kb/s
- **Music (stereo)**: 64-128 kb/s
- **High quality music**: 128-256 kb/s

## Implementation Guidelines

### Decoder Requirements
- Must support all configurations and frame sizes
- Handle packet loss gracefully
- Support sample rate conversion if needed

### Encoder Flexibility
- Can choose any supported configuration
- Should adapt to content type and network conditions
- May use variable bitrate (VBR) or constant bitrate (CBR)

### Error Handling
- Robust to packet loss
- Forward error correction (FEC) available
- Packet loss concealment algorithms specified

## Ogg Encapsulation
- See RFC 7845 for Ogg container specifics
- Uses Opus-specific granule position calculation
- Pre-skip and gain fields in Ogg headers

## Critical Implementation Details

### Sample Rate Handling
- Internal processing at 48 kHz
- Automatic resampling for other rates
- Granule position always in 48 kHz units

### Channel Mapping
- Simple stereo: left/right channels
- Multichannel support via channel mapping tables
- Ambisonic audio support

### Timing and Synchronization
- Precise timing via granule positions
- Pre-skip handling for encoder delay
- Output gain adjustment support

## Common Pitfalls
- Incorrect granule position calculation
- Not handling pre-skip properly
- Assuming fixed frame sizes
- Ignoring channel mapping for multichannel
- Not implementing packet loss concealment

## Reference
Full specification: `docs/rfc6716.txt`
Ogg encapsulation: `docs/rfc7845.txt`