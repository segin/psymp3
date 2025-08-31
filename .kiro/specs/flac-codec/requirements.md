# **FLAC CODEC REQUIREMENTS**

## **Introduction**

This specification defines the requirements for implementing a container-agnostic FLAC audio codec for PsyMP3. The FLAC codec will decode FLAC bitstream data from any container (native FLAC, Ogg FLAC, or potentially ISO FLAC) into standard 16-bit PCM audio. This codec works with the AudioCodec architecture and receives FLAC frame data from various demuxers.

**Key Architectural Insights from Implementation Experience:**
- **Frame-level processing**: FLAC frames are self-contained units that can be decoded independently
- **Variable compression ratios**: Highly compressed streams can have frames as small as 10-14 bytes
- **Container independence**: The codec must work with MediaChunk data regardless of source demuxer
- **Performance criticality**: Efficient decoding is essential for real-time playback of high-resolution files
- **Threading safety**: Must follow PsyMP3's public/private lock pattern for thread safety
- **RFC 9639 compliance**: All implementation must strictly follow the FLAC specification

The implementation must support:
- **Container-agnostic FLAC decoding** from any source (native FLAC, Ogg FLAC)
- **All FLAC bit depths** with conversion to 16-bit PCM output
- **Variable block sizes** and all FLAC compression modes including highly compressed streams
- **Efficient frame-by-frame decoding** without requiring complete file access
- **Sample-accurate seeking support** through decoder reset capabilities
- **Thread-safe operation** following established PsyMP3 threading patterns
- **High-performance decoding** suitable for real-time playback of 24-bit/96kHz+ files

## **Requirements**

### **Requirement 1: FLAC Bitstream Decoding**

**User Story:** As a demuxer component, I want to decode FLAC frame data into PCM samples, so that I can provide audio output regardless of the container format.

#### **Acceptance Criteria**

1. **WHEN** receiving FLAC frames **THEN** the codec **SHALL** decode complete frames into PCM samples
2. **WHEN** processing frame headers **THEN** the codec **SHALL** extract block size, sample rate, channel configuration, and bit depth
3. **WHEN** decoding subframes **THEN** the codec **SHALL** handle all FLAC subframe types (CONSTANT, VERBATIM, FIXED, LPC)
4. **WHEN** processing residuals **THEN** the codec **SHALL** decode Rice/Golomb coded residuals correctly
5. **WHEN** reconstructing samples **THEN** the codec **SHALL** apply prediction and residual data to generate final samples
6. **WHEN** handling stereo modes **THEN** the codec **SHALL** support independent, left-side, right-side, and mid-side stereo
7. **WHEN** validating frames **THEN** the codec **SHALL** verify frame CRC checksums
8. **WHEN** frame validation fails **THEN** the codec **SHALL** handle corrupted frames gracefully

### **Requirement 2: Bit Depth and Sample Format Handling**

**User Story:** As an audio output system, I want consistent 16-bit PCM output from FLAC files, so that I can handle audio uniformly regardless of source bit depth.

#### **Acceptance Criteria**

1. **WHEN** decoding 16-bit FLAC **THEN** the codec **SHALL** output samples directly without conversion
2. **WHEN** decoding 8-bit FLAC **THEN** the codec **SHALL** upscale to 16-bit with proper scaling
3. **WHEN** decoding 24-bit FLAC **THEN** the codec **SHALL** downscale to 16-bit with proper dithering
4. **WHEN** decoding 32-bit FLAC **THEN** the codec **SHALL** downscale to 16-bit with proper scaling
5. **WHEN** performing bit depth conversion **THEN** the codec **SHALL** maintain audio quality and prevent clipping
6. **WHEN** handling signed samples **THEN** the codec **SHALL** ensure proper sign extension and conversion
7. **WHEN** processing different bit depths **THEN** the codec **SHALL** handle all FLAC-supported bit depths (4-32 bits)
8. **WHEN** outputting samples **THEN** the codec **SHALL** provide samples in host byte order

### **Requirement 3: Channel Configuration Support**

**User Story:** As a media player, I want to play FLAC files with various channel configurations, so that I can support mono, stereo, and multi-channel audio.

#### **Acceptance Criteria**

1. **WHEN** decoding mono FLAC **THEN** the codec **SHALL** output single-channel PCM data
2. **WHEN** decoding stereo FLAC **THEN** the codec **SHALL** output interleaved left/right channel data
3. **WHEN** handling multi-channel FLAC **THEN** the codec **SHALL** support up to 8 channels as per FLAC specification
4. **WHEN** processing channel assignments **THEN** the codec **SHALL** handle independent, left-side, right-side, and mid-side configurations
5. **WHEN** decoding side-channel modes **THEN** the codec **SHALL** properly reconstruct left and right channels
6. **WHEN** outputting multi-channel **THEN** the codec **SHALL** provide properly interleaved channel data
7. **WHEN** channel count changes **THEN** the codec **SHALL** adapt output format accordingly
8. **WHEN** unsupported channels **THEN** the codec **SHALL** report error rather than producing incorrect output

### **Requirement 4: Variable Block Size Handling**

**User Story:** As a FLAC decoder, I want to handle variable block sizes efficiently, so that I can decode all types of FLAC files regardless of encoding parameters.

#### **Acceptance Criteria**

1. **WHEN** processing fixed block sizes **THEN** the codec **SHALL** handle standard block sizes (192, 576, 1152, 2304, 4608, etc.)
2. **WHEN** processing variable block sizes **THEN** the codec **SHALL** adapt to different block sizes within the same stream
3. **WHEN** block size changes **THEN** the codec **SHALL** adjust internal buffers and processing accordingly
4. **WHEN** handling large blocks **THEN** the codec **SHALL** support block sizes up to 65535 samples
5. **WHEN** handling small blocks **THEN** the codec **SHALL** support block sizes down to 16 samples
6. **WHEN** buffering output **THEN** the codec **SHALL** provide consistent output regardless of input block size
7. **WHEN** memory allocation occurs **THEN** the codec **SHALL** allocate buffers based on maximum expected block size
8. **WHEN** block size is invalid **THEN** the codec **SHALL** reject invalid block sizes and report errors

### **Requirement 5: Container-Agnostic Operation**

**User Story:** As a demuxer component, I want the FLAC codec to work with any container format, so that I can use the same decoder for native FLAC, Ogg FLAC, and other containers.

#### **Acceptance Criteria**

1. **WHEN** receiving frames from FLACDemuxer **THEN** the codec **SHALL** decode native FLAC frames correctly
2. **WHEN** receiving frames from OggDemuxer **THEN** the codec **SHALL** decode Ogg FLAC frames correctly
3. **WHEN** receiving frames from any demuxer **THEN** the codec **SHALL** not depend on container-specific information
4. **WHEN** initializing **THEN** the codec **SHALL** only require StreamInfo parameters, not container details
5. **WHEN** processing frames **THEN** the codec **SHALL** work with MediaChunk data regardless of source
6. **WHEN** seeking occurs **THEN** the codec **SHALL** reset state without requiring container-specific operations
7. **WHEN** handling metadata **THEN** the codec **SHALL** not process container-specific metadata
8. **WHEN** reporting capabilities **THEN** the codec **SHALL** indicate support for "flac" codec regardless of container

### **Requirement 6: Streaming and Buffering**

**User Story:** As a media player, I want efficient FLAC decoding with appropriate buffering, so that playback is smooth and responsive.

#### **Acceptance Criteria**

1. **WHEN** decoding frames **THEN** the codec **SHALL** process frames incrementally without requiring complete file access
2. **WHEN** buffering output **THEN** the codec **SHALL** implement bounded output buffers to prevent memory exhaustion
3. **WHEN** input is unavailable **THEN** the codec **SHALL** handle partial frame data gracefully
4. **WHEN** output buffer is full **THEN** the codec **SHALL** provide backpressure to prevent overflow
5. **WHEN** flushing buffers **THEN** the codec **SHALL** output any remaining decoded samples
6. **WHEN** resetting state **THEN** the codec **SHALL** clear all internal buffers and state
7. **WHEN** handling large frames **THEN** the codec **SHALL** process frames efficiently without excessive memory usage
8. **WHEN** streaming continuously **THEN** the codec **SHALL** maintain consistent latency and throughput

### **Requirement 7: Error Handling and Recovery**

**User Story:** As a media player, I want robust error handling in FLAC decoding, so that corrupted frames or data don't crash the application.

#### **Acceptance Criteria**

1. **WHEN** encountering invalid frame headers **THEN** the codec **SHALL** skip corrupted frames and continue
2. **WHEN** CRC validation fails **THEN** the codec **SHALL** log errors but attempt to use decoded data
3. **WHEN** subframe decoding fails **THEN** the codec **SHALL** output silence for affected channels
4. **WHEN** prediction fails **THEN** the codec **SHALL** fall back to safe decoding methods
5. **WHEN** memory allocation fails **THEN** the codec **SHALL** return appropriate error codes
6. **WHEN** invalid parameters are encountered **THEN** the codec **SHALL** validate inputs and reject invalid data
7. **WHEN** decoder state becomes inconsistent **THEN** the codec **SHALL** reset to a known good state
8. **WHEN** unrecoverable errors occur **THEN** the codec **SHALL** report errors clearly and stop processing

### **Requirement 8: Performance and Optimization**

**User Story:** As a media player, I want efficient FLAC decoding that doesn't impact playback quality, so that CPU usage remains reasonable even for high-resolution files.

#### **Acceptance Criteria**

1. **WHEN** decoding standard FLAC files **THEN** the codec **SHALL** maintain real-time performance on target hardware
2. **WHEN** processing high-resolution files **THEN** the codec **SHALL** handle 24-bit/96kHz and higher efficiently
3. **WHEN** using prediction **THEN** the codec **SHALL** implement efficient LPC prediction algorithms
4. **WHEN** decoding residuals **THEN** the codec **SHALL** use optimized Rice/Golomb decoding
5. **WHEN** performing bit operations **THEN** the codec **SHALL** use efficient bit manipulation techniques
6. **WHEN** converting bit depths **THEN** the codec **SHALL** use optimized conversion routines
7. **WHEN** handling multi-channel **THEN** the codec **SHALL** process channels efficiently
8. **WHEN** memory access occurs **THEN** the codec **SHALL** optimize for cache efficiency and memory bandwidth

### **Requirement 9: Thread Safety and Concurrency**

**User Story:** As a multi-threaded media player, I want thread-safe FLAC decoding operations, so that multiple codec instances can operate concurrently.

#### **Acceptance Criteria**

1. **WHEN** multiple codec instances exist **THEN** each instance **SHALL** maintain independent state
2. **WHEN** decoding occurs concurrently **THEN** codec instances **SHALL** not interfere with each other
3. **WHEN** shared resources are accessed **THEN** the codec **SHALL** use appropriate synchronization
4. **WHEN** static data is used **THEN** the codec **SHALL** ensure thread-safe access to static resources
5. **WHEN** initialization occurs **THEN** the codec **SHALL** handle concurrent initialization safely
6. **WHEN** cleanup occurs **THEN** the codec **SHALL** ensure no operations are in progress before destruction
7. **WHEN** errors occur **THEN** the codec **SHALL** maintain thread-local error state
8. **WHEN** debugging is enabled **THEN** logging operations **SHALL** be thread-safe

### **Requirement 10: Integration with AudioCodec Architecture**

**User Story:** As a PsyMP3 component, I want the FLAC codec to integrate seamlessly with the AudioCodec architecture, so that it works consistently with other audio codecs.

#### **Acceptance Criteria**

1. **WHEN** implementing AudioCodec interface **THEN** FLACCodec **SHALL** provide all required virtual methods
2. **WHEN** initializing **THEN** the codec **SHALL** configure itself from StreamInfo parameters
3. **WHEN** decoding chunks **THEN** the codec **SHALL** convert MediaChunk data to AudioFrame output
4. **WHEN** flushing **THEN** the codec **SHALL** output any remaining samples as AudioFrame
5. **WHEN** resetting **THEN** the codec **SHALL** clear state for seeking operations
6. **WHEN** reporting capabilities **THEN** the codec **SHALL** indicate support for FLAC streams
7. **WHEN** handling errors **THEN** the codec **SHALL** use PsyMP3's error reporting mechanisms
8. **WHEN** logging debug information **THEN** the codec **SHALL** use PsyMP3's Debug logging system

### **Requirement 11: Compatibility with Existing Implementation**

**User Story:** As a PsyMP3 user, I want the new FLAC codec to maintain compatibility with existing FLAC playback, so that my FLAC files continue to work without issues.

#### **Acceptance Criteria**

1. **WHEN** replacing existing FLAC decoder **THEN** the new codec **SHALL** decode all previously supported FLAC files
2. **WHEN** handling different encoders **THEN** the codec **SHALL** support FLAC files from various encoders
3. **WHEN** processing edge cases **THEN** the codec **SHALL** handle the same edge cases as current implementation
4. **WHEN** performance is measured **THEN** the codec **SHALL** provide comparable or better performance
5. **WHEN** memory usage is measured **THEN** the codec **SHALL** use reasonable memory amounts
6. **WHEN** seeking occurs **THEN** the codec **SHALL** provide equivalent seeking support
7. **WHEN** errors are encountered **THEN** the codec **SHALL** provide equivalent error handling
8. **WHEN** integrated with DemuxedStream **THEN** the codec **SHALL** work seamlessly with the bridge interface

### **Requirement 12: Quality and Accuracy**

**User Story:** As an audiophile, I want bit-perfect FLAC decoding, so that the decoded audio is identical to the original uncompressed audio.

#### **Acceptance Criteria**

1. **WHEN** decoding lossless FLAC **THEN** the codec **SHALL** produce bit-perfect output at the source bit depth
2. **WHEN** converting bit depths **THEN** the codec **SHALL** use appropriate dithering and scaling algorithms
3. **WHEN** handling rounding **THEN** the codec **SHALL** use proper rounding methods to minimize artifacts
4. **WHEN** processing floating-point operations **THEN** the codec **SHALL** maintain sufficient precision
5. **WHEN** validating output **THEN** the codec **SHALL** ensure decoded samples are within valid ranges
6. **WHEN** handling edge cases **THEN** the codec **SHALL** maintain accuracy for unusual sample values
7. **WHEN** processing silence **THEN** the codec **SHALL** output true digital silence (zero samples)
8. **WHEN** comparing with reference **THEN** the codec **SHALL** produce identical output to reference implementations

### **Requirement 13: RFC 9639 Compliance**

**User Story:** As a FLAC format implementer, I want strict compliance with RFC 9639, so that the codec works with all valid FLAC streams and produces correct output.

#### **Acceptance Criteria**

1. **WHEN** parsing frame headers **THEN** the codec **SHALL** follow RFC 9639 frame header specification exactly
2. **WHEN** decoding subframes **THEN** the codec **SHALL** implement all subframe types per RFC 9639
3. **WHEN** processing entropy coding **THEN** the codec **SHALL** follow RFC 9639 Rice/Golomb coding specification
4. **WHEN** handling prediction **THEN** the codec **SHALL** implement LPC prediction per RFC 9639 algorithms
5. **WHEN** validating CRC **THEN** the codec **SHALL** use RFC 9639 CRC-16 polynomial and calculation
6. **WHEN** processing channel assignments **THEN** the codec **SHALL** follow RFC 9639 channel assignment rules
7. **WHEN** handling bit depths **THEN** the codec **SHALL** support all RFC 9639 specified bit depths (4-32 bits)
8. **WHEN** encountering reserved values **THEN** the codec **SHALL** handle them per RFC 9639 error handling

### **Requirement 14: High-Performance Decoding**

**User Story:** As a user of high-resolution audio, I want efficient FLAC decoding that maintains real-time performance, so that 24-bit/96kHz+ files play smoothly without dropouts.

#### **Acceptance Criteria**

1. **WHEN** decoding 44.1kHz/16-bit FLAC **THEN** the codec **SHALL** maintain <1% CPU usage on target hardware
2. **WHEN** decoding 96kHz/24-bit FLAC **THEN** the codec **SHALL** maintain real-time performance without dropouts
3. **WHEN** processing highly compressed frames **THEN** the codec **SHALL** handle 10-14 byte frames efficiently
4. **WHEN** using libFLAC **THEN** the codec **SHALL** leverage libFLAC's optimized decoding routines
5. **WHEN** converting bit depths **THEN** the codec **SHALL** use optimized conversion algorithms
6. **WHEN** processing multiple channels **THEN** the codec **SHALL** handle up to 8 channels efficiently
7. **WHEN** memory allocation occurs **THEN** the codec **SHALL** minimize allocations during decoding
8. **WHEN** benchmarked **THEN** the codec **SHALL** meet or exceed current FLAC implementation performance

### **Requirement 15: Threading Safety Compliance**

**User Story:** As a PsyMP3 developer, I want the FLAC codec to follow established threading patterns, so that it integrates safely with the multi-threaded architecture.

#### **Acceptance Criteria**

1. **WHEN** implementing public methods **THEN** the codec **SHALL** follow public/private lock pattern with `_unlocked` suffixes
2. **WHEN** acquiring locks **THEN** the codec **SHALL** use RAII lock guards for exception safety
3. **WHEN** calling internal methods **THEN** the codec **SHALL** use `_unlocked` versions to prevent deadlocks
4. **WHEN** documenting locks **THEN** the codec **SHALL** document lock acquisition order to prevent deadlocks
5. **WHEN** handling callbacks **THEN** the codec **SHALL** never invoke callbacks while holding internal locks
6. **WHEN** managing state **THEN** the codec **SHALL** use appropriate synchronization primitives
7. **WHEN** multiple instances exist **THEN** each codec instance **SHALL** maintain independent thread-safe state
8. **WHEN** exceptions occur **THEN** the codec **SHALL** ensure locks are released via RAII guards

### **Requirement 16: Conditional Compilation Integration**

**User Story:** As a build system maintainer, I want the FLAC codec to integrate with conditional compilation, so that builds work correctly when FLAC dependencies are unavailable.

#### **Acceptance Criteria**

1. **WHEN** FLAC libraries are available **THEN** the codec **SHALL** compile and register with MediaFactory
2. **WHEN** FLAC libraries are unavailable **THEN** the build **SHALL** succeed without FLAC codec support
3. **WHEN** using preprocessor guards **THEN** the codec **SHALL** use appropriate `#ifdef` guards around FLAC-specific code
4. **WHEN** registering with MediaFactory **THEN** registration **SHALL** be conditionally compiled
5. **WHEN** checking codec availability **THEN** the system **SHALL** correctly report FLAC support status
6. **WHEN** building without FLAC **THEN** no FLAC-related symbols **SHALL** be referenced
7. **WHEN** linking **THEN** FLAC libraries **SHALL** only be linked when available
8. **WHEN** testing **THEN** FLAC codec tests **SHALL** be conditionally compiled based on availability