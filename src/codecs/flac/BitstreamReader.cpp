#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

BitstreamReader::BitstreamReader()
    : m_byte_position(0)
    , m_bit_position(0)
    , m_bit_cache(0)
    , m_cache_bits(0)
    , m_total_bits_read(0)
{
}

BitstreamReader::~BitstreamReader()
{
}

void BitstreamReader::feedData(const uint8_t* data, size_t size)
{
    if (data && size > 0) {
        m_buffer.insert(m_buffer.end(), data, data + size);
    }
}

void BitstreamReader::clearBuffer()
{
    m_buffer.clear();
    m_byte_position = 0;
    m_bit_position = 0;
    m_bit_cache = 0;
    m_cache_bits = 0;
    m_total_bits_read = 0;
}

void BitstreamReader::discardReadBytes()
{
    // Remove bytes that have already been read from the buffer
    // This helps manage memory for long streams
    if (m_byte_position > 0) {
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + m_byte_position);
        m_byte_position = 0;
    }
}

size_t BitstreamReader::getBufferSize() const
{
    return m_buffer.size();
}

size_t BitstreamReader::getAvailableBits() const
{
    size_t bytes_remaining = m_buffer.size() - m_byte_position;
    size_t bits_remaining = (bytes_remaining * 8) + m_cache_bits;
    
    // Subtract any partial byte we're in the middle of
    if (m_bit_position > 0 && bytes_remaining > 0) {
        bits_remaining -= m_bit_position;
    }
    
    return bits_remaining;
}

size_t BitstreamReader::getAvailableBytes() const
{
    return m_buffer.size() - m_byte_position;
}

bool BitstreamReader::hasData() const
{
    return m_cache_bits > 0 || m_byte_position < m_buffer.size();
}

bool BitstreamReader::canRead(uint32_t bit_count) const
{
    return getAvailableBits() >= bit_count;
}

void BitstreamReader::refillCache()
{
    // Fill cache with bytes from buffer (big-endian)
    while (m_cache_bits <= 56 && m_byte_position < m_buffer.size()) {
        uint64_t byte = m_buffer[m_byte_position++];
        m_bit_cache = (m_bit_cache << 8) | byte;
        m_cache_bits += 8;
    }
}

bool BitstreamReader::ensureBits(uint32_t bit_count)
{
    if (bit_count > 64) {
        return false;  // Cannot read more than 64 bits at once
    }
    
    if (m_cache_bits < bit_count) {
        refillCache();
    }
    
    return m_cache_bits >= bit_count;
}

uint32_t BitstreamReader::peekBits(uint32_t bit_count)
{
    // Extract bits from cache (big-endian: MSB first)
    uint32_t shift = m_cache_bits - bit_count;
    uint64_t mask = (1ULL << bit_count) - 1;
    return static_cast<uint32_t>((m_bit_cache >> shift) & mask);
}

void BitstreamReader::consumeBits(uint32_t bit_count)
{
    m_cache_bits -= bit_count;
    m_total_bits_read += bit_count;
    
    // Clear consumed bits from cache
    uint64_t mask = (1ULL << m_cache_bits) - 1;
    m_bit_cache &= mask;
}

bool BitstreamReader::readBits(uint32_t& value, uint32_t bit_count)
{
    if (bit_count == 0) {
        value = 0;
        return true;
    }
    
    if (bit_count > 32) {
        return false;  // Cannot read more than 32 bits into uint32_t
    }
    
    if (!ensureBits(bit_count)) {
        return false;  // Not enough data
    }
    
    value = peekBits(bit_count);
    consumeBits(bit_count);
    
    return true;
}

bool BitstreamReader::readBitsSigned(int32_t& value, uint32_t bit_count)
{
    if (bit_count == 0) {
        value = 0;
        return true;
    }
    
    if (bit_count > 32) {
        return false;
    }
    
    uint32_t unsigned_value;
    if (!readBits(unsigned_value, bit_count)) {
        return false;
    }
    
    // Apply sign extension if MSB is set
    if (bit_count < 32 && (unsigned_value & (1U << (bit_count - 1)))) {
        // Sign extend: set all bits above bit_count to 1
        uint32_t sign_extend_mask = ~((1U << bit_count) - 1);
        unsigned_value |= sign_extend_mask;
    }
    
    value = static_cast<int32_t>(unsigned_value);
    return true;
}

bool BitstreamReader::readBit(bool& value)
{
    uint32_t bit;
    if (!readBits(bit, 1)) {
        return false;
    }
    value = (bit != 0);
    return true;
}

bool BitstreamReader::alignToByte()
{
    // Calculate bits needed to reach next byte boundary
    uint32_t bits_to_skip = (8 - (m_total_bits_read % 8)) % 8;
    
    if (bits_to_skip > 0) {
        // Verify padding bits are zero (per RFC 9639)
        if (!ensureBits(bits_to_skip)) {
            return false;
        }
        
        uint32_t padding = peekBits(bits_to_skip);
        if (padding != 0) {
            // Non-zero padding bits - this is technically invalid
            // but we'll be lenient and skip them anyway
        }
        
        consumeBits(bits_to_skip);
    }
    
    return true;
}

bool BitstreamReader::isAligned() const
{
    return (m_total_bits_read % 8) == 0;
}

bool BitstreamReader::skipBits(uint32_t bit_count)
{
    if (!ensureBits(bit_count)) {
        return false;
    }
    
    consumeBits(bit_count);
    return true;
}

uint64_t BitstreamReader::getBitPosition() const
{
    return m_total_bits_read;
}

uint64_t BitstreamReader::getBytePosition() const
{
    return m_total_bits_read / 8;
}

void BitstreamReader::resetPosition()
{
    // Reset reading position to beginning of buffer
    m_byte_position = 0;
    m_bit_position = 0;
    m_bit_cache = 0;
    m_cache_bits = 0;
    m_total_bits_read = 0;
}

int32_t BitstreamReader::unfoldSigned(uint32_t folded)
{
    // Zigzag decoding: even values are positive, odd values are negative
    // 0 -> 0, 1 -> -1, 2 -> 1, 3 -> -2, 4 -> 2, etc.
    if (folded & 1) {
        return -static_cast<int32_t>((folded + 1) >> 1);
    } else {
        return static_cast<int32_t>(folded >> 1);
    }
}

// Unary decoding: count leading zeros until we hit a 1 bit
bool BitstreamReader::readUnary(uint32_t& value)
{
    value = 0;
    
    // Read bits one at a time until we find a 1
    while (true) {
        if (!ensureBits(1)) {
            return false;  // Ran out of data
        }
        
        uint32_t bit = peekBits(1);
        consumeBits(1);
        
        if (bit == 1) {
            break;  // Found the terminating 1
        }
        
        value++;
        
        // DoS protection: prevent infinite loops on corrupted data (Requirement 48)
        // Per ValidationUtils::MAX_UNARY_VALUE
        if (value > 1000000) {
            Debug::log("flac_codec", "Unary value exceeds maximum (DoS protection): %u", value);
            return false;  // Unreasonably large unary value - potential DoS attack
        }
    }
    
    return true;
}

// UTF-8 coded number decoding per RFC 9639
bool BitstreamReader::readUTF8(uint64_t& value)
{
    // Read first byte to determine length
    if (!ensureBits(8)) {
        return false;
    }
    
    uint32_t first_byte = peekBits(8);
    
    // Determine number of bytes based on leading bits
    if ((first_byte & 0x80) == 0) {
        // 0xxxxxxx - 1 byte (7 bits)
        return readUTF8_1byte(value);
    } else if ((first_byte & 0xE0) == 0xC0) {
        // 110xxxxx - 2 bytes (11 bits)
        return readUTF8_2byte(value);
    } else if ((first_byte & 0xF0) == 0xE0) {
        // 1110xxxx - 3 bytes (16 bits)
        return readUTF8_3byte(value);
    } else if ((first_byte & 0xF8) == 0xF0) {
        // 11110xxx - 4 bytes (21 bits)
        return readUTF8_4byte(value);
    } else if ((first_byte & 0xFC) == 0xF8) {
        // 111110xx - 5 bytes (26 bits)
        return readUTF8_5byte(value);
    } else if ((first_byte & 0xFE) == 0xFC) {
        // 1111110x - 6 bytes (31 bits)
        return readUTF8_6byte(value);
    } else if (first_byte == 0xFE) {
        // 11111110 - 7 bytes (36 bits)
        return readUTF8_7byte(value);
    } else {
        // Invalid UTF-8 pattern
        return false;
    }
}

bool BitstreamReader::readUTF8_1byte(uint64_t& value)
{
    uint32_t byte;
    if (!readBits(byte, 8)) {
        return false;
    }
    
    value = byte & 0x7F;
    return true;
}

bool BitstreamReader::readUTF8_2byte(uint64_t& value)
{
    uint32_t byte1, byte2;
    
    if (!readBits(byte1, 8) || !readBits(byte2, 8)) {
        return false;
    }
    
    // Validate continuation byte
    if ((byte2 & 0xC0) != 0x80) {
        return false;
    }
    
    value = ((byte1 & 0x1F) << 6) | (byte2 & 0x3F);
    return true;
}

bool BitstreamReader::readUTF8_3byte(uint64_t& value)
{
    uint32_t byte1, byte2, byte3;
    
    if (!readBits(byte1, 8) || !readBits(byte2, 8) || !readBits(byte3, 8)) {
        return false;
    }
    
    // Validate continuation bytes
    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80) {
        return false;
    }
    
    value = ((byte1 & 0x0F) << 12) | ((byte2 & 0x3F) << 6) | (byte3 & 0x3F);
    return true;
}

bool BitstreamReader::readUTF8_4byte(uint64_t& value)
{
    uint32_t byte1, byte2, byte3, byte4;
    
    if (!readBits(byte1, 8) || !readBits(byte2, 8) || 
        !readBits(byte3, 8) || !readBits(byte4, 8)) {
        return false;
    }
    
    // Validate continuation bytes
    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80 || (byte4 & 0xC0) != 0x80) {
        return false;
    }
    
    value = ((byte1 & 0x07) << 18) | ((byte2 & 0x3F) << 12) | 
            ((byte3 & 0x3F) << 6) | (byte4 & 0x3F);
    return true;
}

bool BitstreamReader::readUTF8_5byte(uint64_t& value)
{
    uint32_t byte1, byte2, byte3, byte4, byte5;
    
    if (!readBits(byte1, 8) || !readBits(byte2, 8) || 
        !readBits(byte3, 8) || !readBits(byte4, 8) || !readBits(byte5, 8)) {
        return false;
    }
    
    // Validate continuation bytes
    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80 || 
        (byte4 & 0xC0) != 0x80 || (byte5 & 0xC0) != 0x80) {
        return false;
    }
    
    value = ((byte1 & 0x03) << 24) | ((byte2 & 0x3F) << 18) | 
            ((byte3 & 0x3F) << 12) | ((byte4 & 0x3F) << 6) | (byte5 & 0x3F);
    return true;
}

bool BitstreamReader::readUTF8_6byte(uint64_t& value)
{
    uint32_t byte1, byte2, byte3, byte4, byte5, byte6;
    
    if (!readBits(byte1, 8) || !readBits(byte2, 8) || 
        !readBits(byte3, 8) || !readBits(byte4, 8) || 
        !readBits(byte5, 8) || !readBits(byte6, 8)) {
        return false;
    }
    
    // Validate continuation bytes
    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80 || 
        (byte4 & 0xC0) != 0x80 || (byte5 & 0xC0) != 0x80 || (byte6 & 0xC0) != 0x80) {
        return false;
    }
    
    value = ((byte1 & 0x01) << 30) | ((byte2 & 0x3F) << 24) | 
            ((byte3 & 0x3F) << 18) | ((byte4 & 0x3F) << 12) | 
            ((byte5 & 0x3F) << 6) | (byte6 & 0x3F);
    return true;
}

bool BitstreamReader::readUTF8_7byte(uint64_t& value)
{
    uint32_t byte1, byte2, byte3, byte4, byte5, byte6, byte7;
    
    if (!readBits(byte1, 8) || !readBits(byte2, 8) || 
        !readBits(byte3, 8) || !readBits(byte4, 8) || 
        !readBits(byte5, 8) || !readBits(byte6, 8) || !readBits(byte7, 8)) {
        return false;
    }
    
    // Validate continuation bytes
    if ((byte2 & 0xC0) != 0x80 || (byte3 & 0xC0) != 0x80 || 
        (byte4 & 0xC0) != 0x80 || (byte5 & 0xC0) != 0x80 || 
        (byte6 & 0xC0) != 0x80 || (byte7 & 0xC0) != 0x80) {
        return false;
    }
    
    value = ((uint64_t)(byte2 & 0x3F) << 30) | ((byte3 & 0x3F) << 24) | 
            ((byte4 & 0x3F) << 18) | ((byte5 & 0x3F) << 12) | 
            ((byte6 & 0x3F) << 6) | (byte7 & 0x3F);
    return true;
}

// Rice code decoding per RFC 9639
bool BitstreamReader::readRiceCode(int32_t& value, uint32_t rice_param)
{
    // Rice code consists of:
    // 1. Unary-coded quotient (q)
    // 2. Binary-coded remainder (r) using rice_param bits
    // Result: value = (q << rice_param) | r
    
    // Read unary quotient
    uint32_t quotient;
    if (!readUnary(quotient)) {
        return false;
    }
    
    // Read binary remainder
    uint32_t remainder;
    if (!readBits(remainder, rice_param)) {
        return false;
    }
    
    // Combine quotient and remainder
    uint32_t folded = (quotient << rice_param) | remainder;
    
    // Apply zigzag decoding to get signed value
    value = unfoldSigned(folded);
    
    return true;
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3
