#ifndef FRAMEPARSER_H
#define FRAMEPARSER_H

#include <cstdint>

// Forward declarations
class BitstreamReader;
class CRCValidator;

/**
 * Channel assignment modes for FLAC frames
 * Per RFC 9639 Section 9.1
 */
enum class ChannelAssignment {
    INDEPENDENT = 0,    // Independent channels (1-8 channels)
    LEFT_SIDE = 8,      // Left-side stereo (left, side)
    RIGHT_SIDE = 9,     // Right-side stereo (side, right)
    MID_SIDE = 10       // Mid-side stereo (mid, side)
};

/**
 * FLAC frame header structure
 * Per RFC 9639 Section 9
 */
struct FrameHeader {
    // Sync code (0xFFF8-0xFFFF) - validated but not stored
    
    // Blocking strategy
    bool is_variable_block_size;  // false = fixed, true = variable
    
    // Block size in samples
    uint32_t block_size;
    
    // Sample rate in Hz
    uint32_t sample_rate;
    
    // Number of channels (1-8)
    uint32_t channels;
    
    // Channel assignment mode
    ChannelAssignment channel_assignment;
    
    // Bits per sample (4-32)
    uint32_t bit_depth;
    
    // Frame or sample number (UTF-8 coded)
    uint64_t coded_number;  // frame number if fixed, sample number if variable
    
    // CRC-8 of frame header
    uint8_t crc8;
    
    // Constructor
    FrameHeader() 
        : is_variable_block_size(false)
        , block_size(0)
        , sample_rate(0)
        , channels(0)
        , channel_assignment(ChannelAssignment::INDEPENDENT)
        , bit_depth(0)
        , coded_number(0)
        , crc8(0)
    {}
};

/**
 * FLAC frame footer structure
 * Per RFC 9639 Section 9
 */
struct FrameFooter {
    // CRC-16 of entire frame (excluding CRC itself)
    uint16_t crc16;
    
    // Constructor
    FrameFooter() : crc16(0) {}
};

/**
 * FrameParser - Parses FLAC frame headers and footers
 * 
 * This class handles:
 * - Frame sync detection (0xFFF8-0xFFFF pattern)
 * - Frame header parsing with all fields
 * - Frame footer parsing
 * - CRC validation for headers and frames
 * 
 * Per RFC 9639 Section 9
 */
class FrameParser {
public:
    /**
     * Constructor
     * @param reader BitstreamReader for reading frame data
     * @param crc CRCValidator for validating frame integrity
     */
    FrameParser(BitstreamReader* reader, CRCValidator* crc);
    
    /**
     * Destructor
     */
    ~FrameParser();
    
    /**
     * Find frame sync pattern (0xFFF8-0xFFFF)
     * Searches for sync code on byte boundary
     * @return true if sync found, false otherwise
     */
    bool findSync();
    
    /**
     * Parse frame header
     * Reads and validates all frame header fields
     * @param header Output frame header structure
     * @return true if header parsed successfully, false on error
     */
    bool parseFrameHeader(FrameHeader& header);
    
    /**
     * Parse frame footer
     * Reads frame CRC-16 after byte alignment
     * @param footer Output frame footer structure
     * @return true if footer parsed successfully, false on error
     */
    bool parseFrameFooter(FrameFooter& footer);
    
    /**
     * Validate complete frame
     * Checks frame CRC-16 against computed value
     * @param header Frame header
     * @param footer Frame footer
     * @return true if frame is valid, false if CRC mismatch
     */
    bool validateFrame(const FrameHeader& header, const FrameFooter& footer);
    
private:
    BitstreamReader* m_reader;
    CRCValidator* m_crc;
    
    // Frame header parsing helpers
    bool parseBlockSize(FrameHeader& header);
    bool parseSampleRate(FrameHeader& header);
    bool parseChannels(FrameHeader& header);
    bool parseBitDepth(FrameHeader& header);
    bool parseCodedNumber(FrameHeader& header);
    
    // Uncommon parameter handling
    bool parseUncommonBlockSize(FrameHeader& header, uint32_t bits);
    bool parseUncommonSampleRate(FrameHeader& header, uint32_t bits);
    
    // Validation helpers
    bool validateBlockSize(uint32_t block_size) const;
    bool validateSampleRate(uint32_t sample_rate) const;
    bool validateBitDepth(uint32_t bit_depth) const;
    
    // Forbidden pattern detection (Requirement 23)
    bool checkForbiddenSampleRateBits(uint32_t sample_rate_bits) const;
    bool checkForbiddenBlockSize(uint32_t block_size) const;
    
    // Sync detection state
    uint64_t m_last_sync_position;
    
    // CRC computation state
    bool m_computing_header_crc;
    uint8_t m_computed_header_crc;
};

#endif // FRAMEPARSER_H
