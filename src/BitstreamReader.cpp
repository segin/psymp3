#include "psymp3.h"

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
