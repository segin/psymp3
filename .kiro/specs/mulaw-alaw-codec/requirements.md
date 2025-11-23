# μ-LAW/A-LAW CODEC REQUIREMENTS

## Introduction

This specification defines the requirements for implementing separate μ-law (G.711 μ-law) and A-law (G.711 A-law) audio codec classes for PsyMP3. These codecs handle telephony-standard audio compression used in VoIP systems, PBX systems like Asterisk, and telecommunications infrastructure. The codecs support both containerized formats (WAV, AU) and raw bitstreams commonly used in real-time communications.

The implementation provides two distinct codec classes:
- **MuLawCodec**: Handles μ-law (G.711 μ-law) decoding exclusively
- **ALawCodec**: Handles A-law (G.711 A-law) decoding exclusively

This separation aligns with PsyMP3's architecture where codec selection occurs at the factory level based on the codec_name field in StreamInfo, rather than requiring runtime format detection within a single codec.

The implementation must support:
- **Raw μ-law and A-law bitstream decoding** for telephony applications
- **Container-agnostic operation** supporting WAV, AU, and raw formats
- **8 kHz telephony standard** and other sample rates as specified
- **Real-time decoding performance** suitable for VoIP applications
- **Bit-perfect accuracy** matching ITU-T G.711 specification
- **Efficient table-based conversion** for optimal performance

## Glossary

- **μ-law (G.711 μ-law)**: A logarithmic audio compression algorithm standardized by ITU-T G.711, primarily used in North American and Japanese telephony systems. Compresses 14-bit linear PCM samples into 8-bit values.
- **A-law (G.711 A-law)**: A logarithmic audio compression algorithm standardized by ITU-T G.711, primarily used in European telephony systems. Compresses 13-bit linear PCM samples into 8-bit values.
- **MuLawCodec**: The codec class responsible for decoding μ-law compressed audio data into linear PCM samples.
- **ALawCodec**: The codec class responsible for decoding A-law compressed audio data into linear PCM samples.
- **ITU-T G.711**: International Telecommunication Union Telecommunication Standardization Sector recommendation defining μ-law and A-law pulse code modulation (PCM) of voice frequencies.
- **PCM (Pulse Code Modulation)**: A method used to digitally represent sampled analog signals, typically as signed 16-bit linear samples.
- **StreamInfo**: A data structure in PsyMP3 containing metadata about an audio stream, including codec_name, sample rate, and channel count.
- **MediaFactory**: The factory class in PsyMP3 responsible for creating appropriate codec instances based on StreamInfo.
- **AudioCodec**: The base interface class that all audio codecs in PsyMP3 must implement.
- **MediaChunk**: A container for encoded audio data passed to codecs for decoding.
- **AudioFrame**: A container for decoded PCM audio samples output by codecs.
- **VoIP (Voice over IP)**: Technology for delivering voice communications over Internet Protocol networks.
- **PBX (Private Branch Exchange)**: A private telephone network used within an organization, such as Asterisk.
- **WAV**: A Microsoft/IBM audio file format container, supporting various audio codecs including μ-law and A-law.
- **AU**: Sun/NeXT audio file format container, commonly used for μ-law and A-law encoded audio.
- **RTP (Real-time Transport Protocol)**: A network protocol for delivering audio and video over IP networks.
- **Telephony Standard**: The standard 8 kHz sample rate used in traditional telephone systems.
- **Lookup Table**: A pre-computed array mapping compressed values to decompressed PCM samples for efficient conversion.
- **Host Byte Order**: The native byte ordering (endianness) of the system's processor architecture.

## Requirements

### Requirement 1: μ-law Decoding (G.711 μ-law)

**User Story:** As a VoIP application, I want to decode μ-law compressed audio data into PCM samples, so that I can play telephony audio from various sources.

#### Acceptance Criteria

1. WHEN receiving μ-law encoded data THEN the MuLawCodec SHALL decode using ITU-T G.711 μ-law specification
2. WHEN processing 8-bit μ-law samples THEN the MuLawCodec SHALL convert to 16-bit linear PCM
3. WHEN using lookup tables THEN the MuLawCodec SHALL implement efficient table-based conversion
4. WHEN handling telephony standard THEN the MuLawCodec SHALL support 8 kHz sample rate as primary target
5. WHEN processing other sample rates THEN the MuLawCodec SHALL handle rates specified in container headers
6. WHEN decoding silence THEN the MuLawCodec SHALL properly handle μ-law silence encoding (0xFF)
7. WHEN validating input THEN the MuLawCodec SHALL accept all 8-bit μ-law values as valid
8. WHEN outputting PCM THEN the MuLawCodec SHALL produce signed 16-bit samples in host byte order

### Requirement 2: A-law Decoding (G.711 A-law)

**User Story:** As a European telephony system, I want to decode A-law compressed audio data into PCM samples, so that I can handle European standard telephony audio.

#### Acceptance Criteria

1. WHEN receiving A-law encoded data THEN the ALawCodec SHALL decode using ITU-T G.711 A-law specification
2. WHEN processing 8-bit A-law samples THEN the ALawCodec SHALL convert to 16-bit linear PCM
3. WHEN using lookup tables THEN the ALawCodec SHALL implement efficient table-based conversion
4. WHEN handling telephony standard THEN the ALawCodec SHALL support 8 kHz sample rate as primary target
5. WHEN processing other sample rates THEN the ALawCodec SHALL handle rates specified in container headers
6. WHEN decoding quiet audio THEN the ALawCodec SHALL properly handle A-law closest-to-silence encoding (0x55 maps to -8)
7. WHEN validating input THEN the ALawCodec SHALL accept all 8-bit A-law values as valid
8. WHEN outputting PCM THEN the ALawCodec SHALL produce signed 16-bit samples in host byte order

### Requirement 3: Raw Bitstream Support

**User Story:** As an Asterisk PBX system, I want to decode raw μ-law/A-law bitstreams without container headers, so that I can process telephony audio streams directly.

#### Acceptance Criteria

1. WHEN processing raw bitstreams THEN the MuLawCodec and ALawCodec SHALL decode without requiring container headers
2. WHEN initializing for raw streams THEN the MuLawCodec and ALawCodec SHALL use default telephony parameters (8 kHz, mono)
3. WHEN handling stream parameters THEN the MuLawCodec and ALawCodec SHALL accept externally provided sample rate and channel count
4. WHEN processing continuous streams THEN the MuLawCodec and ALawCodec SHALL handle indefinite-length bitstreams
5. WHEN no container is present THEN the MuLawCodec and ALawCodec SHALL assume standard telephony format
6. WHEN detecting format THEN the MuLawCodec and ALawCodec SHALL support format identification from file extension or MIME type
7. WHEN handling VoIP streams THEN the MuLawCodec and ALawCodec SHALL process packet-based audio efficiently
8. WHEN streaming real-time THEN the MuLawCodec and ALawCodec SHALL minimize latency for live audio

### Requirement 4: Container Integration

**User Story:** As a media player, I want to play μ-law/A-law audio from WAV and AU files, so that I can handle containerized telephony audio.

#### Acceptance Criteria

1. WHEN processing WAV files THEN the MuLawCodec SHALL handle WAVE_FORMAT_MULAW (0x0007) and the ALawCodec SHALL handle WAVE_FORMAT_ALAW (0x0006)
2. WHEN processing AU files THEN the MuLawCodec and ALawCodec SHALL handle Sun/NeXT audio format with μ-law/A-law encoding
3. WHEN reading container headers THEN the MuLawCodec and ALawCodec SHALL extract sample rate, channels, and encoding type
4. WHEN validating containers THEN the MuLawCodec and ALawCodec SHALL verify format compatibility with μ-law/A-law specifications
5. WHEN handling metadata THEN the MuLawCodec and ALawCodec SHALL coordinate with container demuxers for format information
6. WHEN processing chunks THEN the MuLawCodec and ALawCodec SHALL decode audio data regardless of container chunk structure
7. WHEN seeking occurs THEN the MuLawCodec and ALawCodec SHALL handle sample-accurate positioning
8. WHEN container specifies parameters THEN the MuLawCodec and ALawCodec SHALL use container-provided sample rate and channel count

### Requirement 5: Performance and Efficiency

**User Story:** As a real-time communication system, I want efficient μ-law/A-law decoding, so that CPU usage remains minimal during high-volume telephony processing.

#### Acceptance Criteria

1. WHEN decoding samples THEN the MuLawCodec and ALawCodec SHALL use pre-computed lookup tables for conversion
2. WHEN processing large volumes THEN the MuLawCodec and ALawCodec SHALL maintain real-time performance for telephony applications
3. WHEN handling multiple streams THEN the MuLawCodec and ALawCodec SHALL support concurrent decoding efficiently
4. WHEN optimizing memory THEN the MuLawCodec and ALawCodec SHALL minimize memory allocation overhead
5. WHEN processing packets THEN the MuLawCodec and ALawCodec SHALL handle small packet sizes efficiently
6. WHEN using SIMD THEN the MuLawCodec and ALawCodec SHALL leverage vectorized operations where beneficial
7. WHEN caching data THEN the MuLawCodec and ALawCodec SHALL optimize for cache-friendly memory access patterns
8. WHEN measuring performance THEN the MuLawCodec and ALawCodec SHALL exceed real-time requirements by significant margin

### Requirement 6: Accuracy and Compliance

**User Story:** As a telecommunications engineer, I want bit-perfect μ-law/A-law decoding, so that audio quality meets ITU-T G.711 standards.

#### Acceptance Criteria

1. WHEN implementing μ-law THEN the MuLawCodec SHALL match ITU-T G.711 μ-law specification exactly
2. WHEN implementing A-law THEN the ALawCodec SHALL match ITU-T G.711 A-law specification exactly
3. WHEN validating against reference THEN the MuLawCodec and ALawCodec SHALL produce identical output to reference implementations
4. WHEN handling edge cases THEN the MuLawCodec and ALawCodec SHALL process all 256 possible input values correctly
5. WHEN testing compliance THEN the MuLawCodec and ALawCodec SHALL pass ITU-T test vectors
6. WHEN processing silence THEN the MuLawCodec and ALawCodec SHALL handle silence suppression values correctly
7. WHEN measuring SNR THEN the MuLawCodec and ALawCodec SHALL achieve expected signal-to-noise ratios
8. WHEN comparing formats THEN the MuLawCodec and ALawCodec SHALL maintain distinct μ-law vs A-law characteristics

### Requirement 7: Channel and Sample Rate Support

**User Story:** As a media application, I want flexible channel and sample rate support for μ-law/A-law audio, so that I can handle various telephony configurations.

#### Acceptance Criteria

1. WHEN processing mono audio THEN the MuLawCodec and ALawCodec SHALL handle single-channel telephony standard
2. WHEN processing stereo audio THEN the MuLawCodec and ALawCodec SHALL support dual-channel configurations
3. WHEN handling 8 kHz audio THEN the MuLawCodec and ALawCodec SHALL optimize for standard telephony sample rate
4. WHEN processing other rates THEN the MuLawCodec and ALawCodec SHALL support 16 kHz, 32 kHz, and 48 kHz as specified
5. WHEN channel count varies THEN the MuLawCodec and ALawCodec SHALL adapt output format accordingly
6. WHEN interleaving channels THEN the MuLawCodec and ALawCodec SHALL properly handle multi-channel sample ordering
7. WHEN sample rate is unspecified THEN the MuLawCodec and ALawCodec SHALL default to 8 kHz for raw streams
8. WHEN validating parameters THEN the MuLawCodec and ALawCodec SHALL reject unsupported configurations gracefully

### Requirement 8: Error Handling and Robustness

**User Story:** As a telephony system, I want robust error handling in μ-law/A-law decoding, so that corrupted or invalid data doesn't disrupt communication.

#### Acceptance Criteria

1. WHEN encountering invalid headers THEN the MuLawCodec and ALawCodec SHALL report format errors clearly
2. WHEN processing corrupted data THEN the MuLawCodec and ALawCodec SHALL continue decoding and minimize artifacts
3. WHEN handling truncated streams THEN the MuLawCodec and ALawCodec SHALL process available data gracefully
4. WHEN memory allocation fails THEN the MuLawCodec and ALawCodec SHALL return appropriate error codes
5. WHEN parameters are invalid THEN the MuLawCodec and ALawCodec SHALL validate inputs and reject invalid configurations
6. WHEN stream ends unexpectedly THEN the MuLawCodec and ALawCodec SHALL handle premature termination
7. WHEN format detection fails THEN the MuLawCodec and ALawCodec SHALL provide fallback behavior
8. WHEN errors occur THEN the MuLawCodec and ALawCodec SHALL maintain decoder state consistency

### Requirement 9: Integration with AudioCodec Architecture

**User Story:** As a PsyMP3 component, I want separate μ-law and A-law codec classes to integrate seamlessly with the AudioCodec architecture, so that they work consistently with other audio codecs.

#### Acceptance Criteria

1. WHEN implementing AudioCodec interface THEN the MuLawCodec and ALawCodec SHALL provide all required virtual methods
2. WHEN initializing THEN the MuLawCodec and ALawCodec SHALL configure themselves from StreamInfo parameters for their specific formats
3. WHEN decoding chunks THEN the MuLawCodec and ALawCodec SHALL convert MediaChunk data to AudioFrame output using format-specific conversion
4. WHEN flushing THEN the MuLawCodec and ALawCodec SHALL output any remaining samples as AudioFrame
5. WHEN resetting THEN the MuLawCodec and ALawCodec SHALL clear state for seeking operations
6. WHEN reporting capabilities THEN the MuLawCodec SHALL indicate support only for μ-law streams and the ALawCodec SHALL indicate support only for A-law streams
7. WHEN handling errors THEN the MuLawCodec and ALawCodec SHALL use PsyMP3's error reporting mechanisms
8. WHEN logging debug information THEN the MuLawCodec and ALawCodec SHALL use PsyMP3's Debug logging system

### Requirement 10: Codec Factory Registration

**User Story:** As a media player, I want automatic selection of the correct codec based on StreamInfo, so that μ-law and A-law formats are handled by their respective specialized codecs.

#### Acceptance Criteria

1. WHEN registering with MediaFactory THEN the MuLawCodec SHALL register for "mulaw" codec_name
2. WHEN registering with MediaFactory THEN the ALawCodec SHALL register for "alaw" codec_name  
3. WHEN StreamInfo contains codec_name "mulaw" THEN the MediaFactory SHALL create MuLawCodec instance
4. WHEN StreamInfo contains codec_name "alaw" THEN the MediaFactory SHALL create ALawCodec instance
5. WHEN canDecode is called THEN the MuLawCodec SHALL return true only for μ-law StreamInfo
6. WHEN canDecode is called THEN the ALawCodec SHALL return true only for A-law StreamInfo
7. WHEN wrong codec is used THEN the MuLawCodec and ALawCodec SHALL reject initialization and return false
8. WHEN codec registration occurs THEN the MuLawCodec and ALawCodec SHALL use conditional compilation guards

### Requirement 11: Thread Safety and Concurrency

**User Story:** As a multi-threaded telephony application, I want thread-safe μ-law/A-law decoding operations, so that multiple codec instances can operate concurrently.

#### Acceptance Criteria

1. WHEN multiple codec instances exist THEN each MuLawCodec and ALawCodec instance SHALL maintain independent state
2. WHEN decoding occurs concurrently THEN the MuLawCodec and ALawCodec instances SHALL not interfere with each other
3. WHEN using lookup tables THEN the MuLawCodec and ALawCodec SHALL use read-only shared tables safely
4. WHEN initialization occurs THEN the MuLawCodec and ALawCodec SHALL handle concurrent initialization safely
5. WHEN cleanup occurs THEN the MuLawCodec and ALawCodec SHALL ensure no operations are in progress before destruction
6. WHEN errors occur THEN the MuLawCodec and ALawCodec SHALL maintain thread-local error state
7. WHEN debugging is enabled THEN the MuLawCodec and ALawCodec logging operations SHALL be thread-safe
8. WHEN shared resources accessed THEN the MuLawCodec and ALawCodec SHALL use appropriate synchronization

### Requirement 12: Compatibility and Standards

**User Story:** As a telecommunications professional, I want standards-compliant μ-law/A-law support, so that audio interoperates with existing telephony infrastructure.

#### Acceptance Criteria

1. WHEN implementing G.711 THEN the MuLawCodec and ALawCodec SHALL comply with ITU-T G.711 recommendation
2. WHEN handling North American systems THEN the MuLawCodec SHALL support μ-law as primary format
3. WHEN handling European systems THEN the ALawCodec SHALL support A-law as primary format
4. WHEN processing legacy files THEN the MuLawCodec and ALawCodec SHALL handle historical telephony audio formats
5. WHEN interfacing with VoIP THEN the MuLawCodec and ALawCodec SHALL support RTP payload formats
6. WHEN validating compliance THEN the MuLawCodec and ALawCodec SHALL pass telecommunications test suites
7. WHEN handling variants THEN the MuLawCodec and ALawCodec SHALL support standard G.711 variants only
8. WHEN documenting behavior THEN the MuLawCodec and ALawCodec SHALL reference appropriate ITU-T specifications