#ifndef RESIDUAL_DECODER_H
#define RESIDUAL_DECODER_H

#include <cstdint>
#include <vector>

// Forward declarations
class BitstreamReader;

/**
 * @brief Coding method for residual encoding
 * 
 * FLAC uses Rice/Golomb coding for residual compression.
 * Two variants exist with different Rice parameter bit widths.
 */
enum class CodingMethod {
    RICE_4BIT = 0,  // 4-bit Rice parameter (0-15)
    RICE_5BIT = 1   // 5-bit Rice parameter (0-31)
};

/**
 * @brief Information about a residual partition
 * 
 * Residuals are divided into 2^partition_order partitions,
 * each with its own Rice parameter or escape code.
 */
struct PartitionInfo {
    uint32_t rice_parameter;  // Rice parameter for this partition
    bool is_escaped;          // True if partition uses escape code
    uint32_t escape_bits;     // Bit width for escaped samples
    uint32_t sample_count;    // Number of samples in this partition
};

/**
 * @brief Decoder for FLAC residual data
 * 
 * The ResidualDecoder handles entropy decoding of residual samples
 * using Rice/Golomb coding. Residuals are the prediction errors that
 * must be added to predictor output to reconstruct the original signal.
 * 
 * RFC 9639 Section 9: Residual Coding
 * 
 * Key features:
 * - Rice code decoding with 4-bit or 5-bit parameters
 * - Partition-based encoding for adaptive compression
 * - Escape code handling for uncompressible partitions
 * - Zigzag encoding for signed values
 * 
 * Thread Safety: Not thread-safe. Caller must synchronize access.
 */
class ResidualDecoder {
public:
    /**
     * @brief Construct a ResidualDecoder
     * @param reader BitstreamReader for reading encoded data
     */
    explicit ResidualDecoder(BitstreamReader* reader);
    
    /**
     * @brief Decode residual samples for a subframe
     * 
     * Decodes the residual coding method, partition order, and all
     * residual samples. The residuals are written to the output buffer
     * and should be added to predictor output to reconstruct samples.
     * 
     * @param output Output buffer for decoded residuals (must have space for block_size samples)
     * @param block_size Number of samples in the block
     * @param predictor_order Number of warm-up samples (not encoded in residual)
     * @return true if decoding succeeded, false on error
     */
    bool decodeResidual(int32_t* output, uint32_t block_size, uint32_t predictor_order);
    
    /**
     * @brief Get the last error message
     * @return Error message string, or empty if no error
     */
    const char* getLastError() const { return m_last_error; }
    
private:
    BitstreamReader* m_reader;  // Bitstream reader (not owned)
    const char* m_last_error;   // Last error message
    
    /**
     * @brief Parse residual coding header
     * 
     * Reads the 2-bit coding method and 4-bit partition order.
     * 
     * @param method Output: coding method (4-bit or 5-bit Rice)
     * @param partition_order Output: partition order (0-15)
     * @return true if parsing succeeded, false on error
     */
    bool parseResidualHeader(CodingMethod& method, uint32_t& partition_order);
    
    /**
     * @brief Decode a single residual partition
     * 
     * Decodes one partition using Rice coding or escape code.
     * 
     * @param output Output buffer for decoded residuals
     * @param info Partition information (parameter, sample count, etc.)
     * @return true if decoding succeeded, false on error
     */
    bool decodePartition(int32_t* output, const PartitionInfo& info);
    
    /**
     * @brief Decode a single Rice-coded value
     * 
     * Decodes one residual using Rice/Golomb coding:
     * 1. Read unary quotient (count leading zeros)
     * 2. Read binary remainder (rice_param bits)
     * 3. Compute folded value: (quotient << rice_param) | remainder
     * 4. Unfold to signed value using zigzag encoding
     * 
     * @param value Output: decoded signed residual value
     * @param rice_param Rice parameter (number of remainder bits)
     * @return true if decoding succeeded, false on error
     */
    bool decodeRiceCode(int32_t& value, uint32_t rice_param);
    
    /**
     * @brief Decode an escaped partition
     * 
     * When Rice coding is inefficient, partitions can be "escaped"
     * by encoding samples directly with a fixed bit width.
     * 
     * @param output Output buffer for decoded residuals
     * @param info Partition information (escape_bits, sample_count)
     * @return true if decoding succeeded, false on error
     */
    bool decodeEscapedPartition(int32_t* output, const PartitionInfo& info);
    
    /**
     * @brief Unfold zigzag-encoded signed value
     * 
     * FLAC uses zigzag encoding to map signed values to unsigned:
     * 0 -> 0, -1 -> 1, 1 -> 2, -2 -> 3, 2 -> 4, ...
     * 
     * Decoding formula:
     * - Even values: folded / 2
     * - Odd values: -(folded + 1) / 2
     * 
     * @param folded Unsigned folded value
     * @return Signed unfolded value
     */
    inline int32_t unfoldSigned(uint32_t folded) {
        return (folded & 1) ? -static_cast<int32_t>((folded + 1) >> 1)
                            : static_cast<int32_t>(folded >> 1);
    }
    
    /**
     * @brief Validate residual value is in valid range
     * 
     * RFC 9639 Section 9.3: Residuals must fit in 32-bit signed range
     * and cannot be the most negative value (-2^31).
     * 
     * @param value Residual value to validate
     * @return true if value is valid, false otherwise
     */
    inline bool isValidResidual(int32_t value) const {
        // Most negative 32-bit value is forbidden
        return value != INT32_MIN;
    }
};

#endif // RESIDUAL_DECODER_H
