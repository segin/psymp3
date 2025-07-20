# **FLAC CODEC DESIGN**

## **Overview**

This design document specifies the implementation of a container-agnostic FLAC audio codec for PsyMP3. The codec decodes FLAC bitstream data from any container (native FLAC, Ogg FLAC, or potentially ISO FLAC) into standard 16-bit PCM audio. This codec works with the AudioCodec architecture and receives FLAC frame data from various demuxers.

The design focuses on efficient, accurate FLAC decoding while maintaining compatibility with the existing FLAC implementation and providing a clean interface for the modern demuxer/codec architecture.

## **Architecture**

### **Class Structure**

```cpp
class FLACCodec : public AudioCodec {
private:
    // FLAC decoder state
    std::unique_ptr<FLAC::Decoder::Stream> m_decoder;
    
    // Stream configuration
    uint32_t m_sample_rate;
    uint16_t m_channels;
    uint16_t m_bits_per_sample;
    uint64_t m_total_samples;
    
    // Decoding state
    std::vector<int16_t> m_output_buffer;
    std::mutex m_buffer_mutex;
    std::condition_variable m_buffer_cv;
    
    // Frame processing
    std::queue<MediaChunk> m_input_queue;
    std::atomic<bool> m_decoding_active{false};
    std::thread m_decoder_thread;
    
    // Position tracking
    std::atomic<uint64_t> m_current_sample{0};
    std::atomic<bool> m_seek_requested{false};
    
public:
    explicit FLACCodec(const StreamInfo& stream_info);
    ~FLACCodec() override;
    
    // AudioCodec interface implementation
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override { return "flac"; }
    bool canDecode(const StreamInfo& stream_info) const override;
};
```

### **Supporting Components**

```cpp
class FLACStreamDecoder : public FLAC::Decoder::Stream {
public:
    explicit FLACStreamDecoder(FLACCodec* parent);
    
protected:
    // FLAC decoder callbacks
    FLAC__StreamDecoderReadStatus read_callback(FLAC__byte buffer[], size_t *bytes) override;
    FLAC__StreamDecoderWriteStatus write_callback(const FLAC__Frame *frame, const FLAC__int32 * const buffer[]) override;
    void metadata_callback(const FLAC__StreamMetadata *metadata) override;
    void error_callback(FLAC__StreamDecoderErrorStatus status) override;
    
private:
    FLACCodec* m_parent;
    std::queue<uint8_t> m_input_buffer;
    std::mutex m_input_mutex;
};

struct FLACFrameInfo {
    uint32_t block_size;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint64_t sample_number;
    uint8_t channel_assignment;
};
```

## **Components and Interfaces**

### **1. Initialization Component**

**Purpose**: Configure codec from StreamInfo and initialize FLAC decoder

**Key Methods**:
- `bool initialize()`: Main initialization entry point
- `bool configureFromStreamInfo()`: Extract parameters from StreamInfo
- `bool initializeFLACDecoder()`: Set up libFLAC decoder
- `bool validateConfiguration()`: Verify codec parameters

**Initialization Process**:
1. Extract FLAC parameters from StreamInfo
2. Validate parameters against FLAC specification limits
3. Initialize libFLAC stream decoder
4. Set up internal buffers and threading
5. Configure bit depth conversion if needed

**Parameter Extraction**:
```cpp
// From StreamInfo
m_sample_rate = stream_info.sample_rate;
m_channels = stream_info.channels;
m_bits_per_sample = stream_info.bits_per_sample;
m_total_samples = stream_info.duration_samples;

// Validate FLAC limits
if (m_sample_rate < 1 || m_sample_rate > 655350) return false;
if (m_channels < 1 || m_channels > 8) return false;
if (m_bits_per_sample < 4 || m_bits_per_sample > 32) return false;
```

### **2. Frame Decoding Component**

**Purpose**: Decode individual FLAC frames into PCM samples

**Key Methods**:
- `AudioFrame decode(const MediaChunk& chunk)`: Main decoding interface
- `bool processFrame(const std::vector<uint8_t>& frame_data)`: Process single frame
- `bool feedDataToDecoder(const uint8_t* data, size_t size)`: Feed data to libFLAC
- `AudioFrame extractDecodedSamples()`: Get decoded samples from buffer

**Decoding Process**:
1. Receive MediaChunk containing complete FLAC frame
2. Feed frame data to libFLAC decoder
3. libFLAC calls write_callback with decoded samples
4. Convert samples to 16-bit PCM if needed
5. Return AudioFrame with converted samples

**Frame Processing Flow**:
```
MediaChunk → FLAC Frame Data → libFLAC Decoder → Raw Samples → Bit Depth Conversion → AudioFrame
```

### **3. Bit Depth Conversion Component**

**Purpose**: Convert FLAC samples to consistent 16-bit PCM output

**Key Methods**:
- `void convertSamples(const FLAC__int32* input, int16_t* output, size_t count, int source_bits)`
- `int16_t convert8BitTo16Bit(FLAC__int32 sample)`
- `int16_t convert24BitTo16Bit(FLAC__int32 sample)`
- `int16_t convert32BitTo16Bit(FLAC__int32 sample)`

**Conversion Algorithms**:

**8-bit to 16-bit** (upscaling):
```cpp
int16_t convert8BitTo16Bit(FLAC__int32 sample) {
    // Scale 8-bit (-128 to 127) to 16-bit (-32768 to 32767)
    return static_cast<int16_t>(sample << 8);
}
```

**24-bit to 16-bit** (downscaling with dithering):
```cpp
int16_t convert24BitTo16Bit(FLAC__int32 sample) {
    // Simple truncation (could add dithering for better quality)
    return static_cast<int16_t>(sample >> 8);
}
```

**32-bit to 16-bit** (downscaling):
```cpp
int16_t convert32BitTo16Bit(FLAC__int32 sample) {
    // Scale down from 32-bit to 16-bit range
    return static_cast<int16_t>(sample >> 16);
}
```

### **4. Channel Processing Component**

**Purpose**: Handle various FLAC channel configurations and stereo modes

**Key Methods**:
- `void processChannelAssignment(const FLAC__Frame* frame, const FLAC__int32* const buffer[])`
- `void processIndependentChannels(const FLAC__Frame* frame, const FLAC__int32* const buffer[])`
- `void processLeftSideStereo(const FLAC__Frame* frame, const FLAC__int32* const buffer[])`
- `void processRightSideStereo(const FLAC__Frame* frame, const FLAC__int32* const buffer[])`
- `void processMidSideStereo(const FLAC__Frame* frame, const FLAC__int32* const buffer[])`

**Channel Assignment Types**:
- **Independent**: Each channel decoded separately
- **Left-Side**: Left channel + (Left-Right) difference
- **Right-Side**: (Left+Right) sum + Right channel  
- **Mid-Side**: (Left+Right) sum + (Left-Right) difference

**Stereo Reconstruction**:
```cpp
// Left-Side stereo reconstruction
void processLeftSideStereo(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    for (uint32_t i = 0; i < frame->header.blocksize; i++) {
        FLAC__int32 left = buffer[0][i];
        FLAC__int32 side = buffer[1][i];
        FLAC__int32 right = left - side;
        
        m_output_buffer.push_back(convertSample(left));
        m_output_buffer.push_back(convertSample(right));
    }
}
```

### **5. Threading and Buffering Component**

**Purpose**: Manage asynchronous decoding and output buffering

**Key Methods**:
- `void startDecoderThread()`: Start background decoding thread
- `void stopDecoderThread()`: Stop and cleanup decoder thread
- `void decoderThreadLoop()`: Main decoder thread function
- `void manageOutputBuffer()`: Handle output buffer flow control

**Threading Model**:
- **Main Thread**: Receives MediaChunks, returns AudioFrames
- **Decoder Thread**: Processes FLAC frames asynchronously
- **Synchronization**: Mutex and condition variables for buffer access

**Buffer Management**:
```cpp
// Output buffer with flow control
constexpr size_t MAX_BUFFER_SAMPLES = 48000 * 2 * 4; // 4 seconds stereo

void manageOutputBuffer() {
    std::unique_lock<std::mutex> lock(m_buffer_mutex);
    m_buffer_cv.wait(lock, [this] {
        return m_output_buffer.size() < MAX_BUFFER_SAMPLES || !m_decoding_active;
    });
}
```

### **6. Error Handling Component**

**Purpose**: Handle FLAC decoding errors and recovery

**Key Methods**:
- `void handleDecoderError(FLAC__StreamDecoderErrorStatus status)`
- `bool recoverFromError()`
- `void resetDecoderState()`
- `bool validateFrameData(const MediaChunk& chunk)`

**Error Recovery Strategies**:
- **Frame CRC errors**: Log warning, use decoded data
- **Lost sync**: Reset decoder, search for next frame
- **Invalid parameters**: Skip frame, continue with next
- **Memory errors**: Return error, stop decoding

## **Data Models**

### **Codec State Machine**

```
[Uninitialized] → [Initializing] → [Ready] → [Decoding] → [Flushing] → [Reset]
                                    ↓           ↑
                                [Error] → [Recovering]
```

### **Frame Processing Pipeline**

```
MediaChunk → Input Queue → FLAC Decoder → Sample Conversion → Output Buffer → AudioFrame
```

### **Threading Model**

```
Main Thread: decode() → Input Queue → [Decoder Thread] → Output Buffer → AudioFrame
                                           ↓
                                    libFLAC Processing
```

## **Error Handling**

### **Initialization Errors**
- **Invalid StreamInfo**: Return false from initialize()
- **libFLAC init failure**: Clean up resources, return false
- **Memory allocation**: Handle gracefully, return error
- **Thread creation**: Fall back to synchronous mode if possible

### **Decoding Errors**
- **Corrupted frames**: Skip frame, output silence for that period
- **CRC mismatches**: Log warning, use available data
- **Invalid frame headers**: Reset decoder, search for sync
- **Buffer overflow**: Apply backpressure, wait for space

### **Threading Errors**
- **Thread synchronization**: Use timeouts to prevent deadlocks
- **Exception in decoder thread**: Catch, log, set error state
- **Resource cleanup**: Ensure proper cleanup on destruction
- **Shutdown coordination**: Graceful thread termination

## **Performance Considerations**

### **Memory Usage**
- **Output buffering**: Bounded buffers to prevent memory exhaustion
- **Input queuing**: Minimal input buffering, process frames immediately
- **libFLAC overhead**: Let libFLAC manage its internal buffers
- **Sample conversion**: In-place conversion where possible

### **CPU Efficiency**
- **Bit depth conversion**: Optimized conversion routines
- **Channel processing**: Efficient stereo reconstruction
- **Threading overhead**: Minimize synchronization overhead
- **libFLAC optimization**: Use libFLAC's optimized routines

### **I/O Efficiency**
- **Frame processing**: Process complete frames to minimize overhead
- **Buffer management**: Efficient buffer allocation and reuse
- **Memory copying**: Minimize unnecessary data copying
- **Cache efficiency**: Structure data for good cache locality

## **Integration Points**

### **With Demuxers**
- Accept MediaChunk data from any demuxer (FLACDemuxer, OggDemuxer)
- Handle frame data regardless of container format
- Process timestamp information for accurate positioning
- Support seeking through reset() method

### **With AudioCodec Architecture**
- Implement all AudioCodec interface methods
- Return consistent AudioFrame format
- Handle initialization from StreamInfo
- Provide codec capability reporting

### **With Existing FLAC Implementation**
- Maintain compatibility with current FLAC file support
- Provide equivalent or better performance
- Support all previously working FLAC files
- Integrate with DemuxedStream bridge

### **With Threading System**
- Use PsyMP3's thread naming conventions
- Integrate with system thread management
- Handle thread-safe logging and error reporting
- Coordinate with player thread synchronization

This design provides efficient, accurate FLAC decoding that works with any container format while maintaining compatibility with existing functionality and integrating cleanly with the modern codec architecture.