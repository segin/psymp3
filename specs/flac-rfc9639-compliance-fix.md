# FLAC RFC 9639 Compliance Fix Specification

## Overview

This specification addresses critical non-conformance issues in the PsyMP3 FLAC codec and demuxer implementation to achieve full RFC 9639 compliance. The current implementation has several disabled or incomplete validation mechanisms that prevent proper FLAC decoding according to the official specification.

## Problem Statement

The current FLAC implementation has the following critical issues:
1. Frame sync pattern validation is incomplete
2. Block size validation doesn't enforce RFC 9639 minimum requirements
3. CRC validation is completely disabled
4. Channel assignment validation for stereo decorrelation is incomplete
5. Streamable subset compliance is missing
6. Wasted bits handling validation is incomplete
7. Sample format validation may be incomplete
8. Error recovery doesn't follow RFC guidelines

## Goals

- Achieve full RFC 9639 compliance for FLAC decoding
- Re-enable and fix CRC validation
- Implement proper frame boundary detection
- Add comprehensive validation for all FLAC format elements
- Ensure robust error handling per RFC guidelines
- Maintain backward compatibility with existing functionality
- Optimize performance while ensuring correctness

## Requirements

### Functional Requirements

1. **Frame Sync Validation**: Implement exact 15-bit sync pattern validation per RFC 9639 Section 9.1
2. **Block Size Compliance**: Enforce RFC 9639 block size requirements (16-65535 samples, except last frame)
3. **CRC Validation**: Re-enable and fix CRC-16 validation with correct polynomial
4. **Channel Assignment**: Proper validation and handling of stereo decorrelation modes
5. **Streamable Subset**: Add validation for streamable subset constraints
6. **Sample Format**: Complete bit depth and sample format validation
7. **Error Recovery**: Implement RFC-compliant error recovery strategies
8. **Metadata Validation**: Ensure all metadata blocks are parsed per RFC requirements

### Non-Functional Requirements

1. **Performance**: Maintain current decoding performance levels
2. **Memory**: No significant increase in memory usage
3. **Compatibility**: Maintain API compatibility with existing code
4. **Threading**: Preserve thread-safety guarantees
5. **Debugging**: Enhanced logging for RFC compliance validation

## Implementation Plan

### Phase 1: Core Frame Validation (Steps 1-8)

#### Step 1: Implement RFC 9639 Frame Sync Pattern Validation
**File**: `src/FLACCodec.cpp`
**Function**: `validateFrameBoundary_unlocked()`

```cpp
bool FLACCodec::validateFrameSync_unlocked(const uint8_t* data, size_t size) const {
    if (size < 2) {
        return false;
    }
    
    // RFC 9639 Section 9.1: Frame sync code is 0b111111111111100 (15 bits)
    // First byte must be 0xFF
    if (data[0] != 0xFF) {
        return false;
    }
    
    // Second byte must have pattern 0b1111100x (where x is blocking strategy bit)
    if ((data[1] & 0xFE) != 0xF8) {
        return false;
    }
    
    return true;
}
```

#### Step 2: Fix Block Size Validation per RFC 9639
**File**: `src/FLACCodec.cpp`
**Function**: `validateBlockSize_unlocked()`

```cpp
bool FLACCodec::validateBlockSize_unlocked(uint32_t block_size, bool is_last_frame) const {
    // RFC 9639 Section 8.2: Block sizes must be 16-65535 samples
    // Exception: Last frame can be 1-15 samples to match audio length
    
    if (is_last_frame) {
        // Last frame: 1-65535 samples allowed
        return block_size >= 1 && block_size <= 65535;
    } else {
        // All other frames: 16-65535 samples required
        return block_size >= 16 && block_size <= 65535;
    }
}
```

#### Step 3: Implement CRC-16 Calculation per RFC 9639
**File**: `src/FLACCodec.cpp`
**Function**: `calculateFrameFooterCRC_unlocked()`

```cpp
uint16_t FLACCodec::calculateFrameFooterCRC_unlocked(const uint8_t* data, size_t size) const {
    // RFC 9639 Section 9.3: CRC-16 with polynomial x^16 + x^15 + x^2 + x^0
    // Polynomial: 0x8005 (reversed: 0xA001)
    
    uint16_t crc = 0x0000; // Initialize with 0
    
    // Process all bytes except the last 2 (which contain the CRC)
    for (size_t i = 0; i < size - 2; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001; // Reversed polynomial
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}
```

#### Step 4: Re-enable and Fix CRC Validation
**File**: `src/FLACCodec.cpp`
**Function**: `validateFrameCRC_unlocked()`

```cpp
bool FLACCodec::validateFrameCRC_unlocked(const uint8_t* data, size_t size) const {
    if (size < 4) { // Minimum frame size with CRC
        return false;
    }
    
    // Extract CRC from last 2 bytes (big-endian)
    uint16_t expected_crc = (static_cast<uint16_t>(data[size - 2]) << 8) | data[size - 1];
    
    // Calculate actual CRC
    uint16_t calculated_crc = calculateFrameFooterCRC_unlocked(data, size);
    
    return expected_crc == calculated_crc;
}
```

#### Step 5: Implement Channel Assignment Validation
**File**: `src/FLACCodec.cpp`
**Function**: `validateChannelAssignment_unlocked()`

```cpp
bool FLACCodec::validateChannelAssignment_unlocked(const FLAC__Frame* frame) const {
    uint8_t channel_assignment = frame->header.channel_assignment;
    uint8_t channels = frame->header.channels;
    uint8_t bits_per_sample = frame->header.bits_per_sample;
    
    // RFC 9639 Section 9.1.3: Channel assignment validation
    switch (channel_assignment) {
        case 0b0000: // 1 channel: mono
            return channels == 1;
        case 0b0001: // 2 channels: left, right
            return channels == 2;
        case 0b0010: // 3 channels: left, right, center
            return channels == 3;
        case 0b0011: // 4 channels: front left, front right, back left, back right
            return channels == 4;
        case 0b0100: // 5 channels: front left, front right, front center, back left, back right
            return channels == 5;
        case 0b0101: // 6 channels: front left, front right, front center, LFE, back left, back right
            return channels == 6;
        case 0b0110: // 7 channels: front left, front right, front center, LFE, back center, side left, side right
            return channels == 7;
        case 0b0111: // 8 channels: front left, front right, front center, LFE, back left, back right, side left, side right
            return channels == 8;
        case 0b1000: // 2 channels: left, right; stored as left-side stereo
        case 0b1001: // 2 channels: left, right; stored as side-right stereo
        case 0b1010: // 2 channels: left, right; stored as mid-side stereo
            // RFC 9639 Section 4.2: Side channel needs one extra bit
            return channels == 2; // Bit depth validation handled separately
        case 0b1011:
        case 0b1100:
        case 0b1101:
        case 0b1110:
        case 0b1111:
            return false; // Reserved values
        default:
            return false;
    }
}
```

#### Step 6: Implement Stereo Decorrelation Bit Depth Validation
**File**: `src/FLACCodec.cpp`
**Function**: `validateStereoDecorationBitDepth_unlocked()`

```cpp
bool FLACCodec::validateStereoDecorationBitDepth_unlocked(const FLAC__Frame* frame) const {
    uint8_t channel_assignment = frame->header.channel_assignment;
    
    // RFC 9639 Section 4.2: Side channel needs one extra bit of bit depth
    if (channel_assignment >= 0b1000 && channel_assignment <= 0b1010) {
        // For stereo decorrelation modes, validate that we can handle the extra bit
        uint8_t base_bits = frame->header.bits_per_sample;
        
        // Side channel effectively uses base_bits + 1
        if (base_bits >= 32) {
            return false; // Would overflow 32-bit samples
        }
        
        // Ensure our sample buffers can handle the extended range
        return true;
    }
    
    return true; // Not a stereo decorrelation mode
}
```

#### Step 7: Implement Wasted Bits Validation
**File**: `src/FLACCodec.cpp`
**Function**: `validateWastedBitsHandling_unlocked()`

```cpp
bool FLACCodec::validateWastedBitsHandling_unlocked(const FLAC__Frame* frame) const {
    // RFC 9639 Section 9.2.2: Wasted bits validation
    
    for (uint8_t channel = 0; channel < frame->header.channels; ++channel) {
        const FLAC__Subframe* subframe = &frame->subframes[channel];
        
        // Validate wasted bits count
        if (subframe->wasted_bits > frame->header.bits_per_sample) {
            return false; // Cannot waste more bits than available
        }
        
        // For stereo decorrelation, validate wasted bits are applied before restoration
        uint8_t channel_assignment = frame->header.channel_assignment;
        if (channel_assignment >= 0b1000 && channel_assignment <= 0b1010) {
            // Side channels have effective bit depth + 1, wasted bits must account for this
            if (channel == 1 && channel_assignment == 0b1000) { // left-side: side channel
                // Side channel validation
            } else if (channel == 0 && channel_assignment == 0b1001) { // side-right: side channel
                // Side channel validation  
            } else if (channel == 1 && channel_assignment == 0b1010) { // mid-side: side channel
                // Side channel validation
            }
        }
    }
    
    return true;
}
```

#### Step 8: Implement Sample Format Range Validation
**File**: `src/FLACCodec.cpp`
**Function**: `validateSampleFormatRanges_unlocked()`

```cpp
bool FLACCodec::validateSampleFormatRanges_unlocked(const FLAC__int32* samples, 
                                                   size_t sample_count, 
                                                   uint8_t bits_per_sample) const {
    // RFC 9639: Validate sample values are within valid range for bit depth
    
    int32_t min_value = -(1 << (bits_per_sample - 1));
    int32_t max_value = (1 << (bits_per_sample - 1)) - 1;
    
    for (size_t i = 0; i < sample_count; ++i) {
        if (samples[i] < min_value || samples[i] > max_value) {
            Debug::log("flac_codec", "[validateSampleFormatRanges_unlocked] Sample ", i, 
                      " value ", samples[i], " outside valid range [", min_value, ", ", max_value, "]");
            return false;
        }
    }
    
    return true;
}
```

### Phase 2: Streamable Subset Compliance (Steps 9-12)

#### Step 9: Implement Streamable Subset Validation
**File**: `src/FLACCodec.cpp`
**Function**: `validateStreamableSubset_unlocked()`

```cpp
bool FLACCodec::validateStreamableSubset_unlocked(const FLAC__Frame* frame) const {
    // RFC 9639 Section 7: Streamable subset restrictions
    
    uint32_t sample_rate = frame->header.sample_rate;
    uint32_t block_size = frame->header.blocksize;
    
    // Maximum block size must not be larger than 16384
    if (block_size > 16384) {
        return false;
    }
    
    // Audio with sample rate <= 48000 Hz must not use blocks > 4608 samples
    if (sample_rate <= 48000 && block_size > 4608) {
        return false;
    }
    
    // Sample rate bits must be 0b0001-0b1110 (not reference STREAMINFO)
    // This validation should be done during frame header parsing
    
    // Bit depth bits must be 0b001-0b111 (not reference STREAMINFO)
    // This validation should be done during frame header parsing
    
    return true;
}
```

#### Step 10: Add Frame Header Streamable Subset Validation
**File**: `src/FLACDemuxer.cpp`
**Function**: `validateFrameHeaderStreamable_unlocked()`

```cpp
bool FLACDemuxer::validateFrameHeaderStreamable_unlocked(const uint8_t* header_data) const {
    // RFC 9639 Section 7: Frame header must not reference STREAMINFO for streamable subset
    
    if (!header_data || !m_streamable_subset_required) {
        return true; // Not enforcing streamable subset
    }
    
    // Extract sample rate bits (4 bits starting at bit 20)
    uint8_t sample_rate_bits = (header_data[2] >> 4) & 0x0F;
    if (sample_rate_bits == 0b0000) {
        return false; // References STREAMINFO for sample rate
    }
    
    // Extract bit depth bits (3 bits starting at bit 1 of 4th byte)
    uint8_t bit_depth_bits = (header_data[3] >> 1) & 0x07;
    if (bit_depth_bits == 0b000) {
        return false; // References STREAMINFO for bit depth
    }
    
    return true;
}
```

#### Step 11: Implement Block Size Constraint Validation
**File**: `src/FLACCodec.cpp`
**Function**: `validateBlockSizeConstraints_unlocked()`

```cpp
bool FLACCodec::validateBlockSizeConstraints_unlocked(uint32_t block_size, 
                                                     uint32_t sample_rate,
                                                     bool enforce_streamable_subset) const {
    // RFC 9639 basic constraints
    if (block_size < 1 || block_size > 65535) {
        return false;
    }
    
    // Streamable subset additional constraints
    if (enforce_streamable_subset) {
        if (block_size > 16384) {
            return false;
        }
        
        if (sample_rate <= 48000 && block_size > 4608) {
            return false;
        }
    }
    
    // STREAMINFO constraints (if available)
    if (m_streaminfo_available) {
        if (block_size < m_min_block_size || block_size > m_max_block_size) {
            // Allow last frame to be smaller than minimum
            if (!m_is_last_frame || block_size > m_max_block_size) {
                return false;
            }
        }
    }
    
    return true;
}
```

#### Step 12: Add Streamable Subset Configuration
**File**: `src/FLACCodec.h` and `src/FLACCodec.cpp`

```cpp
// Add to class declaration
private:
    bool m_enforce_streamable_subset = false;
    bool m_streaminfo_available = false;
    uint32_t m_min_block_size = 16;
    uint32_t m_max_block_size = 65535;

public:
    void setStreamableSubsetMode(bool enforce) {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_enforce_streamable_subset = enforce;
    }
    
    bool getStreamableSubsetMode() const {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        return m_enforce_streamable_subset;
    }
```

### Phase 3: Enhanced Error Handling (Steps 13-16)

#### Step 13: Implement RFC-Compliant Error Recovery
**File**: `src/FLACCodec.cpp`
**Function**: `handleRFCCompliantError_unlocked()`

```cpp
bool FLACCodec::handleRFCCompliantError_unlocked(FLAC__StreamDecoderErrorStatus error_status,
                                                const uint8_t* frame_data,
                                                size_t frame_size) {
    // RFC 9639 compliant error handling strategies
    
    switch (error_status) {
        case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
            // RFC allows recovery by finding next sync pattern
            return recoverFromLostSync_unlocked(frame_data, frame_size);
            
        case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
            // RFC allows skipping bad frame and continuing
            return skipBadFrame_unlocked();
            
        case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
            // RFC allows using data despite CRC mismatch for error resilience
            if (m_strict_crc_validation) {
                return false; // Reject frame in strict mode
            } else {
                // Log error but continue with frame data
                logCRCMismatch_unlocked(frame_data, frame_size);
                return true; // Use frame despite CRC error
            }
            
        case FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM:
            // RFC indicates fundamental format violation - cannot recover
            return false;
            
        default:
            // Unknown error - conservative approach
            return false;
    }
}
```

#### Step 14: Implement Sync Pattern Recovery
**File**: `src/FLACCodec.cpp`
**Function**: `recoverFromLostSync_unlocked()`

```cpp
bool FLACCodec::recoverFromLostSync_unlocked(const uint8_t* data, size_t size) {
    // RFC 9639 Section 9.1: Search for next valid sync pattern
    
    for (size_t offset = 1; offset < size - 1; ++offset) {
        if (validateFrameSync_unlocked(data + offset, size - offset)) {
            // Found potential sync pattern
            Debug::log("flac_codec", "[recoverFromLostSync_unlocked] Found sync pattern at offset ", offset);
            
            // Update decoder position to skip corrupted data
            if (m_decoder) {
                // Clear input buffer and feed data from sync position
                m_decoder->clearInputBuffer();
                if (m_decoder->feedData(data + offset, size - offset)) {
                    return true;
                }
            }
        }
    }
    
    Debug::log("flac_codec", "[recoverFromLostSync_unlocked] No valid sync pattern found");
    return false;
}
```

#### Step 15: Implement CRC Mismatch Logging
**File**: `src/FLACCodec.cpp`
**Function**: `logCRCMismatch_unlocked()`

```cpp
void FLACCodec::logCRCMismatch_unlocked(const uint8_t* frame_data, size_t frame_size) {
    if (frame_size < 4) return;
    
    uint16_t expected_crc = (static_cast<uint16_t>(frame_data[frame_size - 2]) << 8) | 
                           frame_data[frame_size - 1];
    uint16_t calculated_crc = calculateFrameFooterCRC_unlocked(frame_data, frame_size);
    
    Debug::log("flac_codec", "[logCRCMismatch_unlocked] RFC 9639 CRC-16 mismatch: expected=0x", 
              std::hex, expected_crc, ", calculated=0x", calculated_crc, std::dec);
    
    // Update statistics
    m_stats.crc_errors++;
    
    // Check if we should disable CRC validation due to excessive errors
    if (m_crc_error_threshold > 0 && m_stats.crc_errors >= m_crc_error_threshold) {
        m_crc_validation_disabled_due_to_errors = true;
        Debug::log("flac_codec", "[logCRCMismatch_unlocked] Disabling CRC validation due to excessive errors (", 
                  m_stats.crc_errors, " >= ", m_crc_error_threshold, ")");
    }
}
```

#### Step 16: Add Error Context Logging
**File**: `src/FLACCodec.cpp`
**Function**: `logRFCViolation_unlocked()`

```cpp
void FLACCodec::logRFCViolation_unlocked(const std::string& rfc_section,
                                        const std::string& violation_type,
                                        const std::string& details,
                                        uint64_t sample_position) {
    Debug::log("flac_codec", "[RFC 9639 VIOLATION] Section ", rfc_section, 
              ": ", violation_type, " - ", details, " at sample ", sample_position);
    
    // Update violation statistics
    m_stats.rfc_violations++;
    
    // Store violation for debugging
    if (m_rfc_violations.size() < MAX_STORED_VIOLATIONS) {
        RFCViolation violation;
        violation.section = rfc_section;
        violation.type = violation_type;
        violation.details = details;
        violation.sample_position = sample_position;
        violation.timestamp = std::chrono::steady_clock::now();
        
        m_rfc_violations.push_back(violation);
    }
}
```

### Phase 4: Metadata and Frame Processing (Steps 17-20)

#### Step 17: Fix Frame Boundary Detection
**File**: `src/FLACDemuxer.cpp`
**Function**: `findNextFrame_unlocked()`

```cpp
bool FLACDemuxer::findNextFrame_unlocked(FLACFrame& frame) {
    // RFC 9639 compliant frame boundary detection
    
    if (!m_handler) {
        return false;
    }
    
    const size_t SEARCH_BUFFER_SIZE = 8192;
    std::vector<uint8_t> search_buffer(SEARCH_BUFFER_SIZE);
    
    while (!isEOF_unlocked()) {
        // Read data for sync pattern search
        size_t bytes_read = m_handler->read(search_buffer.data(), 1, SEARCH_BUFFER_SIZE);
        if (bytes_read < 2) {
            break; // Not enough data for sync pattern
        }
        
        // Search for sync pattern
        for (size_t i = 0; i < bytes_read - 1; ++i) {
            if (search_buffer[i] == 0xFF && (search_buffer[i + 1] & 0xFE) == 0xF8) {
                // Found potential sync pattern
                uint64_t sync_position = m_current_offset + i;
                
                // Seek to sync position
                if (m_handler->seek(static_cast<off_t>(sync_position), SEEK_SET) == 0) {
                    m_current_offset = sync_position;
                    
                    // Try to parse frame header
                    if (parseFrameHeader_unlocked(frame)) {
                        return true;
                    }
                }
            }
        }
        
        // Move to next search position (overlap to avoid missing sync patterns)
        if (bytes_read >= 2) {
            uint64_t next_position = m_current_offset + bytes_read - 1;
            if (m_handler->seek(static_cast<off_t>(next_position), SEEK_SET) == 0) {
                m_current_offset = next_position;
            } else {
                break;
            }
        } else {
            break;
        }
    }
    
    return false;
}
```

#### Step 18: Implement Complete Frame Header Parsing
**File**: `src/FLACDemuxer.cpp`
**Function**: `parseFrameHeader_unlocked()`

```cpp
bool FLACDemuxer::parseFrameHeader_unlocked(FLACFrame& frame) {
    // RFC 9639 Section 9.1: Complete frame header parsing
    
    const size_t MAX_HEADER_SIZE = 16; // Conservative estimate
    std::vector<uint8_t> header_data(MAX_HEADER_SIZE);
    
    size_t bytes_read = m_handler->read(header_data.data(), 1, MAX_HEADER_SIZE);
    if (bytes_read < 4) {
        return false; // Minimum header size
    }
    
    size_t bit_offset = 0;
    
    // Validate sync pattern (15 bits)
    if (!validateFrameSync_unlocked(header_data.data(), bytes_read)) {
        return false;
    }
    bit_offset += 15;
    
    // Parse blocking strategy (1 bit)
    bool variable_block_size = (header_data[1] & 0x01) != 0;
    bit_offset += 1;
    
    // Parse block size bits (4 bits)
    uint8_t block_size_bits = (header_data[2] >> 4) & 0x0F;
    bit_offset += 4;
    
    // Parse sample rate bits (4 bits)
    uint8_t sample_rate_bits = header_data[2] & 0x0F;
    bit_offset += 4;
    
    // Parse channel assignment (4 bits)
    uint8_t channel_assignment = (header_data[3] >> 4) & 0x0F;
    bit_offset += 4;
    
    // Parse bit depth bits (3 bits)
    uint8_t bit_depth_bits = (header_data[3] >> 1) & 0x07;
    bit_offset += 3;
    
    // Reserved bit (1 bit) - must be 0
    if ((header_data[3] & 0x01) != 0) {
        return false; // RFC violation
    }
    bit_offset += 1;
    
    // Parse coded number (variable length)
    uint64_t coded_number;
    size_t coded_number_bytes;
    if (!parseCodedNumber_unlocked(header_data.data() + 4, bytes_read - 4, 
                                  coded_number, coded_number_bytes)) {
        return false;
    }
    bit_offset += coded_number_bytes * 8;
    
    // Parse uncommon block size if needed
    uint32_t block_size;
    size_t uncommon_size_bytes = 0;
    if (!parseBlockSize_unlocked(block_size_bits, header_data.data() + 4 + coded_number_bytes,
                                bytes_read - 4 - coded_number_bytes, block_size, uncommon_size_bytes)) {
        return false;
    }
    bit_offset += uncommon_size_bytes * 8;
    
    // Parse uncommon sample rate if needed
    uint32_t sample_rate;
    size_t uncommon_rate_bytes = 0;
    if (!parseSampleRate_unlocked(sample_rate_bits, header_data.data() + 4 + coded_number_bytes + uncommon_size_bytes,
                                 bytes_read - 4 - coded_number_bytes - uncommon_size_bytes, 
                                 sample_rate, uncommon_rate_bytes)) {
        return false;
    }
    bit_offset += uncommon_rate_bytes * 8;
    
    // Parse and validate header CRC
    size_t header_size_bytes = (bit_offset + 7) / 8 + 1; // +1 for CRC byte
    if (header_size_bytes > bytes_read) {
        return false; // Not enough data
    }
    
    uint8_t expected_crc = header_data[header_size_bytes - 1];
    uint8_t calculated_crc = calculateHeaderCRC_unlocked(header_data.data(), header_size_bytes - 1);
    
    if (expected_crc != calculated_crc) {
        Debug::log("flac", "[parseFrameHeader_unlocked] Header CRC mismatch: expected=0x", 
                  std::hex, static_cast<int>(expected_crc), ", calculated=0x", 
                  static_cast<int>(calculated_crc), std::dec);
        return false;
    }
    
    // Fill frame structure
    frame.file_offset = m_current_offset;
    frame.block_size = block_size;
    frame.sample_rate = sample_rate;
    frame.channels = getChannelCount_unlocked(channel_assignment);
    frame.bits_per_sample = getBitDepth_unlocked(bit_depth_bits);
    frame.channel_assignment = channel_assignment;
    frame.variable_block_size = variable_block_size;
    
    if (variable_block_size) {
        frame.sample_offset = coded_number;
    } else {
        frame.sample_offset = coded_number * block_size;
    }
    
    // Validate frame parameters
    if (!validateFrameParameters_unlocked(frame)) {
        return false;
    }
    
    // Update current offset to end of header
    m_current_offset += header_size_bytes;
    
    return true;
}
```

#### Step 19: Implement Header CRC Calculation
**File**: `src/FLACDemuxer.cpp`
**Function**: `calculateHeaderCRC_unlocked()`

```cpp
uint8_t FLACDemuxer::calculateHeaderCRC_unlocked(const uint8_t* data, size_t size) const {
    // RFC 9639 Section 9.1.8: Header CRC-8 with polynomial x^8 + x^2 + x^1 + x^0
    // Polynomial: 0x07
    
    uint8_t crc = 0x00; // Initialize with 0
    
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
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

#### Step 20: Add Comprehensive Frame Validation
**File**: `src/FLACDemuxer.cpp`
**Function**: `validateFrameParameters_unlocked()`

```cpp
bool FLACDemuxer::validateFrameParameters_unlocked(const FLACFrame& frame) const {
    // RFC 9639 comprehensive frame parameter validation
    
    // Validate block size
    if (!validateBlockSize_unlocked(frame.block_size, false)) { // Assume not last frame for now
        Debug::log("flac", "[validateFrameParameters_unlocked] Invalid block size: ", frame.block_size);
        return false;
    }
    
    // Validate sample rate
    if (frame.sample_rate == 0 || frame.sample_rate > 1048575) {
        Debug::log("flac", "[validateFrameParameters_unlocked] Invalid sample rate: ", frame.sample_rate);
        return false;
    }
    
    // Validate channel count
    if (frame.channels < 1 || frame.channels > 8) {
        Debug::log("flac", "[validateFrameParameters_unlocked] Invalid channel count: ", frame.channels);
        return false;
    }
    
    // Validate bit depth
    if (frame.bits_per_sample < 4 || frame.bits_per_sample > 32) {
        Debug::log("flac", "[validateFrameParameters_unlocked] Invalid bit depth: ", frame.bits_per_sample);
        return false;
    }
    
    // Validate channel assignment
    if (!validateChannelAssignmentValue_unlocked(frame.channel_assignment, frame.channels)) {
        Debug::log("flac", "[validateFrameParameters_unlocked] Invalid channel assignment: ", 
                  static_cast<int>(frame.channel_assignment), " for ", frame.channels, " channels");
        return false;
    }
    
    // Validate consistency with STREAMINFO if available
    if (m_streaminfo.isValid()) {
        if (frame.sample_rate != m_streaminfo.sample_rate) {
            Debug::log("flac", "[validateFrameParameters_unlocked] Sample rate mismatch with STREAMINFO: ", 
                      frame.sample_rate, " vs ", m_streaminfo.sample_rate);
            return false;
        }
        
        if (frame.channels != m_streaminfo.channels) {
            Debug::log("flac", "[validateFrameParameters_unlocked] Channel count mismatch with STREAMINFO: ", 
                      frame.channels, " vs ", m_streaminfo.channels);
            return false;
        }
        
        if (frame.bits_per_sample != m_streaminfo.bits_per_sample) {
            Debug::log("flac", "[validateFrameParameters_unlocked] Bit depth mismatch with STREAMINFO: ", 
                      frame.bits_per_sample, " vs ", m_streaminfo.bits_per_sample);
            return false;
        }
    }
    
    return true;
}
```

### Phase 5: Integration and Testing (Steps 21-25)

#### Step 21: Update Main Decode Function
**File**: `src/FLACCodec.cpp`
**Function**: `processFrameData_unlocked()`

Update the main frame processing function to use all new validation methods:

```cpp
// Replace existing CRC validation with:
if (shouldValidateCRC_unlocked() && size >= 4) {
    if (!validateFrameCRC_unlocked(data, size)) {
        if (!handleRFCCompliantError_unlocked(FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH, data, size)) {
            return false;
        }
    }
}

// Add comprehensive frame validation before processing:
if (!validateFrameBoundary_unlocked(data, size)) {
    return handleRFCCompliantError_unlocked(FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER, data, size);
}
```

#### Step 22: Update Write Callback Validation
**File**: `src/FLACCodec.cpp`
**Function**: `handleWriteCallback_unlocked()`

Add all new validation calls:

```cpp
// Add after existing parameter validation:
if (!validateChannelAssignment_unlocked(frame)) {
    logRFCViolation_unlocked("9.1.3", "Invalid channel assignment", 
                            "Channel assignment validation failed", frame->header.number.sample_number);
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
}

if (!validateStereoDecorationBitDepth_unlocked(frame)) {
    logRFCViolation_unlocked("4.2", "Stereo decorrelation bit depth error", 
                            "Side channel bit depth validation failed", frame->header.number.sample_number);
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
}

if (!validateWastedBitsHandling_unlocked(frame)) {
    logRFCViolation_unlocked("9.2.2", "Wasted bits validation failed", 
                            "Wasted bits handling validation failed", frame->header.number.sample_number);
    // Continue processing - wasted bits errors are often recoverable
}

if (m_enforce_streamable_subset && !validateStreamableSubset_unlocked(frame)) {
    logRFCViolation_unlocked("7", "Streamable subset violation", 
                            "Frame violates streamable subset constraints", frame->header.number.sample_number);
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
}
```

#### Step 23: Add Configuration Methods
**File**: `src/FLACCodec.h` and `src/FLACCodec.cpp`

Add public methods for configuration:

```cpp
// Public configuration methods
void enableRFC9639StrictMode(bool strict = true);
void setStreamableSubsetMode(bool enforce = true);
void setCRCValidationMode(bool enabled = true, bool strict = false);
void setErrorRecoveryMode(bool enabled = true);

// Statistics and debugging
struct RFC9639Stats {
    size_t rfc_violations = 0;
    size_t crc_errors = 0;
    size_t sync_recoveries = 0;
    size_t frames_validated = 0;
    size_t validation_failures = 0;
};

RFC9639Stats getRFC9639Stats() const;
std::vector<RFCViolation> getRFCViolations() const;
void clearRFCViolations();
```

#### Step 24: Add Unit Tests
**File**: `tests/test_flac_rfc9639_compliance.cpp`

```cpp
#include "gtest/gtest.h"
#include "FLACCodec.h"
#include "FLACDemuxer.h"

class FLACRfc9639ComplianceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test fixtures
    }
};

TEST_F(FLACRfc9639ComplianceTest, FrameSyncValidation) {
    // Test frame sync pattern validation
    uint8_t valid_sync[] = {0xFF, 0xF8, 0x00, 0x00};
    uint8_t invalid_sync[] = {0xFF, 0xF7, 0x00, 0x00};
    
    FLACCodec codec(StreamInfo{});
    // Test validation methods
}

TEST_F(FLACRfc9639ComplianceTest, BlockSizeValidation) {
    // Test block size validation per RFC 9639
}

TEST_F(FLACRfc9639ComplianceTest, CRCValidation) {
    // Test CRC-16 calculation and validation
}

TEST_F(FLACRfc9639ComplianceTest, ChannelAssignmentValidation) {
    // Test channel assignment validation
}

TEST_F(FLACRfc9639ComplianceTest, StreamableSubsetCompliance) {
    // Test streamable subset constraints
}
```

#### Step 25: Update Documentation
**File**: `docs/FLAC_RFC9639_COMPLIANCE.md`

```markdown
# FLAC RFC 9639 Compliance

This document describes the RFC 9639 compliance features implemented in the PsyMP3 FLAC codec.

## Compliance Features

### Frame Validation
- 15-bit sync pattern validation per Section 9.1
- Block size validation per Section 8.2 and 9.1.1
- CRC-16 validation per Section 9.3
- Channel assignment validation per Section 9.1.3

### Streamable Subset Support
- Block size constraints per Section 7
- Frame header independence validation
- Sample rate and bit depth constraints

### Error Recovery
- RFC-compliant error handling strategies
- Sync pattern recovery
- CRC mismatch handling options

## Configuration

### Strict Mode
```cpp
codec.enableRFC9639StrictMode(true);
```

### Streamable Subset Mode
```cpp
codec.setStreamableSubsetMode(true);
```

### CRC Validation
```cpp
codec.setCRCValidationMode(true, false); // enabled, not strict
```

## Statistics and Debugging

The codec provides detailed statistics about RFC compliance:

```cpp
auto stats = codec.getRFC9639Stats();
auto violations = codec.getRFCViolations();
```
```

## Success Criteria

1. **All CRC validation re-enabled and working correctly**
2. **Frame sync pattern detection working reliably**
3. **Block size validation enforcing RFC 9639 constraints**
4. **Channel assignment validation handling stereo decorrelation**
5. **Streamable subset mode available and functional**
6. **Error recovery following RFC guidelines**
7. **Comprehensive unit test coverage**
8. **No regression in decoding performance**
9. **All existing functionality preserved**
10. **Documentation updated and complete**

## Implementation Notes

- Maintain thread-safety throughout all changes
- Use existing threading patterns (public/private lock pattern)
- Preserve API compatibility where possible
- Add comprehensive logging for debugging
- Follow existing code style and conventions
- Ensure all validation is optional/configurable for compatibility

## Testing Strategy

1. **Unit tests** for each validation function
2. **Integration tests** with real FLAC files
3. **Performance benchmarks** to ensure no regression
4. **Compliance tests** against RFC 9639 examples
5. **Error injection tests** for recovery mechanisms
6. **Thread safety tests** for concurrent access

This specification provides a comprehensive roadmap to achieve full RFC 9639 compliance while maintaining the existing functionality and performance characteristics of the FLAC implementation.