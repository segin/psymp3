# **VORBIS CODEC REQUIREMENTS**

## **Introduction**

This specification defines the requirements for implementing a container-agnostic Vorbis audio codec for PsyMP3. The Vorbis codec will decode Vorbis bitstream data from Ogg containers into standard 16-bit PCM audio. This codec works with the AudioCodec architecture and receives Vorbis packet data from the OggDemuxer.

The implementation must support:
- **Container-agnostic Vorbis decoding** from Ogg containers
- **All Vorbis quality levels** and encoding parameters
- **Variable bitrate and quality modes** with proper frame handling
- **Efficient packet-by-packet decoding** without requiring complete file access
- **Sample-accurate seeking support** through decoder reset capabilities
- **Thread-safe operation** for multi-threaded playback scenarios

## **Requirements**

### **Requirement 1: Vorbis Bitstream Decoding**

**User Story:** As an OggDemuxer component, I want to decode Vorbis packet data into PCM samples, so that I can provide audio output from Ogg Vorbis files.

#### **Acceptance Criteria**

1. **WHEN** receiving Vorbis header packets **THEN** the codec **SHALL** process identification, comment, and setup headers in sequence
2. **WHEN** processing identification header **THEN** the codec **SHALL** extract sample rate, channels, bitrate bounds, and block sizes
3. **WHEN** processing comment header **THEN** the codec **SHALL** extract Vorbis comments for metadata (handled by demuxer)
4. **WHEN** processing setup header **THEN** the codec **SHALL** initialize decoder with codebook and floor/residue configurations
5. **WHEN** decoding audio packets **THEN** the codec **SHALL** decode complete Vorbis frames into PCM samples
6. **WHEN** handling variable block sizes **THEN** the codec **SHALL** support both short and long blocks as specified
7. **WHEN** processing overlapped frames **THEN** the codec **SHALL** apply proper windowing and overlap-add reconstruction
8. **WHEN** frame validation fails **THEN** the codec **SHALL** handle corrupted packets gracefully

### **Requirement 2: libvorbis Integration**

**User Story:** As a Vorbis decoder, I want to use the proven libvorbis library, so that I can provide reliable and accurate Vorbis decoding.

#### **Acceptance Criteria**

1. **WHEN** initializing decoder **THEN** the codec **SHALL** use libvorbis vorbis_info and vorbis_dsp_state structures
2. **WHEN** processing headers **THEN** the codec **SHALL** use vorbis_synthesis_headerin for header validation
3. **WHEN** decoding packets **THEN** the codec **SHALL** use vorbis_synthesis and vorbis_synthesis_blockin
4. **WHEN** extracting PCM **THEN** the codec **SHALL** use vorbis_synthesis_pcmout for sample retrieval
5. **WHEN** managing decoder state **THEN** the codec **SHALL** properly initialize and cleanup libvorbis structures
6. **WHEN** handling errors **THEN** the codec **SHALL** interpret libvorbis error codes appropriately
7. **WHEN** resetting decoder **THEN** the codec **SHALL** reinitialize libvorbis state for seeking
8. **WHEN** processing end-of-stream **THEN** the codec **SHALL** handle libvorbis cleanup properly

### **Requirement 3: Variable Bitrate and Quality Handling**

**User Story:** As a media player, I want to play Vorbis files with different quality settings and bitrate modes, so that I can support various encoding configurations.

#### **Acceptance Criteria**

1. **WHEN** decoding constant bitrate Vorbis **THEN** the codec **SHALL** handle fixed bitrate streams efficiently
2. **WHEN** decoding variable bitrate Vorbis **THEN** the codec **SHALL** adapt to changing bitrates within the stream
3. **WHEN** decoding quality-based Vorbis **THEN** the codec **SHALL** handle quality mode encoding properly
4. **WHEN** processing different quality levels **THEN** the codec **SHALL** support quality levels from -1 to 10
5. **WHEN** handling bitrate bounds **THEN** the codec **SHALL** respect minimum, nominal, and maximum bitrate settings
6. **WHEN** decoding high-quality streams **THEN** the codec **SHALL** maintain audio fidelity without artifacts
7. **WHEN** decoding low-bitrate streams **THEN** the codec **SHALL** handle bandwidth limitations gracefully
8. **WHEN** switching between modes **THEN** the codec **SHALL** adapt seamlessly within the same stream

### **Requirement 4: Block Size and Windowing**

**User Story:** As a Vorbis decoder, I want to handle variable block sizes correctly, so that I can properly reconstruct the audio signal with correct overlap-add processing.

#### **Acceptance Criteria**

1. **WHEN** processing short blocks **THEN** the codec **SHALL** handle 64-2048 sample blocks as specified
2. **WHEN** processing long blocks **THEN** the codec **SHALL** handle 128-8192 sample blocks as specified
3. **WHEN** applying windowing **THEN** the codec **SHALL** use proper Vorbis window functions
4. **WHEN** performing overlap-add **THEN** the codec **SHALL** correctly combine overlapping portions of adjacent blocks
5. **WHEN** handling block transitions **THEN** the codec **SHALL** manage short-to-long and long-to-short transitions
6. **WHEN** managing decoder delay **THEN** the codec **SHALL** account for overlap-add latency
7. **WHEN** starting streams **THEN** the codec **SHALL** handle initial block overlap correctly
8. **WHEN** ending streams **THEN** the codec **SHALL** output remaining samples from final overlap

### **Requirement 5: Channel Configuration Support**

**User Story:** As a media player, I want to play Vorbis files with various channel configurations, so that I can support mono, stereo, and multi-channel audio.

#### **Acceptance Criteria**

1. **WHEN** decoding mono Vorbis **THEN** the codec **SHALL** output single-channel PCM data
2. **WHEN** decoding stereo Vorbis **THEN** the codec **SHALL** output interleaved left/right channel data
3. **WHEN** handling multi-channel Vorbis **THEN** the codec **SHALL** support up to 255 channels as per specification
4. **WHEN** processing channel coupling **THEN** the codec **SHALL** handle magnitude/angle channel pairs correctly
5. **WHEN** outputting multi-channel **THEN** the codec **SHALL** provide properly interleaved channel data
6. **WHEN** channel count changes **THEN** the codec **SHALL** adapt output format accordingly (shouldn't happen in practice)
7. **WHEN** handling channel mapping **THEN** the codec **SHALL** follow Vorbis channel ordering conventions
8. **WHEN** unsupported channels **THEN** the codec **SHALL** report error rather than producing incorrect output

### **Requirement 6: Container-Agnostic Operation**

**User Story:** As an OggDemuxer, I want the Vorbis codec to work independently of container details, so that it focuses purely on Vorbis bitstream decoding.

#### **Acceptance Criteria**

1. **WHEN** receiving packets from OggDemuxer **THEN** the codec **SHALL** decode based on packet data only
2. **WHEN** initializing **THEN** the codec **SHALL** only require StreamInfo parameters, not container details
3. **WHEN** processing packets **THEN** the codec **SHALL** work with MediaChunk data regardless of source
4. **WHEN** seeking occurs **THEN** the codec **SHALL** reset state without requiring container-specific operations
5. **WHEN** handling metadata **THEN** the codec **SHALL** not process container-specific metadata
6. **WHEN** reporting capabilities **THEN** the codec **SHALL** indicate support for "vorbis" codec regardless of container
7. **WHEN** managing state **THEN** the codec **SHALL** maintain decoder state independently of container
8. **WHEN** handling errors **THEN** the codec **SHALL** focus on bitstream errors, not container issues

### **Requirement 7: Streaming and Buffering**

**User Story:** As a media player, I want efficient Vorbis decoding with appropriate buffering, so that playback is smooth and responsive.

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

**User Story:** As a media player, I want robust error handling in Vorbis decoding, so that corrupted packets or data don't crash the application.

#### **Acceptance Criteria**

1. **WHEN** encountering invalid header packets **THEN** the codec **SHALL** reject initialization and report errors
2. **WHEN** packet validation fails **THEN** the codec **SHALL** skip corrupted packets and continue
3. **WHEN** synthesis fails **THEN** the codec **SHALL** output silence for affected frames
4. **WHEN** libvorbis reports errors **THEN** the codec **SHALL** interpret and handle error conditions appropriately
5. **WHEN** memory allocation fails **THEN** the codec **SHALL** return appropriate error codes
6. **WHEN** invalid parameters are encountered **THEN** the codec **SHALL** validate inputs and reject invalid data
7. **WHEN** decoder state becomes inconsistent **THEN** the codec **SHALL** reset to a known good state
8. **WHEN** unrecoverable errors occur **THEN** the codec **SHALL** report errors clearly and stop processing

### **Requirement 9: Performance and Optimization**

**User Story:** As a media player, I want efficient Vorbis decoding that doesn't impact playback quality, so that CPU usage remains reasonable even for high-quality files.

#### **Acceptance Criteria**

1. **WHEN** decoding standard Vorbis files **THEN** the codec **SHALL** maintain real-time performance on target hardware
2. **WHEN** processing high-quality files **THEN** the codec **SHALL** handle quality level 10 efficiently
3. **WHEN** using libvorbis **THEN** the codec **SHALL** leverage libvorbis optimizations and SIMD support
4. **WHEN** performing overlap-add **THEN** the codec **SHALL** use efficient windowing and reconstruction algorithms
5. **WHEN** handling multi-channel **THEN** the codec **SHALL** process channels efficiently
6. **WHEN** managing memory **THEN** the codec **SHALL** minimize allocation overhead and fragmentation
7. **WHEN** processing variable bitrate **THEN** the codec **SHALL** adapt efficiently to bitrate changes
8. **WHEN** memory access occurs **THEN** the codec **SHALL** optimize for cache efficiency and memory bandwidth

### **Requirement 10: Thread Safety and Concurrency**

**User Story:** As a multi-threaded media player, I want thread-safe Vorbis decoding operations, so that multiple codec instances can operate concurrently.

#### **Acceptance Criteria**

1. **WHEN** multiple codec instances exist **THEN** each instance **SHALL** maintain independent libvorbis state
2. **WHEN** decoding occurs concurrently **THEN** codec instances **SHALL** not interfere with each other
3. **WHEN** shared resources are accessed **THEN** the codec **SHALL** use appropriate synchronization
4. **WHEN** libvorbis is used **THEN** the codec **SHALL** ensure thread-safe usage of libvorbis functions
5. **WHEN** initialization occurs **THEN** the codec **SHALL** handle concurrent initialization safely
6. **WHEN** cleanup occurs **THEN** the codec **SHALL** ensure no operations are in progress before destruction
7. **WHEN** errors occur **THEN** the codec **SHALL** maintain thread-local error state
8. **WHEN** debugging is enabled **THEN** logging operations **SHALL** be thread-safe

### **Requirement 11: Integration with AudioCodec Architecture**

**User Story:** As a PsyMP3 component, I want the Vorbis codec to integrate seamlessly with the AudioCodec architecture, so that it works consistently with other audio codecs.

#### **Acceptance Criteria**

1. **WHEN** implementing AudioCodec interface **THEN** VorbisCodec **SHALL** provide all required virtual methods
2. **WHEN** initializing **THEN** the codec **SHALL** configure itself from StreamInfo parameters
3. **WHEN** decoding chunks **THEN** the codec **SHALL** convert MediaChunk data to AudioFrame output
4. **WHEN** flushing **THEN** the codec **SHALL** output any remaining samples as AudioFrame
5. **WHEN** resetting **THEN** the codec **SHALL** clear state for seeking operations
6. **WHEN** reporting capabilities **THEN** the codec **SHALL** indicate support for Vorbis streams
7. **WHEN** handling errors **THEN** the codec **SHALL** use PsyMP3's error reporting mechanisms
8. **WHEN** logging debug information **THEN** the codec **SHALL** use PsyMP3's Debug logging system

### **Requirement 12: Compatibility with Existing Implementation**

**User Story:** As a PsyMP3 user, I want the new Vorbis codec to maintain compatibility with existing Vorbis playback, so that my Ogg Vorbis files continue to work without issues.

#### **Acceptance Criteria**

1. **WHEN** replacing existing Vorbis decoder **THEN** the new codec **SHALL** decode all previously supported Vorbis files
2. **WHEN** handling different encoders **THEN** the codec **SHALL** support Vorbis files from various encoders (oggenc, etc.)
3. **WHEN** processing edge cases **THEN** the codec **SHALL** handle the same edge cases as current implementation
4. **WHEN** performance is measured **THEN** the codec **SHALL** provide comparable or better performance
5. **WHEN** memory usage is measured **THEN** the codec **SHALL** use reasonable memory amounts
6. **WHEN** seeking occurs **THEN** the codec **SHALL** provide equivalent seeking support through reset
7. **WHEN** errors are encountered **THEN** the codec **SHALL** provide equivalent error handling
8. **WHEN** integrated with DemuxedStream **THEN** the codec **SHALL** work seamlessly with the bridge interface

### **Requirement 13: Quality and Accuracy**

**User Story:** As an audiophile, I want high-quality Vorbis decoding, so that the decoded audio maintains the intended quality of the original encoding.

#### **Acceptance Criteria**

1. **WHEN** decoding lossless-quality Vorbis **THEN** the codec **SHALL** produce high-fidelity output
2. **WHEN** handling different quality levels **THEN** the codec **SHALL** maintain appropriate quality for each level
3. **WHEN** processing floating-point operations **THEN** the codec **SHALL** maintain sufficient precision
4. **WHEN** applying windowing **THEN** the codec **SHALL** use correct window functions and overlap-add
5. **WHEN** validating output **THEN** the codec **SHALL** ensure decoded samples are within valid ranges
6. **WHEN** handling edge cases **THEN** the codec **SHALL** maintain accuracy for unusual sample values
7. **WHEN** processing silence **THEN** the codec **SHALL** output true digital silence when appropriate
8. **WHEN** comparing with reference **THEN** the codec **SHALL** produce output consistent with libvorbis reference

### **Requirement 14: Metadata Integration**

**User Story:** As a media player, I want Vorbis comment metadata to be available, so that I can display track information extracted by the demuxer.

#### **Acceptance Criteria**

1. **WHEN** processing comment headers **THEN** the codec **SHALL** make header data available to demuxer
2. **WHEN** header validation occurs **THEN** the codec **SHALL** validate comment header structure
3. **WHEN** initializing **THEN** the codec **SHALL** not directly process metadata (handled by demuxer)
4. **WHEN** providing header data **THEN** the codec **SHALL** ensure comment header is accessible
5. **WHEN** handling encoding **THEN** the codec **SHALL** support UTF-8 encoded comments (via demuxer)
6. **WHEN** processing vendor strings **THEN** the codec **SHALL** handle encoder identification
7. **WHEN** validating headers **THEN** the codec **SHALL** ensure all three headers are present and valid
8. **WHEN** reporting stream info **THEN** the codec **SHALL** coordinate with demuxer for metadata population