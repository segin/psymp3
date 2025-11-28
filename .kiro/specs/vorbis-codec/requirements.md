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

## **Glossary**

- **VorbisCodec**: The audio codec component that decodes Vorbis bitstream data into PCM audio samples
- **libvorbis**: The reference Vorbis decoder library providing vorbis_info, vorbis_dsp_state, and related structures
- **Vorbis Packet**: A unit of compressed Vorbis audio data extracted from an Ogg container
- **Header Packet**: One of three initialization packets (identification, comment, setup) required before audio decoding
- **Identification Header**: First header packet containing sample rate, channels, bitrate bounds, and block sizes
- **Comment Header**: Second header packet containing metadata (vendor string, user comments)
- **Setup Header**: Third header packet containing codebook and floor/residue configurations
- **PCM**: Pulse Code Modulation - uncompressed digital audio representation
- **AudioFrame**: Output structure containing decoded PCM samples with metadata
- **MediaChunk**: Input structure containing compressed Vorbis packet data
- **StreamInfo**: Configuration structure containing stream parameters for codec initialization
- **Block Size**: The number of samples in a Vorbis audio block (short: 64-2048, long: 128-8192)
- **Overlap-Add**: Signal reconstruction technique combining overlapping windowed blocks
- **VBR**: Variable Bitrate - encoding mode where bitrate varies based on audio complexity
- **CBR**: Constant Bitrate - encoding mode with fixed bitrate throughout the stream
- **Quality Level**: Vorbis encoding quality parameter ranging from -1 to 10
- **Channel Coupling**: Vorbis stereo encoding technique using magnitude/angle representation
- **OggDemuxer**: The demuxer component that extracts Vorbis packets from Ogg containers
- **DemuxedStream**: Bridge interface connecting demuxers to codecs in PsyMP3 architecture

## **Requirements**

### **Requirement 1: Vorbis Bitstream Decoding**

**User Story:** As an OggDemuxer component, I want to decode Vorbis packet data into PCM samples, so that I can provide audio output from Ogg Vorbis files.

#### **Acceptance Criteria**

1. **WHEN** receiving Vorbis header packets **THEN** the VorbisCodec **SHALL** process identification (\x01vorbis), comment (\x03vorbis), and setup (\x05vorbis) headers in sequence
2. **WHEN** processing identification header **THEN** the VorbisCodec **SHALL** extract version, channels, rate, bitrate_upper, bitrate_nominal, bitrate_lower from vorbis_info structure
3. **WHEN** processing comment header **THEN** the VorbisCodec **SHALL** validate header structure and make vorbis_comment data available
4. **WHEN** processing setup header **THEN** the VorbisCodec **SHALL** complete codec_setup initialization via vorbis_synthesis_headerin()
5. **WHEN** decoding audio packets **THEN** the VorbisCodec **SHALL** decode complete Vorbis frames into interleaved 16-bit PCM samples
6. **WHEN** handling variable block sizes **THEN** the VorbisCodec **SHALL** support both short blocks (zo=0) and long blocks (zo=1) via vorbis_info_blocksize()
7. **WHEN** processing overlapped frames **THEN** the VorbisCodec **SHALL** delegate windowing and overlap-add reconstruction to libvorbis
8. **WHEN** frame validation fails **THEN** the VorbisCodec **SHALL** skip corrupted packets and continue with next packet

### **Requirement 2: libvorbis Integration**

**User Story:** As a Vorbis decoder, I want to use the proven libvorbis library, so that I can provide reliable and accurate Vorbis decoding.

#### **Acceptance Criteria**

1. **WHEN** initializing decoder **THEN** the VorbisCodec **SHALL** use vorbis_info_init() and vorbis_comment_init() to initialize libvorbis structures
2. **WHEN** processing headers **THEN** the VorbisCodec **SHALL** use vorbis_synthesis_headerin() for all three header packets (identification, comment, setup)
3. **WHEN** all headers are processed **THEN** the VorbisCodec **SHALL** use vorbis_synthesis_init() and vorbis_block_init() to initialize the synthesis engine
4. **WHEN** decoding audio packets **THEN** the VorbisCodec **SHALL** use vorbis_synthesis() followed by vorbis_synthesis_blockin()
5. **WHEN** extracting PCM samples **THEN** the VorbisCodec **SHALL** use vorbis_synthesis_pcmout() and vorbis_synthesis_read() to retrieve and consume samples
6. **WHEN** handling libvorbis errors **THEN** the VorbisCodec **SHALL** interpret error codes (OV_EREAD, OV_EFAULT, OV_EINVAL, OV_ENOTVORBIS, OV_EBADHEADER) appropriately
7. **WHEN** resetting decoder for seeking **THEN** the VorbisCodec **SHALL** use vorbis_synthesis_restart() to reset synthesis state without reinitializing headers
8. **WHEN** destroying decoder **THEN** the VorbisCodec **SHALL** call vorbis_block_clear(), vorbis_dsp_clear(), vorbis_comment_clear(), and vorbis_info_clear() in reverse initialization order

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

1. **WHEN** processing short blocks **THEN** the VorbisCodec **SHALL** handle blocksize_0 values (power of 2 from 64 to 8192 samples) as specified in identification header
2. **WHEN** processing long blocks **THEN** the VorbisCodec **SHALL** handle blocksize_1 values (power of 2 from 64 to 8192 samples, where blocksize_1 >= blocksize_0) as specified in identification header
3. **WHEN** applying windowing **THEN** the VorbisCodec **SHALL** delegate to libvorbis for proper Vorbis window function application via vorbis_synthesis()
4. **WHEN** performing overlap-add **THEN** the VorbisCodec **SHALL** delegate to libvorbis for correct 50% overlap combination via vorbis_synthesis_blockin()
5. **WHEN** handling block transitions **THEN** the VorbisCodec **SHALL** allow libvorbis to manage short-to-long and long-to-short window transitions
6. **WHEN** managing decoder delay **THEN** the VorbisCodec **SHALL** account for overlap-add latency inherent in libvorbis synthesis
7. **WHEN** starting streams **THEN** the VorbisCodec **SHALL** handle initial block overlap correctly (first block produces no output)
8. **WHEN** ending streams **THEN** the VorbisCodec **SHALL** call flush() to output remaining samples from final overlap buffer

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

1. **WHEN** vorbis_synthesis_headerin() returns OV_ENOTVORBIS **THEN** the VorbisCodec **SHALL** reject the packet as not Vorbis data
2. **WHEN** vorbis_synthesis_headerin() returns OV_EBADHEADER **THEN** the VorbisCodec **SHALL** reject initialization due to corrupted header
3. **WHEN** vorbis_synthesis() returns non-zero **THEN** the VorbisCodec **SHALL** skip the corrupted packet and continue with next packet
4. **WHEN** vorbis_synthesis_blockin() returns non-zero **THEN** the VorbisCodec **SHALL** log the error and attempt recovery
5. **WHEN** OV_EFAULT is returned **THEN** the VorbisCodec **SHALL** reset decoder state due to internal inconsistency
6. **WHEN** memory allocation fails **THEN** the VorbisCodec **SHALL** throw BadFormatException with descriptive message
7. **WHEN** decoder state becomes inconsistent **THEN** the VorbisCodec **SHALL** call vorbis_synthesis_restart() to reset to known good state
8. **WHEN** unrecoverable errors occur **THEN** the VorbisCodec **SHALL** report errors through PsyMP3's Debug logging system and stop processing

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