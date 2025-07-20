# **OPUS CODEC REQUIREMENTS**

## **Introduction**

This specification defines the requirements for implementing a container-agnostic Opus audio codec for PsyMP3. The Opus codec will decode Opus bitstream data from Ogg containers into standard 16-bit PCM audio. This codec works with the AudioCodec architecture and receives Opus packet data from the OggDemuxer.

The implementation must support:
- **Container-agnostic Opus decoding** from Ogg containers
- **All Opus modes** including SILK, CELT, and hybrid modes
- **Variable bitrate and adaptive modes** with proper frame handling
- **Efficient packet-by-packet decoding** without requiring complete file access
- **Sample-accurate seeking support** through decoder reset capabilities
- **Thread-safe operation** for multi-threaded playback scenarios

## **Requirements**

### **Requirement 1: Opus Bitstream Decoding**

**User Story:** As an OggDemuxer component, I want to decode Opus packet data into PCM samples, so that I can provide audio output from Ogg Opus files.

#### **Acceptance Criteria**

1. **WHEN** receiving Opus header packets **THEN** the codec **SHALL** process identification and comment headers in sequence
2. **WHEN** processing identification header **THEN** the codec **SHALL** extract sample rate, channels, pre-skip, and gain parameters
3. **WHEN** processing comment header **THEN** the codec **SHALL** extract Opus comments for metadata (handled by demuxer)
4. **WHEN** decoding audio packets **THEN** the codec **SHALL** decode complete Opus frames into PCM samples
5. **WHEN** handling variable frame sizes **THEN** the codec **SHALL** support 2.5ms to 60ms frame durations
6. **WHEN** processing different modes **THEN** the codec **SHALL** handle SILK, CELT, and hybrid mode packets
7. **WHEN** applying pre-skip **THEN** the codec **SHALL** discard initial samples as specified in header
8. **WHEN** frame validation fails **THEN** the codec **SHALL** handle corrupted packets gracefully

### **Requirement 2: libopus Integration**

**User Story:** As an Opus decoder, I want to use the proven libopus library, so that I can provide reliable and accurate Opus decoding.

#### **Acceptance Criteria**

1. **WHEN** initializing decoder **THEN** the codec **SHALL** use libopus OpusDecoder structure
2. **WHEN** processing headers **THEN** the codec **SHALL** use opus_header parsing functions
3. **WHEN** decoding packets **THEN** the codec **SHALL** use opus_decode or opus_decode_float
4. **WHEN** managing decoder state **THEN** the codec **SHALL** properly initialize and cleanup libopus structures
5. **WHEN** handling errors **THEN** the codec **SHALL** interpret libopus error codes appropriately
6. **WHEN** resetting decoder **THEN** the codec **SHALL** use opus_decoder_ctl for reset operations
7. **WHEN** configuring decoder **THEN** the codec **SHALL** set appropriate decoder parameters
8. **WHEN** processing end-of-stream **THEN** the codec **SHALL** handle libopus cleanup properly

### **Requirement 3: Multi-Mode Support**

**User Story:** As a media player, I want to play Opus files encoded with different modes, so that I can support various encoding configurations optimized for different content types.

#### **Acceptance Criteria**

1. **WHEN** decoding SILK mode packets **THEN** the codec **SHALL** handle speech-optimized encoding efficiently
2. **WHEN** decoding CELT mode packets **THEN** the codec **SHALL** handle music-optimized encoding efficiently
3. **WHEN** decoding hybrid mode packets **THEN** the codec **SHALL** handle combined SILK+CELT encoding
4. **WHEN** switching between modes **THEN** the codec **SHALL** adapt seamlessly within the same stream
5. **WHEN** processing different bandwidths **THEN** the codec **SHALL** support narrowband to fullband
6. **WHEN** handling frame durations **THEN** the codec **SHALL** support 2.5, 5, 10, 20, 40, and 60ms frames
7. **WHEN** decoding variable complexity **THEN** the codec **SHALL** handle different encoder complexity settings
8. **WHEN** processing adaptive modes **THEN** the codec **SHALL** handle encoder mode switching

### **Requirement 4: Sample Rate and Channel Handling**

**User Story:** As a media player, I want to play Opus files with various sample rates and channel configurations, so that I can support different audio formats.

#### **Acceptance Criteria**

1. **WHEN** decoding 48kHz Opus **THEN** the codec **SHALL** output native 48kHz PCM data
2. **WHEN** handling other sample rates **THEN** the codec **SHALL** decode at 48kHz and let resampling be handled externally
3. **WHEN** decoding mono Opus **THEN** the codec **SHALL** output single-channel PCM data
4. **WHEN** decoding stereo Opus **THEN** the codec **SHALL** output interleaved left/right channel data
5. **WHEN** handling multi-channel Opus **THEN** the codec **SHALL** support up to 255 channels as per specification
6. **WHEN** processing channel mapping **THEN** the codec **SHALL** follow Opus channel ordering conventions
7. **WHEN** handling coupled channels **THEN** the codec **SHALL** process stereo coupling correctly
8. **WHEN** unsupported configurations **THEN** the codec **SHALL** report error rather than producing incorrect output

### **Requirement 5: Pre-skip and Gain Handling**

**User Story:** As an Opus decoder, I want to handle pre-skip and output gain correctly, so that decoded audio starts at the correct position with proper levels.

#### **Acceptance Criteria**

1. **WHEN** processing pre-skip value **THEN** the codec **SHALL** discard the specified number of initial samples
2. **WHEN** applying output gain **THEN** the codec **SHALL** apply gain adjustment as specified in header
3. **WHEN** handling zero pre-skip **THEN** the codec **SHALL** output all decoded samples
4. **WHEN** managing decoder delay **THEN** the codec **SHALL** account for algorithmic delay in pre-skip
5. **WHEN** starting playback **THEN** the codec **SHALL** ensure correct audio timing
6. **WHEN** seeking occurs **THEN** the codec **SHALL** handle pre-skip appropriately after reset
7. **WHEN** gain is zero **THEN** the codec **SHALL** apply no gain adjustment
8. **WHEN** calculating output **THEN** the codec **SHALL** ensure proper sample alignment

### **Requirement 6: Container-Agnostic Operation**

**User Story:** As an OggDemuxer, I want the Opus codec to work independently of container details, so that it focuses purely on Opus bitstream decoding.

#### **Acceptance Criteria**

1. **WHEN** receiving packets from OggDemuxer **THEN** the codec **SHALL** decode based on packet data only
2. **WHEN** initializing **THEN** the codec **SHALL** only require StreamInfo parameters, not container details
3. **WHEN** processing packets **THEN** the codec **SHALL** work with MediaChunk data regardless of source
4. **WHEN** seeking occurs **THEN** the codec **SHALL** reset state without requiring container-specific operations
5. **WHEN** handling metadata **THEN** the codec **SHALL** not process container-specific metadata
6. **WHEN** reporting capabilities **THEN** the codec **SHALL** indicate support for "opus" codec regardless of container
7. **WHEN** managing state **THEN** the codec **SHALL** maintain decoder state independently of container
8. **WHEN** handling errors **THEN** the codec **SHALL** focus on bitstream errors, not container issues

### **Requirement 7: Streaming and Buffering**

**User Story:** As a media player, I want efficient Opus decoding with appropriate buffering, so that playback is smooth and responsive.

#### **Acceptance Criteria**

1. **WHEN** decoding packets **THEN** the codec **SHALL** process packets incrementally without requiring complete file access
2. **WHEN** buffering output **THEN** the codec **SHALL** implement bounded output buffers to prevent memory exhaustion
3. **WHEN** input is unavailable **THEN** the codec **SHALL** handle partial packet data gracefully
4. **WHEN** output buffer is full **THEN** the codec **SHALL** provide backpressure to prevent overflow
5. **WHEN** flushing buffers **THEN** the codec **SHALL** output any remaining decoded samples
6. **WHEN** resetting state **THEN** the codec **SHALL** clear all internal buffers and decoder state
7. **WHEN** handling large packets **THEN** the codec **SHALL** process packets efficiently without excessive memory usage
8. **WHEN** streaming continuously **THEN** the codec **SHALL** maintain consistent latency and throughput

### **Requirement 8: Error Handling and Recovery**

**User Story:** As a media player, I want robust error handling in Opus decoding, so that corrupted packets or data don't crash the application.

#### **Acceptance Criteria**

1. **WHEN** encountering invalid header packets **THEN** the codec **SHALL** reject initialization and report errors
2. **WHEN** packet validation fails **THEN** the codec **SHALL** skip corrupted packets and continue
3. **WHEN** decoding fails **THEN** the codec **SHALL** output silence for affected frames
4. **WHEN** libopus reports errors **THEN** the codec **SHALL** interpret and handle error conditions appropriately
5. **WHEN** memory allocation fails **THEN** the codec **SHALL** return appropriate error codes
6. **WHEN** invalid parameters are encountered **THEN** the codec **SHALL** validate inputs and reject invalid data
7. **WHEN** decoder state becomes inconsistent **THEN** the codec **SHALL** reset to a known good state
8. **WHEN** unrecoverable errors occur **THEN** the codec **SHALL** report errors clearly and stop processing

### **Requirement 9: Performance and Optimization**

**User Story:** As a media player, I want efficient Opus decoding that doesn't impact playback quality, so that CPU usage remains reasonable even for high-quality files.

#### **Acceptance Criteria**

1. **WHEN** decoding standard Opus files **THEN** the codec **SHALL** maintain real-time performance on target hardware
2. **WHEN** processing high-bitrate files **THEN** the codec **SHALL** handle maximum bitrate efficiently
3. **WHEN** using libopus **THEN** the codec **SHALL** leverage libopus optimizations and SIMD support
4. **WHEN** handling different modes **THEN** the codec **SHALL** process SILK, CELT, and hybrid modes efficiently
5. **WHEN** processing multi-channel **THEN** the codec **SHALL** handle channels efficiently
6. **WHEN** managing memory **THEN** the codec **SHALL** minimize allocation overhead and fragmentation
7. **WHEN** processing variable frame sizes **THEN** the codec **SHALL** adapt efficiently to frame changes
8. **WHEN** memory access occurs **THEN** the codec **SHALL** optimize for cache efficiency and memory bandwidth

### **Requirement 10: Thread Safety and Concurrency**

**User Story:** As a multi-threaded media player, I want thread-safe Opus decoding operations, so that multiple codec instances can operate concurrently.

#### **Acceptance Criteria**

1. **WHEN** multiple codec instances exist **THEN** each instance **SHALL** maintain independent libopus state
2. **WHEN** decoding occurs concurrently **THEN** codec instances **SHALL** not interfere with each other
3. **WHEN** shared resources are accessed **THEN** the codec **SHALL** use appropriate synchronization
4. **WHEN** libopus is used **THEN** the codec **SHALL** ensure thread-safe usage of libopus functions
5. **WHEN** initialization occurs **THEN** the codec **SHALL** handle concurrent initialization safely
6. **WHEN** cleanup occurs **THEN** the codec **SHALL** ensure no operations are in progress before destruction
7. **WHEN** errors occur **THEN** the codec **SHALL** maintain thread-local error state
8. **WHEN** debugging is enabled **THEN** logging operations **SHALL** be thread-safe

### **Requirement 11: Integration with AudioCodec Architecture**

**User Story:** As a PsyMP3 component, I want the Opus codec to integrate seamlessly with the AudioCodec architecture, so that it works consistently with other audio codecs.

#### **Acceptance Criteria**

1. **WHEN** implementing AudioCodec interface **THEN** OpusCodec **SHALL** provide all required virtual methods
2. **WHEN** initializing **THEN** the codec **SHALL** configure itself from StreamInfo parameters
3. **WHEN** decoding chunks **THEN** the codec **SHALL** convert MediaChunk data to AudioFrame output
4. **WHEN** flushing **THEN** the codec **SHALL** output any remaining samples as AudioFrame
5. **WHEN** resetting **THEN** the codec **SHALL** clear state for seeking operations
6. **WHEN** reporting capabilities **THEN** the codec **SHALL** indicate support for Opus streams
7. **WHEN** handling errors **THEN** the codec **SHALL** use PsyMP3's error reporting mechanisms
8. **WHEN** logging debug information **THEN** the codec **SHALL** use PsyMP3's Debug logging system

### **Requirement 12: Compatibility and Standards Compliance**

**User Story:** As a PsyMP3 user, I want the Opus codec to support all standard Opus files, so that I can play Opus content from various sources.

#### **Acceptance Criteria**

1. **WHEN** decoding RFC 6716 compliant streams **THEN** the codec **SHALL** handle all standard Opus bitstreams
2. **WHEN** processing different encoders **THEN** the codec **SHALL** support Opus files from various encoders
3. **WHEN** handling edge cases **THEN** the codec **SHALL** process unusual but valid Opus streams
4. **WHEN** performance is measured **THEN** the codec **SHALL** provide efficient decoding performance
5. **WHEN** memory usage is measured **THEN** the codec **SHALL** use reasonable memory amounts
6. **WHEN** seeking occurs **THEN** the codec **SHALL** provide seeking support through reset
7. **WHEN** errors are encountered **THEN** the codec **SHALL** provide robust error handling
8. **WHEN** integrated with DemuxedStream **THEN** the codec **SHALL** work seamlessly with the bridge interface

### **Requirement 13: Quality and Accuracy**

**User Story:** As an audiophile, I want high-quality Opus decoding, so that the decoded audio maintains the intended quality of the original encoding.

#### **Acceptance Criteria**

1. **WHEN** decoding high-bitrate Opus **THEN** the codec **SHALL** produce high-fidelity output
2. **WHEN** handling different modes **THEN** the codec **SHALL** maintain appropriate quality for each mode
3. **WHEN** processing floating-point operations **THEN** the codec **SHALL** maintain sufficient precision
4. **WHEN** applying gain **THEN** the codec **SHALL** use correct gain calculations
5. **WHEN** validating output **THEN** the codec **SHALL** ensure decoded samples are within valid ranges
6. **WHEN** handling edge cases **THEN** the codec **SHALL** maintain accuracy for unusual sample values
7. **WHEN** processing silence **THEN** the codec **SHALL** output true digital silence when appropriate
8. **WHEN** comparing with reference **THEN** the codec **SHALL** produce output consistent with libopus reference

### **Requirement 14: Metadata Integration**

**User Story:** As a media player, I want Opus comment metadata to be available, so that I can display track information extracted by the demuxer.

#### **Acceptance Criteria**

1. **WHEN** processing comment headers **THEN** the codec **SHALL** make header data available to demuxer
2. **WHEN** header validation occurs **THEN** the codec **SHALL** validate comment header structure
3. **WHEN** initializing **THEN** the codec **SHALL** not directly process metadata (handled by demuxer)
4. **WHEN** providing header data **THEN** the codec **SHALL** ensure comment header is accessible
5. **WHEN** handling encoding **THEN** the codec **SHALL** support UTF-8 encoded comments (via demuxer)
6. **WHEN** processing vendor strings **THEN** the codec **SHALL** handle encoder identification
7. **WHEN** validating headers **THEN** the codec **SHALL** ensure both headers are present and valid
8. **WHEN** reporting stream info **THEN** the codec **SHALL** coordinate with demuxer for metadata population