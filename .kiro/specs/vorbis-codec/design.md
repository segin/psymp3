# **VORBIS CODEC DESIGN**

## **Overview**

This design document specifies the implementation of a container-agnostic Vorbis audio codec for PsyMP3. The codec decodes Vorbis bitstream data from any container (primarily Ogg Vorbis) into standard 16-bit PCM audio. This codec works with the AudioCodec architecture and receives Vorbis packet data from various demuxers.

The design focuses on efficient, accurate Vorbis decoding using libvorbis while maintaining compatibility with the existing Vorbis implementation and providing a clean interface for the modern demuxer/codec architecture.

## **Architecture**

### **Class Structure**

```cpp
class VorbisCodec : public AudioCodec {
private:
    // libvorbis decoder state
    vorbis_info m_vorbis_info;
    vorbis_comment m_vorbis_comment;
    vorbis_dsp_state m_vorbis_dsp;
    vorbis_block m_vorbis_block;
    
    // Stream configuration
    uint32_t m_sample_rate = 0;
    uint16_t m_channels = 0;
    uint16_t m_bits_per_sample = 16;  // Always output 16-bit
    
    // Header processing state
    int m_header_packets_received = 0;
    bool m_decoder_initialized = false;
    
    // Decoding buffers
    std::vector<int16_t> m_output_buffer;
    float** m_pcm_channels = nullptr;
    
    // Block size handling
    uint32_t m_block_size_short = 0;
    uint32_t m_block_size_long = 0;
    
    // Position tracking
    std::atomic<uint64_t> m_samples_decoded{0};
    std::atomic<uint64_t> m_granule_position{0};
    
    // Error handling
    std::atomic<bool> m_error_state{false};
    std::string m_last_error;
    
public:
    explicit VorbisCodec(const StreamInfo& stream_info);
    ~VorbisCodec() override;
    
    // AudioCodec interface implementation
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "vorbis"; }
    bool canDecode(const StreamInfo& stream_info) const override;
    
private:
    // Header processing
    bool processHeaderPacket(const std::vector<uint8_t>& packet_data);
    bool processIdentificationHeader(const std::vector<uint8_t>& packet_data);
    bool processCommentHeader(const std::vector<uint8_t>& packet_data);
    bool processSetupHeader(const std::vector<uint8_t>& packet_data);
    
    // Audio decoding
    AudioFrame decodeAudioPacket(const std::vector<uint8_t>& packet_data);
    bool synthesizeBlock();
    void convertFloatToPCM(float** pcm, int samples, AudioFrame& frame);
    
    // Block size and windowing
    void handleVariableBlockSizes(const vorbis_block* block);
    void applyWindowingAndOverlap(AudioFrame& frame);
    
    // Utility methods
    bool validateVorbisPacket(const std::vector<uint8_t>& packet_data);
    void handleVorbisError(int vorbis_error);
    void resetDecoderState();
    void cleanupVorbisStructures();
};
```

### **Supporting Structures**

```cpp
struct VorbisHeaderInfo {
    uint32_t version;
    uint8_t channels;
    uint32_t sample_rate;
    uint32_t bitrate_maximum;
    uint32_t bitrate_nominal;
    uint32_t bitrate_minimum;
    uint8_t blocksize_0;  // Short block size (power of 2)
    uint8_t blocksize_1;  // Long block size (power of 2)
    
    bool isValid() const;
    static VorbisHeaderInfo parseFromPacket(const std::vector<uint8_t>& packet_data);
};

struct VorbisCommentInfo {
    std::string vendor_string;
    std::vector<std::pair<std::string, std::string>> user_comments;
    
    static VorbisCommentInfo parseFromPacket(const std::vector<uint8_t>& packet_data);
};
```

## **Components and Interfaces**

### **1. Initialization Component**

**Purpose**: Configure codec from StreamInfo and initialize libvorbis decoder

**Key Methods**:
- `bool initialize()`: Main initialization entry point
- `bool configureFromStreamInfo()`: Extract parameters from StreamInfo
- `bool initializeVorbisStructures()`: Set up libvorbis structures
- `bool validateConfiguration()`: Verify codec parameters

**Initialization Process**:
1. Extract Vorbis parameters from StreamInfo
2. Initialize libvorbis info, comment, dsp_state, and block structures
3. Validate parameters against Vorbis specification
4. Set up internal buffers for decoding
5. Configure block size parameters

**libvorbis Structure Initialization**:
```cpp
bool initializeVorbisStructures() {
    // Initialize libvorbis structures
    vorbis_info_init(&m_vorbis_info);
    vorbis_comment_init(&m_vorbis_comment);
    
    // DSP state and block will be initialized after headers
    return true;
}
```

### **2. Header Processing Component**

**Purpose**: Process Vorbis identification, comment, and setup headers

**Key Methods**:
- `bool processHeaderPacket(const std::vector<uint8_t>& packet_data)`: Route header processing
- `bool processIdentificationHeader(const std::vector<uint8_t>& packet_data)`: Parse ID header
- `bool processCommentHeader(const std::vector<uint8_t>& packet_data)`: Parse comment header
- `bool processSetupHeader(const std::vector<uint8_t>& packet_data)`: Parse setup header

**Header Processing Flow**:
1. **Identification Header**: Extract sample rate, channels, bitrate bounds, block sizes
2. **Comment Header**: Extract metadata (handled by demuxer, validated by codec)
3. **Setup Header**: Initialize decoder with codebook and floor/residue configurations

**Header Processing with libvorbis**:
```cpp
bool processIdentificationHeader(const std::vector<uint8_t>& packet_data) {
    ogg_packet packet;
    packet.packet = const_cast<unsigned char*>(packet_data.data());
    packet.bytes = packet_data.size();
    packet.b_o_s = (m_header_packets_received == 0) ? 1 : 0;
    packet.e_o_s = 0;
    packet.granulepos = -1;
    packet.packetno = m_header_packets_received;
    
    int result = vorbis_synthesis_headerin(&m_vorbis_info, &m_vorbis_comment, &packet);
    if (result < 0) {
        handleVorbisError(result);
        return false;
    }
    
    // Extract configuration after first header
    if (m_header_packets_received == 0) {
        m_sample_rate = m_vorbis_info.rate;
        m_channels = m_vorbis_info.channels;
        m_block_size_short = 1 << m_vorbis_info.blocksize[0];
        m_block_size_long = 1 << m_vorbis_info.blocksize[1];
    }
    
    return true;
}
```

### **3. Audio Decoding Component**

**Purpose**: Decode Vorbis audio packets into PCM samples

**Key Methods**:
- `AudioFrame decodeAudioPacket(const std::vector<uint8_t>& packet_data)`: Main decoding
- `bool synthesizeBlock()`: Process Vorbis block with libvorbis
- `void convertFloatToPCM(float** pcm, int samples, AudioFrame& frame)`: Convert to 16-bit
- `bool validateVorbisPacket(const std::vector<uint8_t>& packet_data)`: Packet validation

**Decoding Process**:
1. Validate incoming Vorbis packet
2. Call vorbis_synthesis() to decode packet into block
3. Call vorbis_synthesis_blockin() to add block to DSP state
4. Extract PCM samples using vorbis_synthesis_pcmout()
5. Convert float samples to 16-bit PCM
6. Return AudioFrame with converted samples

**Core Decoding Logic**:
```cpp
AudioFrame decodeAudioPacket(const std::vector<uint8_t>& packet_data) {
    AudioFrame frame;
    
    // Create ogg_packet structure
    ogg_packet packet;
    packet.packet = const_cast<unsigned char*>(packet_data.data());
    packet.bytes = packet_data.size();
    packet.b_o_s = 0;
    packet.e_o_s = 0;
    packet.granulepos = -1;
    packet.packetno = m_header_packets_received + m_samples_decoded;
    
    // Decode packet into block
    if (vorbis_synthesis(&m_vorbis_block, &packet) == 0) {
        vorbis_synthesis_blockin(&m_vorbis_dsp, &m_vorbis_block);
    }
    
    // Extract PCM samples
    int samples_available;
    while ((samples_available = vorbis_synthesis_pcmout(&m_vorbis_dsp, &m_pcm_channels)) > 0) {
        // Convert float samples to 16-bit PCM
        convertFloatToPCM(m_pcm_channels, samples_available, frame);
        
        // Tell libvorbis how many samples we consumed
        vorbis_synthesis_read(&m_vorbis_dsp, samples_available);
        
        break; // Process one block at a time
    }
    
    return frame;
}
```

### **4. Float to PCM Conversion Component**

**Purpose**: Convert libvorbis float output to 16-bit PCM

**Key Methods**:
- `void convertFloatToPCM(float** pcm, int samples, AudioFrame& frame)`: Main conversion
- `int16_t floatToInt16(float sample)`: Per-sample conversion with clamping
- `void interleaveChannels(float** pcm, int samples, std::vector<int16_t>& output)`: Channel interleaving

**Conversion Implementation**:
```cpp
void convertFloatToPCM(float** pcm, int samples, AudioFrame& frame) {
    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.samples.clear();
    frame.samples.reserve(samples * m_channels);
    
    // Interleave channels and convert to 16-bit
    for (int i = 0; i < samples; i++) {
        for (int ch = 0; ch < m_channels; ch++) {
            float sample = pcm[ch][i];
            
            // Clamp and convert to 16-bit
            int32_t val = static_cast<int32_t>(sample * 32767.0f);
            val = std::clamp(val, -32768L, 32767L);
            frame.samples.push_back(static_cast<int16_t>(val));
        }
    }
}
```

### **5. Block Size and Windowing Component**

**Purpose**: Handle Vorbis variable block sizes and windowing

**Key Methods**:
- `void handleVariableBlockSizes(const vorbis_block* block)`: Process block size changes
- `void applyWindowingAndOverlap(AudioFrame& frame)`: Apply overlap-add processing
- `uint32_t getCurrentBlockSize()`: Get current block size
- `void manageOverlapBuffer()`: Handle overlap-add state

**Block Size Handling**:
- **Short Blocks**: 64-2048 samples (typically 256)
- **Long Blocks**: 128-8192 samples (typically 2048)
- **Transitions**: Handle short-to-long and long-to-short transitions
- **Overlap-Add**: Proper windowing and reconstruction

**Windowing Implementation**:
```cpp
void handleVariableBlockSizes(const vorbis_block* block) {
    // libvorbis handles windowing and overlap-add internally
    // We just need to track block sizes for buffer management
    uint32_t current_block_size = block->pcmend;
    
    // Adjust output buffer size if needed
    size_t required_size = current_block_size * m_channels;
    if (m_output_buffer.size() < required_size) {
        m_output_buffer.resize(required_size);
    }
}
```

### **6. Channel Configuration Component**

**Purpose**: Handle various Vorbis channel configurations

**Key Methods**:
- `bool validateChannelConfiguration(uint8_t channels)`: Validate channel count
- `void processChannelMapping(AudioFrame& frame)`: Apply Vorbis channel ordering
- `bool handleChannelCoupling()`: Process magnitude/angle channel pairs
- `void outputMultiChannelData(AudioFrame& frame)`: Format multi-channel output

**Channel Support**:
- **Mono**: Single channel output
- **Stereo**: Left/right channel interleaved output
- **Multi-channel**: Up to 255 channels as per Vorbis specification
- **Channel Coupling**: Magnitude/angle channel pairs for stereo

### **7. Error Handling Component**

**Purpose**: Handle libvorbis errors and provide recovery

**Key Methods**:
- `void handleVorbisError(int vorbis_error)`: Process libvorbis error codes
- `bool recoverFromError()`: Attempt error recovery
- `void resetDecoderState()`: Reset decoder after errors
- `void cleanupVorbisStructures()`: Clean up libvorbis structures

**Error Recovery Strategies**:
- **OV_EREAD**: I/O error, skip packet
- **OV_EFAULT**: Internal error, reset decoder
- **OV_EIMPL**: Unimplemented feature, report error
- **OV_EINVAL**: Invalid data, skip packet
- **OV_ENOTVORBIS**: Not Vorbis data, reject stream
- **OV_EBADHEADER**: Bad header, reject initialization
- **OV_EVERSION**: Version mismatch, check compatibility

## **Data Models**

### **Codec State Machine**

```
[Uninitialized] → [WaitingForHeaders] → [ProcessingHeaders] → [Ready] → [Decoding] → [Flushing] → [Reset]
                                                ↓                           ↑
                                            [Error] → [Recovering]
```

### **Header Processing Pipeline**

```
MediaChunk → Header Detection → ID Header → Comment Header → Setup Header → Decoder Ready
```

### **Audio Processing Pipeline**

```
MediaChunk → Packet Validation → vorbis_synthesis → vorbis_synthesis_blockin → vorbis_synthesis_pcmout → Float to PCM → AudioFrame
```

## **Error Handling**

### **Initialization Errors**
- **Invalid StreamInfo**: Return false from initialize()
- **libvorbis init failure**: Clean up structures, return false
- **Memory allocation**: Handle gracefully, return error
- **Invalid headers**: Reject initialization, log specific errors

### **Decoding Errors**
- **Corrupted packets**: Skip packet, output silence for that period
- **Synthesis errors**: Reset decoder state, attempt recovery
- **Invalid packet format**: Log warning, continue with next packet
- **Buffer overflow**: Increase buffer size, retry operation

### **Header Processing Errors**
- **Missing headers**: Wait for all three headers before proceeding
- **Invalid header format**: Reject stream, report format error
- **Header sequence errors**: Reset header processing, start over
- **Unsupported features**: Report specific unsupported feature

## **Performance Considerations**

### **Memory Usage**
- **Output buffering**: Use appropriately sized buffers for maximum block size
- **libvorbis overhead**: Let libvorbis manage its internal buffers efficiently
- **Float conversion**: Minimize temporary buffer allocations
- **Channel interleaving**: Efficient interleaving during conversion

### **CPU Efficiency**
- **libvorbis optimization**: Use libvorbis built-in optimizations
- **Float to PCM conversion**: Optimized conversion with SIMD where available
- **Block processing**: Efficient handling of variable block sizes
- **Error handling**: Fast error detection and recovery paths

### **Decoding Efficiency**
- **Quality handling**: Support all quality levels (-1 to 10) efficiently
- **Bitrate adaptation**: Handle VBR and quality-based encoding
- **Block transitions**: Efficient short/long block transitions
- **Overlap-add**: Let libvorbis handle windowing efficiently

## **Integration Points**

### **With Demuxers**
- Accept MediaChunk data from any demuxer (OggDemuxer primarily)
- Handle packet data regardless of container format
- Process granule position information for accurate positioning
- Support seeking through reset() method

### **With AudioCodec Architecture**
- Implement all AudioCodec interface methods consistently
- Return standard AudioFrame format (16-bit PCM)
- Handle initialization from StreamInfo parameters
- Provide codec capability reporting

### **With Existing Vorbis Implementation**
- Maintain compatibility with current Vorbis file support
- Provide equivalent or better performance than existing implementation
- Support all previously working Vorbis files
- Integrate with DemuxedStream bridge seamlessly

### **With libvorbis Library**
- Use libvorbis reference implementation for accuracy
- Handle libvorbis API correctly and efficiently
- Manage libvorbis structure lifecycle properly
- Utilize libvorbis error reporting and recovery

This design provides efficient, accurate Vorbis decoding that works with any container format while maintaining compatibility with existing functionality and integrating cleanly with the modern codec architecture.