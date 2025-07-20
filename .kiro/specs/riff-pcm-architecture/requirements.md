# **RIFF/PCM ARCHITECTURE REQUIREMENTS**

## **Introduction**

This specification defines the requirements for implementing a cleaner RIFF/PCM architecture for PsyMP3, separating RIFF container parsing from PCM audio decoding. This follows the same container-agnostic pattern established for FLAC, enabling the same PCM codec to work with RIFF/WAV, AIFF, and potentially other containers.

The implementation must support:
- **Separate RIFFDemuxer** for RIFF container format (WAV, AVI future extension)
- **Separate AIFFDemuxer** for AIFF container format
- **Container-agnostic PCMCodec** for all PCM variants (8/16/24/32-bit, signed/unsigned, little/big endian)
- **Integration** with the modern demuxer/codec architecture
- **Backward compatibility** with existing WAV/AIFF file support

## **Requirements**

### **Requirement 1: RIFFDemuxer Implementation**

**User Story:** As a media player, I want to parse RIFF container files correctly, so that I can extract PCM and other audio data from WAV files and future RIFF-based formats.

#### **Acceptance Criteria**

1. **WHEN** a RIFF file is opened **THEN** the demuxer **SHALL** validate the "RIFF" signature and file size
2. **WHEN** parsing RIFF chunks **THEN** the demuxer **SHALL** read chunk headers with FourCC and size information
3. **WHEN** encountering WAVE format **THEN** the demuxer **SHALL** validate the "WAVE" format identifier
4. **WHEN** processing fmt chunk **THEN** the demuxer **SHALL** extract audio format parameters (sample rate, channels, bit depth, format tag)
5. **WHEN** processing data chunk **THEN** the demuxer **SHALL** locate and map audio data region
6. **WHEN** handling fact chunk **THEN** the demuxer **SHALL** extract sample count for compressed formats
7. **WHEN** encountering unknown chunks **THEN** the demuxer **SHALL** skip unknown chunks gracefully
8. **WHEN** seeking **THEN** the demuxer **SHALL** calculate byte offsets based on sample position and format parameters

### **Requirement 2: AIFFDemuxer Implementation**

**User Story:** As a media player, I want to parse AIFF container files correctly, so that I can extract PCM audio data from AIFF files with proper endianness handling.

#### **Acceptance Criteria**

1. **WHEN** an AIFF file is opened **THEN** the demuxer **SHALL** validate the "FORM" signature and handle big-endian byte order
2. **WHEN** parsing IFF chunks **THEN** the demuxer **SHALL** read big-endian chunk headers correctly
3. **WHEN** encountering AIFF format **THEN** the demuxer **SHALL** validate the "AIFF" format identifier
4. **WHEN** processing COMM chunk **THEN** the demuxer **SHALL** extract sample rate, channels, bit depth, and compression type
5. **WHEN** processing SSND chunk **THEN** the demuxer **SHALL** handle offset and block size fields correctly
6. **WHEN** handling IEEE 80-bit floats **THEN** the demuxer **SHALL** convert AIFF sample rates to standard format
7. **WHEN** processing compression types **THEN** the demuxer **SHALL** identify PCM variants (NONE, sowt, fl32, etc.)
8. **WHEN** seeking **THEN** the demuxer **SHALL** account for AIFF-specific data layout and endianness

### **Requirement 3: Container-Agnostic PCMCodec**

**User Story:** As a demuxer component, I want to decode PCM audio data regardless of container format, so that the same codec works with RIFF, AIFF, and other containers.

#### **Acceptance Criteria**

1. **WHEN** receiving PCM data from any demuxer **THEN** the codec **SHALL** decode based on StreamInfo parameters only
2. **WHEN** handling 8-bit PCM **THEN** the codec **SHALL** support both signed and unsigned variants
3. **WHEN** handling 16-bit PCM **THEN** the codec **SHALL** support both little-endian and big-endian byte order
4. **WHEN** handling 24-bit PCM **THEN** the codec **SHALL** support packed and padded variants
5. **WHEN** handling 32-bit PCM **THEN** the codec **SHALL** support both integer and floating-point variants
6. **WHEN** converting to 16-bit output **THEN** the codec **SHALL** use appropriate scaling and dithering
7. **WHEN** handling multi-channel audio **THEN** the codec **SHALL** support proper channel interleaving
8. **WHEN** processing compressed PCM **THEN** the codec **SHALL** support A-law and μ-law decompression

### **Requirement 4: Format Detection and Integration**

**User Story:** As a media factory, I want to automatically detect RIFF and AIFF formats, so that appropriate demuxers are created for different container types.

#### **Acceptance Criteria**

1. **WHEN** detecting RIFF files **THEN** the factory **SHALL** recognize "RIFF" signature and create RIFFDemuxer
2. **WHEN** detecting AIFF files **THEN** the factory **SHALL** recognize "FORM" signature and create AIFFDemuxer
3. **WHEN** analyzing format parameters **THEN** the factory **SHALL** determine appropriate PCM codec variant
4. **WHEN** registering formats **THEN** the system **SHALL** support .wav, .wave, .aif, .aiff, .aifc extensions
5. **WHEN** handling MIME types **THEN** the system **SHALL** support audio/wav, audio/aiff, audio/x-aiff types
6. **WHEN** creating streams **THEN** the factory **SHALL** route to DemuxedStream with appropriate demuxer/codec pair
7. **WHEN** maintaining compatibility **THEN** the system **SHALL** support all currently working WAV/AIFF files
8. **WHEN** handling edge cases **THEN** the system **SHALL** gracefully handle malformed or unusual files

### **Requirement 5: PCM Format Support**

**User Story:** As an audio codec, I want to support all common PCM variants, so that I can decode audio from various sources with different bit depths and encodings.

#### **Acceptance Criteria**

1. **WHEN** decoding 8-bit unsigned PCM **THEN** the codec **SHALL** convert to signed 16-bit with proper offset
2. **WHEN** decoding 8-bit signed PCM **THEN** the codec **SHALL** convert to 16-bit with proper scaling
3. **WHEN** decoding 16-bit PCM **THEN** the codec **SHALL** handle both little-endian and big-endian byte order
4. **WHEN** decoding 24-bit PCM **THEN** the codec **SHALL** downscale to 16-bit with optional dithering
5. **WHEN** decoding 32-bit integer PCM **THEN** the codec **SHALL** downscale to 16-bit with proper scaling
6. **WHEN** decoding 32-bit float PCM **THEN** the codec **SHALL** convert to 16-bit integer with clipping protection
7. **WHEN** decoding A-law **THEN** the codec **SHALL** expand to linear PCM using standard A-law tables
8. **WHEN** decoding μ-law **THEN** the codec **SHALL** expand to linear PCM using standard μ-law tables

### **Requirement 6: Seeking and Position Tracking**

**User Story:** As a media player, I want accurate seeking in PCM files, so that users can navigate through audio content efficiently.

#### **Acceptance Criteria**

1. **WHEN** seeking in uncompressed PCM **THEN** demuxers **SHALL** calculate exact byte positions from sample positions
2. **WHEN** seeking in compressed PCM **THEN** demuxers **SHALL** use fact chunk or estimate positions appropriately
3. **WHEN** handling variable bitrate **THEN** demuxers **SHALL** account for format-specific compression ratios
4. **WHEN** tracking position **THEN** demuxers **SHALL** maintain accurate sample and time-based positions
5. **WHEN** reporting duration **THEN** demuxers **SHALL** calculate from data size and format parameters
6. **WHEN** handling large files **THEN** demuxers **SHALL** support files larger than 2GB with 64-bit addressing
7. **WHEN** seeking near boundaries **THEN** demuxers **SHALL** handle start/end of file gracefully
8. **WHEN** frame alignment is required **THEN** demuxers **SHALL** ensure proper sample frame boundaries

### **Requirement 7: Metadata and Tag Support**

**User Story:** As a media player, I want to extract metadata from RIFF and AIFF files, so that I can display track information to users.

#### **Acceptance Criteria**

1. **WHEN** processing RIFF INFO chunks **THEN** RIFFDemuxer **SHALL** extract metadata fields (IART, INAM, IALB, etc.)
2. **WHEN** processing AIFF NAME chunks **THEN** AIFFDemuxer **SHALL** extract title and other metadata
3. **WHEN** handling ID3 tags in RIFF **THEN** RIFFDemuxer **SHALL** support ID3v2 tags in id3 chunks
4. **WHEN** processing AIFF annotations **THEN** AIFFDemuxer **SHALL** extract ANNO chunk text
5. **WHEN** encoding metadata **THEN** demuxers **SHALL** handle various text encodings appropriately
6. **WHEN** populating StreamInfo **THEN** demuxers **SHALL** include extracted metadata in artist/title/album fields
7. **WHEN** metadata is missing **THEN** demuxers **SHALL** provide empty strings rather than null values
8. **WHEN** handling large metadata **THEN** demuxers **SHALL** implement reasonable size limits

### **Requirement 8: Error Handling and Robustness**

**User Story:** As a media player, I want robust error handling in PCM processing, so that corrupted or unusual files don't crash the application.

#### **Acceptance Criteria**

1. **WHEN** encountering invalid signatures **THEN** demuxers **SHALL** reject non-matching files gracefully
2. **WHEN** chunk sizes are invalid **THEN** demuxers **SHALL** skip corrupted chunks and continue
3. **WHEN** format parameters are inconsistent **THEN** demuxers **SHALL** use reasonable defaults and log warnings
4. **WHEN** data chunks are truncated **THEN** demuxers **SHALL** handle incomplete files gracefully
5. **WHEN** unsupported formats are encountered **THEN** codecs **SHALL** report unsupported rather than crash
6. **WHEN** memory allocation fails **THEN** components **SHALL** return appropriate error codes
7. **WHEN** I/O operations fail **THEN** demuxers **SHALL** handle read errors and EOF conditions properly
8. **WHEN** codec parameters are invalid **THEN** PCMCodec **SHALL** validate inputs and reject invalid configurations

### **Requirement 9: Performance and Memory Management**

**User Story:** As a media player, I want efficient PCM processing with minimal memory usage, so that large files can be handled without excessive resource consumption.

#### **Acceptance Criteria**

1. **WHEN** processing large files **THEN** demuxers **SHALL** use streaming approach without loading entire files
2. **WHEN** converting PCM formats **THEN** codecs **SHALL** use efficient conversion algorithms
3. **WHEN** seeking operations occur **THEN** demuxers **SHALL** minimize I/O operations through direct calculation
4. **WHEN** buffering data **THEN** components **SHALL** implement bounded buffers to prevent memory exhaustion
5. **WHEN** handling multi-channel audio **THEN** codecs **SHALL** process channels efficiently
6. **WHEN** cleaning up resources **THEN** components **SHALL** properly free all allocated memory
7. **WHEN** processing very long files **THEN** components **SHALL** maintain acceptable performance characteristics
8. **WHEN** caching metadata **THEN** demuxers **SHALL** store frequently accessed information efficiently

### **Requirement 10: Integration with Existing Architecture**

**User Story:** As a PsyMP3 component, I want the RIFF/PCM architecture to integrate seamlessly with the existing demuxer/codec system, so that it works consistently with other formats.

#### **Acceptance Criteria**

1. **WHEN** implementing demuxer interfaces **THEN** RIFFDemuxer and AIFFDemuxer **SHALL** provide all required methods
2. **WHEN** implementing codec interface **THEN** PCMCodec **SHALL** work with the AudioCodec architecture
3. **WHEN** returning stream information **THEN** demuxers **SHALL** populate StreamInfo with accurate PCM parameters
4. **WHEN** providing media chunks **THEN** demuxers **SHALL** return properly formatted MediaChunk objects
5. **WHEN** handling IOHandler operations **THEN** demuxers **SHALL** use the provided IOHandler interface exclusively
6. **WHEN** logging debug information **THEN** components **SHALL** use the PsyMP3 Debug logging system
7. **WHEN** reporting errors **THEN** components **SHALL** use consistent error codes and messages
8. **WHEN** working with DemuxedStream **THEN** components **SHALL** integrate seamlessly with the bridge interface

### **Requirement 11: Backward Compatibility**

**User Story:** As a PsyMP3 user, I want the new RIFF/PCM architecture to maintain compatibility with existing audio files, so that my WAV and AIFF files continue to work without issues.

#### **Acceptance Criteria**

1. **WHEN** replacing existing WAV code **THEN** the new implementation **SHALL** support all previously supported WAV variants
2. **WHEN** replacing existing AIFF code **THEN** the new implementation **SHALL** support all previously supported AIFF variants
3. **WHEN** handling metadata **THEN** demuxers **SHALL** extract the same metadata fields as current implementations
4. **WHEN** seeking **THEN** demuxers **SHALL** provide at least the same seeking accuracy as current implementations
5. **WHEN** processing files **THEN** demuxers **SHALL** handle all files that currently work
6. **WHEN** reporting duration **THEN** demuxers **SHALL** provide consistent duration calculations
7. **WHEN** handling errors **THEN** demuxers **SHALL** provide equivalent error handling behavior
8. **WHEN** performance is measured **THEN** the new architecture **SHALL** provide comparable or better performance

### **Requirement 12: Thread Safety and Concurrency**

**User Story:** As a multi-threaded media player, I want thread-safe RIFF/PCM processing, so that seeking and reading can occur concurrently without corruption.

#### **Acceptance Criteria**

1. **WHEN** multiple threads access demuxers **THEN** implementations **SHALL** protect shared state appropriately
2. **WHEN** seeking occurs during playback **THEN** demuxers **SHALL** handle concurrent operations safely
3. **WHEN** I/O operations are in progress **THEN** demuxers **SHALL** prevent race conditions on IOHandler
4. **WHEN** codec instances are accessed **THEN** PCMCodec **SHALL** maintain thread-local state
5. **WHEN** shared resources are used **THEN** components **SHALL** use appropriate synchronization
6. **WHEN** cleanup occurs **THEN** destructors **SHALL** ensure no operations are in progress
7. **WHEN** errors occur in one thread **THEN** components **SHALL** propagate error state safely
8. **WHEN** debugging is enabled **THEN** logging operations **SHALL** be thread-safe