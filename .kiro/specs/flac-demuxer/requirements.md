# **FLAC DEMUXER REQUIREMENTS - REVISED**

## **Introduction**

This specification defines the requirements for implementing a native FLAC container demuxer for PsyMP3, based on lessons learned from real-world implementation and debugging. The FLAC Demuxer handles native FLAC files (.flac) by parsing the FLAC container format and extracting FLAC bitstream data for decoding.

**Key Implementation Insights:**
- **Frame size estimation** is critical for performance - must use STREAMINFO minimum frame size
- **Highly compressed FLAC** can have frames as small as 14 bytes, requiring accurate estimation
- **Binary search seeking** is fundamentally incompatible with compressed audio streams
- **Thread safety** requires careful public/private method patterns to prevent deadlocks
- **Debug logging** with method identification tokens is essential for troubleshooting

The implementation must support:
- **Native FLAC container format** as defined in RFC 9639 (FLAC specification)
- **Accurate frame size estimation** using STREAMINFO metadata for performance
- **Efficient frame boundary detection** with minimal I/O operations
- **Robust error recovery** for corrupted or incomplete streams
- **Thread-safe operations** using proper synchronization patterns

## **Glossary**

- **FLAC Demuxer**: The software component responsible for parsing FLAC container files and extracting audio frame data
- **FLAC Container**: The file format structure defined in RFC 9639 that encapsulates FLAC audio bitstream data
- **Stream Marker**: The 4-byte ASCII signature "fLaC" (0x664C6143) that identifies a valid FLAC stream
- **Metadata Block**: A data structure containing information about the FLAC stream, appearing before audio frames
- **Metadata Block Header**: A 4-byte header containing is_last flag, block type, and block length
- **STREAMINFO Block**: A mandatory FLAC metadata block (type 0) containing essential stream parameters including sample rate, channels, bit depth, and frame size bounds
- **PADDING Block**: An optional FLAC metadata block (type 1) containing arbitrary padding bytes
- **APPLICATION Block**: An optional FLAC metadata block (type 2) containing third-party application data
- **SEEKTABLE Block**: An optional FLAC metadata block (type 3) containing seek points for efficient timestamp-based navigation
- **VORBIS_COMMENT Block**: An optional FLAC metadata block (type 4) containing UTF-8 encoded metadata fields such as artist, title, and album information
- **CUESHEET Block**: An optional FLAC metadata block (type 5) containing CD-DA track and index information
- **PICTURE Block**: An optional FLAC metadata block (type 6) containing embedded artwork data
- **Frame**: A complete audio data unit consisting of frame header, subframes, padding, and frame footer
- **Frame Header**: The header portion of a frame containing sync code, block size, sample rate, channels, and bit depth
- **Frame Footer**: The footer portion of a frame containing a 16-bit CRC checksum
- **Subframe**: An encoded audio channel within a frame
- **Frame Boundary**: The start position of a FLAC audio frame, identified by a sync code pattern
- **Sync Code**: A 15-bit pattern (0b111111111111100) that marks the beginning of a FLAC frame
- **Blocking Strategy**: A bit indicating whether the stream uses fixed block size (0) or variable block size (1)
- **Block Size**: The number of interchannel samples contained in a frame
- **Block Size Bits**: 4 bits in the frame header encoding the block size according to RFC 9639 Table 14
- **Sample Rate Bits**: 4 bits in the frame header encoding the sample rate
- **Channel Bits**: 4 bits in the frame header encoding channel count and interchannel decorrelation mode
- **Bit Depth Bits**: 3 bits in the frame header encoding the sample bit depth
- **Coded Number**: A UTF-8-like variable-length encoded number representing frame number or sample number
- **Uncommon Block Size**: A block size not in the standard lookup table, stored as 8-bit or 16-bit value
- **Uncommon Sample Rate**: A sample rate not in the standard lookup table, stored as 8-bit or 16-bit value
- **CRC-8**: An 8-bit cyclic redundancy check used for frame header validation
- **CRC-16**: A 16-bit cyclic redundancy check used for frame footer validation
- **Seek Point**: A data structure in SEEKTABLE containing sample number, byte offset, and frame sample count
- **Placeholder Seek Point**: A seek point with sample number 0xFFFFFFFFFFFFFFFF reserved for future use
- **Interchannel Decorrelation**: A technique for reducing redundancy between stereo channels (independent, left-side, right-side, mid-side)
- **Big-Endian**: Byte order where most significant byte comes first (used for most FLAC integers)
- **Little-Endian**: Byte order where least significant byte comes first (used only for Vorbis comment lengths)
- **Forbidden Pattern**: A bit pattern that MUST NOT appear in any valid FLAC bitstream per RFC 9639 Table 1
- **Reserved Pattern**: A bit pattern reserved for future use that should be skipped by current decoders
- **Streamable Subset**: A restricted subset of FLAC format ensuring streams can be decoded without seeking
- **MediaChunk**: A PsyMP3 data structure containing a complete audio frame with timing and stream identification
- **IOHandler**: A PsyMP3 interface for abstracting file and network I/O operations
- **StreamInfo**: A PsyMP3 data structure containing stream parameters such as codec type, sample rate, and duration
- **FLACCodec**: The PsyMP3 component responsible for decoding FLAC bitstream data into PCM audio samples
- **PsyMP3**: The media player application that integrates the FLAC Demuxer
- **RFC 9639**: The official FLAC specification document defining the container format and bitstream structure

## **Requirements**

### **Requirement 1: Stream Marker Validation (RFC 9639 Section 6)**

**User Story:** As a FLAC parser, I want to validate the stream marker correctly, so that I can reject non-FLAC files immediately and prevent processing invalid data.

#### **Acceptance Criteria**

1. WHEN opening a file, THE FLAC Demuxer SHALL read the first 4 bytes
2. WHEN validating stream marker, THE FLAC Demuxer SHALL verify bytes equal 0x66 0x4C 0x61 0x43 (fLaC in ASCII)
3. IF stream marker is invalid, THEN THE FLAC Demuxer SHALL reject the file without crashing
4. WHEN stream marker is valid, THE FLAC Demuxer SHALL proceed to metadata block parsing
5. WHEN logging validation, THE FLAC Demuxer SHALL report exact byte values found versus expected

### **Requirement 2: Metadata Block Header Parsing (RFC 9639 Section 8.1)**

**User Story:** As a FLAC parser, I want to parse metadata block headers correctly, so that I can identify and process all metadata blocks in the stream.

#### **Acceptance Criteria**

1. WHEN reading metadata block header, THE FLAC Demuxer SHALL read 4 bytes
2. WHEN parsing header byte 0, THE FLAC Demuxer SHALL extract bit 7 as is_last flag
3. WHEN parsing header byte 0, THE FLAC Demuxer SHALL extract bits 0-6 as block type
4. IF block type equals 127, THEN THE FLAC Demuxer SHALL reject the file as forbidden pattern
5. WHEN parsing header bytes 1-3, THE FLAC Demuxer SHALL extract 24-bit big-endian block length
6. WHEN block type is 0, THE FLAC Demuxer SHALL identify block as STREAMINFO
7. WHEN block type is 1, THE FLAC Demuxer SHALL identify block as PADDING
8. WHEN block type is 2, THE FLAC Demuxer SHALL identify block as APPLICATION
9. WHEN block type is 3, THE FLAC Demuxer SHALL identify block as SEEKTABLE
10. WHEN block type is 4, THE FLAC Demuxer SHALL identify block as VORBIS_COMMENT
11. WHEN block type is 5, THE FLAC Demuxer SHALL identify block as CUESHEET
12. WHEN block type is 6, THE FLAC Demuxer SHALL identify block as PICTURE
13. WHEN block type is 7-126, THE FLAC Demuxer SHALL treat block as reserved and skip it
14. WHEN is_last flag is 1, THE FLAC Demuxer SHALL mark this as final metadata block

### **Requirement 3: STREAMINFO Block Parsing (RFC 9639 Section 8.2)**

**User Story:** As a FLAC parser, I want to parse STREAMINFO blocks with complete RFC compliance, so that I can extract all stream parameters accurately.

#### **Acceptance Criteria**

1. WHEN STREAMINFO Block is first metadata block, THE FLAC Demuxer SHALL parse it completely
2. IF STREAMINFO Block is not first, THEN THE FLAC Demuxer SHALL reject the stream
3. WHEN parsing STREAMINFO, THE FLAC Demuxer SHALL read exactly 34 bytes of data
4. WHEN reading minimum block size, THE FLAC Demuxer SHALL extract u(16) big-endian value
5. WHEN reading maximum block size, THE FLAC Demuxer SHALL extract u(16) big-endian value
6. IF minimum block size is less than 16, THEN THE FLAC Demuxer SHALL reject as forbidden pattern
7. IF maximum block size is less than 16, THEN THE FLAC Demuxer SHALL reject as forbidden pattern
8. IF minimum block size exceeds maximum block size, THEN THE FLAC Demuxer SHALL reject the stream
9. WHEN reading minimum frame size, THE FLAC Demuxer SHALL extract u(24) big-endian value
10. WHEN reading maximum frame size, THE FLAC Demuxer SHALL extract u(24) big-endian value
11. IF minimum frame size is 0, THEN THE FLAC Demuxer SHALL treat value as unknown
12. IF maximum frame size is 0, THEN THE FLAC Demuxer SHALL treat value as unknown
13. WHEN reading sample rate, THE FLAC Demuxer SHALL extract u(20) big-endian value
14. IF sample rate is 0 and stream contains audio, THEN THE FLAC Demuxer SHALL reject the stream
15. WHEN reading channel count, THE FLAC Demuxer SHALL extract u(3) value and add 1
16. WHEN reading bit depth, THE FLAC Demuxer SHALL extract u(5) value and add 1
17. WHEN reading total samples, THE FLAC Demuxer SHALL extract u(36) big-endian value
18. IF total samples is 0, THEN THE FLAC Demuxer SHALL treat stream length as unknown
19. WHEN reading MD5 checksum, THE FLAC Demuxer SHALL extract 128 bits
20. IF MD5 checksum is all zeros, THEN THE FLAC Demuxer SHALL treat checksum as unavailable

### **Requirement 4: Frame Sync Code Detection (RFC 9639 Section 9.1)**

**User Story:** As a FLAC parser, I want to detect frame sync codes correctly, so that I can identify frame boundaries accurately.

#### **Acceptance Criteria**

1. WHEN searching for frame start, THE FLAC Demuxer SHALL look for 15-bit pattern 0b111111111111100
2. WHEN sync code is found, THE FLAC Demuxer SHALL verify it is byte-aligned
3. WHEN reading blocking strategy bit, THE FLAC Demuxer SHALL extract bit following sync code
4. IF blocking strategy bit is 0, THEN THE FLAC Demuxer SHALL treat stream as fixed block size
5. IF blocking strategy bit is 1, THEN THE FLAC Demuxer SHALL treat stream as variable block size
6. WHEN fixed block size stream, THE FLAC Demuxer SHALL expect first two frame bytes as 0xFF 0xF8
7. WHEN variable block size stream, THE FLAC Demuxer SHALL expect first two frame bytes as 0xFF 0xF9
8. IF blocking strategy changes mid-stream, THEN THE FLAC Demuxer SHALL reject the stream

### **Requirement 5: Frame Header Block Size Parsing (RFC 9639 Section 9.1.1)**

**User Story:** As a FLAC parser, I want to parse block size bits correctly per RFC 9639 Table 14, so that I can determine the correct number of samples in each frame.

#### **Acceptance Criteria**

1. WHEN reading block size bits, THE FLAC Demuxer SHALL extract bits 4-7 of frame byte 2
2. IF block size bits are 0b0000, THEN THE FLAC Demuxer SHALL reject as reserved pattern
3. IF block size bits are 0b0001, THEN THE FLAC Demuxer SHALL set block size to 192
4. IF block size bits are 0b0010, THEN THE FLAC Demuxer SHALL set block size to 576
5. IF block size bits are 0b0011, THEN THE FLAC Demuxer SHALL set block size to 1152
6. IF block size bits are 0b0100, THEN THE FLAC Demuxer SHALL set block size to 2304
7. IF block size bits are 0b0101, THEN THE FLAC Demuxer SHALL set block size to 4608
8. IF block size bits are 0b0110, THEN THE FLAC Demuxer SHALL read 8-bit uncommon block size and add 1
9. IF block size bits are 0b0111, THEN THE FLAC Demuxer SHALL read 16-bit uncommon block size and add 1
10. IF block size bits are 0b1000, THEN THE FLAC Demuxer SHALL set block size to 256
11. IF block size bits are 0b1001, THEN THE FLAC Demuxer SHALL set block size to 512
12. IF block size bits are 0b1010, THEN THE FLAC Demuxer SHALL set block size to 1024
13. IF block size bits are 0b1011, THEN THE FLAC Demuxer SHALL set block size to 2048
14. IF block size bits are 0b1100, THEN THE FLAC Demuxer SHALL set block size to 4096
15. IF block size bits are 0b1101, THEN THE FLAC Demuxer SHALL set block size to 8192
16. IF block size bits are 0b1110, THEN THE FLAC Demuxer SHALL set block size to 16384
17. IF block size bits are 0b1111, THEN THE FLAC Demuxer SHALL set block size to 32768
18. IF uncommon block size equals 65536, THEN THE FLAC Demuxer SHALL reject as forbidden pattern

### **Requirement 6: Frame Header Sample Rate Parsing (RFC 9639 Section 9.1.2)**

**User Story:** As a FLAC parser, I want to parse sample rate bits correctly per RFC 9639, so that I can determine the correct sample rate for each frame.

#### **Acceptance Criteria**

1. WHEN reading sample rate bits, THE FLAC Demuxer SHALL extract bits 0-3 of frame byte 2
2. IF sample rate bits are 0b0000, THEN THE FLAC Demuxer SHALL use sample rate from STREAMINFO Block
3. IF sample rate bits are 0b0001, THEN THE FLAC Demuxer SHALL set sample rate to 88200 Hz
4. IF sample rate bits are 0b0010, THEN THE FLAC Demuxer SHALL set sample rate to 176400 Hz
5. IF sample rate bits are 0b0011, THEN THE FLAC Demuxer SHALL set sample rate to 192000 Hz
6. IF sample rate bits are 0b0100, THEN THE FLAC Demuxer SHALL set sample rate to 8000 Hz
7. IF sample rate bits are 0b0101, THEN THE FLAC Demuxer SHALL set sample rate to 16000 Hz
8. IF sample rate bits are 0b0110, THEN THE FLAC Demuxer SHALL set sample rate to 22050 Hz
9. IF sample rate bits are 0b0111, THEN THE FLAC Demuxer SHALL set sample rate to 24000 Hz
10. IF sample rate bits are 0b1000, THEN THE FLAC Demuxer SHALL set sample rate to 32000 Hz
11. IF sample rate bits are 0b1001, THEN THE FLAC Demuxer SHALL set sample rate to 44100 Hz
12. IF sample rate bits are 0b1010, THEN THE FLAC Demuxer SHALL set sample rate to 48000 Hz
13. IF sample rate bits are 0b1011, THEN THE FLAC Demuxer SHALL set sample rate to 96000 Hz
14. IF sample rate bits are 0b1100, THEN THE FLAC Demuxer SHALL read 8-bit uncommon sample rate in kHz
15. IF sample rate bits are 0b1101, THEN THE FLAC Demuxer SHALL read 16-bit uncommon sample rate in Hz
16. IF sample rate bits are 0b1110, THEN THE FLAC Demuxer SHALL read 16-bit uncommon sample rate in tens of Hz
17. IF sample rate bits are 0b1111, THEN THE FLAC Demuxer SHALL reject as forbidden pattern

### **Requirement 7: Frame Header Channel Assignment Parsing (RFC 9639 Section 9.1.3)**

**User Story:** As a FLAC parser, I want to parse channel bits correctly, so that I can determine channel count and interchannel decorrelation mode.

#### **Acceptance Criteria**

1. WHEN reading channel bits, THE FLAC Demuxer SHALL extract bits 4-7 of frame byte 3
2. IF channel bits are 0b0000-0b0111, THEN THE FLAC Demuxer SHALL set channel count to bits value plus 1
3. IF channel bits are 0b0000-0b0111, THEN THE FLAC Demuxer SHALL set mode to independent
4. IF channel bits are 0b1000, THEN THE FLAC Demuxer SHALL set mode to left-side stereo
5. IF channel bits are 0b1001, THEN THE FLAC Demuxer SHALL set mode to right-side stereo
6. IF channel bits are 0b1010, THEN THE FLAC Demuxer SHALL set mode to mid-side stereo
7. IF channel bits are 0b1011-0b1111, THEN THE FLAC Demuxer SHALL reject as reserved pattern

### **Requirement 8: Frame Header Bit Depth Parsing (RFC 9639 Section 9.1.4)**

**User Story:** As a FLAC parser, I want to parse bit depth bits correctly, so that I can determine the sample bit depth for each frame.

#### **Acceptance Criteria**

1. WHEN reading bit depth bits, THE FLAC Demuxer SHALL extract bits 1-3 of frame byte 3
2. IF bit depth bits are 0b000, THEN THE FLAC Demuxer SHALL use bit depth from STREAMINFO Block
3. IF bit depth bits are 0b001, THEN THE FLAC Demuxer SHALL set bit depth to 8 bits
4. IF bit depth bits are 0b010, THEN THE FLAC Demuxer SHALL set bit depth to 12 bits
5. IF bit depth bits are 0b011, THEN THE FLAC Demuxer SHALL reject as reserved pattern
6. IF bit depth bits are 0b100, THEN THE FLAC Demuxer SHALL set bit depth to 16 bits
7. IF bit depth bits are 0b101, THEN THE FLAC Demuxer SHALL set bit depth to 20 bits
8. IF bit depth bits are 0b110, THEN THE FLAC Demuxer SHALL set bit depth to 24 bits
9. IF bit depth bits are 0b111, THEN THE FLAC Demuxer SHALL set bit depth to 32 bits

### **Requirement 9: Frame Header Coded Number Parsing (RFC 9639 Section 9.1.5)**

**User Story:** As a FLAC parser, I want to parse coded numbers correctly using UTF-8-like encoding, so that I can track frame or sample numbers accurately.

#### **Acceptance Criteria**

1. WHEN reading coded number, THE FLAC Demuxer SHALL use UTF-8-like variable-length encoding
2. IF first byte is 0b0xxxxxxx, THEN THE FLAC Demuxer SHALL read 1-byte coded number
3. IF first byte is 0b110xxxxx, THEN THE FLAC Demuxer SHALL read 2-byte coded number
4. IF first byte is 0b1110xxxx, THEN THE FLAC Demuxer SHALL read 3-byte coded number
5. IF first byte is 0b11110xxx, THEN THE FLAC Demuxer SHALL read 4-byte coded number
6. IF first byte is 0b111110xx, THEN THE FLAC Demuxer SHALL read 5-byte coded number
7. IF first byte is 0b1111110x, THEN THE FLAC Demuxer SHALL read 6-byte coded number
8. IF first byte is 0b11111110, THEN THE FLAC Demuxer SHALL read 7-byte coded number
9. WHEN fixed block size stream, THE FLAC Demuxer SHALL interpret coded number as frame number
10. WHEN variable block size stream, THE FLAC Demuxer SHALL interpret coded number as sample number

### **Requirement 10: Frame Header CRC Validation (RFC 9639 Section 9.1.8)**

**User Story:** As a FLAC parser, I want to validate frame header CRC checksums, so that I can detect corrupted frame headers.

#### **Acceptance Criteria**

1. WHEN frame header is complete, THE FLAC Demuxer SHALL read 8-bit CRC
2. WHEN calculating CRC, THE FLAC Demuxer SHALL use CRC-8 with polynomial 0x07
3. WHEN calculating CRC, THE FLAC Demuxer SHALL include all frame header bytes except CRC itself
4. IF CRC validation fails, THEN THE FLAC Demuxer SHALL log error with frame position
5. WHEN CRC validation fails, THE FLAC Demuxer SHALL attempt resynchronization to next frame
6. WHERE strict mode is enabled, THE FLAC Demuxer SHALL reject stream on CRC failure

### **Requirement 11: Frame Footer CRC Validation (RFC 9639 Section 9.3)**

**User Story:** As a FLAC parser, I want to validate frame footer CRC checksums, so that I can detect corrupted frame data.

#### **Acceptance Criteria**

1. WHEN frame data is complete, THE FLAC Demuxer SHALL ensure byte alignment with zero padding
2. WHEN reading frame footer, THE FLAC Demuxer SHALL read 16-bit CRC
3. WHEN calculating CRC, THE FLAC Demuxer SHALL use CRC-16 with polynomial x^16 + x^15 + x^2 + x^0
4. WHEN calculating CRC, THE FLAC Demuxer SHALL initialize CRC to 0
5. WHEN calculating CRC, THE FLAC Demuxer SHALL include entire frame from sync code to end of subframes
6. IF CRC validation fails, THEN THE FLAC Demuxer SHALL log error with frame position
7. WHEN CRC validation fails, THE FLAC Demuxer SHALL attempt to continue with next frame
8. WHERE strict mode is enabled, THE FLAC Demuxer SHALL reject stream on CRC failure

### **Requirement 12: SEEKTABLE Block Parsing (RFC 9639 Section 8.5)**

**User Story:** As a FLAC parser, I want to parse seek tables correctly, so that I can enable efficient seeking within the stream.

#### **Acceptance Criteria**

1. WHEN encountering SEEKTABLE Block, THE FLAC Demuxer SHALL calculate seek point count as block_length divided by 18
2. WHEN reading seek point, THE FLAC Demuxer SHALL read u(64) sample number
3. WHEN reading seek point, THE FLAC Demuxer SHALL read u(64) byte offset from first frame header
4. WHEN reading seek point, THE FLAC Demuxer SHALL read u(16) number of samples in target frame
5. IF sample number is 0xFFFFFFFFFFFFFFFF, THEN THE FLAC Demuxer SHALL treat seek point as placeholder
6. WHEN storing seek points, THE FLAC Demuxer SHALL verify they are sorted in ascending order by sample number
7. IF seek points are not sorted, THEN THE FLAC Demuxer SHALL log warning but continue processing
8. WHEN using seek points, THE FLAC Demuxer SHALL find closest seek point not exceeding target sample

### **Requirement 13: VORBIS_COMMENT Block Parsing (RFC 9639 Section 8.6)**

**User Story:** As a FLAC parser, I want to parse Vorbis comments correctly with little-endian encoding, so that I can extract metadata tags.

#### **Acceptance Criteria**

1. WHEN reading VORBIS_COMMENT Block, THE FLAC Demuxer SHALL read u(32) little-endian vendor string length
2. WHEN reading vendor string, THE FLAC Demuxer SHALL read UTF-8 string of specified length
3. WHEN reading field count, THE FLAC Demuxer SHALL read u(32) little-endian number of fields
4. WHEN reading each field, THE FLAC Demuxer SHALL read u(32) little-endian field length
5. WHEN reading field content, THE FLAC Demuxer SHALL read UTF-8 string of specified length
6. WHEN parsing field, THE FLAC Demuxer SHALL split on first equals sign into name and value
7. WHEN comparing field names, THE FLAC Demuxer SHALL use case-insensitive comparison
8. WHEN field name contains characters outside 0x20-0x7E or equals 0x3D, THEN THE FLAC Demuxer SHALL reject field

### **Requirement 14: PICTURE Block Parsing (RFC 9639 Section 8.8)**

**User Story:** As a FLAC parser, I want to parse picture blocks correctly, so that I can extract embedded artwork.

#### **Acceptance Criteria**

1. WHEN reading PICTURE Block, THE FLAC Demuxer SHALL read u(32) picture type
2. WHEN reading media type length, THE FLAC Demuxer SHALL read u(32) length in bytes
3. WHEN reading media type, THE FLAC Demuxer SHALL read ASCII string of specified length
4. WHEN reading description length, THE FLAC Demuxer SHALL read u(32) length in bytes
5. WHEN reading description, THE FLAC Demuxer SHALL read UTF-8 string of specified length
6. WHEN reading picture dimensions, THE FLAC Demuxer SHALL read u(32) width in pixels
7. WHEN reading picture dimensions, THE FLAC Demuxer SHALL read u(32) height in pixels
8. WHEN reading color depth, THE FLAC Demuxer SHALL read u(32) bits per pixel
9. WHEN reading indexed colors, THE FLAC Demuxer SHALL read u(32) number of colors
10. WHEN reading picture data length, THE FLAC Demuxer SHALL read u(32) length in bytes
11. WHEN reading picture data, THE FLAC Demuxer SHALL read binary data of specified length
12. IF media type is "-->", THEN THE FLAC Demuxer SHALL treat picture data as URI

### **Requirement 15: Forbidden Pattern Detection (RFC 9639 Section 5, Table 1)**

**User Story:** As a FLAC parser, I want to detect all forbidden patterns per RFC 9639, so that I can reject invalid streams.

#### **Acceptance Criteria**

1. IF metadata block type is 127, THEN THE FLAC Demuxer SHALL reject stream as forbidden
2. IF STREAMINFO minimum block size is less than 16, THEN THE FLAC Demuxer SHALL reject as forbidden
3. IF STREAMINFO maximum block size is less than 16, THEN THE FLAC Demuxer SHALL reject as forbidden
4. IF frame sample rate bits are 0b1111, THEN THE FLAC Demuxer SHALL reject as forbidden
5. IF uncommon block size equals 65536, THEN THE FLAC Demuxer SHALL reject as forbidden

### **Requirement 16: Big-Endian Integer Parsing (RFC 9639 Section 5)**

**User Story:** As a FLAC parser, I want to parse all integers as big-endian except Vorbis comment fields, so that I comply with RFC 9639 encoding rules.

#### **Acceptance Criteria**

1. WHEN reading metadata block lengths, THE FLAC Demuxer SHALL use big-endian byte order
2. WHEN reading STREAMINFO fields, THE FLAC Demuxer SHALL use big-endian byte order
3. WHEN reading frame header fields, THE FLAC Demuxer SHALL use big-endian byte order
4. WHEN reading VORBIS_COMMENT lengths, THE FLAC Demuxer SHALL use little-endian byte order
5. WHEN reading seek point fields, THE FLAC Demuxer SHALL use big-endian byte order

### **Requirement 17: Streamable Subset Compliance (RFC 9639 Section 7)**

**User Story:** As a FLAC parser, I want to detect streamable subset violations, so that I can warn about streams that may not be decodable without seeking.

#### **Acceptance Criteria**

1. IF sample rate bits are 0b0000, THEN THE FLAC Demuxer SHALL mark stream as non-streamable
2. IF bit depth bits are 0b000, THEN THE FLAC Demuxer SHALL mark stream as non-streamable
3. IF maximum block size exceeds 16384, THEN THE FLAC Demuxer SHALL mark stream as non-streamable
4. IF sample rate is 48000 Hz or less and block size exceeds 4608, THEN THE FLAC Demuxer SHALL mark stream as non-streamable
5. WHEN WAVEFORMATEXTENSIBLE_CHANNEL_MASK is present, THE FLAC Demuxer SHALL mark stream as non-streamable

## **Requirements**

### **Requirement 18: Accurate Frame Size Estimation and Efficient Boundary Detection**

**User Story:** As a FLAC codec, I want to receive properly formatted FLAC frame data with minimal I/O overhead, so that I can decode audio samples efficiently even from highly compressed streams.

#### **Acceptance Criteria**

1. WHEN estimating frame sizes, THE FLAC Demuxer SHALL use STREAMINFO minimum frame size as primary estimate
2. WHEN handling fixed block size streams, THE FLAC Demuxer SHALL use minimum frame size directly without scaling
3. WHEN detecting Frame Boundaries, THE FLAC Demuxer SHALL limit search scope to 512 bytes maximum to prevent excessive I/O operations
4. IF Frame Boundary detection fails, THEN THE FLAC Demuxer SHALL use conservative STREAMINFO-based fallback sizes
5. WHEN reading highly compressed frames, THE FLAC Demuxer SHALL handle frames as small as 14 bytes efficiently
6. IF frame errors occur, THEN THE FLAC Demuxer SHALL provide silence output for unrecoverable frames
7. WHEN streaming frames, THE FLAC Demuxer SHALL maintain accurate sample position tracking
8. WHEN providing frame data, THE FLAC Demuxer SHALL include complete frames with proper MediaChunk formatting

### **Requirement 19: Seeking Operations with SEEKTABLE Support**

**User Story:** As a media player, I want to seek to specific timestamps in FLAC files using seek tables, so that users can navigate through audio content efficiently.

#### **Acceptance Criteria**

1. WHEN seeking to beginning, THE FLAC Demuxer SHALL reset to first audio frame efficiently
2. WHEN seeking with SEEKTABLE Block, THE FLAC Demuxer SHALL use seek points for approximate positioning
3. WHEN SEEKTABLE Block is available, THE FLAC Demuxer SHALL find closest seek point not exceeding target sample
4. WHEN seeking without SEEKTABLE Block, THE FLAC Demuxer SHALL acknowledge seeking limitations
5. IF seek operation fails, THEN THE FLAC Demuxer SHALL maintain current position and provide clear error reporting
6. WHEN seeking beyond stream end, THE FLAC Demuxer SHALL clamp to last valid position
7. WHERE frame indexing is implemented, THE FLAC Demuxer SHALL support frame indexing during initial parsing
8. WHEN using seek points, THE FLAC Demuxer SHALL add byte offset to first frame header position

### **Requirement 20: Duration and Position Tracking**

**User Story:** As a media player, I want accurate duration and position information for FLAC files, so that I can display progress and enable seeking.

#### **Acceptance Criteria**

1. WHEN calculating duration, THE FLAC Demuxer SHALL use total samples from STREAMINFO Block
2. IF STREAMINFO Block total samples is 0, THEN THE FLAC Demuxer SHALL treat duration as unknown
3. WHEN tracking position, THE FLAC Demuxer SHALL maintain current sample position accurately using coded numbers
4. WHEN converting to time, THE FLAC Demuxer SHALL divide sample position by sample rate for millisecond conversion
5. WHEN handling variable bitrate, THE FLAC Demuxer SHALL account for compression ratio variations
6. WHEN reporting position, THE FLAC Demuxer SHALL provide both sample-based position and time-based position
7. IF position is unknown, THEN THE FLAC Demuxer SHALL return zero as default value
8. WHEN handling files exceeding 4GB, THE FLAC Demuxer SHALL use 64-bit integers to prevent overflow

### **Requirement 21: Error Handling and Robustness**

**User Story:** As a media player, I want robust error handling in FLAC parsing, so that corrupted or unusual files don't crash the application.

#### **Acceptance Criteria**

1. IF invalid stream markers are encountered, THEN THE FLAC Demuxer SHALL reject non-FLAC files without crashing
2. IF metadata blocks are corrupted, THEN THE FLAC Demuxer SHALL skip damaged blocks and continue processing
3. IF STREAMINFO Block is missing, THEN THE FLAC Demuxer SHALL attempt to derive parameters from frame headers
4. IF frame sync is lost, THEN THE FLAC Demuxer SHALL resynchronize to next valid Sync Code
5. IF CRC errors occur, THEN THE FLAC Demuxer SHALL log errors but attempt to continue playback
6. IF file truncation occurs, THEN THE FLAC Demuxer SHALL handle incomplete files without crashing
7. IF memory allocation fails, THEN THE FLAC Demuxer SHALL return appropriate error codes
8. IF I/O operations fail, THEN THE FLAC Demuxer SHALL handle read errors and EOF conditions without crashing

### **Requirement 22: Performance Optimization Based on Real-World Lessons**

**User Story:** As a media player, I want efficient FLAC processing with minimal I/O overhead, so that highly compressed files can be processed without performance degradation.

#### **Acceptance Criteria**

1. WHEN estimating frame sizes, THE FLAC Demuxer SHALL avoid complex theoretical calculations that produce inaccurate estimates
2. WHEN detecting Frame Boundaries, THE FLAC Demuxer SHALL limit search operations to 512 bytes maximum
3. WHEN processing highly compressed streams, THE FLAC Demuxer SHALL reduce I/O operations to tens per frame
4. WHEN using STREAMINFO Block data, THE FLAC Demuxer SHALL prioritize minimum frame size over complex scaling algorithms
5. WHEN handling frame reading, THE FLAC Demuxer SHALL complete processing in milliseconds
6. WHEN managing memory, THE FLAC Demuxer SHALL use accurate frame size estimates to prevent buffer waste
7. WHEN debugging performance, THE FLAC Demuxer SHALL provide method-specific logging tokens for identification
8. WHEN processing sequential frames, THE FLAC Demuxer SHALL maintain consistent performance across frame types

### **Requirement 23: Integration with Demuxer Architecture**

**User Story:** As a PsyMP3 component, I want the FLAC demuxer to integrate seamlessly with the existing demuxer architecture, so that it works consistently with other format demuxers.

#### **Acceptance Criteria**

1. WHEN implementing the Demuxer interface, THE FLAC Demuxer SHALL provide all required virtual methods
2. WHEN returning stream information, THE FLAC Demuxer SHALL populate StreamInfo with accurate FLAC parameters
3. WHEN providing media chunks, THE FLAC Demuxer SHALL return properly formatted MediaChunk objects with FLAC frames
4. WHEN reporting position, THE FLAC Demuxer SHALL return timestamps in milliseconds consistently
5. WHEN handling IOHandler operations, THE FLAC Demuxer SHALL use the provided IOHandler interface exclusively
6. WHEN logging debug information, THE FLAC Demuxer SHALL use the PsyMP3 Debug logging system
7. WHEN reporting errors, THE FLAC Demuxer SHALL use consistent error codes and messages
8. WHEN working with FLACCodec, THE FLAC Demuxer SHALL provide compatible frame data format

### **Requirement 24: Compatibility with Existing FLAC Implementation**

**User Story:** As a PsyMP3 user, I want the new FLAC demuxer to maintain compatibility with existing FLAC playback, so that my FLAC files continue to work without issues.

#### **Acceptance Criteria**

1. WHEN replacing existing FLAC code, THE FLAC Demuxer SHALL support all previously supported FLAC variants
2. WHEN handling metadata, THE FLAC Demuxer SHALL extract the same metadata fields as the current implementation
3. WHEN seeking, THE FLAC Demuxer SHALL provide at least the same seeking accuracy as current implementation
4. WHEN processing files, THE FLAC Demuxer SHALL handle all FLAC files that currently work
5. WHEN reporting duration, THE FLAC Demuxer SHALL provide consistent duration calculations
6. WHEN handling errors, THE FLAC Demuxer SHALL provide equivalent error handling behavior
7. WHEN integrating with Stream interface, THE FLAC Demuxer SHALL work through DemuxedStream bridge
8. WHEN performance is measured, THE FLAC Demuxer SHALL provide comparable or better performance

### **Requirement 25: Thread Safety Using Public/Private Lock Pattern**

**User Story:** As a multi-threaded media player, I want thread-safe FLAC demuxing operations using proven patterns, so that seeking and reading can occur concurrently without deadlocks.

#### **Acceptance Criteria**

1. WHEN implementing public methods, THE FLAC Demuxer SHALL acquire locks and call private unlocked implementations
2. WHEN calling internal methods, THE FLAC Demuxer SHALL use unlocked versions to prevent deadlocks
3. WHEN multiple locks are needed, THE FLAC Demuxer SHALL acquire locks in documented order to prevent deadlocks
4. WHEN using RAII lock guards, THE FLAC Demuxer SHALL ensure exception safety for all operations
5. WHEN invoking callbacks, THE FLAC Demuxer SHALL release all internal locks before callback invocation
6. WHEN updating position tracking, THE FLAC Demuxer SHALL use atomic operations for sample counters
7. WHEN handling errors, THE FLAC Demuxer SHALL use atomic error state flags for thread-safe propagation
8. WHEN debugging threading issues, THE FLAC Demuxer SHALL provide clear method identification for lock analysis

### **Requirement 26: Debug Logging and Troubleshooting Support**

**User Story:** As a developer debugging FLAC issues, I want comprehensive logging with method identification, so that I can quickly identify which code paths are executing and causing problems.

#### **Acceptance Criteria**

1. WHEN logging debug messages, THE FLAC Demuxer SHALL include method-specific tokens such as [parseStreamInfo]
2. WHEN multiple methods use similar messages, THE FLAC Demuxer SHALL use unique identifiers to distinguish them
3. WHEN frame size estimation occurs, THE FLAC Demuxer SHALL log which calculation method is being used
4. WHEN Frame Boundary detection runs, THE FLAC Demuxer SHALL log search scope and results
5. WHEN seeking operations execute, THE FLAC Demuxer SHALL log strategy selection and outcomes
6. WHEN error recovery activates, THE FLAC Demuxer SHALL log recovery attempts and success or failure status
7. WHEN performance issues occur, THE FLAC Demuxer SHALL provide timing information for critical operations
8. WHEN troubleshooting, THE FLAC Demuxer SHALL log sufficient detail to reproduce and fix issues

