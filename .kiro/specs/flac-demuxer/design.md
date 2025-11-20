# **FLAC DEMUXER DESIGN - REVISED**

## **Overview**

This design document specifies the implementation of a native FLAC container demuxer for PsyMP3 with full RFC 9639 compliance, incorporating lessons learned from real-world debugging and performance optimization. The demuxer handles native FLAC files (.flac) by parsing the FLAC container format per RFC 9639 specifications and extracting FLAC bitstream data for decoding.

**Critical Design Insights:**
- **RFC 9639 compliance** requires strict validation of all forbidden patterns and reserved fields
- **Frame size estimation** must prioritize STREAMINFO minimum frame size over theoretical calculations
- **Highly compressed FLAC** streams can have frames as small as 14 bytes requiring accurate estimation
- **Binary search seeking** is fundamentally incompatible with variable-length compressed frames
- **Thread safety** requires public/private method patterns with `_unlocked` suffixes
- **Debug logging** must include method identification tokens for effective troubleshooting
- **Big-endian parsing** is mandatory for all fields except Vorbis comment lengths (little-endian)

The design separates container parsing from bitstream decoding while ensuring complete RFC 9639 compliance and optimizing for the performance characteristics of highly compressed FLAC streams.

## **Architecture**

### **Class Structure**

```cpp
class FLACDemuxer : public Demuxer {
private:
    // FLAC container state
    bool m_parsed = false;
    uint64_t m_file_size = 0;
    uint64_t m_audio_data_offset = 0;  // Start of FLAC frames
    uint64_t m_current_offset = 0;     // Current read position
    bool m_eof = false;
    
    // FLAC metadata
    FLAC__StreamMetadata_StreamInfo m_streaminfo;
    std::vector<FLAC__StreamMetadata_SeekPoint> m_seektable;
    std::map<std::string, std::string> m_vorbis_comments;
    std::vector<uint8_t> m_picture_data;  // Embedded artwork
    
    // Frame parsing state
    uint64_t m_current_sample = 0;
    uint32_t m_last_block_size = 0;
    
public:
    explicit FLACDemuxer(std::unique_ptr<IOHandler> handler);
    
    // Demuxer interface implementation
    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    MediaChunk readChunk() override;
    MediaChunk readChunk(uint32_t stream_id) override;
    bool seekTo(uint64_t timestamp_ms) override;
    // ... other interface methods
};
```

### **Supporting Data Structures**

```cpp
struct FLACMetadataBlock {
    uint8_t type;
    bool is_last;
    uint32_t length;
    uint64_t data_offset;
};

struct FLACFrame {
    uint64_t sample_offset;      // Sample position of this frame
    uint64_t file_offset;        // File position of frame start
    uint32_t block_size;         // Samples in this frame
    uint32_t frame_size;         // Bytes in this frame (if known)
};

// FLAC metadata block types
enum FLACMetadataType {
    FLAC_METADATA_TYPE_STREAMINFO = 0,
    FLAC_METADATA_TYPE_PADDING = 1,
    FLAC_METADATA_TYPE_APPLICATION = 2,
    FLAC_METADATA_TYPE_SEEKTABLE = 3,
    FLAC_METADATA_TYPE_VORBIS_COMMENT = 4,
    FLAC_METADATA_TYPE_CUESHEET = 5,
    FLAC_METADATA_TYPE_PICTURE = 6
};
```

## **Components and Interfaces**

### **1. Container Parsing Component (RFC 9639 Section 6, 8)**

**Purpose**: Parse FLAC container structure and metadata blocks with full RFC 9639 compliance

**Key Methods**:
- `bool parseContainer()`: Main parsing entry point with stream marker validation
- `bool validateStreamMarker()`: Verify 0x664C6143 (fLaC) marker per RFC 9639 Section 6
- `bool parseMetadataBlocks()`: Process all metadata blocks with forbidden pattern detection
- `bool parseMetadataBlockHeader(FLACMetadataBlock& block)`: Parse 4-byte header per RFC 9639 Section 8.1
- `bool parseStreamInfo(const FLACMetadataBlock& block)`: Parse STREAMINFO per RFC 9639 Section 8.2
- `bool parseSeekTable(const FLACMetadataBlock& block)`: Parse SEEKTABLE per RFC 9639 Section 8.5
- `bool parseVorbisComment(const FLACMetadataBlock& block)`: Parse comments per RFC 9639 Section 8.6
- `bool parsePicture(const FLACMetadataBlock& block)`: Parse PICTURE per RFC 9639 Section 8.8

**RFC 9639 Compliance Requirements**:
- **Stream Marker**: Must be exactly 0x66 0x4C 0x61 0x43 (fLaC in ASCII)
- **Metadata Block Header**: 
  - Bit 7: is_last flag (0=more blocks, 1=last block)
  - Bits 0-6: block type (0-126 valid, 127 forbidden per RFC 9639 Table 1)
  - Bytes 1-3: 24-bit big-endian block length
- **STREAMINFO Requirements**:
  - Must be first metadata block (RFC 9639 Section 8.2)
  - Exactly 34 bytes of data
  - Minimum/maximum block size must be 16-65535 (values <16 forbidden per RFC 9639 Table 1)
  - Sample rate must not be 0 for audio streams
- **Forbidden Patterns** (RFC 9639 Section 5, Table 1):
  - Metadata block type 127
  - STREAMINFO min/max block size < 16
  - Sample rate bits 0b1111
  - Uncommon block size 65536
- **Big-Endian Encoding**: All integers except Vorbis comment lengths (RFC 9639 Section 5)

**Design Decisions**:
- Parse all metadata blocks during initialization
- Store essential metadata (STREAMINFO, SEEKTABLE) for runtime use
- Skip unknown (type 7-126) or application-specific blocks gracefully
- Validate block structure and sizes before processing
- Reject streams with forbidden patterns immediately
- Support multiple PADDING, APPLICATION, and PICTURE blocks
- Enforce single STREAMINFO, SEEKTABLE, VORBIS_COMMENT, and CUESHEET blocks

**FLAC Container Structure (RFC 9639 Section 6)**:
```
[fLaC] (4 bytes) - Stream marker (0x664C6143)
[METADATA_BLOCK_STREAMINFO] (mandatory, always first, type=0)
[METADATA_BLOCK_*] (optional blocks, types 1-6, 7-126 reserved)
...
[METADATA_BLOCK_*] (last block has is_last=1)
[FLAC_FRAME] (first audio frame with sync code 0xFFF8/0xFFF9)
[FLAC_FRAME] ...
[FLAC_FRAME] (last audio frame)
```

### **2. Metadata Processing Component (RFC 9639 Section 8)**

**Purpose**: Extract and store FLAC metadata for playback and seeking with RFC 9639 compliance

**STREAMINFO Processing (RFC 9639 Section 8.2)**:
- **u(16)** Minimum block size (16-65535, for buffer allocation)
- **u(16)** Maximum block size (16-65535, must be >= minimum)
- **u(24)** Minimum frame size (0=unknown, for seeking optimization)
- **u(24)** Maximum frame size (0=unknown, for seeking optimization)
- **u(20)** Sample rate in Hz (1-1048575, must not be 0 for audio)
- **u(3)** Number of channels minus 1 (0-7, representing 1-8 channels)
- **u(5)** Bits per sample minus 1 (3-31, representing 4-32 bits)
- **u(36)** Total samples (0=unknown, for duration calculation)
- **u(128)** MD5 checksum (0=unavailable, for integrity verification)
- **Validation**: Reject if min/max block size < 16 (forbidden pattern)
- **Validation**: Reject if min block size > max block size
- **Validation**: Reject if sample rate is 0 for audio streams

**SEEKTABLE Processing (RFC 9639 Section 8.5)**:
- **Seek Point Count**: block_length / 18 bytes per seek point
- **Per Seek Point**:
  - **u(64)** Sample number (0xFFFFFFFFFFFFFFFF = placeholder)
  - **u(64)** Byte offset from first frame header
  - **u(16)** Number of samples in target frame
- **Validation**: Verify seek points are sorted in ascending order by sample number
- **Validation**: Verify seek points are unique (except placeholders)
- **Validation**: Placeholders must all occur at end of table
- **Usage**: Find closest seek point not exceeding target sample for seeking

**VORBIS_COMMENT Processing (RFC 9639 Section 8.6)**:
- **u(32) little-endian** Vendor string length
- **UTF-8 string** Vendor string (not null-terminated)
- **u(32) little-endian** Number of comment fields
- **Per Field**:
  - **u(32) little-endian** Field length
  - **UTF-8 string** Field content (NAME=value format, not null-terminated)
- **Field Name Rules**:
  - Must contain only printable ASCII 0x20-0x7E except 0x3D (equals sign)
  - Case-insensitive comparison (A-Z equivalent to a-z)
- **Standard Fields**: TITLE, ARTIST, ALBUM (not formally defined but widely recognized)
- **Special Field**: WAVEFORMATEXTENSIBLE_CHANNEL_MASK for non-default channel ordering
- **Note**: Little-endian encoding is exception to FLAC's big-endian rule (for Vorbis compatibility)

**PICTURE Processing (RFC 9639 Section 8.8)**:
- **u(32)** Picture type (0-20 defined, see RFC 9639 Table 13)
- **u(32)** Media type string length
- **ASCII string** Media type (MIME type or "-->" for URI)
- **u(32)** Description string length
- **UTF-8 string** Description
- **u(32)** Width in pixels (0 if unknown/not applicable)
- **u(32)** Height in pixels (0 if unknown/not applicable)
- **u(32)** Color depth in bits per pixel (0 if unknown)
- **u(32)** Number of colors for indexed images (0 for non-indexed)
- **u(32)** Picture data length
- **Binary data** Picture data or URI
- **Validation**: Only one each of picture types 1 (32x32 icon) and 2 (general icon) allowed
- **Usage**: Type 3 (front cover) typically displayed during playback

### **3. Frame Identification Component (RFC 9639 Section 9.1)**

**Purpose**: Locate and parse FLAC frame headers for streaming with full RFC 9639 compliance

**Key Methods**:
- `bool findNextFrame(FLACFrame& frame)`: Locate next frame with limited search scope
- `bool parseFrameHeader(FLACFrame& frame)`: Parse frame header per RFC 9639 Section 9.1
- `bool parseBlockSizeBits(uint8_t bits, uint32_t& block_size)`: Parse per RFC 9639 Table 14
- `bool parseSampleRateBits(uint8_t bits, uint32_t& sample_rate)`: Parse sample rate encoding
- `bool parseChannelBits(uint8_t bits, uint8_t& channels, ChannelMode& mode)`: Parse channel assignment
- `bool parseBitDepthBits(uint8_t bits, uint8_t& bit_depth)`: Parse bit depth encoding
- `bool parseCodedNumber(uint64_t& number)`: Parse UTF-8-like variable-length encoding
- `bool validateFrameHeaderCRC(const uint8_t* header, size_t length, uint8_t crc)`: Validate CRC-8
- `bool validateFrameHeader(const FLACFrame& frame)`: Validate header consistency
- `uint32_t calculateFrameSize(const FLACFrame& frame)`: Accurate size estimation using STREAMINFO

**Frame Sync Detection (RFC 9639 Section 9.1)**:
- **Sync Code**: 15-bit pattern 0b111111111111100 (0xFFF8 for fixed, 0xFFF9 for variable)
- **Byte Alignment**: Frame must start on byte boundary
- **Blocking Strategy**: Bit following sync code (0=fixed block size, 1=variable block size)
- **Validation**: Blocking strategy must not change mid-stream
- **False Positive Prevention**: Validate complete frame header structure and CRC-8

**Frame Header Structure (RFC 9639 Section 9.1)**:
```
Byte 0-1: Sync code (15 bits): 0b111111111111100
Byte 1:   Reserved (1 bit): 0
Byte 1:   Blocking strategy (1 bit): 0=fixed, 1=variable
Byte 2:   Block size bits (4 bits): encoded per RFC 9639 Table 14
Byte 2:   Sample rate bits (4 bits): encoded sample rate
Byte 3:   Channel bits (4 bits): channel configuration
Byte 3:   Bit depth bits (3 bits): bits per sample
Byte 3:   Reserved (1 bit): 0
Byte 4+:  Coded number (1-7 bytes): UTF-8-like encoding of frame/sample number
Byte N:   Uncommon block size (0, 1, or 2 bytes): if block size bits = 0b0110 or 0b0111
Byte N+:  Uncommon sample rate (0, 1, or 2 bytes): if sample rate bits = 0b1100, 0b1101, or 0b1110
Byte N+:  CRC-8 (1 byte): header checksum with polynomial 0x07
```

**Block Size Encoding (RFC 9639 Section 9.1.1, Table 14)**:
- 0b0000: Reserved (reject)
- 0b0001: 192 samples
- 0b0010: 576 samples
- 0b0011: 1152 samples
- 0b0100: 2304 samples
- 0b0101: 4608 samples
- 0b0110: 8-bit uncommon block size minus 1 follows
- 0b0111: 16-bit uncommon block size minus 1 follows
- 0b1000-0b1111: 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 samples
- **Forbidden**: Uncommon block size value 65536 (RFC 9639 Table 1)

**Sample Rate Encoding (RFC 9639 Section 9.1.2)**:
- 0b0000: Get from STREAMINFO (non-streamable)
- 0b0001-0b1011: Standard rates (88.2k, 176.4k, 192k, 8k, 16k, 22.05k, 24k, 32k, 44.1k, 48k, 96k)
- 0b1100: 8-bit uncommon rate in kHz follows
- 0b1101: 16-bit uncommon rate in Hz follows
- 0b1110: 16-bit uncommon rate in tens of Hz follows
- 0b1111: Forbidden (RFC 9639 Table 1)

**Channel Assignment (RFC 9639 Section 9.1.3)**:
- 0b0000-0b0111: 1-8 independent channels
- 0b1000: Left-side stereo (left + side)
- 0b1001: Right-side stereo (side + right)
- 0b1010: Mid-side stereo (mid + side)
- 0b1011-0b1111: Reserved (reject)

**Bit Depth Encoding (RFC 9639 Section 9.1.4)**:
- 0b000: Get from STREAMINFO (non-streamable)
- 0b001: 8 bits
- 0b010: 12 bits
- 0b011: Reserved (reject)
- 0b100: 16 bits
- 0b101: 20 bits
- 0b110: 24 bits
- 0b111: 32 bits

**Coded Number (RFC 9639 Section 9.1.5)**:
- UTF-8-like variable-length encoding (1-7 bytes)
- Fixed block size: Represents frame number
- Variable block size: Represents sample number of first sample in frame
- Used for sample-accurate seeking and position tracking

**Critical Frame Size Estimation Algorithm**:

Based on real-world debugging, frame size estimation is critical for performance:

```cpp
uint32_t calculateFrameSize(const FLACFrame& frame) {
    // Method 1: Use STREAMINFO minimum frame size (preferred)
    if (m_streaminfo.isValid() && m_streaminfo.min_frame_size > 0) {
        uint32_t estimated_size = m_streaminfo.min_frame_size;
        
        // For fixed block size streams, use minimum directly
        if (m_streaminfo.min_block_size == m_streaminfo.max_block_size) {
            Debug::log("flac", "[calculateFrameSize] Fixed block size detected, using minimum frame size: ", estimated_size, " bytes");
            return estimated_size;  // Critical: return immediately for fixed block size
        }
        
        // Variable block size: minimal scaling only
        // ... conservative scaling logic
    }
    
    // Method 2: Conservative fallback (avoid complex calculations)
    return 64;  // Conservative minimum for unknown streams
}
```

**Key Lessons:**
- **STREAMINFO minimum frame size** is the most accurate estimate for highly compressed streams
- **Fixed block size streams** should use minimum frame size directly without scaling
- **Complex theoretical calculations** often produce inaccurate estimates (e.g., 6942 bytes vs actual 14 bytes)
- **Conservative fallbacks** prevent excessive I/O when estimation fails

### **4. Seeking Component with Architectural Limitations**

**Purpose**: Implement timestamp-based seeking with understanding of compressed audio limitations

**Key Methods**:
- `bool seekTo(uint64_t timestamp_ms)`: Main seeking interface with fallback handling
- `bool seekWithTable(uint64_t target_sample)`: Use SEEKTABLE for seeking (preferred)
- `bool seekBinary(uint64_t target_sample)`: Binary search (limited effectiveness)
- `bool seekLinear(uint64_t target_sample)`: Linear search (future frame indexing)

**Seeking Strategies with Real-World Constraints**:

1. **SEEKTABLE-based seeking** (fastest, most reliable):
   - Find closest seek point before target sample
   - Seek to that file position in compressed stream
   - Parse frames forward to exact position

2. **Binary search seeking** (architectural limitation):
   - **Problem**: Cannot predict frame positions in compressed data
   - **Reality**: Fails because frame boundaries are unpredictable
   - **Fallback**: Return to beginning position when search fails

3. **Future frame indexing** (recommended approach):
   - Build frame index during initial parsing
   - Cache discovered frame positions for seeking
   - Provide sample-accurate positioning with known positions

### **5. Data Streaming Component**

**Purpose**: Provide FLAC frame data to codec

**Key Methods**:
- `MediaChunk readChunk()`: Read next FLAC frame
- `MediaChunk readChunk(uint32_t stream_id)`: Read from specific stream
- `bool readFrameData(FLACFrame& frame, std::vector<uint8_t>& data)`: Read complete frame

**Streaming Logic**:
1. Locate next frame sync code
2. Parse frame header to determine frame size
3. Read complete frame including CRC
4. Update position tracking
5. Return frame as MediaChunk

**Frame Data Format**:
- MediaChunk contains complete FLAC frame (header + subframes + footer)
- timestamp_samples set to frame's sample position
- stream_id always 0 (FLAC is single-stream)
- is_keyframe always true (all FLAC frames are independent)

## **Data Models**

### **Stream State Machine**

```
[Uninitialized] → [Parsing Metadata] → [Metadata Complete] → [Streaming Frames]
                                    ↓
                               [Ready for Seeking] ← [After Seek]
```

### **Seeking Flow**

```
Timestamp → Target Sample → Seek Strategy Selection → File Position → Frame Parsing → Position Update
```

### **Frame Processing Flow**

```
File Position → Sync Search → Header Parse → Frame Size → Complete Frame Read → MediaChunk
```

## **CRC Validation (RFC 9639 Sections 9.1.8, 9.3)**

### **Frame Header CRC-8 (RFC 9639 Section 9.1.8)**

**Purpose**: Detect corrupted frame headers to prevent false sync detection

**Algorithm**:
- **Polynomial**: 0x07 (x^8 + x^2 + x + 1)
- **Initialization**: 0
- **Coverage**: All frame header bytes except the CRC-8 byte itself
- **Size**: 8 bits (1 byte)

**Implementation**:
```cpp
uint8_t calculateCRC8(const uint8_t* data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

**Error Handling**:
- Log CRC mismatch with frame position
- Attempt resynchronization to next frame
- In strict mode, reject stream on CRC failure

### **Frame Footer CRC-16 (RFC 9639 Section 9.3)**

**Purpose**: Detect corrupted frame data to ensure audio integrity

**Algorithm**:
- **Polynomial**: x^16 + x^15 + x^2 + x^0 (0x18005)
- **Initialization**: 0
- **Coverage**: Entire frame from sync code through subframes (excluding CRC-16 itself)
- **Size**: 16 bits (2 bytes)
- **Padding**: Zero bits added before CRC-16 to achieve byte alignment

**Implementation**:
```cpp
uint16_t calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x8005;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

**Error Handling**:
- Log CRC mismatch with frame position
- Attempt to continue with next frame
- In strict mode, reject stream on CRC failure
- Consider frame data potentially corrupted

## **Error Handling**

### **Container Level Errors**
- **Invalid fLaC marker**: Reject file as non-FLAC
- **Corrupted metadata blocks**: Skip damaged blocks, continue with available data
- **Missing STREAMINFO**: Attempt to derive parameters from first frame
- **Invalid block sizes**: Use reasonable defaults, log warnings

### **Frame Level Errors**
- **Lost frame sync**: Search for next valid frame sync code
- **Invalid frame headers**: Skip frame, continue with next
- **CRC mismatches**: Log error, use frame data anyway
- **Truncated frames**: Handle EOF gracefully

### **Seeking Errors**
- **Seek beyond file**: Clamp to last valid position
- **Corrupted seek table**: Fall back to binary search
- **Frame parsing failures**: Use approximate positioning
- **I/O errors**: Propagate IOHandler errors

## **Thread Safety Architecture**

### **Public/Private Lock Pattern**

All thread-safe classes must follow this mandatory pattern:

```cpp
class FLACDemuxer : public Demuxer {
public:
    // Public methods acquire locks and call private implementations
    bool parseContainer() {
        std::lock_guard<std::mutex> state_lock(m_state_mutex);
        std::lock_guard<std::mutex> metadata_lock(m_metadata_mutex);
        return parseContainer_unlocked();
    }
    
    MediaChunk readChunk() {
        std::lock_guard<std::mutex> state_lock(m_state_mutex);
        return readChunk_unlocked();
    }
    
private:
    // Private methods assume locks are already held
    bool parseContainer_unlocked();
    MediaChunk readChunk_unlocked();
    
    // Internal calls use _unlocked versions to prevent deadlocks
    void internalMethod() {
        // CORRECT: Call unlocked version
        auto result = readChunk_unlocked();
        
        // INCORRECT: Would cause deadlock
        // auto result = readChunk();
    }
    
    // Lock acquisition order (documented to prevent deadlocks):
    // 1. m_state_mutex (acquired first)
    // 2. m_metadata_mutex (acquired second)
    mutable std::mutex m_state_mutex;
    mutable std::mutex m_metadata_mutex;
};
```

### **Debug Method Identification**

All debug logging must include method identification tokens:

```cpp
Debug::log("flac", "[calculateFrameSize] Frame size estimate from STREAMINFO min: ", size, " bytes");
Debug::log("flac", "[readFrame] Using estimated frame size: ", size, " bytes");
Debug::log("flac", "[skipCorruptedFrame] Frame size estimate from STREAMINFO: ", size, " bytes");
```

## **Performance Considerations**

### **Frame Processing Optimization**
- **STREAMINFO-based estimation**: Use minimum frame size for accurate estimates
- **Limited boundary detection**: Restrict search scope to 512 bytes maximum
- **Efficient I/O patterns**: Reduce seeks from hundreds to tens per frame
- **Conservative fallbacks**: Use STREAMINFO data when boundary detection fails

### **Memory Usage**
- **Accurate frame sizing**: Prevent buffer waste with correct size estimates
- **Metadata caching**: Store only essential metadata in memory
- **Seek table optimization**: Use efficient data structures for seek points
- **Large file support**: Handle files >4GB with 64-bit offsets

### **I/O Efficiency**
- **Sequential optimization**: Optimize for forward playback with minimal seeks
- **Frame boundary optimization**: Use efficient search patterns for sync detection
- **Buffer management**: Use accurate frame size estimates for buffer allocation
- **Network stream support**: Consider read-ahead for streaming sources

## **Integration Points**

### **With FLACCodec**
- Provide complete FLAC frames in MediaChunk format
- Include accurate timestamp information
- Ensure frame data includes all necessary headers and footers
- Coordinate seeking operations (demuxer seeks, codec resets)

### **With IOHandler**
- Use IOHandler exclusively for all file I/O
- Support both FileIOHandler and HTTPIOHandler
- Handle large file operations properly
- Propagate I/O errors appropriately

### **With MediaFactory**
- Register for .flac file extension
- Register for audio/flac MIME type
- Provide format detection based on fLaC marker
- Integrate with content detection system

### **With DemuxedStream**
- Work through DemuxedStream bridge for legacy compatibility
- Provide StreamInfo with accurate FLAC parameters
- Support seeking through demuxer interface
- Handle metadata extraction for display

## **Testing Strategy**

### **RFC 9639 Compliance Testing**

**Forbidden Pattern Detection**:
- Test rejection of metadata block type 127
- Test rejection of STREAMINFO min/max block size < 16
- Test rejection of sample rate bits 0b1111
- Test rejection of uncommon block size 65536
- Test rejection of predictor coefficient precision bits 0b1111 (codec responsibility)
- Test rejection of negative predictor right shift (codec responsibility)

**Stream Marker Validation**:
- Test valid fLaC marker (0x664C6143)
- Test invalid markers (corrupted, wrong format)
- Test partial markers (truncated files)

**Metadata Block Header Parsing**:
- Test all valid block types (0-6)
- Test reserved block types (7-126) are skipped
- Test is_last flag handling
- Test 24-bit big-endian length parsing
- Test block length validation

**STREAMINFO Block Parsing**:
- Test all field extractions (min/max block size, frame size, sample rate, channels, bit depth, total samples, MD5)
- Test validation of min/max block size >= 16
- Test validation of min <= max block size
- Test sample rate validation (not 0 for audio)
- Test channel count (1-8)
- Test bit depth (4-32)
- Test 64-bit file support (total samples > 2^32)

**Frame Header Parsing**:
- Test sync code detection (0xFFF8 for fixed, 0xFFF9 for variable)
- Test blocking strategy consistency
- Test all block size encodings per RFC 9639 Table 14
- Test all sample rate encodings
- Test all channel assignments (independent, left-side, right-side, mid-side)
- Test all bit depth encodings
- Test coded number parsing (1-7 byte UTF-8-like encoding)
- Test uncommon block size parsing (8-bit and 16-bit)
- Test uncommon sample rate parsing (kHz, Hz, tens of Hz)

**CRC Validation**:
- Test frame header CRC-8 calculation and validation
- Test frame footer CRC-16 calculation and validation
- Test CRC mismatch handling
- Test resynchronization after CRC errors

**Seek Table Parsing**:
- Test seek point extraction (sample number, byte offset, frame samples)
- Test placeholder seek point handling (0xFFFFFFFFFFFFFFFF)
- Test seek point sorting validation
- Test seek point uniqueness validation

**Vorbis Comment Parsing**:
- Test little-endian length parsing (exception to big-endian rule)
- Test UTF-8 vendor string parsing
- Test UTF-8 field parsing
- Test field name validation (printable ASCII except equals)
- Test case-insensitive field name comparison
- Test NAME=value splitting

**Picture Block Parsing**:
- Test all picture type values (0-20)
- Test media type parsing
- Test description parsing
- Test dimension and color depth parsing
- Test picture data extraction
- Test URI handling (media type "-->")

### **Performance Testing**

**Frame Size Estimation**:
- Test STREAMINFO minimum frame size usage
- Test fixed block size stream optimization
- Test highly compressed stream handling (frames as small as 14 bytes)
- Test frame boundary detection with 512-byte search limit
- Test I/O operation count (should be tens, not hundreds per frame)

**Memory Usage**:
- Test accurate frame size estimation prevents buffer waste
- Test metadata caching efficiency
- Test large file support (>4GB)
- Test seek table memory usage

**I/O Efficiency**:
- Test sequential playback optimization
- Test frame boundary search patterns
- Test buffer allocation accuracy
- Test network stream support

### **Error Handling Testing**

**Container Level**:
- Test invalid fLaC marker rejection
- Test corrupted metadata block handling
- Test missing STREAMINFO recovery
- Test invalid block size handling

**Frame Level**:
- Test lost frame sync recovery
- Test invalid frame header handling
- Test CRC mismatch handling
- Test truncated frame handling

**Seeking**:
- Test seek beyond file clamping
- Test corrupted seek table fallback
- Test frame parsing failure handling
- Test I/O error propagation

### **Thread Safety Testing**

**Public/Private Lock Pattern**:
- Test concurrent parseContainer and readChunk calls
- Test concurrent seeking and reading
- Test lock acquisition order compliance
- Test RAII lock guard exception safety
- Test callback invocation without locks held
- Test atomic operation usage for counters

**Deadlock Prevention**:
- Test internal method calls use _unlocked versions
- Test documented lock acquisition order
- Test no public method calls other public methods

### **Integration Testing**

**With FLACCodec**:
- Test complete frame delivery in MediaChunk format
- Test accurate timestamp information
- Test frame data includes all headers and footers
- Test seeking coordination

**With IOHandler**:
- Test FileIOHandler integration
- Test HTTPIOHandler integration
- Test large file operations
- Test I/O error propagation

**With MediaFactory**:
- Test .flac extension registration
- Test audio/flac MIME type registration
- Test fLaC marker detection
- Test content detection integration

**With DemuxedStream**:
- Test legacy compatibility bridge
- Test StreamInfo population
- Test seeking through demuxer interface
- Test metadata extraction

### **Streamable Subset Testing (RFC 9639 Section 7)**

**Streamable Subset Compliance**:
- Test sample rate bits 0b0001-0b1110 (streamable)
- Test sample rate bits 0b0000 (non-streamable)
- Test bit depth bits 0b001-0b111 (streamable)
- Test bit depth bits 0b000 (non-streamable)
- Test maximum block size <= 16384 (streamable)
- Test maximum block size > 16384 (non-streamable)
- Test sample rate <= 48kHz with block size <= 4608 (streamable)
- Test WAVEFORMATEXTENSIBLE_CHANNEL_MASK presence (non-streamable)

### **Compatibility Testing**

**Existing Implementation**:
- Test all previously supported FLAC variants
- Test metadata field extraction compatibility
- Test seeking accuracy compatibility
- Test duration calculation compatibility
- Test error handling compatibility
- Test performance comparison

This design provides a clean separation between FLAC container parsing and FLAC bitstream decoding with full RFC 9639 compliance, enabling efficient and maintainable FLAC support that can work with multiple container formats.