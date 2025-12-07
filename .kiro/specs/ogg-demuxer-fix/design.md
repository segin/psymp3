# **OGG DEMUXER IMPLEMENTATION DESIGN**

## **Overview**

This design document specifies a complete rewrite of the OggDemuxer for PsyMP3. The previous implementation failed to produce working playback due to fundamental issues in how libogg was integrated and how the demuxer lifecycle was managed.

This design takes a different approach: **delegate as much as possible to libogg** rather than reimplementing Ogg parsing logic. The reference implementations (libvorbisfile, libopusfile) succeed because they use libogg correctly and minimally - they don't try to parse pages manually or maintain parallel state.

### **Key Design Principles**

1. **Trust libogg**: Use libogg's `ogg_sync_state` and `ogg_stream_state` as the single source of truth for container parsing. Do not maintain parallel page/packet structures.

2. **Simple State Machine**: The demuxer has exactly three states: INIT → HEADERS → STREAMING. No complex sub-states.

3. **Lazy Evaluation**: Don't parse everything upfront. Parse headers on open, calculate duration on demand, stream packets as requested.

4. **Reference Implementation Fidelity**: Follow libvorbisfile's `_fetch_headers()`, `_get_data()`, `_get_next_page()`, and `_get_prev_page()` patterns exactly.

5. **Separation of Concerns**: The OggDemuxer handles container parsing only. Codec-specific logic (Vorbis/Opus/FLAC header parsing) is delegated to codec-specific helper classes.


## **Architecture**

### **Component Diagram**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              OggDemuxer                                      │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐  │
│  │  OggSyncManager │  │ OggStreamManager│  │     CodecHeaderParser       │  │
│  │                 │  │                 │  │  ┌─────────┐ ┌───────────┐  │  │
│  │ ogg_sync_state  │  │ ogg_stream_state│  │  │ Vorbis  │ │   Opus    │  │  │
│  │ I/O buffering   │  │ per serial #    │  │  │ Parser  │ │  Parser   │  │  │
│  │ page extraction │  │ packet assembly │  │  ├─────────┤ ├───────────┤  │  │
│  └────────┬────────┘  └────────┬────────┘  │  │  FLAC   │ │  Speex    │  │  │
│           │                    │           │  │ Parser  │ │  Parser   │  │  │
│           └────────┬───────────┘           │  └─────────┘ └───────────┘  │  │
│                    │                       └─────────────────────────────┘  │
│           ┌────────▼────────┐                                               │
│           │  SeekingEngine  │                                               │
│           │                 │                                               │
│           │ bisection search│                                               │
│           │ backward scan   │                                               │
│           │ duration calc   │                                               │
│           └─────────────────┘                                               │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
                              ┌───────────────┐
                              │   IOHandler   │
                              │ (FileIOHandler│
                              │  HTTPIOHandler)│
                              └───────────────┘
```

### **Class Structure**

```cpp
namespace PsyMP3 {
namespace Demuxer {
namespace Ogg {

/**
 * @brief Manages libogg's ogg_sync_state for I/O and page extraction
 * 
 * This class encapsulates all interaction with ogg_sync_state.
 * It handles buffering data from IOHandler and extracting pages.
 */
class OggSyncManager {
public:
    explicit OggSyncManager(IOHandler* handler);
    ~OggSyncManager();
    
    // Core operations (following libvorbisfile patterns)
    int getData(size_t bytes_requested = CHUNKSIZE);  // _get_data()
    int getNextPage(ogg_page* page, int64_t boundary = -1);  // _get_next_page()
    int getPrevPage(ogg_page* page, int64_t begin, int64_t end);  // _get_prev_page()
    int getPrevPageSerial(ogg_page* page, uint32_t serial, int64_t begin, int64_t end);
    
    // State management
    void reset();  // ogg_sync_reset() wrapper
    int64_t tell() const;  // Current file position
    bool seek(int64_t offset, int whence);  // Seek in file
    bool isEOF() const;
    
    static constexpr size_t CHUNKSIZE = 65536;
    
private:
    IOHandler* m_handler;  // Not owned
    ogg_sync_state m_sync;
    int64_t m_offset;  // Current read position in file
    bool m_eof;
};


/**
 * @brief Manages logical bitstreams and their ogg_stream_state instances
 * 
 * Handles stream multiplexing (grouped and chained), maintains per-stream
 * state, and routes pages to appropriate stream states.
 */
class OggStreamManager {
public:
    OggStreamManager();
    ~OggStreamManager();
    
    // Stream lifecycle
    bool addStream(uint32_t serial);
    bool removeStream(uint32_t serial);
    void clearAllStreams();
    bool hasStream(uint32_t serial) const;
    
    // Page/packet operations
    int submitPage(ogg_page* page);  // ogg_stream_pagein()
    int getPacket(uint32_t serial, ogg_packet* packet);  // ogg_stream_packetout()
    int peekPacket(uint32_t serial, ogg_packet* packet);  // ogg_stream_packetpeek()
    
    // Stream state
    void resetStream(uint32_t serial);  // ogg_stream_reset()
    void resetAllStreams();
    
    // Iteration
    std::vector<uint32_t> getSerialNumbers() const;
    ogg_stream_state* getStreamState(uint32_t serial);
    
private:
    std::map<uint32_t, ogg_stream_state> m_streams;
};

/**
 * @brief Parsed stream information
 */
struct OggStreamInfo {
    uint32_t serial_number;
    std::string codec_name;  // "vorbis", "opus", "flac", "speex", "theora", "unknown"
    std::string codec_type;  // "audio", "video", "unknown"
    
    // Audio properties (populated by codec header parsers)
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint32_t bitrate = 0;
    uint8_t bits_per_sample = 0;
    uint64_t total_samples = 0;
    uint64_t pre_skip = 0;  // Opus only
    
    // Header state
    std::vector<std::vector<uint8_t>> header_packets;
    size_t expected_headers = 0;
    bool headers_complete = false;
    
    // Metadata
    std::string title, artist, album;
    
    // Runtime state
    uint64_t current_granule = 0;
    uint32_t last_page_sequence = 0;
    bool page_sequence_valid = false;
};


/**
 * @brief Codec-specific header parsing interface
 */
class CodecHeaderParser {
public:
    virtual ~CodecHeaderParser() = default;
    
    // Identify codec from first packet
    static std::string identifyCodec(const uint8_t* data, size_t size);
    
    // Parse header packet, return true if more headers expected
    virtual bool parseHeader(OggStreamInfo& info, const ogg_packet* packet) = 0;
    
    // Check if all required headers received
    virtual bool headersComplete(const OggStreamInfo& info) const = 0;
    
    // Factory method
    static std::unique_ptr<CodecHeaderParser> create(const std::string& codec_name);
};

class VorbisHeaderParser : public CodecHeaderParser {
public:
    bool parseHeader(OggStreamInfo& info, const ogg_packet* packet) override;
    bool headersComplete(const OggStreamInfo& info) const override;
    // Vorbis requires 3 headers: identification, comment, setup
};

class OpusHeaderParser : public CodecHeaderParser {
public:
    bool parseHeader(OggStreamInfo& info, const ogg_packet* packet) override;
    bool headersComplete(const OggStreamInfo& info) const override;
    // Opus requires 2 headers: OpusHead, OpusTags
};

class FLACHeaderParser : public CodecHeaderParser {
public:
    bool parseHeader(OggStreamInfo& info, const ogg_packet* packet) override;
    bool headersComplete(const OggStreamInfo& info) const override;
    // FLAC-in-Ogg: identification (with STREAMINFO), then metadata blocks
    
    // FLAC-specific: extract STREAMINFO from identification packet
    static bool parseStreamInfo(OggStreamInfo& info, const uint8_t* data, size_t size);
};

class SpeexHeaderParser : public CodecHeaderParser {
public:
    bool parseHeader(OggStreamInfo& info, const ogg_packet* packet) override;
    bool headersComplete(const OggStreamInfo& info) const override;
    // Speex requires 2 headers: identification, comment
};


/**
 * @brief Seeking and duration calculation engine
 * 
 * Implements bisection search following libvorbisfile/libopusfile patterns.
 */
class OggSeekingEngine {
public:
    OggSeekingEngine(OggSyncManager* sync, OggStreamManager* streams);
    
    // Duration calculation (following op_get_last_page patterns)
    uint64_t calculateDuration(uint32_t serial, uint64_t file_size);
    uint64_t getLastGranule(uint32_t serial, int64_t begin, int64_t end);
    
    // Seeking (following ov_pcm_seek_page patterns)
    bool seekToGranule(uint32_t serial, uint64_t target_granule, 
                       int64_t begin, int64_t end);
    bool seekToTime(uint32_t serial, uint64_t timestamp_ms,
                    const OggStreamInfo& info, int64_t begin, int64_t end);
    
    // Granule position arithmetic (following libopusfile)
    static int granposAdd(int64_t* dst, int64_t src, int64_t delta);
    static int granposDiff(int64_t* diff, int64_t a, int64_t b);
    static int granposCmp(int64_t a, int64_t b);
    
    // Time conversion
    static uint64_t granuleToMs(uint64_t granule, const OggStreamInfo& info);
    static uint64_t msToGranule(uint64_t ms, const OggStreamInfo& info);
    
private:
    OggSyncManager* m_sync;  // Not owned
    OggStreamManager* m_streams;  // Not owned
    
    // Bisection search helpers
    int64_t bisectForward(uint32_t serial, int64_t begin, int64_t end,
                          uint64_t target_granule, int64_t* best_offset);
    int64_t scanBackward(uint32_t serial, int64_t end, size_t chunk_size);
};


/**
 * @brief Main OggDemuxer class
 * 
 * Coordinates the sync manager, stream manager, header parsers, and seeking engine
 * to provide the Demuxer interface.
 */
class OggDemuxer : public Demuxer {
public:
    explicit OggDemuxer(std::unique_ptr<IOHandler> handler);
    ~OggDemuxer() override;
    
    // Demuxer interface
    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    MediaChunk readChunk() override;
    MediaChunk readChunk(uint32_t stream_id) override;
    bool seekTo(uint64_t timestamp_ms) override;
    bool isEOF() const override;
    uint64_t getDuration() const override;
    uint64_t getPosition() const override;
    uint64_t getGranulePosition(uint32_t stream_id) const override;
    
    // Static registration
    static void registerDemuxer();
    static bool canHandle(IOHandler* handler);
    
private:
    // State machine
    enum class State { INIT, HEADERS, STREAMING, ERROR };
    State m_state = State::INIT;
    
    // Components
    std::unique_ptr<OggSyncManager> m_sync;
    std::unique_ptr<OggStreamManager> m_stream_mgr;
    std::unique_ptr<OggSeekingEngine> m_seeking;
    std::map<uint32_t, std::unique_ptr<CodecHeaderParser>> m_header_parsers;
    
    // Stream information
    std::map<uint32_t, OggStreamInfo> m_stream_info;
    uint32_t m_primary_serial = 0;  // Primary audio stream
    
    // File information
    uint64_t m_file_size = 0;
    uint64_t m_data_start = 0;  // Offset after all headers
    uint64_t m_duration_ms = 0;
    
    // Runtime state
    uint64_t m_current_position_ms = 0;
    bool m_headers_sent = false;
    
    // Thread safety (public/private lock pattern)
    mutable std::mutex m_mutex;
    
    // Private implementation methods (assume lock held)
    bool parseContainer_unlocked();
    bool fetchHeaders_unlocked();  // Following _fetch_headers()
    bool processHeaderPacket_unlocked(uint32_t serial, ogg_packet* packet);
    MediaChunk readChunk_unlocked(uint32_t stream_id);
    bool seekTo_unlocked(uint64_t timestamp_ms);
    
    // Helper methods
    uint32_t selectPrimaryStream() const;
    bool allHeadersComplete() const;
};

} // namespace Ogg
} // namespace Demuxer
} // namespace PsyMP3
```


## **Components and Interfaces**

### **1. OggSyncManager - I/O and Page Extraction**

**Purpose**: Encapsulate all libogg `ogg_sync_state` operations and I/O buffering.

**Key Design Decisions**:

1. **Single Point of I/O**: All file reads go through this class. No other component touches IOHandler directly.

2. **Following `_get_data()` Pattern** (libvorbisfile):
   ```cpp
   int OggSyncManager::getData(size_t bytes_requested) {
       if (m_eof) return 0;
       
       char* buffer = ogg_sync_buffer(&m_sync, bytes_requested);
       if (!buffer) return -1;  // OV_EFAULT
       
       size_t bytes_read = m_handler->read(buffer, bytes_requested);
       if (bytes_read == 0) {
           m_eof = true;
           return 0;
       }
       
       int ret = ogg_sync_wrote(&m_sync, bytes_read);
       if (ret < 0) return -1;
       
       m_offset += bytes_read;
       return bytes_read;
   }
   ```

3. **Following `_get_next_page()` Pattern** (libvorbisfile):
   - Use `ogg_sync_pageseek()` NOT `ogg_sync_pageout()` for seeking
   - `ogg_sync_pageseek()` returns bytes skipped (negative) or page size (positive)
   - Handle partial pages by requesting more data
   - Respect boundary parameter for bisection search

4. **Following `_get_prev_page()` Pattern** (libvorbisfile):
   - Seek backwards in CHUNKSIZE increments
   - Read forward to find pages
   - Return the last complete page found before the end position

**Error Handling**:
- Return negative values for errors (following libogg conventions)
- `-1`: General error / OV_EFAULT
- `-2`: Read error / OV_EREAD  
- `-3`: EOF reached
- Positive: Success (bytes read or page size)

### **2. OggStreamManager - Logical Bitstream Management**

**Purpose**: Manage multiple `ogg_stream_state` instances for multiplexed streams.

**Key Design Decisions**:

1. **Lazy Stream Creation**: Streams are created when first BOS page is encountered.

2. **Serial Number Routing**: Pages are routed to streams by serial number using `ogg_page_serialno()`.

3. **Proper Initialization**:
   ```cpp
   bool OggStreamManager::addStream(uint32_t serial) {
       if (m_streams.count(serial)) return false;  // Duplicate serial
       
       ogg_stream_state& os = m_streams[serial];
       if (ogg_stream_init(&os, serial) != 0) {
           m_streams.erase(serial);
           return false;
       }
       return true;
   }
   ```

4. **Proper Cleanup**:
   ```cpp
   OggStreamManager::~OggStreamManager() {
       for (auto& [serial, os] : m_streams) {
           ogg_stream_clear(&os);
       }
   }
   ```

**Multiplexing Support**:
- **Grouped**: Multiple BOS pages at start, interleaved data pages
- **Chained**: Sequential streams (EOS followed by BOS)
- Detection: BOS page after data pages indicates chain boundary


### **3. CodecHeaderParser - Codec-Specific Header Processing**

**Purpose**: Parse codec-specific headers and extract stream properties.

**Codec Identification** (from first BOS packet):

| Codec   | Signature                | Bytes | Reference           |
|---------|--------------------------|-------|---------------------|
| Vorbis  | `\x01vorbis`             | 7     | Vorbis I Spec       |
| Opus    | `OpusHead`               | 8     | RFC 7845 §5.1       |
| FLAC    | `\x7fFLAC`               | 5     | RFC 9639 §10.1      |
| Speex   | `Speex   ` (with spaces) | 8     | Speex Spec          |
| Theora  | `\x80theora`             | 7     | Theora Spec         |

**Vorbis Header Processing** (3 headers required):

1. **Identification Header** (`\x01vorbis`):
   - Bytes 7-10: vorbis_version (must be 0)
   - Byte 11: audio_channels
   - Bytes 12-15: audio_sample_rate (little-endian)
   - Bytes 16-19: bitrate_maximum
   - Bytes 20-23: bitrate_nominal
   - Bytes 24-27: bitrate_minimum
   - Byte 28: blocksize info

2. **Comment Header** (`\x03vorbis`):
   - Vendor string length + string
   - Comment count + comments (TAG=value format)

3. **Setup Header** (`\x05vorbis`):
   - Codebook and mode configuration (preserve as-is for decoder)

**Opus Header Processing** (2 headers required, RFC 7845):

1. **OpusHead** (19+ bytes):
   - Bytes 0-7: "OpusHead" signature
   - Byte 8: version (must be 1)
   - Byte 9: channel_count
   - Bytes 10-11: pre_skip (little-endian)
   - Bytes 12-15: input_sample_rate (little-endian, informational only)
   - Bytes 16-17: output_gain
   - Byte 18: channel_mapping_family
   - If family != 0: stream_count, coupled_count, channel_mapping[]

2. **OpusTags**:
   - Bytes 0-7: "OpusTags" signature
   - Same format as Vorbis comments

**FLAC-in-Ogg Header Processing** (RFC 9639 §10.1):

1. **Identification Packet** (51 bytes, first page exactly 79 bytes):
   - Bytes 0-4: `\x7fFLAC` signature
   - Bytes 5-6: mapping version (major.minor, expect 1.0)
   - Bytes 7-8: header_packets count (big-endian, 0 = unknown)
   - Bytes 9-12: `fLaC` native signature
   - Bytes 13-16: metadata block header (type 0 = STREAMINFO)
   - Bytes 17-50: STREAMINFO (34 bytes)

2. **Subsequent Header Packets**:
   - First SHOULD be Vorbis comment (type 4)
   - Other metadata blocks until last-metadata-block flag

**STREAMINFO Parsing** (34 bytes, RFC 9639 §8.2):
```
Bits 0-15:   minimum_block_size
Bits 16-31:  maximum_block_size
Bits 32-55:  minimum_frame_size (24 bits)
Bits 56-79:  maximum_frame_size (24 bits)
Bits 80-99:  sample_rate (20 bits)
Bits 100-102: channels - 1 (3 bits)
Bits 103-107: bits_per_sample - 1 (5 bits)
Bits 108-143: total_samples (36 bits)
Bits 144-271: MD5 signature (128 bits)
```


### **4. OggSeekingEngine - Seeking and Duration**

**Purpose**: Implement bisection search and duration calculation following reference implementations.

**Duration Calculation** (following `op_get_last_page()`):

1. **Priority Order**:
   - FLAC: Use total_samples from STREAMINFO if non-zero
   - Opus: Use total_samples from OpusHead if available
   - All: Scan backwards for last granule position

2. **Backward Scanning Algorithm**:
   ```cpp
   uint64_t OggSeekingEngine::getLastGranule(uint32_t serial, int64_t begin, int64_t end) {
       // Start with small chunk, grow exponentially
       size_t chunk_size = CHUNKSIZE;
       int64_t offset = end;
       uint64_t best_granule = 0;
       
       while (offset > begin) {
           int64_t scan_start = std::max(begin, offset - chunk_size);
           m_sync->seek(scan_start, SEEK_SET);
           m_sync->reset();
           
           ogg_page page;
           while (m_sync->getNextPage(&page, offset) > 0) {
               if (ogg_page_serialno(&page) == serial) {
                   int64_t gp = ogg_page_granulepos(&page);
                   if (gp > 0 && gp != -1) {
                       best_granule = gp;
                   }
               }
           }
           
           if (best_granule > 0) return best_granule;
           
           offset = scan_start;
           chunk_size = std::min(chunk_size * 2, (size_t)(4 * CHUNKSIZE));
       }
       
       return best_granule;
   }
   ```

**Bisection Search** (following `ov_pcm_seek_page()`):

1. **Algorithm**:
   - Convert target time to granule position
   - Initialize search interval [begin, end]
   - While interval > threshold:
     - Seek to midpoint
     - Find next page with matching serial
     - Compare granule position to target
     - Adjust interval accordingly
   - When interval small, switch to linear scan

2. **Key Implementation Details**:
   - Use `ogg_sync_pageseek()` for page discovery (handles partial pages)
   - Reset `ogg_sync_state` after each seek
   - Handle granule position -1 (no packets complete on page)
   - Account for pre-skip in Opus streams

3. **Post-Seek State**:
   - Reset stream state with `ogg_stream_reset()`
   - Do NOT resend headers (decoder maintains state)
   - Resume packet extraction from new position

**Granule Position Arithmetic** (following libopusfile):

```cpp
// Safe addition with overflow detection
int OggSeekingEngine::granposAdd(int64_t* dst, int64_t src, int64_t delta) {
    if (src < 0) return -1;  // Invalid source
    if (delta < 0) {
        if (src < -delta) return -1;  // Underflow
    } else {
        if (src > INT64_MAX - delta) return -1;  // Overflow
    }
    *dst = src + delta;
    return 0;
}

// Safe comparison (handles wraparound)
int OggSeekingEngine::granposCmp(int64_t a, int64_t b) {
    if (a < 0 && b >= 0) return 1;   // Invalid a > valid b
    if (a >= 0 && b < 0) return -1;  // Valid a < invalid b
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}
```

**Time Conversion**:

```cpp
uint64_t OggSeekingEngine::granuleToMs(uint64_t granule, const OggStreamInfo& info) {
    if (info.sample_rate == 0) return 0;
    
    if (info.codec_name == "opus") {
        // Opus uses 48kHz granule rate, account for pre-skip
        uint64_t samples = (granule > info.pre_skip) ? (granule - info.pre_skip) : 0;
        return (samples * 1000) / 48000;
    } else {
        // Vorbis, FLAC: granule = sample count at stream sample rate
        return (granule * 1000) / info.sample_rate;
    }
}
```


### **5. OggDemuxer - Main Coordinator**

**Purpose**: Coordinate all components and implement the Demuxer interface.

**Initialization Flow** (`parseContainer()`):

```
1. Get file size from IOHandler
2. Create OggSyncManager and OggStreamManager
3. Call fetchHeaders_unlocked():
   a. Read pages until all BOS pages seen
   b. For each BOS page:
      - Create stream in OggStreamManager
      - Identify codec from first packet
      - Create appropriate CodecHeaderParser
   c. Continue reading pages until all headers complete
   d. Record data_start offset (first data page position)
4. Create OggSeekingEngine
5. Calculate duration using seeking engine
6. Select primary audio stream
7. Seek back to data_start for playback
8. Transition to STREAMING state
```

**Header Fetching** (following `_fetch_headers()`):

```cpp
bool OggDemuxer::fetchHeaders_unlocked() {
    ogg_page page;
    bool seen_data = false;
    int max_pages = 1000;  // Prevent infinite loop
    
    for (int i = 0; i < max_pages && !allHeadersComplete(); ++i) {
        int ret = m_sync->getNextPage(&page, -1);
        if (ret < 0) {
            if (ret == -3) break;  // EOF
            return false;  // Error
        }
        
        uint32_t serial = ogg_page_serialno(&page);
        
        if (ogg_page_bos(&page)) {
            // New stream - create and identify
            if (!m_stream_mgr->addStream(serial)) {
                continue;  // Duplicate serial, skip
            }
            m_stream_mgr->submitPage(&page);
            
            ogg_packet packet;
            if (m_stream_mgr->getPacket(serial, &packet) > 0) {
                std::string codec = CodecHeaderParser::identifyCodec(
                    packet.packet, packet.bytes);
                
                m_stream_info[serial].serial_number = serial;
                m_stream_info[serial].codec_name = codec;
                m_header_parsers[serial] = CodecHeaderParser::create(codec);
                
                if (m_header_parsers[serial]) {
                    processHeaderPacket_unlocked(serial, &packet);
                }
            }
        } else if (m_stream_mgr->hasStream(serial)) {
            // Existing stream - process header or note data start
            m_stream_mgr->submitPage(&page);
            
            ogg_packet packet;
            while (m_stream_mgr->getPacket(serial, &packet) > 0) {
                if (!m_stream_info[serial].headers_complete) {
                    processHeaderPacket_unlocked(serial, &packet);
                } else if (!seen_data) {
                    // First data packet - record position
                    m_data_start = m_sync->tell() - pageTotalSize(&page);
                    seen_data = true;
                }
            }
        }
    }
    
    return !m_stream_info.empty();
}
```


**Packet Reading** (`readChunk()`):

```cpp
MediaChunk OggDemuxer::readChunk_unlocked(uint32_t stream_id) {
    if (m_state != State::STREAMING) {
        return MediaChunk{};
    }
    
    auto info_it = m_stream_info.find(stream_id);
    if (info_it == m_stream_info.end()) {
        return MediaChunk{};
    }
    
    OggStreamInfo& info = info_it->second;
    
    // First call: send header packets
    if (!m_headers_sent) {
        if (!info.header_packets.empty()) {
            MediaChunk chunk;
            chunk.stream_id = stream_id;
            chunk.is_header = true;
            
            // Concatenate all header packets
            for (const auto& hdr : info.header_packets) {
                chunk.data.insert(chunk.data.end(), hdr.begin(), hdr.end());
            }
            
            m_headers_sent = true;
            return chunk;
        }
        m_headers_sent = true;
    }
    
    // Try to get a packet from the stream
    ogg_packet packet;
    int ret = m_stream_mgr->getPacket(stream_id, &packet);
    
    while (ret == 0) {
        // No packet available, need more pages
        ogg_page page;
        ret = m_sync->getNextPage(&page, -1);
        
        if (ret < 0) {
            if (ret == -3) {
                // EOF
                return MediaChunk{};
            }
            Debug::log("ogg", "Error reading page: ", ret);
            return MediaChunk{};
        }
        
        uint32_t page_serial = ogg_page_serialno(&page);
        
        // Check for chain boundary
        if (ogg_page_bos(&page)) {
            // New chain - for now, treat as EOF
            // TODO: Handle chained streams properly
            return MediaChunk{};
        }
        
        // Submit page to appropriate stream
        if (m_stream_mgr->hasStream(page_serial)) {
            m_stream_mgr->submitPage(&page);
            
            // Update granule position
            int64_t gp = ogg_page_granulepos(&page);
            if (gp >= 0 && page_serial == stream_id) {
                info.current_granule = gp;
                m_current_position_ms = OggSeekingEngine::granuleToMs(gp, info);
            }
            
            // Check page sequence for loss detection
            uint32_t seq = ogg_page_pageno(&page);
            if (info.page_sequence_valid && seq != info.last_page_sequence + 1) {
                Debug::log("ogg", "Page loss detected: expected ", 
                          info.last_page_sequence + 1, " got ", seq);
            }
            info.last_page_sequence = seq;
            info.page_sequence_valid = true;
        }
        
        // Try to get packet again
        ret = m_stream_mgr->getPacket(stream_id, &packet);
    }
    
    if (ret < 0) {
        // Hole in data
        Debug::log("ogg", "Hole in stream ", stream_id);
        return MediaChunk{};
    }
    
    // Build MediaChunk from packet
    MediaChunk chunk;
    chunk.stream_id = stream_id;
    chunk.data.assign(packet.packet, packet.packet + packet.bytes);
    chunk.timestamp_ms = m_current_position_ms;
    chunk.is_keyframe = true;  // Ogg packets are independently decodable
    chunk.is_header = false;
    
    if (packet.e_o_s) {
        chunk.is_eos = true;
    }
    
    return chunk;
}
```
