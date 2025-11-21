#ifndef BITSTREAMREADER_H
#define BITSTREAMREADER_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

/**
 * BitstreamReader - Efficient bit-level reading from byte-aligned input stream
 * 
 * Provides bit-level access to FLAC bitstream with support for:
 * - Variable-length bit field reading
 * - Unary-coded values
 * - UTF-8 coded numbers
 * - Rice/Golomb codes
 * - Byte alignment
 * 
 * Uses big-endian bit ordering per RFC 9639.
 * Maintains 64-bit cache for efficient multi-bit reads.
 */
class BitstreamReader {
public:
    BitstreamReader();
    ~BitstreamReader();
    
    // Input management
    void feedData(const uint8_t* data, size_t size);
    void clearBuffer();
    void discardReadBytes();  // Remove already-read bytes from buffer
    size_t getAvailableBits() const;
    size_t getAvailableBytes() const;
    size_t getBufferSize() const;
    
    // Basic bit reading
    bool readBits(uint32_t& value, uint32_t bit_count);
    bool readBitsSigned(int32_t& value, uint32_t bit_count);
    bool readBit(bool& value);
    
    // Special encoding readers
    bool readUnary(uint32_t& value);
    bool readUTF8(uint64_t& value);
    bool readRiceCode(int32_t& value, uint32_t rice_param);
    
    // Alignment
    bool alignToByte();
    bool isAligned() const;
    bool skipBits(uint32_t bit_count);
    
    // Position tracking
    uint64_t getBitPosition() const;
    uint64_t getBytePosition() const;
    void resetPosition();
    
    // State queries
    bool hasData() const;
    bool canRead(uint32_t bit_count) const;
    
private:
    // Input buffer
    std::vector<uint8_t> m_buffer;
    size_t m_byte_position;      // Current byte position in buffer
    uint32_t m_bit_position;     // Bit position within current byte (0-7)
    
    // Bit cache for efficient reading (big-endian)
    uint64_t m_bit_cache;        // Cached bits for reading
    uint32_t m_cache_bits;       // Number of valid bits in cache
    
    // Total bits read (for position tracking)
    uint64_t m_total_bits_read;
    
    // Internal helpers
    void refillCache();
    bool ensureBits(uint32_t bit_count);
    uint32_t peekBits(uint32_t bit_count);
    void consumeBits(uint32_t bit_count);
    
    // UTF-8 decoding helpers
    bool readUTF8_1byte(uint64_t& value);
    bool readUTF8_2byte(uint64_t& value);
    bool readUTF8_3byte(uint64_t& value);
    bool readUTF8_4byte(uint64_t& value);
    bool readUTF8_5byte(uint64_t& value);
    bool readUTF8_6byte(uint64_t& value);
    bool readUTF8_7byte(uint64_t& value);
    
    // Rice code helpers
    int32_t unfoldSigned(uint32_t folded);
};

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // BITSTREAMREADER_H
