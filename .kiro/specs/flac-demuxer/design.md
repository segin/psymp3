# **FLAC DEMUXER DESIGN**

## **Overview**

This design document specifies the implementation of a native FLAC container demuxer for PsyMP3. The demuxer handles native FLAC files (.flac) by parsing the FLAC container format and extracting FLAC bitstream data for decoding. This demuxer works in conjunction with a separate FLACCodec to provide container-agnostic FLAC decoding.

The design separates container parsing from bitstream decoding, enabling the same FLACCodec to work with FLAC data from any container (native FLAC, Ogg FLAC, or future ISO FLAC).

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

### **1. Container Parsing Component**

**Purpose**: Parse FLAC container structure and metadata blocks

**Key Methods**:
- `bool parseContainer()`: Main parsing entry point
- `bool parseMetadataBlocks()`: Process all metadata blocks
- `bool parseStreamInfo(const FLACMetadataBlock& block)`: Parse STREAMINFO
- `bool parseSeekTable(const FLACMetadataBlock& block)`: Parse SEEKTABLE
- `bool parseVorbisComment(const FLACMetadataBlock& block)`: Parse comments

**Design Decisions**:
- Parse all metadata blocks during initialization
- Store essential metadata (STREAMINFO, SEEKTABLE) for runtime use
- Skip unknown or application-specific blocks gracefully
- Validate block structure and sizes before processing

**FLAC Container Structure**:
```
[fLaC] (4 bytes) - Stream marker
[METADATA_BLOCK_STREAMINFO] (mandatory, always first)
[METADATA_BLOCK_*] (optional blocks)
...
[METADATA_BLOCK_*] (last block has is_last=1)
[FLAC_FRAME] (first audio frame)
[FLAC_FRAME] ...
[FLAC_FRAME] (last audio frame)
```

### **2. Metadata Processing Component**

**Purpose**: Extract and store FLAC metadata for playback and seeking

**STREAMINFO Processing**:
- Minimum/maximum block size (for buffer allocation)
- Minimum/maximum frame size (for seeking optimization)
- Sample rate, channels, bits per sample
- Total samples (for duration calculation)
- MD5 signature (for integrity verification)

**SEEKTABLE Processing**:
- Extract seek points (sample number, byte offset, samples in target frame)
- Build internal seek table for efficient seeking
- Handle placeholder points (0xFFFFFFFFFFFFFFFF)

**VORBIS_COMMENT Processing**:
- Parse vendor string and user comments
- Extract standard fields (ARTIST, TITLE, ALBUM, etc.)
- Handle UTF-8 encoding properly
- Store in key-value map for easy access

### **3. Frame Identification Component**

**Purpose**: Locate and parse FLAC frame headers for streaming

**Key Methods**:
- `bool findNextFrame(FLACFrame& frame)`: Locate next frame
- `bool parseFrameHeader(FLACFrame& frame)`: Parse frame header
- `bool validateFrameHeader(const FLACFrame& frame)`: Validate header
- `uint32_t calculateFrameSize(const FLACFrame& frame)`: Estimate frame size

**Frame Sync Detection**:
- Look for sync code (0xFFF8 to 0xFFFF)
- Validate frame header structure
- Check for consistent parameters with STREAMINFO
- Handle variable block sizes and sample rates

**Frame Header Structure**:
```
Sync code (14 bits): 11111111111110xx
Reserved (1 bit): 0
Blocking strategy (1 bit): 0=fixed, 1=variable
Block size (4 bits): encoded block size
Sample rate (4 bits): encoded sample rate
Channel assignment (4 bits): channel configuration
Sample size (3 bits): bits per sample
Reserved (1 bit): 0
Frame/sample number (8-56 bits): UTF-8 coded
Block size (0-16 bits): if not encoded in header
Sample rate (0-16 bits): if not encoded in header
CRC-8 (8 bits): header checksum
```

### **4. Seeking Component**

**Purpose**: Implement efficient timestamp-based seeking

**Key Methods**:
- `bool seekTo(uint64_t timestamp_ms)`: Main seeking interface
- `bool seekWithTable(uint64_t target_sample)`: Use SEEKTABLE for seeking
- `bool seekBinary(uint64_t target_sample)`: Binary search through frames
- `bool seekLinear(uint64_t target_sample)`: Linear search (fallback)

**Seeking Strategies**:

1. **SEEKTABLE-based seeking** (fastest):
   - Find closest seek point before target
   - Seek to that file position
   - Parse frames forward to exact position

2. **Binary search seeking** (medium speed):
   - Use file size and average bitrate for initial estimate
   - Binary search through file looking for frame sync codes
   - Refine position based on frame sample numbers

3. **Linear seeking** (slowest, most accurate):
   - Parse frames sequentially from current position
   - Guaranteed sample-accurate positioning
   - Used for fine-tuning after coarse seeking

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

## **Performance Considerations**

### **Memory Usage**
- **Metadata caching**: Store only essential metadata in memory
- **Seek table optimization**: Use efficient data structures for seek points
- **Frame buffering**: Minimal buffering, stream frames on demand
- **Large file support**: Handle files >2GB with 64-bit offsets

### **I/O Efficiency**
- **Sequential optimization**: Optimize for forward playback
- **Seek optimization**: Minimize I/O operations during seeking
- **Buffer management**: Use appropriate buffer sizes for different operations
- **Read-ahead**: Consider read-ahead for network streams

### **CPU Efficiency**
- **Frame sync detection**: Use efficient bit manipulation for sync search
- **Header parsing**: Optimize common header parsing operations
- **Seek table lookup**: Use binary search for large seek tables
- **UTF-8 decoding**: Efficient UTF-8 handling for metadata

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

This design provides a clean separation between FLAC container parsing and FLAC bitstream decoding, enabling efficient and maintainable FLAC support that can work with multiple container formats.