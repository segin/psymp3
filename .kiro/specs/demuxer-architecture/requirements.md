# **DEMUXER ARCHITECTURE REQUIREMENTS**

## **Introduction**

This specification defines the requirements for PsyMP3's modern demuxer architecture, which provides a modular, extensible system for handling various media container formats. The architecture separates container parsing (demuxing) from audio/video decoding (codecs), enabling support for multiple formats through a unified interface.

The architecture consists of:
- **Base Demuxer Interface**: Abstract interface for all container demuxers
- **Concrete Demuxer Implementations**: Format-specific demuxers (Ogg, RIFF/WAV, AIFF, ISO/MP4, Raw)
- **MediaFactory System**: Extensible factory for format detection and stream creation
- **DemuxedStream Integration**: Bridge between new architecture and legacy Stream interface
- **IOHandler Integration**: Unified I/O abstraction for local files and HTTP streams

## **Requirements**

### **Requirement 1: Base Demuxer Interface**

**User Story:** As a codec developer, I want a consistent interface for accessing media containers, so that I can decode audio data regardless of the container format.

#### **Acceptance Criteria**

1. **WHEN** implementing a demuxer **THEN** it **SHALL** inherit from the base Demuxer class and implement all pure virtual methods
2. **WHEN** constructing a demuxer **THEN** it **SHALL** accept a unique_ptr<IOHandler> for I/O operations
3. **WHEN** parsing containers **THEN** the parseContainer() method **SHALL** return true on success and populate stream information
4. **WHEN** querying streams **THEN** getStreams() **SHALL** return vector of StreamInfo with complete metadata
5. **WHEN** reading data **THEN** readChunk() methods **SHALL** return MediaChunk objects with properly formatted data
6. **WHEN** seeking **THEN** seekTo() **SHALL** accept millisecond timestamps and return success/failure status
7. **WHEN** checking EOF **THEN** isEOF() **SHALL** accurately reflect end-of-stream condition
8. **WHEN** querying duration **THEN** getDuration() **SHALL** return total duration in milliseconds or 0 if unknown
9. **WHEN** tracking position **THEN** getPosition() **SHALL** return current playback position in milliseconds
10. **WHEN** handling Ogg formats **THEN** getGranulePosition() **SHALL** return codec-specific granule positions

### **Requirement 2: StreamInfo and MediaChunk Data Structures**

**User Story:** As a media processing component, I want comprehensive metadata about media streams, so that I can properly configure decoders and display information.

#### **Acceptance Criteria**

1. **WHEN** populating StreamInfo **THEN** it **SHALL** include stream_id, codec_type, codec_name, and codec_tag
2. **WHEN** describing audio streams **THEN** StreamInfo **SHALL** include sample_rate, channels, bits_per_sample, and bitrate
3. **WHEN** providing codec data **THEN** StreamInfo **SHALL** include codec_data vector for decoder initialization
4. **WHEN** reporting timing **THEN** StreamInfo **SHALL** include duration_samples and duration_ms
5. **WHEN** including metadata **THEN** StreamInfo **SHALL** provide artist, title, and album fields
6. **WHEN** creating MediaChunk **THEN** it **SHALL** include stream_id, data vector, and timing information
7. **WHEN** handling Ogg formats **THEN** MediaChunk **SHALL** include granule_position for timing
8. **WHEN** handling other formats **THEN** MediaChunk **SHALL** include timestamp_samples for timing
9. **WHEN** marking keyframes **THEN** MediaChunk **SHALL** set is_keyframe appropriately (usually true for audio)
10. **WHEN** tracking file position **THEN** MediaChunk **SHALL** include file_offset for seeking optimization

### **Requirement 3: OggDemuxer Implementation**

**User Story:** As a media player, I want to play Ogg-encapsulated audio files (Vorbis, Opus, FLAC), so that I can support open-source audio formats.

#### **Acceptance Criteria**

1. **WHEN** parsing Ogg containers **THEN** OggDemuxer **SHALL** use libogg for proper page and packet handling
2. **WHEN** identifying codecs **THEN** the demuxer **SHALL** recognize Vorbis, Opus, FLAC, and Speex signatures
3. **WHEN** processing headers **THEN** the demuxer **SHALL** parse codec-specific identification and comment headers
4. **WHEN** extracting metadata **THEN** the demuxer **SHALL** parse Vorbis comments and OpusTags
5. **WHEN** handling multiple streams **THEN** the demuxer **SHALL** support multiplexed logical bitstreams
6. **WHEN** seeking **THEN** the demuxer **SHALL** use bisection search with granule position targeting
7. **WHEN** calculating duration **THEN** the demuxer **SHALL** use last granule position or header information
8. **WHEN** streaming packets **THEN** the demuxer **SHALL** maintain packet order and handle page boundaries
9. **WHEN** handling Opus streams **THEN** the demuxer **SHALL** account for pre-skip samples in timing calculations
10. **WHEN** resetting after seeks **THEN** the demuxer **SHALL** NOT resend header packets to maintain decoder state

### **Requirement 4: ChunkDemuxer Implementation**

**User Story:** As a media player, I want to play chunk-based audio files (WAV, AIFF), so that I can support common uncompressed audio formats.

#### **Acceptance Criteria**

1. **WHEN** parsing RIFF/WAV files **THEN** ChunkDemuxer **SHALL** handle little-endian chunk structure
2. **WHEN** parsing AIFF files **THEN** ChunkDemuxer **SHALL** handle big-endian IFF chunk structure
3. **WHEN** reading format chunks **THEN** the demuxer **SHALL** extract sample rate, channels, and bit depth
4. **WHEN** processing data chunks **THEN** the demuxer **SHALL** locate and map audio data regions
5. **WHEN** handling compressed formats **THEN** the demuxer **SHALL** support various WAVE format tags
6. **WHEN** parsing AIFF compression **THEN** the demuxer **SHALL** handle NONE, sowt, fl32, and other compression types
7. **WHEN** seeking **THEN** the demuxer **SHALL** calculate byte offsets based on sample position and format
8. **WHEN** reading data **THEN** the demuxer **SHALL** respect block alignment and frame boundaries
9. **WHEN** handling IEEE 80-bit floats **THEN** the demuxer **SHALL** convert AIFF sample rates correctly
10. **WHEN** detecting endianness **THEN** the demuxer **SHALL** automatically handle FORM vs RIFF containers

### **Requirement 5: ISODemuxer Implementation**

**User Story:** As a media player, I want to play ISO Base Media files (MP4, M4A), so that I can support modern container formats.

#### **Acceptance Criteria**

1. **WHEN** parsing MP4 containers **THEN** ISODemuxer **SHALL** handle hierarchical box structure
2. **WHEN** reading box headers **THEN** the demuxer **SHALL** support both 32-bit and 64-bit size fields
3. **WHEN** processing movie headers **THEN** the demuxer **SHALL** extract track information and timescales
4. **WHEN** parsing sample tables **THEN** the demuxer **SHALL** build chunk offset, size, and timing tables
5. **WHEN** handling multiple tracks **THEN** the demuxer **SHALL** identify and prioritize audio tracks
6. **WHEN** seeking **THEN** the demuxer **SHALL** use sample tables for precise positioning
7. **WHEN** reading samples **THEN** the demuxer **SHALL** calculate chunk positions and extract sample data
8. **WHEN** processing codecs **THEN** the demuxer **SHALL** map format FourCCs to codec names
9. **WHEN** calculating duration **THEN** the demuxer **SHALL** use track duration and timescale information
10. **WHEN** handling fragmented MP4 **THEN** the demuxer **SHALL** support basic fragmentation (future extension)

### **Requirement 6: RawAudioDemuxer Implementation**

**User Story:** As a media player, I want to play raw audio files (PCM, A-law, μ-law), so that I can support telephony and embedded audio formats.

#### **Acceptance Criteria**

1. **WHEN** detecting raw formats **THEN** RawAudioDemuxer **SHALL** use file extension-based identification
2. **WHEN** configuring formats **THEN** the demuxer **SHALL** support explicit RawAudioConfig parameters
3. **WHEN** handling telephony formats **THEN** the demuxer **SHALL** default to 8kHz, mono for A-law/μ-law
4. **WHEN** processing PCM formats **THEN** the demuxer **SHALL** support various bit depths and endianness
5. **WHEN** calculating duration **THEN** the demuxer **SHALL** use file size and format parameters
6. **WHEN** seeking **THEN** the demuxer **SHALL** calculate byte offsets based on sample position
7. **WHEN** reading data **THEN** the demuxer **SHALL** respect frame boundaries and channel interleaving
8. **WHEN** handling unknown extensions **THEN** the demuxer **SHALL** provide reasonable defaults
9. **WHEN** validating configurations **THEN** the demuxer **SHALL** ensure consistent format parameters
10. **WHEN** streaming data **THEN** the demuxer **SHALL** provide fixed-size chunks for consistent playback

### **Requirement 7: DemuxerFactory System**

**User Story:** As a media processing system, I want automatic format detection and demuxer creation, so that I can handle various file types transparently.

#### **Acceptance Criteria**

1. **WHEN** creating demuxers **THEN** DemuxerFactory **SHALL** accept IOHandler and optional file path
2. **WHEN** probing formats **THEN** the factory **SHALL** examine magic bytes for format identification
3. **WHEN** detecting RIFF **THEN** the factory **SHALL** recognize "RIFF" signature and create ChunkDemuxer
4. **WHEN** detecting AIFF **THEN** the factory **SHALL** recognize "FORM" signature and create ChunkDemuxer
5. **WHEN** detecting Ogg **THEN** the factory **SHALL** recognize "OggS" signature and create OggDemuxer
6. **WHEN** detecting MP4 **THEN** the factory **SHALL** recognize "ftyp" box and create ISODemuxer
7. **WHEN** detecting raw audio **THEN** the factory **SHALL** use file extension hints for RawAudioDemuxer
8. **WHEN** format detection fails **THEN** the factory **SHALL** return nullptr rather than throwing exceptions
9. **WHEN** multiple formats match **THEN** the factory **SHALL** prioritize based on signature strength
10. **WHEN** handling edge cases **THEN** the factory **SHALL** validate IOHandler before format probing

### **Requirement 8: MediaFactory Integration**

**User Story:** As a PsyMP3 component, I want a comprehensive media factory system, so that I can handle various formats and protocols through a unified interface.

#### **Acceptance Criteria**

1. **WHEN** registering formats **THEN** MediaFactory **SHALL** support dynamic format registration with metadata
2. **WHEN** detecting content **THEN** the factory **SHALL** use multiple detection methods (extension, MIME, magic bytes)
3. **WHEN** creating streams **THEN** the factory **SHALL** route to appropriate stream implementations
4. **WHEN** handling HTTP URLs **THEN** the factory **SHALL** create HTTPIOHandler instances
5. **WHEN** handling local files **THEN** the factory **SHALL** create FileIOHandler instances
6. **WHEN** analyzing content **THEN** the factory **SHALL** return ContentInfo with confidence scores
7. **WHEN** supporting streaming **THEN** the factory **SHALL** indicate format streaming capabilities
8. **WHEN** querying formats **THEN** the factory **SHALL** provide comprehensive format metadata
9. **WHEN** handling MIME types **THEN** the factory **SHALL** map between MIME types and extensions
10. **WHEN** extending functionality **THEN** the factory **SHALL** support plugin-based format registration

### **Requirement 9: DemuxedStream Bridge**

**User Story:** As a legacy PsyMP3 component, I want to use the new demuxer architecture through the existing Stream interface, so that I can benefit from modern demuxing without code changes.

#### **Acceptance Criteria**

1. **WHEN** implementing Stream interface **THEN** DemuxedStream **SHALL** provide all required Stream methods
2. **WHEN** initializing **THEN** DemuxedStream **SHALL** create appropriate demuxer and codec instances
3. **WHEN** selecting streams **THEN** DemuxedStream **SHALL** automatically choose the best audio stream
4. **WHEN** reading data **THEN** DemuxedStream **SHALL** decode compressed data to PCM samples
5. **WHEN** seeking **THEN** DemuxedStream **SHALL** translate position requests to demuxer seek operations
6. **WHEN** buffering **THEN** DemuxedStream **SHALL** maintain efficient chunk and frame buffers
7. **WHEN** handling metadata **THEN** DemuxedStream **SHALL** prefer container metadata over file-based tags
8. **WHEN** switching streams **THEN** DemuxedStream **SHALL** support dynamic stream switching
9. **WHEN** reporting properties **THEN** DemuxedStream **SHALL** update Stream base class with current format info
10. **WHEN** handling errors **THEN** DemuxedStream **SHALL** gracefully degrade and report issues

### **Requirement 10: Performance and Efficiency**

**User Story:** As a media player, I want efficient demuxing operations that don't impact playback quality, so that audio streaming remains smooth and responsive.

#### **Acceptance Criteria**

1. **WHEN** parsing containers **THEN** demuxers **SHALL** use streaming approach without loading entire files
2. **WHEN** seeking **THEN** demuxers **SHALL** use efficient algorithms (bisection search, sample tables)
3. **WHEN** buffering data **THEN** demuxers **SHALL** implement bounded buffers to prevent memory exhaustion
4. **WHEN** processing packets **THEN** demuxers **SHALL** minimize memory copying and allocation
5. **WHEN** handling large files **THEN** demuxers **SHALL** maintain acceptable performance characteristics
6. **WHEN** caching metadata **THEN** demuxers **SHALL** store frequently accessed information efficiently
7. **WHEN** reading sequentially **THEN** demuxers **SHALL** optimize for forward reading patterns
8. **WHEN** handling multiple streams **THEN** demuxers **SHALL** share resources efficiently between streams
9. **WHEN** processing headers **THEN** demuxers **SHALL** parse headers incrementally without excessive buffering
10. **WHEN** managing state **THEN** demuxers **SHALL** use minimal state tracking for position and timing

### **Requirement 11: Error Handling and Robustness**

**User Story:** As a media player, I want robust error handling in demuxing operations, so that corrupted files or network issues don't crash the application.

#### **Acceptance Criteria**

1. **WHEN** encountering corrupted containers **THEN** demuxers **SHALL** skip damaged sections and continue processing
2. **WHEN** parsing fails **THEN** demuxers **SHALL** return false from parseContainer() rather than throwing exceptions
3. **WHEN** I/O operations fail **THEN** demuxers **SHALL** handle read errors and EOF conditions gracefully
4. **WHEN** seeking beyond boundaries **THEN** demuxers **SHALL** clamp to valid ranges and report success/failure
5. **WHEN** invalid parameters are provided **THEN** demuxers **SHALL** validate inputs and return appropriate errors
6. **WHEN** memory allocation fails **THEN** demuxers **SHALL** handle allocation failures without crashing
7. **WHEN** format detection fails **THEN** factories **SHALL** return null pointers rather than invalid objects
8. **WHEN** codec initialization fails **THEN** DemuxedStream **SHALL** fall back gracefully or report errors
9. **WHEN** stream switching fails **THEN** components **SHALL** maintain current stream and report failure
10. **WHEN** cleanup is required **THEN** destructors **SHALL** ensure all resources are properly released

### **Requirement 12: Thread Safety and Concurrency**

**User Story:** As a multi-threaded media player, I want thread-safe demuxing operations, so that seeking and reading can occur concurrently without corruption.

#### **Acceptance Criteria**

1. **WHEN** multiple threads access demuxers **THEN** implementations **SHALL** protect shared state appropriately
2. **WHEN** seeking occurs during playback **THEN** demuxers **SHALL** handle concurrent operations safely
3. **WHEN** I/O operations are in progress **THEN** demuxers **SHALL** prevent race conditions on IOHandler
4. **WHEN** packet queues are accessed **THEN** demuxers **SHALL** ensure thread-safe queue operations
5. **WHEN** stream state is modified **THEN** demuxers **SHALL** use atomic operations where appropriate
6. **WHEN** factory methods are called **THEN** MediaFactory **SHALL** handle concurrent format registration
7. **WHEN** cleanup occurs **THEN** destructors **SHALL** ensure no operations are in progress
8. **WHEN** errors occur in one thread **THEN** demuxers **SHALL** propagate error state safely to other threads
9. **WHEN** caching data **THEN** demuxers **SHALL** implement thread-safe caching mechanisms
10. **WHEN** debugging is enabled **THEN** logging operations **SHALL** be thread-safe

### **Requirement 13: Extensibility and Plugin Support**

**User Story:** As a PsyMP3 developer, I want an extensible demuxer architecture, so that I can add support for new formats without modifying core code.

#### **Acceptance Criteria**

1. **WHEN** adding new formats **THEN** developers **SHALL** be able to implement Demuxer interface independently
2. **WHEN** registering formats **THEN** MediaFactory **SHALL** support runtime format registration
3. **WHEN** detecting content **THEN** the system **SHALL** support custom content detectors
4. **WHEN** creating streams **THEN** the system **SHALL** support custom stream factory functions
5. **WHEN** handling protocols **THEN** the system **SHALL** support custom IOHandler implementations
6. **WHEN** extending metadata **THEN** StreamInfo **SHALL** be extensible for format-specific information
7. **WHEN** adding codecs **THEN** the system **SHALL** integrate with AudioCodec architecture
8. **WHEN** implementing plugins **THEN** the system **SHALL** provide stable ABI for external modules
9. **WHEN** versioning formats **THEN** the system **SHALL** support format version detection and handling
10. **WHEN** configuring behavior **THEN** the system **SHALL** support runtime configuration options

### **Requirement 14: Integration and API Consistency**

**User Story:** As a PsyMP3 component, I want the demuxer architecture to integrate seamlessly with the rest of the codebase, so that it works consistently with other subsystems.

#### **Acceptance Criteria**

1. **WHEN** integrating with IOHandler **THEN** demuxers **SHALL** use IOHandler interface exclusively for I/O
2. **WHEN** reporting errors **THEN** the system **SHALL** use PsyMP3's exception hierarchy and error codes
3. **WHEN** logging debug information **THEN** the system **SHALL** use PsyMP3's Debug logging with appropriate categories
4. **WHEN** handling URIs **THEN** the system **SHALL** integrate with PsyMP3's URI parsing components
5. **WHEN** working with TagLib **THEN** components **SHALL** accept TagLib::String parameters for compatibility
6. **WHEN** managing memory **THEN** the system **SHALL** follow PsyMP3's RAII and smart pointer conventions
7. **WHEN** handling configuration **THEN** the system **SHALL** respect PsyMP3's configuration system
8. **WHEN** providing metadata **THEN** the system **SHALL** integrate with existing metadata display systems
9. **WHEN** supporting seeking **THEN** the system **SHALL** coordinate with player position tracking
10. **WHEN** handling playback **THEN** the system **SHALL** integrate with audio output and buffering systems