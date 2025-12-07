#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

/**
 * FrameParser Implementation
 * 
 * This file implements FLAC frame parsing per RFC 9639 Section 9.
 * 
 * FORBIDDEN PATTERNS (Requirement 23):
 * The following bit patterns are explicitly forbidden by RFC 9639 and must be rejected:
 * 
 * 1. Metadata block type = 127 (0x7F)
 *    - Checked in MetadataParser (not implemented yet)
 * 
 * 2. Sample rate bits = 0b1111
 *    - Checked in parseFrameHeader() via checkForbiddenSampleRateBits()
 * 
 * 3. Uncommon block size = 65536
 *    - Checked in parseUncommonBlockSize() via checkForbiddenBlockSize()
 * 
 * 4. Predictor coefficient precision bits = 0b1111
 *    - Will be checked in SubframeDecoder (not implemented yet)
 * 
 * 5. Negative predictor right shift
 *    - Will be checked in SubframeDecoder (not implemented yet)
 * 
 * 6. Reserved subframe types
 *    - Will be checked in SubframeDecoder (not implemented yet)
 * 
 * All forbidden patterns result in immediate frame rejection.
 */

FrameParser::FrameParser(BitstreamReader* reader, CRCValidator* crc)
    : m_reader(reader)
    , m_crc(crc)
    , m_last_sync_position(0)
    , m_computing_header_crc(false)
    , m_computed_header_crc(0)
{
}

FrameParser::~FrameParser()
{
}

/**
 * Find frame sync pattern (0xFFF8-0xFFFF)
 * Per RFC 9639 Section 9: Frame sync is 14 bits of 1 followed by 2 reserved bits
 * Valid sync patterns: 0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE, 0xFFFF
 * Sync must be on byte boundary
 * 
 * NOTE: The demuxer already provides chunks starting at the sync code.
 * This function verifies the sync is present and positions the reader correctly.
 */
bool FrameParser::findSync()
{
    // Ensure we're on a byte boundary
    if (!m_reader->isAligned()) {
        if (!m_reader->alignToByte()) {
            return false;
        }
    }
    
    
    // Search for sync pattern
    // We need to find 0xFF followed by 0xF8-0xFF
    while (m_reader->getAvailableBits() >= 16) {
        // Save position before reading potential sync
        uint64_t sync_start_bit_position = m_reader->getBitPosition();
        
        // Read potential sync word (16 bits)
        uint32_t sync_word = 0;
        if (!m_reader->readBits(sync_word, 16)) {
            return false;
        }
        
        // Check if it's a valid sync pattern (0xFFF8-0xFFFF)
        // Per RFC 9639 Section 9.1: sync code is 14 bits of 1 followed by 2 bits
        if ((sync_word & 0xFFF8) == 0xFFF8) {
            // Found sync pattern!
            // We need to restore position to the START of the sync code
            // so that parseFrameHeader() can read and validate it
            
            // Reset to beginning and advance to sync start position
            m_reader->resetPosition();
            
            // Advance to the sync start position (in bytes)
            uint64_t bytes_to_skip = sync_start_bit_position / 8;
            for (uint64_t i = 0; i < bytes_to_skip; i++) {
                uint32_t dummy;
                if (!m_reader->readBits(dummy, 8)) {
                    return false;
                }
            }
            
            m_last_sync_position = m_reader->getBitPosition();
            Debug::log("flac_codec", "Found frame sync at bit position ", 
                      static_cast<unsigned long long>(m_last_sync_position));
            return true;
        }
        
        // Not a valid sync pattern
        // Instead of continuing byte-by-byte, back up to one byte after sync_start
        // to check if the second byte could be the start of a new sync
        m_reader->resetPosition();
        
        // Advance to one byte after where we started checking
        uint64_t next_check_position = (sync_start_bit_position / 8) + 1;
        for (uint64_t i = 0; i < next_check_position; i++) {
            uint32_t dummy;
            if (!m_reader->readBits(dummy, 8)) {
                break;
            }
        }
    }
    
    // Sync not found
    Debug::log("flac_codec", "Frame sync not found");
    return false;
}


/**
 * Parse frame header
 * Per RFC 9639 Section 9
 */
bool FrameParser::parseFrameHeader(FrameHeader& header)
{
    // Reset CRC computation
    m_crc->resetCRC8();
    m_computing_header_crc = true;
    
    // Read sync code + reserved bit + blocking strategy (16 bits total)
    // Per RFC 9639 Section 9.1:
    // - 14 bits: sync pattern (all 1s)
    // - 1 bit: reserved (must be 0)
    // - 1 bit: blocking strategy (0=fixed, 1=variable)
    uint32_t sync = 0;
    if (!m_reader->readBits(sync, 16)) {
        Debug::log("flac_codec", "Failed to read frame sync");
        return false;
    }
    
    Debug::log("flac_codec", "[parseFrameHeader] Read sync word: 0x", std::hex, sync, std::dec);
    
    // Validate sync code: top 14 bits must be 1, reserved bit must be 0
    // 0xFFFC = 0b1111111111111100 (14 ones, then 00)
    // Valid patterns: 0xFFF8 (fixed blocking) or 0xFFF9 (variable blocking)
    if ((sync & 0xFFFC) != 0xFFF8) {
        Debug::log("flac_codec", "Invalid frame sync: 0x", std::hex, sync, std::dec);
        return false;
    }
    
    // Extract blocking strategy from the LSB of the sync word
    // 0 = fixed block size, 1 = variable block size
    uint32_t blocking_strategy = sync & 0x0001;
    header.is_variable_block_size = (blocking_strategy == 1);
    
    // Update CRC with sync bytes
    m_crc->updateCRC8((sync >> 8) & 0xFF);
    m_crc->updateCRC8(sync & 0xFF);
    
    // Read block size bits (4 bits)
    uint32_t block_size_bits = 0;
    if (!m_reader->readBits(block_size_bits, 4)) {
        Debug::log("flac_codec", "Failed to read block size bits");
        return false;
    }
    
    // Read sample rate bits (4 bits)
    uint32_t sample_rate_bits = 0;
    if (!m_reader->readBits(sample_rate_bits, 4)) {
        Debug::log("flac_codec", "Failed to read sample rate bits");
        return false;
    }
    
    Debug::log("flac_codec", "[parseFrameHeader] blocking_strategy=", blocking_strategy,
              ", block_size_bits=0b", std::hex, block_size_bits, std::dec,
              ", sample_rate_bits=0b", std::hex, sample_rate_bits, std::dec);
    
    // Check for forbidden sample rate bits (Requirement 23)
    if (!checkForbiddenSampleRateBits(sample_rate_bits)) {
        Debug::log("flac_codec", "Forbidden sample rate bits detected: 0b%04b", sample_rate_bits);
        return false;
    }
    
    // Update CRC with first byte after sync
    uint8_t byte1 = (blocking_strategy << 7) | (block_size_bits << 4) | sample_rate_bits;
    m_crc->updateCRC8(byte1);
    
    // Read channel assignment (4 bits)
    uint32_t channel_bits = 0;
    if (!m_reader->readBits(channel_bits, 4)) {
        Debug::log("flac_codec", "Failed to read channel assignment");
        return false;
    }
    
    // Read sample size bits (3 bits)
    uint32_t bit_depth_bits = 0;
    if (!m_reader->readBits(bit_depth_bits, 3)) {
        Debug::log("flac_codec", "Failed to read bit depth bits");
        return false;
    }
    
    // Read reserved bit (1 bit) - must be 0
    uint32_t reserved = 0;
    if (!m_reader->readBits(reserved, 1)) {
        Debug::log("flac_codec", "Failed to read reserved bit");
        return false;
    }
    
    if (reserved != 0) {
        Debug::log("flac_codec", "Reserved bit is not 0");
        return false;
    }
    
    // Update CRC with second byte
    uint8_t byte2 = (channel_bits << 4) | (bit_depth_bits << 1) | reserved;
    m_crc->updateCRC8(byte2);
    
    // Parse coded number (UTF-8 encoded frame or sample number)
    if (!parseCodedNumber(header)) {
        Debug::log("flac_codec", "Failed to parse coded number");
        return false;
    }
    
    // Handle block size lookup
    // NOTE: Validation moved to after value is set
    if (block_size_bits == 0b0110) {
        if (!parseUncommonBlockSize(header, 8)) {
            return false;
        }
    } else if (block_size_bits == 0b0111) {
        if (!parseUncommonBlockSize(header, 16)) {
            return false;
        }
    } else {
        // Standard block size lookup
        static const uint32_t block_size_table[] = {
            0,     // 0000: reserved
            192,   // 0001: 192
            576,   // 0010: 576
            1152,  // 0011: 1152
            2304,  // 0100: 2304
            4608,  // 0101: 4608
            0,     // 0110: 8-bit follows
            0,     // 0111: 16-bit follows
            256,   // 1000: 256
            512,   // 1001: 512
            1024,  // 1010: 1024
            2048,  // 1011: 2048
            4096,  // 1100: 4096
            8192,  // 1101: 8192
            16384, // 1110: 16384
            32768  // 1111: 32768
        };
        
        header.block_size = block_size_table[block_size_bits];
        if (header.block_size == 0 && block_size_bits != 0b0110 && block_size_bits != 0b0111) {
            Debug::log("flac_codec", "Reserved block size bits: 0x", std::hex, block_size_bits, std::dec);
            return false;
        }
    }
    
    // Validate block size (now that it's set)
    if (!parseBlockSize(header)) {
        Debug::log("flac_codec", "Invalid block size: ", header.block_size);
        return false;
    }
    
    // Handle uncommon sample rate
    if (sample_rate_bits == 0b1100) {
        if (!parseUncommonSampleRate(header, 8)) {
            return false;
        }
    } else if (sample_rate_bits == 0b1101) {
        if (!parseUncommonSampleRate(header, 16)) {
            return false;
        }
    } else if (sample_rate_bits == 0b1110) {
        if (!parseUncommonSampleRate(header, 16)) {
            // Sample rate in Hz/10
            header.sample_rate *= 10;
            return true;
        }
    } else {
        // Standard sample rate lookup
        static const uint32_t sample_rate_table[] = {
            0,      // 0000: get from STREAMINFO
            88200,  // 0001: 88.2 kHz
            176400, // 0010: 176.4 kHz
            192000, // 0011: 192 kHz
            8000,   // 0100: 8 kHz
            16000,  // 0101: 16 kHz
            22050,  // 0110: 22.05 kHz
            24000,  // 0111: 24 kHz
            32000,  // 1000: 32 kHz
            44100,  // 1001: 44.1 kHz
            48000,  // 1010: 48 kHz
            96000,  // 1011: 96 kHz
            0,      // 1100: 8-bit kHz follows
            0,      // 1101: 16-bit Hz follows
            0,      // 1110: 16-bit Hz/10 follows
            0       // 1111: forbidden
        };
        
        header.sample_rate = sample_rate_table[sample_rate_bits];
    }
    
    // Decode channel assignment
    if (channel_bits <= 7) {
        // Independent channels (0-7 = 1-8 channels)
        header.channels = channel_bits + 1;
        header.channel_assignment = ChannelAssignment::INDEPENDENT;
    } else if (channel_bits == 8) {
        // Left-side stereo
        header.channels = 2;
        header.channel_assignment = ChannelAssignment::LEFT_SIDE;
    } else if (channel_bits == 9) {
        // Right-side stereo
        header.channels = 2;
        header.channel_assignment = ChannelAssignment::RIGHT_SIDE;
    } else if (channel_bits == 10) {
        // Mid-side stereo
        header.channels = 2;
        header.channel_assignment = ChannelAssignment::MID_SIDE;
    } else {
        // Reserved (11-15)
        Debug::log("flac_codec", "Reserved channel assignment: ", channel_bits);
        return false;
    }
    
    // Validate channels (now that it's set)
    if (!parseChannels(header)) {
        Debug::log("flac_codec", "Invalid channels: ", header.channels);
        return false;
    }
    
    // Decode bit depth
    static const uint32_t bit_depth_table[] = {
        0,  // 000: get from STREAMINFO
        8,  // 001: 8 bits
        12, // 010: 12 bits
        0,  // 011: reserved
        16, // 100: 16 bits
        20, // 101: 20 bits
        24, // 110: 24 bits
        0   // 111: reserved
    };
    
    header.bit_depth = bit_depth_table[bit_depth_bits];
    if (header.bit_depth == 0 && bit_depth_bits != 0) {
        Debug::log("flac_codec", "Reserved bit depth bits: 0x", std::hex, bit_depth_bits, std::dec);
        return false;
    }
    
    // Validate bit depth (now that it's set)
    if (!parseBitDepth(header)) {
        Debug::log("flac_codec", "Invalid bit depth: ", header.bit_depth);
        return false;
    }
    
    // Read CRC-8
    uint32_t crc8 = 0;
    if (!m_reader->readBits(crc8, 8)) {
        Debug::log("flac_codec", "Failed to read CRC-8");
        return false;
    }
    header.crc8 = static_cast<uint8_t>(crc8);
    
    // Validate CRC-8
    m_computed_header_crc = m_crc->getCRC8();
    if (m_computed_header_crc != header.crc8) {
        Debug::log("flac_codec", "Frame header CRC-8 mismatch: computed=0x%02X, expected=0x%02X",
                  m_computed_header_crc, header.crc8);
        return false;
    }
    
    m_computing_header_crc = false;
    
    Debug::log("flac_codec", "Parsed frame header: block_size=%u, sample_rate=%u, channels=%u, bit_depth=%u",
              header.block_size, header.sample_rate, header.channels, header.bit_depth);
    
    return true;
}

bool FrameParser::parseBlockSize(FrameHeader& header)
{
    // Block size is already parsed in parseFrameHeader
    return validateBlockSize(header.block_size);
}

bool FrameParser::parseSampleRate(FrameHeader& header)
{
    // Sample rate is already parsed in parseFrameHeader
    return validateSampleRate(header.sample_rate);
}

bool FrameParser::parseChannels(FrameHeader& header)
{
    // Channels are already parsed in parseFrameHeader
    return (header.channels >= 1 && header.channels <= 8);
}

bool FrameParser::parseBitDepth(FrameHeader& header)
{
    // Bit depth is already parsed in parseFrameHeader
    return validateBitDepth(header.bit_depth);
}


/**
 * Parse UTF-8 coded number (frame or sample number)
 * Per RFC 9639 Section 9: Uses UTF-8-like encoding for variable-length integers
 * 
 * 1 byte:  0xxxxxxx                           (7 bits, 0-127)
 * 2 bytes: 110xxxxx 10xxxxxx                  (11 bits, 128-2047)
 * 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx         (16 bits, 2048-65535)
 * 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx (21 bits, 65536-2097151)
 * 5 bytes: 111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (26 bits)
 * 6 bytes: 1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (31 bits)
 * 7 bytes: 11111110 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx (36 bits)
 */
bool FrameParser::parseCodedNumber(FrameHeader& header)
{
    // Read first byte to determine length
    uint32_t first_byte = 0;
    if (!m_reader->readBits(first_byte, 8)) {
        Debug::log("flac_codec", "Failed to read coded number first byte");
        return false;
    }
    
    // Update CRC
    m_crc->updateCRC8(static_cast<uint8_t>(first_byte));
    
    uint64_t value = 0;
    uint32_t num_bytes = 0;
    
    // Determine number of bytes and extract initial bits
    if ((first_byte & 0x80) == 0) {
        // 1 byte: 0xxxxxxx
        num_bytes = 1;
        value = first_byte & 0x7F;
    } else if ((first_byte & 0xE0) == 0xC0) {
        // 2 bytes: 110xxxxx
        num_bytes = 2;
        value = first_byte & 0x1F;
    } else if ((first_byte & 0xF0) == 0xE0) {
        // 3 bytes: 1110xxxx
        num_bytes = 3;
        value = first_byte & 0x0F;
    } else if ((first_byte & 0xF8) == 0xF0) {
        // 4 bytes: 11110xxx
        num_bytes = 4;
        value = first_byte & 0x07;
    } else if ((first_byte & 0xFC) == 0xF8) {
        // 5 bytes: 111110xx
        num_bytes = 5;
        value = first_byte & 0x03;
    } else if ((first_byte & 0xFE) == 0xFC) {
        // 6 bytes: 1111110x
        num_bytes = 6;
        value = first_byte & 0x01;
    } else if (first_byte == 0xFE) {
        // 7 bytes: 11111110
        num_bytes = 7;
        value = 0;
    } else {
        Debug::log("flac_codec", "Invalid UTF-8 coded number first byte: 0x%02X", first_byte);
        return false;
    }
    
    // Read continuation bytes (10xxxxxx)
    for (uint32_t i = 1; i < num_bytes; i++) {
        uint32_t cont_byte = 0;
        if (!m_reader->readBits(cont_byte, 8)) {
            Debug::log("flac_codec", "Failed to read coded number continuation byte %u", i);
            return false;
        }
        
        // Update CRC
        m_crc->updateCRC8(static_cast<uint8_t>(cont_byte));
        
        // Validate continuation byte format (must be 10xxxxxx)
        if ((cont_byte & 0xC0) != 0x80) {
            Debug::log("flac_codec", "Invalid UTF-8 continuation byte: 0x%02X", cont_byte);
            return false;
        }
        
        // Extract 6 bits and add to value
        value = (value << 6) | (cont_byte & 0x3F);
    }
    
    header.coded_number = value;
    
    Debug::log("flac_codec", "Parsed coded number: %llu (%u bytes)",
              (unsigned long long)value, num_bytes);
    
    return true;
}


/**
 * Parse uncommon block size
 * Per RFC 9639 Section 9:
 * - 8-bit: block size - 1 (for 0b0110)
 * - 16-bit: block size - 1 (for 0b0111)
 * Forbidden: 65536 (would be encoded as 65535 + 1)
 */
bool FrameParser::parseUncommonBlockSize(FrameHeader& header, uint32_t bits)
{
    uint32_t size_minus_one = 0;
    if (!m_reader->readBits(size_minus_one, bits)) {
        Debug::log("flac_codec", "Failed to read uncommon block size (%u bits)", bits);
        return false;
    }
    
    // Update CRC
    if (bits == 8) {
        m_crc->updateCRC8(static_cast<uint8_t>(size_minus_one));
    } else if (bits == 16) {
        m_crc->updateCRC8(static_cast<uint8_t>((size_minus_one >> 8) & 0xFF));
        m_crc->updateCRC8(static_cast<uint8_t>(size_minus_one & 0xFF));
    }
    
    header.block_size = size_minus_one + 1;
    
    // Check for forbidden block size (Requirement 23)
    if (!checkForbiddenBlockSize(header.block_size)) {
        Debug::log("flac_codec", "Forbidden block size detected: %u", header.block_size);
        return false;
    }
    
    // Validate: block size must be at least 16 (except for last frame)
    if (header.block_size < 16) {
        Debug::log("flac_codec", "Invalid block size: %u (must be >= 16)", header.block_size);
        return false;
    }
    
    Debug::log("flac_codec", "Parsed uncommon block size: %u", header.block_size);
    return true;
}

/**
 * Parse uncommon sample rate
 * Per RFC 9639 Section 9:
 * - 0b1100: 8-bit sample rate in kHz
 * - 0b1101: 16-bit sample rate in Hz
 * - 0b1110: 16-bit sample rate in Hz/10 (multiply by 10)
 * - 0b1111: forbidden
 */
bool FrameParser::parseUncommonSampleRate(FrameHeader& header, uint32_t bits)
{
    uint32_t rate = 0;
    if (!m_reader->readBits(rate, bits)) {
        Debug::log("flac_codec", "Failed to read uncommon sample rate (%u bits)", bits);
        return false;
    }
    
    // Update CRC
    if (bits == 8) {
        m_crc->updateCRC8(static_cast<uint8_t>(rate));
        // Rate is in kHz, convert to Hz
        header.sample_rate = rate * 1000;
    } else if (bits == 16) {
        m_crc->updateCRC8(static_cast<uint8_t>((rate >> 8) & 0xFF));
        m_crc->updateCRC8(static_cast<uint8_t>(rate & 0xFF));
        // Rate is in Hz (or Hz/10, caller will multiply by 10 if needed)
        header.sample_rate = rate;
    }
    
    // Validate sample rate is non-zero
    if (header.sample_rate == 0) {
        Debug::log("flac_codec", "Invalid sample rate: 0");
        return false;
    }
    
    Debug::log("flac_codec", "Parsed uncommon sample rate: %u Hz", header.sample_rate);
    return true;
}


/**
 * Parse frame footer
 * Per RFC 9639 Section 9: After all subframes, align to byte boundary with zero padding,
 * then read 16-bit CRC-16
 */
bool FrameParser::parseFrameFooter(FrameFooter& footer)
{
    // Align to byte boundary
    if (!m_reader->isAligned()) {
        // Read padding bits and verify they are zero
        uint32_t bit_position = m_reader->getBitPosition() % 8;
        if (bit_position != 0) {
            uint32_t padding_bits = 8 - bit_position;
            uint32_t padding = 0;
            
            if (!m_reader->readBits(padding, padding_bits)) {
                Debug::log("flac_codec", "Failed to read padding bits");
                return false;
            }
            
            if (padding != 0) {
                Debug::log("flac_codec", "Warning: Non-zero padding bits: 0x%X", padding);
                // Per RFC 9639, non-zero padding may indicate corruption but we continue
            }
        }
    }
    
    // Read CRC-16 (big-endian)
    uint32_t crc16 = 0;
    if (!m_reader->readBits(crc16, 16)) {
        Debug::log("flac_codec", "Failed to read frame CRC-16");
        return false;
    }
    
    footer.crc16 = static_cast<uint16_t>(crc16);
    
    Debug::log("flac_codec", "Parsed frame footer: CRC-16=0x%04X", footer.crc16);
    return true;
}

/**
 * Validate complete frame
 * Compares computed CRC-16 against frame footer CRC
 */
bool FrameParser::validateFrame(const FrameHeader& header, const FrameFooter& footer)
{
    // Get computed CRC-16
    uint16_t computed_crc = m_crc->getCRC16();
    
    if (computed_crc != footer.crc16) {
        Debug::log("flac_codec", "Frame CRC-16 mismatch: computed=0x%04X, expected=0x%04X",
                  computed_crc, footer.crc16);
        // Per RFC 9639, CRC-16 failure is a warning but decoder may continue
        return false;
    }
    
    Debug::log("flac_codec", "Frame CRC-16 validated successfully");
    return true;
}

// Validation helper functions
bool FrameParser::validateBlockSize(uint32_t block_size) const
{
    // Per RFC 9639: block size must be 16-65535 (except last frame can be smaller)
    if (block_size < 16 || block_size > 65535) {
        Debug::log("flac_codec", "Invalid block size: %u (must be 16-65535)", block_size);
        return false;
    }
    return true;
}

bool FrameParser::validateSampleRate(uint32_t sample_rate) const
{
    // Sample rate can be 0 (get from STREAMINFO) or 1-1048575 Hz
    // 0 is valid (means use STREAMINFO)
    if (sample_rate > 1048575) {
        Debug::log("flac_codec", "Invalid sample rate: %u (max 1048575 Hz)", sample_rate);
        return false;
    }
    return true;
}

bool FrameParser::validateBitDepth(uint32_t bit_depth) const
{
    // Bit depth can be 0 (get from STREAMINFO) or 4-32 bits
    // 0 is valid (means use STREAMINFO)
    if (bit_depth > 32) {
        Debug::log("flac_codec", "Invalid bit depth: %u (max 32 bits)", bit_depth);
        return false;
    }
    return true;
}

/**
 * Check for forbidden sample rate bits pattern
 * Per RFC 9639 and Requirement 23: 0b1111 is forbidden
 */
bool FrameParser::checkForbiddenSampleRateBits(uint32_t sample_rate_bits) const
{
    if (sample_rate_bits == 0b1111) {
        Debug::log("flac_codec", "Forbidden pattern: sample rate bits = 0b1111");
        return false;
    }
    return true;
}

/**
 * Check for forbidden block size
 * Per RFC 9639 and Requirement 23: 65536 is forbidden
 */
bool FrameParser::checkForbiddenBlockSize(uint32_t block_size) const
{
    if (block_size == 65536) {
        Debug::log("flac_codec", "Forbidden pattern: block size = 65536");
        return false;
    }
    return true;
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3
