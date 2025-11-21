// CRCValidator.h - CRC-8 and CRC-16 validation for FLAC frames
// Implements RFC 9639 CRC validation with polynomials:
// - CRC-8: 0x07 (x^8 + x^2 + x^1 + x^0)
// - CRC-16: 0x8005 (x^16 + x^15 + x^2 + x^0)
//
// References:
// - RFC 9639: FLAC specification
// - RFC 3385: Internet Protocol Packet Length Assurance (CRC background)

#ifndef CRCVALIDATOR_H
#define CRCVALIDATOR_H

#include <cstdint>
#include <cstddef>

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

/**
 * CRCValidator provides CRC-8 and CRC-16 checksum computation and validation
 * for FLAC frame integrity checking per RFC 9639.
 * 
 * CRC-8 is used for frame headers to enable quick rejection of invalid frames.
 * CRC-16 is used for complete frames to detect corruption in frame data.
 * 
 * Both CRC algorithms use precomputed lookup tables for performance.
 */
class CRCValidator {
public:
    CRCValidator();
    ~CRCValidator() = default;
    
    // Prevent copying (use references instead)
    CRCValidator(const CRCValidator&) = delete;
    CRCValidator& operator=(const CRCValidator&) = delete;
    
    // Allow moving
    CRCValidator(CRCValidator&&) = default;
    CRCValidator& operator=(CRCValidator&&) = default;
    
    // ========================================================================
    // One-shot CRC computation
    // ========================================================================
    
    /**
     * Compute CRC-8 checksum over data buffer.
     * Uses polynomial 0x07 per RFC 9639.
     * 
     * @param data Pointer to data buffer
     * @param length Number of bytes to process
     * @return 8-bit CRC checksum
     */
    uint8_t computeCRC8(const uint8_t* data, size_t length);
    
    /**
     * Compute CRC-16 checksum over data buffer.
     * Uses polynomial 0x8005 per RFC 9639.
     * 
     * @param data Pointer to data buffer
     * @param length Number of bytes to process
     * @return 16-bit CRC checksum
     */
    uint16_t computeCRC16(const uint8_t* data, size_t length);
    
    // ========================================================================
    // Incremental CRC computation (for streaming)
    // ========================================================================
    
    /**
     * Reset CRC-8 accumulator to initial value.
     */
    void resetCRC8();
    
    /**
     * Reset CRC-16 accumulator to initial value.
     */
    void resetCRC16();
    
    /**
     * Update CRC-8 accumulator with one byte.
     * 
     * @param byte Byte to add to CRC computation
     */
    void updateCRC8(uint8_t byte);
    
    /**
     * Update CRC-16 accumulator with one byte.
     * 
     * @param byte Byte to add to CRC computation
     */
    void updateCRC16(uint8_t byte);
    
    /**
     * Update CRC-8 accumulator with multiple bytes.
     * 
     * @param data Pointer to data buffer
     * @param length Number of bytes to process
     */
    void updateCRC8(const uint8_t* data, size_t length);
    
    /**
     * Update CRC-16 accumulator with multiple bytes.
     * 
     * @param data Pointer to data buffer
     * @param length Number of bytes to process
     */
    void updateCRC16(const uint8_t* data, size_t length);
    
    /**
     * Get current CRC-8 accumulator value.
     * 
     * @return Current 8-bit CRC value
     */
    uint8_t getCRC8() const;
    
    /**
     * Get current CRC-16 accumulator value.
     * 
     * @return Current 16-bit CRC value
     */
    uint16_t getCRC16() const;
    
private:
    // CRC-8 lookup table (polynomial 0x07)
    static const uint8_t CRC8_TABLE[256];
    
    // CRC-16 lookup table (polynomial 0x8005)
    static const uint16_t CRC16_TABLE[256];
    
    // Current CRC accumulators for incremental computation
    uint8_t m_crc8;
    uint16_t m_crc16;
    
    // Static initialization flag
    static bool s_tables_initialized;
    
    // Initialize lookup tables (called once)
    static void initializeTables();
};

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // CRCVALIDATOR_H
