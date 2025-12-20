# **VORBIS CODEC DESIGN**

## **Overview**

This design document specifies the implementation of a container-agnostic Vorbis audio codec for PsyMP3. The VorbisCodec decodes Vorbis bitstream data from Ogg containers into standard 16-bit PCM audio using the libvorbis reference library.

The design follows the established AudioCodec architecture pattern used by OpusCodec and other PsyMP3 codecs, ensuring consistent integration with the demuxer/codec pipeline.

## **Architecture**

### **Class Hierarchy**

```
AudioCodec (base class)
    └── VorbisCodec
```

### **Data Flow**

```
MediaChunk (from OggDemuxer)
    │
    ▼
┌─────────────────────┐
│ Header Detection    │──► Header Packets ──► vorbis_synthesis_headerin()
└─────────────────────┘
    │
    ▼
┌─────────────────────┐
│ vorbis_synthesis()  │──► Decode packet into vorbis_block
└─────────────────────┘
    │
    ▼
┌─────────────────────┐
│ vorbis_synthesis_   │──► Submit block to DSP state
│ blockin()           │
└─────────────────────┘
    │
    ▼
┌─────────────────────┐
│ vorbis_synthesis_   │──► Extract float PCM samples
│ pcmout()            │
└─────────────────────┘
    │
    ▼
┌─────────────────────┐
│ Float to PCM        │──► Convert [-1.0, 1.0] to [-32768, 32767]
│ Conversion          │    Interleave channels
└─────────────────────┘
    │
    ▼
AudioFrame (16-bit PCM output)
```

## **Components and Interfaces**

### **1. VorbisCodec Class**

```cpp
namespace PsyMP3 {
namespace Codec {
namespace Vorbis {

class VorbisCodec : public AudioCodec {
public:
    explicit VorbisCodec(const StreamInfo& stream_info);
    ~VorbisCodec() override;
    
    // AudioCodec interface
    bool initialize() override;
    AudioFrame decode(const MediaChunk& chunk) override;
    AudioFrame flush() override;
    void reset() override;
    std::string getCodecName() const override;
    bool canDecode(const StreamInfo& stream_info) const override;
    
    // Vorbis-specific accessors
    const vorbis_comment* getVorbisComment() const;
    
private:
    // libvorbis structures
    vorbis_info m_vorbis_info;
    vorbis_comment m_vorbis_comment;
    vorbis_dsp_state m_vorbis_dsp;
    vorbis_block m_vorbis_block;
    
    // Initialization state tracking
    bool m_info_initialized = false;
    bool m_comment_initialized = false;
    bool m_dsp_initialized = false;
    bool m_block_initialized = false;
    
    // Stream configuration
    uint32_t m_sample_rate = 0;
    uint16_t m_channels = 0;
    int32_t m_bitrate_upper = 0;
    int32_t m_bitrate_nominal = 0;
    int32_t m_bitrate_lower = 0;
    uint32_t m_block_size_short = 0;
    uint32_t m_block_size_long = 0;
    
    // Header processing state
    int m_header_packets_received = 0;
    bool m_decoder_initialized = false;
    
    // Position tracking
    std::atomic<uint64_t> m_samples_decoded{0};
    
    // Error handling
    std::atomic<bool> m_error_state{false};
    std::string m_last_error;
    
    // Output buffer management
    std::vector<int16_t> m_output_buffer;
    static constexpr size_t MAX_BUFFER_SAMPLES = 2 * 48000 * 8;
    
    // Threading safety
    mutable std::mutex m_mutex;
    
    // Private implementation methods
    bool initialize_unlocked();
    AudioFrame decode_unlocked(const MediaChunk& chunk);
    AudioFrame flush_unlocked();
    void reset_unlocked();
    
    // Header processing
    bool processHeaderPacket_unlocked(const std::vector<uint8_t>& data);
    bool validateBlockSizes_unlocked() const;
    
    // Audio decoding
    AudioFrame decodeAudioPacket_unlocked(const std::vector<uint8_t>& data);
    int extractPCMSamples_unlocked(AudioFrame& frame);
    void convertFloatToPCM_unlocked(float** pcm, int samples, AudioFrame& frame);
    
    // Utility methods
    bool isHeaderPacket_unlocked(const std::vector<uint8_t>& data) const;
    void handleVorbisError_unlocked(int error_code);
    void cleanupVorbisStructures_unlocked();
    void resetDecoderState_unlocked();
};

} // namespace Vorbis
} // namespace Codec
} // namespace PsyMP3
```

### **2. Header Processing**

Header packets are identified by type byte followed by "vorbis" signature:
- 0x01vorbis = Identification header
- 0x03vorbis = Comment header  
- 0x05vorbis = Setup header

All headers processed via `vorbis_synthesis_headerin()`.

### **3. Audio Decoding Pipeline**

1. `vorbis_synthesis()` - Decode packet into block
2. `vorbis_synthesis_blockin()` - Submit block to DSP
3. `vorbis_synthesis_pcmout()` - Get float samples
4. `vorbis_synthesis_read()` - Consume samples
5. Convert float to 16-bit PCM with clamping

### **4. Float to PCM Conversion**

```cpp
void convertFloatToPCM_unlocked(float** pcm, int samples, AudioFrame& frame) {
    for (int i = 0; i < samples; i++) {
        for (int ch = 0; ch < m_channels; ch++) {
            float sample = pcm[ch][i];
            int32_t val = static_cast<int32_t>(sample * 32767.0f);
            val = std::clamp(val, -32768, 32767);
            frame.samples.push_back(static_cast<int16_t>(val));
        }
    }
}
```

## **Data Models**

### **Codec State Machine**

```
Uninitialized → Awaiting ID Header → Awaiting Comment → Awaiting Setup → Ready ⟷ Decoding
                                                                           ↓
                                                                        Error
```

### **libvorbis Structure Lifecycle**

```
Init:    vorbis_info_init() → vorbis_comment_init()
Headers: vorbis_synthesis_headerin() × 3
Decode:  vorbis_synthesis_init() → vorbis_block_init()
Reset:   vorbis_synthesis_restart()
Cleanup: vorbis_block_clear() → vorbis_dsp_clear() → 
         vorbis_comment_clear() → vorbis_info_clear()
```

## **Error Handling**

| Error Code | Action |
|------------|--------|
| OV_ENOTVORBIS | Reject stream |
| OV_EBADHEADER | Reject initialization |
| OV_EBADPACKET | Skip packet, continue |
| OV_ENOTAUDIO | Skip packet, continue |
| OV_EFAULT | Reset decoder state |
| OV_EVERSION | Reject stream |



## **Correctness Properties**

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Header Sequence Validation
*For any* sequence of Vorbis header packets, the VorbisCodec shall accept only the correct sequence (identification → comment → setup) and reject any other ordering.
**Validates: Requirements 3.1, 3.7**

### Property 2: Identification Header Field Extraction
*For any* valid Vorbis identification header, the extracted values (version, channels, rate, bitrate_upper, bitrate_nominal, bitrate_lower) shall match the values encoded in the header packet.
**Validates: Requirements 4.1-4.7**

### Property 3: Block Size Constraint
*For any* valid Vorbis stream, the block sizes extracted from the identification header shall satisfy: blocksize_0 <= blocksize_1, and both shall be powers of 2 in the range [64, 8192].
**Validates: Requirements 5.1-5.3**

### Property 4: Error Code Handling
*For any* libvorbis error code, the VorbisCodec shall handle it according to the specified recovery strategy without crashing.
**Validates: Requirements 12.1-12.10**

### Property 5: Corrupted Packet Recovery
*For any* stream containing corrupted audio packets interspersed with valid packets, the VorbisCodec shall skip corrupted packets and successfully decode subsequent valid packets.
**Validates: Requirements 13.1-13.4**

### Property 6: Reset Preserves Headers
*For any* initialized VorbisCodec, calling reset() shall clear synthesis state while preserving header information, allowing continued decoding without re-sending headers.
**Validates: Requirements 10.1-10.3**

### Property 7: Flush Outputs Remaining Samples
*For any* VorbisCodec with buffered samples, calling flush() shall return an AudioFrame containing all remaining decoded samples.
**Validates: Requirements 11.1-11.3**

### Property 8: Channel Count Consistency
*For any* valid Vorbis stream with N channels, all decoded AudioFrames shall contain samples with exactly N interleaved channels.
**Validates: Requirements 9.1-9.3**

### Property 9: Float to PCM Clamping
*For any* float sample value outside [-1.0, 1.0], the conversion shall clamp to the valid 16-bit range [-32768, 32767].
**Validates: Requirements 8.2-8.3**

### Property 10: Container-Agnostic Decoding
*For any* valid Vorbis packet data, the VorbisCodec shall decode it identically regardless of whether it came from OggDemuxer or any other source providing MediaChunk data.
**Validates: Requirements 14.1-14.3**

### Property 11: Instance Independence
*For any* two VorbisCodec instances decoding different streams concurrently, the decoding of one stream shall not affect the output of the other.
**Validates: Requirements 16.1-16.2**

### Property 12: Bounded Buffer Size
*For any* sequence of decoded packets, the internal output buffer size shall never exceed the configured maximum.
**Validates: Requirements 15.2-15.3**

### Property 13: MediaChunk to AudioFrame Conversion
*For any* valid MediaChunk containing Vorbis audio data, the VorbisCodec shall produce an AudioFrame with correct sample_rate, channels, and non-empty samples vector.
**Validates: Requirements 17.3**

### Property 14: Cleanup Order
*For any* VorbisCodec destruction, libvorbis structures shall be cleaned up in reverse initialization order (block, dsp, comment, info).
**Validates: Requirements 2.1-2.5**

## **Testing Strategy**

### **Property-Based Testing Framework**

The implementation shall use **RapidCheck** as the property-based testing library for C++.

Each property-based test shall:
- Run a minimum of **100 iterations**
- Be tagged with: `// **Feature: vorbis-codec, Property N: property_text**`
- Use smart generators for valid Vorbis data structures

### **Test Categories**

1. **Header Processing Tests**
   - Valid header sequence acceptance
   - Invalid header sequence rejection
   - Field extraction accuracy
   - Block size validation

2. **Audio Decoding Tests**
   - PCM output format verification
   - Channel interleaving correctness
   - Float to PCM conversion accuracy

3. **Error Handling Tests**
   - Error code interpretation
   - Corrupted packet recovery
   - State reset on errors

4. **Thread Safety Tests**
   - Concurrent instance operation
   - Instance independence verification

5. **Integration Tests**
   - OggDemuxer integration
   - Real Vorbis file decoding

### **Test Data Generators**

Property-based tests shall use generators for:
- Valid Vorbis identification headers with random parameters
- Random but valid audio packet sequences
- Corrupted packets (bit flips, truncation)
- Various channel configurations (1-8 channels)

## **Integration Points**

### **With OggDemuxer**
- Receives MediaChunk with Vorbis packet data
- Header packets identified by packet type byte
- Granule position available for timing

### **With AudioCodecFactory**
- Registered for "vorbis" codec name
- Created via factory function
- Conditional compilation with HAVE_OGGDEMUXER

### **With DemuxedStream**
- Standard AudioCodec interface
- decode() returns AudioFrame
- reset() supports seeking

