# **DEMUXER ARCHITECTURE REQUIREMENTS**

## **Introduction**

This specification defines the requirements for PsyMP3's modern demuxer architecture, which provides a modular, extensible system for handling various media container formats. The architecture separates container parsing (demuxing) from audio/video decoding (codecs), enabling support for multiple formats through a unified interface.

The architecture consists of:
- **Base Demuxer Interface**: Abstract interface for all container demuxers
- **Concrete Demuxer Implementations**: Format-specific demuxers (Ogg, RIFF/WAV, AIFF, ISO/MP4, Raw)
- **MediaFactory System**: Extensible factory for format detection and stream creation
- **DemuxedStream Integration**: Bridge between new architecture and legacy Stream interface
- **IOHandler Integration**: Unified I/O abstraction for local files and HTTP streams

## **Glossary**

- **Demuxer**: A software component that parses media container formats and extracts individual streams (audio, video, subtitle) from the container
- **Container Format**: A file format that encapsulates one or more media streams along with metadata (e.g., MP4, Ogg, WAV)
- **MediaChunk**: A data structure containing a portion of compressed media data along with timing and metadata information
- **StreamInfo**: A data structure containing comprehensive metadata about a media stream including codec type, sample rate, channels, and duration
- **IOHandler**: An abstraction layer for input/output operations supporting both local files and network streams
- **MediaFactory**: A factory system responsible for automatic format detection and appropriate stream creation
- **DemuxedStream**: A bridge component that adapts the demuxer architecture to the legacy Stream interface
- **Granule Position**: A codec-specific timestamp value used in Ogg containers for precise timing
- **Sample Table**: A data structure in ISO/MP4 containers that maps timestamps to file positions for seeking
- **Chunk-Based Format**: A container format organized as a sequence of labeled data chunks (e.g., RIFF, AIFF)
- **Magic Bytes**: Specific byte sequences at the beginning of files used for format identification
- **Bisection Search**: A binary search algorithm used for efficient seeking in variable-rate media streams
- **Pre-skip**: The number of samples that should be discarded at the beginning of Opus streams for proper synchronization

## **Requirements**

### **Requirement 1: Base Demuxer Interface**

**User Story:** As a codec developer, I want a consistent interface for accessing media containers, so that I can decode audio data regardless of the container format.

#### **Acceptance Criteria**

1. WHEN a developer implements a Demuxer THEN the Demuxer **SHALL** inherit from the base Demuxer class and implement all pure virtual methods
2. WHEN the system constructs a Demuxer THEN the Demuxer **SHALL** accept a unique_ptr<IOHandler> for I/O operations
3. WHEN the Demuxer parses a container THEN the parseContainer() method **SHALL** return true on success and populate stream information
4. WHEN a component queries available streams THEN the getStreams() method **SHALL** return a vector of StreamInfo with complete metadata
5. WHEN a component reads media data THEN the readChunk() method **SHALL** return MediaChunk objects with properly formatted data
6. WHEN a component requests seeking THEN the seekTo() method **SHALL** accept millisecond timestamps and return success or failure status
7. WHEN a component checks end-of-file status THEN the isEOF() method **SHALL** accurately reflect the end-of-stream condition
8. WHEN a component queries stream duration THEN the getDuration() method **SHALL** return total duration in milliseconds or 0 if unknown
9. WHEN a component tracks playback position THEN the getPosition() method **SHALL** return current playback position in milliseconds
10. WHEN the Demuxer handles Ogg formats THEN the getGranulePosition() method **SHALL** return codec-specific granule positions

### **Requirement 2: StreamInfo and MediaChunk Data Structures**

**User Story:** As a media processing component, I want comprehensive metadata about media streams, so that I can properly configure decoders and display information.

#### **Acceptance Criteria**

1. WHEN the Demuxer populates StreamInfo THEN the StreamInfo **SHALL** include stream_id, codec_type, codec_name, and codec_tag
2. WHEN the Demuxer describes audio streams THEN the StreamInfo **SHALL** include sample_rate, channels, bits_per_sample, and bitrate
3. WHEN the Demuxer provides codec initialization data THEN the StreamInfo **SHALL** include a codec_data vector for decoder initialization
4. WHEN the Demuxer reports timing information THEN the StreamInfo **SHALL** include duration_samples and duration_ms
5. WHEN the Demuxer includes metadata THEN the StreamInfo **SHALL** provide artist, title, and album fields
6. WHEN the Demuxer creates a MediaChunk THEN the MediaChunk **SHALL** include stream_id, data vector, and timing information
7. WHEN the Demuxer handles Ogg formats THEN the MediaChunk **SHALL** include granule_position for timing
8. WHEN the Demuxer handles non-Ogg formats THEN the MediaChunk **SHALL** include timestamp_samples for timing
9. WHEN the Demuxer marks keyframes THEN the MediaChunk **SHALL** set is_keyframe to true for audio frames
10. WHEN the Demuxer tracks file position THEN the MediaChunk **SHALL** include file_offset for seeking optimization

### **Requirement 3: OggDemuxer Implementation**

**User Story:** As a media player, I want to play Ogg-encapsulated audio files (Vorbis, Opus, FLAC), so that I can support open-source audio formats.

#### **Acceptance Criteria**

1. WHEN the OggDemuxer parses Ogg containers THEN the OggDemuxer **SHALL** use libogg for proper page and packet handling
2. WHEN the OggDemuxer identifies codecs THEN the OggDemuxer **SHALL** recognize Vorbis, Opus, FLAC, and Speex signatures
3. WHEN the OggDemuxer processes headers THEN the OggDemuxer **SHALL** parse codec-specific identification and comment headers
4. WHEN the OggDemuxer extracts metadata THEN the OggDemuxer **SHALL** parse Vorbis comments and OpusTags
5. WHEN the OggDemuxer handles files with multiple streams THEN the OggDemuxer **SHALL** support multiplexed logical bitstreams
6. WHEN the OggDemuxer performs seeking THEN the OggDemuxer **SHALL** use bisection search with granule position targeting
7. WHEN the OggDemuxer calculates duration THEN the OggDemuxer **SHALL** use last granule position or header information
8. WHEN the OggDemuxer streams packets THEN the OggDemuxer **SHALL** maintain packet order and handle page boundaries correctly
9. WHEN the OggDemuxer handles Opus streams THEN the OggDemuxer **SHALL** account for pre-skip samples in timing calculations
10. WHEN the OggDemuxer resets after seeking THEN the OggDemuxer **SHALL** NOT resend header packets to maintain decoder state

### **Requirement 4: ChunkDemuxer Implementation**

**User Story:** As a media player, I want to play chunk-based audio files (WAV, AIFF), so that I can support common uncompressed audio formats.

#### **Acceptance Criteria**

1. WHEN the ChunkDemuxer parses RIFF/WAV files THEN the ChunkDemuxer **SHALL** handle little-endian chunk structure
2. WHEN the ChunkDemuxer parses AIFF files THEN the ChunkDemuxer **SHALL** handle big-endian IFF chunk structure
3. WHEN the ChunkDemuxer reads format chunks THEN the ChunkDemuxer **SHALL** extract sample rate, channels, and bit depth
4. WHEN the ChunkDemuxer processes data chunks THEN the ChunkDemuxer **SHALL** locate and map audio data regions
5. WHEN the ChunkDemuxer handles compressed formats THEN the ChunkDemuxer **SHALL** support various WAVE format tags
6. WHEN the ChunkDemuxer parses AIFF compression THEN the ChunkDemuxer **SHALL** handle NONE, sowt, fl32, and other compression types
7. WHEN the ChunkDemuxer performs seeking THEN the ChunkDemuxer **SHALL** calculate byte offsets based on sample position and format
8. WHEN the ChunkDemuxer reads audio data THEN the ChunkDemuxer **SHALL** respect block alignment and frame boundaries
9. WHEN the ChunkDemuxer handles IEEE 80-bit floats THEN the ChunkDemuxer **SHALL** convert AIFF sample rates correctly
10. WHEN the ChunkDemuxer detects container type THEN the ChunkDemuxer **SHALL** automatically handle FORM and RIFF containers

### **Requirement 5: ISODemuxer Implementation**

**User Story:** As a media player, I want to play ISO Base Media files (MP4, M4A), so that I can support modern container formats.

#### **Acceptance Criteria**

1. WHEN the ISODemuxer parses MP4 containers THEN the ISODemuxer **SHALL** handle hierarchical box structure
2. WHEN the ISODemuxer reads box headers THEN the ISODemuxer **SHALL** support both 32-bit and 64-bit size fields
3. WHEN the ISODemuxer processes movie headers THEN the ISODemuxer **SHALL** extract track information and timescales
4. WHEN the ISODemuxer parses sample tables THEN the ISODemuxer **SHALL** build chunk offset, size, and timing tables
5. WHEN the ISODemuxer handles files with multiple tracks THEN the ISODemuxer **SHALL** identify and prioritize audio tracks
6. WHEN the ISODemuxer performs seeking THEN the ISODemuxer **SHALL** use sample tables for precise positioning
7. WHEN the ISODemuxer reads samples THEN the ISODemuxer **SHALL** calculate chunk positions and extract sample data
8. WHEN the ISODemuxer processes codec information THEN the ISODemuxer **SHALL** map format FourCCs to codec names
9. WHEN the ISODemuxer calculates duration THEN the ISODemuxer **SHALL** use track duration and timescale information
10. WHEN the ISODemuxer handles fragmented MP4 THEN the ISODemuxer **SHALL** support basic fragmentation for future extension

### **Requirement 6: RawAudioDemuxer Implementation**

**User Story:** As a media player, I want to play raw audio files (PCM, A-law, μ-law), so that I can support telephony and embedded audio formats.

#### **Acceptance Criteria**

1. WHEN the RawAudioDemuxer detects raw formats THEN the RawAudioDemuxer **SHALL** use file extension-based identification
2. WHEN the RawAudioDemuxer configures formats THEN the RawAudioDemuxer **SHALL** support explicit RawAudioConfig parameters
3. WHEN the RawAudioDemuxer handles telephony formats THEN the RawAudioDemuxer **SHALL** default to 8kHz mono for A-law and μ-law
4. WHEN the RawAudioDemuxer processes PCM formats THEN the RawAudioDemuxer **SHALL** support various bit depths and endianness
5. WHEN the RawAudioDemuxer calculates duration THEN the RawAudioDemuxer **SHALL** use file size and format parameters
6. WHEN the RawAudioDemuxer performs seeking THEN the RawAudioDemuxer **SHALL** calculate byte offsets based on sample position
7. WHEN the RawAudioDemuxer reads audio data THEN the RawAudioDemuxer **SHALL** respect frame boundaries and channel interleaving
8. WHEN the RawAudioDemuxer handles files with unknown extensions THEN the RawAudioDemuxer **SHALL** provide reasonable defaults
9. WHEN the RawAudioDemuxer validates configurations THEN the RawAudioDemuxer **SHALL** ensure consistent format parameters
10. WHEN the RawAudioDemuxer streams data THEN the RawAudioDemuxer **SHALL** provide fixed-size chunks for consistent playback

### **Requirement 7: DemuxerFactory System**

**User Story:** As a media processing system, I want automatic format detection and demuxer creation, so that I can handle various file types transparently.

#### **Acceptance Criteria**

1. WHEN the DemuxerFactory creates demuxers THEN the DemuxerFactory **SHALL** accept an IOHandler and optional file path
2. WHEN the DemuxerFactory probes formats THEN the DemuxerFactory **SHALL** examine magic bytes for format identification
3. WHEN the DemuxerFactory detects RIFF format THEN the DemuxerFactory **SHALL** recognize "RIFF" signature and create ChunkDemuxer
4. WHEN the DemuxerFactory detects AIFF format THEN the DemuxerFactory **SHALL** recognize "FORM" signature and create ChunkDemuxer
5. WHEN the DemuxerFactory detects Ogg format THEN the DemuxerFactory **SHALL** recognize "OggS" signature and create OggDemuxer
6. WHEN the DemuxerFactory detects MP4 format THEN the DemuxerFactory **SHALL** recognize "ftyp" box and create ISODemuxer
7. WHEN the DemuxerFactory detects raw audio THEN the DemuxerFactory **SHALL** use file extension hints for RawAudioDemuxer
8. IF format detection fails THEN the DemuxerFactory **SHALL** return nullptr rather than throwing exceptions
9. WHEN multiple formats match THEN the DemuxerFactory **SHALL** prioritize based on signature strength
10. WHEN the DemuxerFactory handles edge cases THEN the DemuxerFactory **SHALL** validate IOHandler before format probing

### **Requirement 8: MediaFactory Integration**

**User Story:** As a PsyMP3 component, I want a comprehensive media factory system, so that I can handle various formats and protocols through a unified interface.

#### **Acceptance Criteria**

1. WHEN the MediaFactory registers formats THEN the MediaFactory **SHALL** support dynamic format registration with metadata
2. WHEN the MediaFactory detects content THEN the MediaFactory **SHALL** use multiple detection methods including extension, MIME type, and magic bytes
3. WHEN the MediaFactory creates streams THEN the MediaFactory **SHALL** route to appropriate stream implementations
4. WHEN the MediaFactory handles HTTP URLs THEN the MediaFactory **SHALL** create HTTPIOHandler instances
5. WHEN the MediaFactory handles local files THEN the MediaFactory **SHALL** create FileIOHandler instances
6. WHEN the MediaFactory analyzes content THEN the MediaFactory **SHALL** return ContentInfo with confidence scores
7. WHEN the MediaFactory evaluates streaming support THEN the MediaFactory **SHALL** indicate format streaming capabilities
8. WHEN components query formats THEN the MediaFactory **SHALL** provide comprehensive format metadata
9. WHEN the MediaFactory handles MIME types THEN the MediaFactory **SHALL** map between MIME types and extensions
10. WHEN developers extend functionality THEN the MediaFactory **SHALL** support plugin-based format registration

### **Requirement 9: DemuxedStream Bridge**

**User Story:** As a legacy PsyMP3 component, I want to use the new demuxer architecture through the existing Stream interface, so that I can benefit from modern demuxing without code changes.

#### **Acceptance Criteria**

1. WHEN the DemuxedStream implements the Stream interface THEN the DemuxedStream **SHALL** provide all required Stream methods
2. WHEN the DemuxedStream initializes THEN the DemuxedStream **SHALL** create appropriate demuxer and codec instances
3. WHEN the DemuxedStream selects streams THEN the DemuxedStream **SHALL** automatically choose the best audio stream
4. WHEN the DemuxedStream reads data THEN the DemuxedStream **SHALL** decode compressed data to PCM samples
5. WHEN the DemuxedStream performs seeking THEN the DemuxedStream **SHALL** translate position requests to demuxer seek operations
6. WHEN the DemuxedStream buffers data THEN the DemuxedStream **SHALL** maintain efficient chunk and frame buffers
7. WHEN the DemuxedStream handles metadata THEN the DemuxedStream **SHALL** prefer container metadata over file-based tags
8. WHEN the DemuxedStream switches streams THEN the DemuxedStream **SHALL** support dynamic stream switching
9. WHEN the DemuxedStream reports properties THEN the DemuxedStream **SHALL** update Stream base class with current format information
10. IF errors occur THEN the DemuxedStream **SHALL** gracefully degrade and report issues

### **Requirement 10: Performance and Efficiency**

**User Story:** As a media player, I want efficient demuxing operations that don't impact playback quality, so that audio streaming remains smooth and responsive.

#### **Acceptance Criteria**

1. WHEN demuxers parse containers THEN demuxers **SHALL** use a streaming approach without loading entire files into memory
2. WHEN demuxers perform seeking THEN demuxers **SHALL** use efficient algorithms such as bisection search or sample tables
3. WHEN demuxers buffer data THEN demuxers **SHALL** implement bounded buffers to prevent memory exhaustion
4. WHEN demuxers process packets THEN demuxers **SHALL** minimize memory copying and allocation operations
5. WHEN demuxers handle large files THEN demuxers **SHALL** maintain acceptable performance characteristics
6. WHEN demuxers cache metadata THEN demuxers **SHALL** store frequently accessed information efficiently
7. WHEN demuxers read sequentially THEN demuxers **SHALL** optimize for forward reading patterns
8. WHEN demuxers handle files with multiple streams THEN demuxers **SHALL** share resources efficiently between streams
9. WHEN demuxers process headers THEN demuxers **SHALL** parse headers incrementally without excessive buffering
10. WHEN demuxers manage state THEN demuxers **SHALL** use minimal state tracking for position and timing

### **Requirement 11: Error Handling and Robustness**

**User Story:** As a media player, I want robust error handling in demuxing operations, so that corrupted files or network issues don't crash the application.

#### **Acceptance Criteria**

1. IF demuxers encounter corrupted containers THEN demuxers **SHALL** skip damaged sections and continue processing
2. IF parsing fails THEN demuxers **SHALL** return false from parseContainer() rather than throwing exceptions
3. IF I/O operations fail THEN demuxers **SHALL** handle read errors and EOF conditions gracefully
4. IF seeking requests exceed file boundaries THEN demuxers **SHALL** clamp to valid ranges and report success or failure
5. IF invalid parameters are provided THEN demuxers **SHALL** validate inputs and return appropriate errors
6. IF memory allocation fails THEN demuxers **SHALL** handle allocation failures without crashing
7. IF format detection fails THEN factories **SHALL** return null pointers rather than invalid objects
8. IF codec initialization fails THEN the DemuxedStream **SHALL** fall back gracefully or report errors
9. IF stream switching fails THEN components **SHALL** maintain the current stream and report failure
10. WHEN cleanup is required THEN destructors **SHALL** ensure all resources are properly released

### **Requirement 12: Thread Safety and Concurrency**

**User Story:** As a multi-threaded media player, I want thread-safe demuxing operations, so that seeking and reading can occur concurrently without corruption.

#### **Acceptance Criteria**

1. WHEN multiple threads access demuxers THEN demuxer implementations **SHALL** protect shared state appropriately
2. WHEN seeking occurs during playback THEN demuxers **SHALL** handle concurrent operations safely
3. WHEN I/O operations are in progress THEN demuxers **SHALL** prevent race conditions on IOHandler
4. WHEN multiple threads access packet queues THEN demuxers **SHALL** ensure thread-safe queue operations
5. WHEN stream state is modified THEN demuxers **SHALL** use atomic operations where appropriate
6. WHEN multiple threads call factory methods THEN the MediaFactory **SHALL** handle concurrent format registration
7. WHEN cleanup occurs THEN destructors **SHALL** ensure no operations are in progress
8. IF errors occur in one thread THEN demuxers **SHALL** propagate error state safely to other threads
9. WHEN demuxers cache data THEN demuxers **SHALL** implement thread-safe caching mechanisms
10. WHEN debugging is enabled THEN logging operations **SHALL** be thread-safe

### **Requirement 13: Extensibility and Plugin Support**

**User Story:** As a PsyMP3 developer, I want an extensible demuxer architecture, so that I can add support for new formats without modifying core code.

#### **Acceptance Criteria**

1. WHEN developers add new formats THEN developers **SHALL** be able to implement the Demuxer interface independently
2. WHEN developers register formats THEN the MediaFactory **SHALL** support runtime format registration
3. WHEN the system detects content THEN the system **SHALL** support custom content detectors
4. WHEN the system creates streams THEN the system **SHALL** support custom stream factory functions
5. WHEN the system handles protocols THEN the system **SHALL** support custom IOHandler implementations
6. WHEN developers extend metadata THEN the StreamInfo structure **SHALL** be extensible for format-specific information
7. WHEN developers add codecs THEN the system **SHALL** integrate with the AudioCodec architecture
8. WHEN developers implement plugins THEN the system **SHALL** provide a stable ABI for external modules
9. WHEN the system versions formats THEN the system **SHALL** support format version detection and handling
10. WHEN developers configure behavior THEN the system **SHALL** support runtime configuration options

### **Requirement 14: Integration and API Consistency**

**User Story:** As a PsyMP3 component, I want the demuxer architecture to integrate seamlessly with the rest of the codebase, so that it works consistently with other subsystems.

#### **Acceptance Criteria**

1. WHEN demuxers integrate with IOHandler THEN demuxers **SHALL** use the IOHandler interface exclusively for I/O operations
2. WHEN the system reports errors THEN the system **SHALL** use PsyMP3's exception hierarchy and error codes
3. WHEN the system logs debug information THEN the system **SHALL** use PsyMP3's Debug logging with appropriate categories
4. WHEN the system handles URIs THEN the system **SHALL** integrate with PsyMP3's URI parsing components
5. WHEN components work with TagLib THEN components **SHALL** accept TagLib::String parameters for compatibility
6. WHEN the system manages memory THEN the system **SHALL** follow PsyMP3's RAII and smart pointer conventions
7. WHEN the system handles configuration THEN the system **SHALL** respect PsyMP3's configuration system
8. WHEN the system provides metadata THEN the system **SHALL** integrate with existing metadata display systems
9. WHEN the system supports seeking THEN the system **SHALL** coordinate with player position tracking
10. WHEN the system handles playback THEN the system **SHALL** integrate with audio output and buffering systems