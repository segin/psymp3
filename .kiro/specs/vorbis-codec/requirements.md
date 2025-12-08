# **VORBIS CODEC REQUIREMENTS**

## **Introduction**

This specification defines the requirements for implementing a container-agnostic Vorbis audio codec for PsyMP3. The VorbisCodec decodes Vorbis bitstream data from Ogg containers into standard 16-bit PCM audio using the libvorbis reference library.

The implementation provides:
- **Container-agnostic Vorbis decoding** receiving packets from OggDemuxer
- **Complete libvorbis API integration** for reliable, specification-compliant decoding
- **All Vorbis quality levels** (-1 to 10) and encoding configurations
- **Variable bitrate handling** with proper frame processing
- **Efficient packet-by-packet decoding** without requiring complete file access
- **Sample-accurate seeking support** through decoder reset capabilities
- **Thread-safe operation** for multi-threaded playback scenarios

## **Glossary**

- **VorbisCodec**: The audio codec component that decodes Vorbis bitstream data into PCM audio samples
- **libvorbis**: The reference Vorbis decoder library providing core decoding functionality
- **vorbis_info**: libvorbis structure containing stream parameters (sample rate, channels, bitrates, block sizes)
- **vorbis_comment**: libvorbis structure containing metadata (vendor string, user comments)
- **vorbis_dsp_state**: libvorbis structure maintaining decoder synthesis state for PCM generation
- **vorbis_block**: libvorbis structure representing a single decoded audio block
- **ogg_packet**: Structure containing a single Vorbis packet with data pointer, size, and flags
- **Vorbis Packet**: A unit of compressed Vorbis audio data extracted from an Ogg container
- **Header Packet**: One of three initialization packets (identification, comment, setup) required before audio decoding
- **Identification Header**: First header packet (type 0x01) containing sample rate, channels, bitrate bounds, and block sizes
- **Comment Header**: Second header packet (type 0x03) containing metadata (vendor string, user comments)
- **Setup Header**: Third header packet (type 0x05) containing codebook and floor/residue configurations
- **PCM**: Pulse Code Modulation - uncompressed digital audio representation
- **AudioFrame**: Output structure containing decoded PCM samples with metadata
- **MediaChunk**: Input structure containing compressed Vorbis packet data
- **StreamInfo**: Configuration structure containing stream parameters for codec initialization
- **Block Size**: The number of samples in a Vorbis audio block (short: 64-8192, long: 64-8192, where long >= short)
- **Overlap-Add**: Signal reconstruction technique combining overlapping windowed blocks with 50% overlap
- **MDCT**: Modified Discrete Cosine Transform - frequency domain transform used in Vorbis encoding
- **VBR**: Variable Bitrate - encoding mode where bitrate varies based on audio complexity
- **ABR**: Average Bitrate - encoding mode targeting a specific average bitrate
- **Quality Level**: Vorbis encoding quality parameter ranging from -1 to 10
- **Channel Coupling**: Vorbis stereo encoding technique using magnitude/angle representation
- **Granule Position**: PCM sample position used for timing and seeking in Ogg containers
- **OggDemuxer**: The demuxer component that extracts Vorbis packets from Ogg containers
- **DemuxedStream**: Bridge interface connecting demuxers to codecs in PsyMP3 architecture
- **Framing Flag**: Single bit at end of each header packet that must be set to 1 for validity

## **Requirements**

### **Requirement 1: libvorbis Structure Initialization**

**User Story:** As a VorbisCodec, I want to properly initialize all libvorbis structures, so that the decoder is ready to process Vorbis packets.

#### **Acceptance Criteria**

1. WHEN initializing the decoder THEN the VorbisCodec SHALL call vorbis_info_init() to initialize the vorbis_info structure
2. WHEN initializing the decoder THEN the VorbisCodec SHALL call vorbis_comment_init() to initialize the vorbis_comment structure
3. WHEN all three header packets are processed successfully THEN the VorbisCodec SHALL call vorbis_synthesis_init() to initialize the vorbis_dsp_state structure
4. WHEN vorbis_synthesis_init() succeeds THEN the VorbisCodec SHALL call vorbis_block_init() to initialize the vorbis_block structure
5. WHEN vorbis_synthesis_init() returns non-zero THEN the VorbisCodec SHALL report initialization failure and clean up allocated structures
6. WHEN vorbis_block_init() returns non-zero THEN the VorbisCodec SHALL report initialization failure and clean up allocated structures

### **Requirement 2: libvorbis Structure Cleanup**

**User Story:** As a VorbisCodec, I want to properly clean up all libvorbis structures, so that memory is released and resources are freed.

#### **Acceptance Criteria**

1. WHEN destroying the decoder THEN the VorbisCodec SHALL call vorbis_block_clear() to release the vorbis_block structure
2. WHEN destroying the decoder THEN the VorbisCodec SHALL call vorbis_dsp_clear() to release the vorbis_dsp_state structure
3. WHEN destroying the decoder THEN the VorbisCodec SHALL call vorbis_comment_clear() to release the vorbis_comment structure
4. WHEN destroying the decoder THEN the VorbisCodec SHALL call vorbis_info_clear() to release the vorbis_info structure
5. WHEN destroying the decoder THEN the VorbisCodec SHALL call cleanup functions in reverse initialization order (block, dsp, comment, info)
6. WHEN cleanup occurs during partial initialization THEN the VorbisCodec SHALL only clean up structures that were successfully initialized

### **Requirement 3: Header Packet Processing**

**User Story:** As a VorbisCodec, I want to process Vorbis header packets correctly, so that the decoder is configured for audio decoding.

#### **Acceptance Criteria**

1. WHEN receiving header packets THEN the VorbisCodec SHALL process identification (0x01vorbis), comment (0x03vorbis), and setup (0x05vorbis) headers in sequence
2. WHEN processing any header packet THEN the VorbisCodec SHALL call vorbis_synthesis_headerin() with the vorbis_info, vorbis_comment, and ogg_packet structures
3. WHEN vorbis_synthesis_headerin() returns 0 THEN the VorbisCodec SHALL increment the header packet counter and continue processing
4. WHEN vorbis_synthesis_headerin() returns OV_ENOTVORBIS THEN the VorbisCodec SHALL reject the packet as not valid Vorbis data
5. WHEN vorbis_synthesis_headerin() returns OV_EBADHEADER THEN the VorbisCodec SHALL reject initialization due to corrupted or invalid header
6. WHEN vorbis_synthesis_headerin() returns OV_EFAULT THEN the VorbisCodec SHALL report internal library fault and reset state
7. WHEN header packets arrive out of sequence THEN the VorbisCodec SHALL reject the stream and report header sequence error
8. WHEN fewer than three header packets are received before audio packets THEN the VorbisCodec SHALL reject audio packets until all headers are processed

### **Requirement 4: Identification Header Extraction**

**User Story:** As a VorbisCodec, I want to extract stream parameters from the identification header, so that I can configure the decoder correctly.

#### **Acceptance Criteria**

1. WHEN processing identification header THEN the VorbisCodec SHALL extract the Vorbis version from vorbis_info.version
2. WHEN processing identification header THEN the VorbisCodec SHALL extract the channel count from vorbis_info.channels
3. WHEN processing identification header THEN the VorbisCodec SHALL extract the sample rate from vorbis_info.rate
4. WHEN processing identification header THEN the VorbisCodec SHALL extract bitrate_maximum from vorbis_info.bitrate_upper
5. WHEN processing identification header THEN the VorbisCodec SHALL extract bitrate_nominal from vorbis_info.bitrate_nominal
6. WHEN processing identification header THEN the VorbisCodec SHALL extract bitrate_minimum from vorbis_info.bitrate_lower
7. WHEN processing identification header THEN the VorbisCodec SHALL extract block sizes using vorbis_info_blocksize() for indices 0 (short) and 1 (long)
8. WHEN vorbis_info.version is non-zero THEN the VorbisCodec SHALL reject the stream as unsupported Vorbis version

### **Requirement 5: Block Size Validation**

**User Story:** As a VorbisCodec, I want to validate block size parameters, so that the decoder handles variable block sizes correctly.

#### **Acceptance Criteria**

1. WHEN extracting block sizes THEN the VorbisCodec SHALL verify blocksize_0 is a power of 2 in the range 64 to 8192
2. WHEN extracting block sizes THEN the VorbisCodec SHALL verify blocksize_1 is a power of 2 in the range 64 to 8192
3. WHEN extracting block sizes THEN the VorbisCodec SHALL verify blocksize_1 is greater than or equal to blocksize_0
4. WHEN block size validation fails THEN the VorbisCodec SHALL reject the stream with invalid block size error
5. WHEN processing audio packets THEN the VorbisCodec SHALL handle both short blocks (blocksize_0) and long blocks (blocksize_1)
6. WHEN block transitions occur THEN the VorbisCodec SHALL allow libvorbis to manage short-to-long and long-to-short window transitions

### **Requirement 6: Audio Packet Decoding**

**User Story:** As a VorbisCodec, I want to decode Vorbis audio packets into PCM samples, so that audio can be played back.

#### **Acceptance Criteria**

1. WHEN decoding an audio packet THEN the VorbisCodec SHALL create an ogg_packet structure with packet data, size, and flags
2. WHEN decoding an audio packet THEN the VorbisCodec SHALL call vorbis_synthesis() with the vorbis_block and ogg_packet structures
3. WHEN vorbis_synthesis() returns 0 THEN the VorbisCodec SHALL call vorbis_synthesis_blockin() with the vorbis_dsp_state and vorbis_block
4. WHEN vorbis_synthesis() returns OV_ENOTAUDIO THEN the VorbisCodec SHALL skip the packet as it is not an audio packet
5. WHEN vorbis_synthesis() returns OV_EBADPACKET THEN the VorbisCodec SHALL skip the corrupted packet and continue with the next packet
6. WHEN vorbis_synthesis_blockin() returns non-zero THEN the VorbisCodec SHALL log the error and attempt recovery
7. WHEN decoding the first audio packet THEN the VorbisCodec SHALL handle the initial block that produces no output due to overlap-add

### **Requirement 7: PCM Sample Extraction**

**User Story:** As a VorbisCodec, I want to extract decoded PCM samples from libvorbis, so that audio frames can be output.

#### **Acceptance Criteria**

1. WHEN extracting PCM samples THEN the VorbisCodec SHALL call vorbis_synthesis_pcmout() to get a pointer to decoded float samples
2. WHEN vorbis_synthesis_pcmout() returns a positive sample count THEN the VorbisCodec SHALL process the available samples
3. WHEN vorbis_synthesis_pcmout() returns 0 THEN the VorbisCodec SHALL indicate no samples are currently available
4. WHEN samples are processed THEN the VorbisCodec SHALL call vorbis_synthesis_read() with the number of samples consumed
5. WHEN vorbis_synthesis_read() is called THEN the VorbisCodec SHALL pass the exact number of samples that were processed
6. WHEN multiple blocks of samples are available THEN the VorbisCodec SHALL extract all available samples before returning

### **Requirement 8: Float to PCM Conversion**

**User Story:** As a VorbisCodec, I want to convert libvorbis float output to 16-bit PCM, so that audio can be played through the audio system.

#### **Acceptance Criteria**

1. WHEN converting float samples THEN the VorbisCodec SHALL scale float values in range [-1.0, 1.0] to 16-bit range [-32768, 32767]
2. WHEN float samples exceed 1.0 THEN the VorbisCodec SHALL clamp the output to 32767
3. WHEN float samples are below -1.0 THEN the VorbisCodec SHALL clamp the output to -32768
4. WHEN converting multi-channel audio THEN the VorbisCodec SHALL interleave channels in Vorbis channel order
5. WHEN outputting samples THEN the VorbisCodec SHALL produce 16-bit signed integer PCM samples
6. WHEN processing silence THEN the VorbisCodec SHALL output true digital silence (zero samples)

### **Requirement 9: Channel Configuration**

**User Story:** As a VorbisCodec, I want to handle various channel configurations, so that mono, stereo, and multi-channel audio can be decoded.

#### **Acceptance Criteria**

1. WHEN decoding mono Vorbis (1 channel) THEN the VorbisCodec SHALL output single-channel PCM data
2. WHEN decoding stereo Vorbis (2 channels) THEN the VorbisCodec SHALL output interleaved left/right channel data
3. WHEN decoding multi-channel Vorbis (3-255 channels) THEN the VorbisCodec SHALL output properly interleaved channel data
4. WHEN channel count is 0 THEN the VorbisCodec SHALL reject the stream with invalid channel count error
5. WHEN outputting multi-channel audio THEN the VorbisCodec SHALL follow Vorbis channel ordering conventions
6. WHEN channel coupling is used THEN the VorbisCodec SHALL allow libvorbis to handle magnitude/angle decoupling

### **Requirement 10: Decoder Reset for Seeking**

**User Story:** As a VorbisCodec, I want to reset the decoder state for seeking, so that playback can resume from a new position.

#### **Acceptance Criteria**

1. WHEN reset is requested THEN the VorbisCodec SHALL call vorbis_synthesis_restart() to reset synthesis state
2. WHEN vorbis_synthesis_restart() is called THEN the VorbisCodec SHALL preserve header information (vorbis_info, vorbis_comment)
3. WHEN reset completes THEN the VorbisCodec SHALL be ready to decode audio packets from a new position
4. WHEN reset is called before headers are processed THEN the VorbisCodec SHALL have no effect
5. WHEN reset is called THEN the VorbisCodec SHALL clear any buffered PCM samples
6. WHEN reset is called THEN the VorbisCodec SHALL reset internal position tracking to zero

### **Requirement 11: Flush Operation**

**User Story:** As a VorbisCodec, I want to flush remaining samples at end of stream, so that all decoded audio is output.

#### **Acceptance Criteria**

1. WHEN flush is requested THEN the VorbisCodec SHALL extract all remaining samples from vorbis_synthesis_pcmout()
2. WHEN flush completes THEN the VorbisCodec SHALL return an AudioFrame containing all remaining decoded samples
3. WHEN no samples remain THEN the VorbisCodec SHALL return an empty AudioFrame
4. WHEN flush is called before headers are processed THEN the VorbisCodec SHALL return an empty AudioFrame
5. WHEN flush is called THEN the VorbisCodec SHALL mark the returned AudioFrame as end-of-stream if applicable

### **Requirement 12: Error Code Handling**

**User Story:** As a VorbisCodec, I want to handle all libvorbis error codes appropriately, so that errors are reported and recovered from correctly.

#### **Acceptance Criteria**

1. WHEN OV_EREAD is returned THEN the VorbisCodec SHALL report read error and skip the current packet
2. WHEN OV_EFAULT is returned THEN the VorbisCodec SHALL report internal fault and reset decoder state
3. WHEN OV_EIMPL is returned THEN the VorbisCodec SHALL report unimplemented feature error
4. WHEN OV_EINVAL is returned THEN the VorbisCodec SHALL report invalid argument and skip the current packet
5. WHEN OV_ENOTVORBIS is returned THEN the VorbisCodec SHALL reject the data as not Vorbis format
6. WHEN OV_EBADHEADER is returned THEN the VorbisCodec SHALL reject initialization due to bad header
7. WHEN OV_EVERSION is returned THEN the VorbisCodec SHALL reject the stream due to version mismatch
8. WHEN OV_ENOTAUDIO is returned THEN the VorbisCodec SHALL skip the packet as non-audio data
9. WHEN OV_EBADPACKET is returned THEN the VorbisCodec SHALL skip the corrupted packet and continue
10. WHEN OV_EBADLINK is returned THEN the VorbisCodec SHALL report link corruption error

### **Requirement 13: Corrupted Packet Recovery**

**User Story:** As a VorbisCodec, I want to recover from corrupted packets, so that playback continues despite data errors.

#### **Acceptance Criteria**

1. WHEN vorbis_synthesis() fails on a packet THEN the VorbisCodec SHALL skip the packet and continue with the next
2. WHEN multiple consecutive packets fail THEN the VorbisCodec SHALL continue attempting to decode subsequent packets
3. WHEN decoder state becomes inconsistent THEN the VorbisCodec SHALL call vorbis_synthesis_restart() to reset
4. WHEN recovery succeeds THEN the VorbisCodec SHALL resume normal decoding without re-sending headers
5. WHEN unrecoverable error occurs THEN the VorbisCodec SHALL report the error through PsyMP3's Debug logging system

### **Requirement 14: Container-Agnostic Operation**

**User Story:** As a VorbisCodec, I want to operate independently of container details, so that I focus purely on Vorbis bitstream decoding.

#### **Acceptance Criteria**

1. WHEN receiving packets THEN the VorbisCodec SHALL decode based on packet data only, not container metadata
2. WHEN initializing THEN the VorbisCodec SHALL require only StreamInfo parameters, not container-specific details
3. WHEN processing packets THEN the VorbisCodec SHALL work with MediaChunk data regardless of source container
4. WHEN seeking occurs THEN the VorbisCodec SHALL reset state without requiring container-specific operations
5. WHEN reporting capabilities THEN the VorbisCodec SHALL indicate support for "vorbis" codec identifier
6. WHEN handling errors THEN the VorbisCodec SHALL focus on bitstream errors, not container issues

### **Requirement 15: Streaming and Buffering**

**User Story:** As a VorbisCodec, I want efficient streaming with appropriate buffering, so that playback is smooth and memory usage is bounded.

#### **Acceptance Criteria**

1. WHEN decoding packets THEN the VorbisCodec SHALL process packets incrementally without requiring complete file access
2. WHEN buffering output THEN the VorbisCodec SHALL limit internal buffers to prevent memory exhaustion
3. WHEN output buffer reaches maximum size THEN the VorbisCodec SHALL provide backpressure indication
4. WHEN resetting state THEN the VorbisCodec SHALL clear all internal buffers
5. WHEN handling large packets THEN the VorbisCodec SHALL process efficiently without excessive memory allocation

### **Requirement 16: Thread Safety**

**User Story:** As a VorbisCodec, I want thread-safe operation, so that multiple codec instances can operate concurrently.

#### **Acceptance Criteria**

1. WHEN multiple codec instances exist THEN each instance SHALL maintain independent libvorbis state
2. WHEN decoding occurs concurrently THEN codec instances SHALL not interfere with each other
3. WHEN initialization occurs concurrently THEN the VorbisCodec SHALL handle concurrent initialization safely
4. WHEN cleanup occurs THEN the VorbisCodec SHALL ensure no operations are in progress before destruction
5. WHEN errors occur THEN the VorbisCodec SHALL maintain instance-local error state
6. WHEN logging debug information THEN the VorbisCodec SHALL use thread-safe logging operations

### **Requirement 17: AudioCodec Interface Integration**

**User Story:** As a PsyMP3 component, I want the VorbisCodec to integrate with the AudioCodec architecture, so that it works consistently with other codecs.

#### **Acceptance Criteria**

1. WHEN implementing AudioCodec interface THEN VorbisCodec SHALL provide initialize(), decode(), flush(), reset(), getCodecName(), and canDecode() methods
2. WHEN initializing THEN the VorbisCodec SHALL configure itself from StreamInfo parameters
3. WHEN decoding THEN the VorbisCodec SHALL convert MediaChunk input to AudioFrame output
4. WHEN flushing THEN the VorbisCodec SHALL output remaining samples as AudioFrame
5. WHEN resetting THEN the VorbisCodec SHALL clear state for seeking operations
6. WHEN reporting codec name THEN the VorbisCodec SHALL return "vorbis" string
7. WHEN checking decode capability THEN the VorbisCodec SHALL return true for StreamInfo with "vorbis" codec

### **Requirement 18: Quality and Bitrate Handling**

**User Story:** As a VorbisCodec, I want to handle all Vorbis quality levels and bitrate modes, so that any valid Vorbis stream can be decoded.

#### **Acceptance Criteria**

1. WHEN decoding quality level -1 streams THEN the VorbisCodec SHALL decode correctly at approximately 45 kb/s
2. WHEN decoding quality level 10 streams THEN the VorbisCodec SHALL decode correctly at approximately 500 kb/s
3. WHEN decoding VBR streams THEN the VorbisCodec SHALL adapt to varying bitrates within the stream
4. WHEN decoding ABR streams THEN the VorbisCodec SHALL handle average bitrate targeting
5. WHEN bitrate bounds are specified THEN the VorbisCodec SHALL respect minimum, nominal, and maximum values
6. WHEN bitrate bounds are -1 (unset) THEN the VorbisCodec SHALL handle unbounded bitrate correctly

### **Requirement 19: Metadata Access**

**User Story:** As a VorbisCodec, I want to provide access to Vorbis comment metadata, so that the demuxer can extract track information.

#### **Acceptance Criteria**

1. WHEN comment header is processed THEN the VorbisCodec SHALL make vorbis_comment structure accessible
2. WHEN vendor string is present THEN the VorbisCodec SHALL preserve the encoder identification string
3. WHEN user comments are present THEN the VorbisCodec SHALL preserve all field=value comment pairs
4. WHEN metadata is requested before headers are processed THEN the VorbisCodec SHALL indicate metadata unavailable
5. WHEN comment header validation fails THEN the VorbisCodec SHALL report the specific validation error

### **Requirement 20: Compatibility**

**User Story:** As a PsyMP3 user, I want the VorbisCodec to decode all valid Vorbis files, so that my audio collection plays correctly.

#### **Acceptance Criteria**

1. WHEN decoding files from various encoders THEN the VorbisCodec SHALL support oggenc, ffmpeg, and other standard encoders
2. WHEN decoding edge cases THEN the VorbisCodec SHALL handle minimum and maximum parameter values
3. WHEN performance is measured THEN the VorbisCodec SHALL maintain real-time decoding on target hardware
4. WHEN integrated with DemuxedStream THEN the VorbisCodec SHALL work seamlessly with the bridge interface
5. WHEN decoding reference test vectors THEN the VorbisCodec SHALL produce output consistent with libvorbis reference

