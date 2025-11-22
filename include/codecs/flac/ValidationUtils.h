#ifndef VALIDATION_UTILS_H
#define VALIDATION_UTILS_H

#include <cstdint>
#include <cstddef>
#include <limits>

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

/**
 * @brief Validation and security utilities for FLAC decoder
 * 
 * This class provides validation functions to protect against:
 * - Buffer overflows
 * - Integer overflows
 * - Excessive resource usage
 * - Malicious input (DoS attacks)
 * - Invalid data patterns
 * 
 * Per Requirement 48: Security and DoS Protection
 */
class ValidationUtils {
public:
    // ========================================================================
    // RESOURCE LIMITS (Requirement 48, Task 18.2)
    // ========================================================================
    
    /**
     * @brief Maximum allowed block size
     * 
     * RFC 9639 allows up to 65535 samples per block.
     * We enforce this limit to prevent excessive memory allocation.
     */
    static constexpr uint32_t MAX_BLOCK_SIZE = 65535;
    
    /**
     * @brief Minimum allowed block size
     * 
     * RFC 9639 requires minimum 16 samples (except last frame).
     */
    static constexpr uint32_t MIN_BLOCK_SIZE = 16;
    
    /**
     * @brief Maximum allowed partition order
     * 
     * RFC 9639 allows partition order 0-15 (2^0 to 2^15 partitions).
     * We limit to 15 to prevent excessive computation.
     */
    static constexpr uint32_t MAX_PARTITION_ORDER = 15;
    
    /**
     * @brief Maximum allowed LPC order
     * 
     * RFC 9639 allows LPC order 1-32.
     */
    static constexpr uint32_t MAX_LPC_ORDER = 32;
    
    /**
     * @brief Maximum allowed channels
     * 
     * RFC 9639 supports 1-8 channels.
     */
    static constexpr uint32_t MAX_CHANNELS = 8;
    
    /**
     * @brief Maximum allowed bit depth
     * 
     * RFC 9639 supports 4-32 bits per sample.
     */
    static constexpr uint32_t MAX_BIT_DEPTH = 32;
    static constexpr uint32_t MIN_BIT_DEPTH = 4;
    
    /**
     * @brief Maximum allowed sample rate
     * 
     * RFC 9639 allows up to 1048575 Hz (20-bit field).
     */
    static constexpr uint32_t MAX_SAMPLE_RATE = 1048575;
    
    /**
     * @brief Maximum unary value before considering it a DoS attack
     * 
     * Unary codes can theoretically be infinite. We limit to prevent
     * infinite loops on malicious input.
     */
    static constexpr uint32_t MAX_UNARY_VALUE = 1000000;
    
    /**
     * @brief Maximum sync search iterations
     * 
     * Limit sync search to prevent infinite loops on corrupted streams.
     */
    static constexpr uint32_t MAX_SYNC_SEARCH_BYTES = 1048576;  // 1 MB
    
    /**
     * @brief Maximum metadata block length
     * 
     * RFC 9639 allows 24-bit length (16 MB max).
     * We limit to prevent excessive memory allocation.
     */
    static constexpr uint32_t MAX_METADATA_BLOCK_LENGTH = 16777215;
    
    // ========================================================================
    // BOUNDS CHECKING (Task 18.1)
    // ========================================================================
    
    /**
     * @brief Check if buffer access is within bounds
     * 
     * @param buffer_size Total buffer size in elements
     * @param offset Starting offset for access
     * @param count Number of elements to access
     * @return true if access is safe, false if out of bounds
     */
    static inline bool checkBufferBounds(size_t buffer_size, size_t offset, size_t count) {
        // Check for overflow in offset + count
        if (offset > buffer_size) {
            return false;
        }
        if (count > buffer_size - offset) {
            return false;
        }
        return true;
    }
    
    /**
     * @brief Check if array index is within bounds
     * 
     * @param array_size Total array size
     * @param index Index to check
     * @return true if index is valid, false if out of bounds
     */
    static inline bool checkArrayIndex(size_t array_size, size_t index) {
        return index < array_size;
    }
    
    /**
     * @brief Check if multiplication will overflow
     * 
     * @param a First operand
     * @param b Second operand
     * @param result Output: result of multiplication if no overflow
     * @return true if multiplication is safe, false if overflow would occur
     */
    static inline bool checkMultiplyOverflow(uint32_t a, uint32_t b, uint32_t& result) {
        if (a == 0 || b == 0) {
            result = 0;
            return true;
        }
        
        // Check if a * b would overflow
        if (a > UINT32_MAX / b) {
            return false;
        }
        
        result = a * b;
        return true;
    }
    
    /**
     * @brief Check if addition will overflow
     * 
     * @param a First operand
     * @param b Second operand
     * @param result Output: result of addition if no overflow
     * @return true if addition is safe, false if overflow would occur
     */
    static inline bool checkAddOverflow(uint32_t a, uint32_t b, uint32_t& result) {
        if (a > UINT32_MAX - b) {
            return false;
        }
        
        result = a + b;
        return true;
    }
    
    /**
     * @brief Check if left shift will overflow
     * 
     * @param value Value to shift
     * @param shift Shift amount
     * @param result Output: result of shift if no overflow
     * @return true if shift is safe, false if overflow would occur
     */
    static inline bool checkShiftOverflow(uint32_t value, uint32_t shift, uint32_t& result) {
        if (shift >= 32) {
            return false;
        }
        
        // Check if value << shift would overflow
        if (value > (UINT32_MAX >> shift)) {
            return false;
        }
        
        result = value << shift;
        return true;
    }
    
    // ========================================================================
    // INPUT VALIDATION (Task 18.3)
    // ========================================================================
    
    /**
     * @brief Validate block size
     * 
     * Per RFC 9639 and Requirement 58:
     * - Minimum 16 samples (except last frame)
     * - Maximum 65535 samples
     * - 65536 is forbidden
     * 
     * @param block_size Block size to validate
     * @param allow_small Allow block size < 16 (for last frame)
     * @return true if valid, false otherwise
     */
    static inline bool validateBlockSize(uint32_t block_size, bool allow_small = false) {
        // Check forbidden value (Requirement 23)
        if (block_size == 65536) {
            return false;
        }
        
        // Check minimum (Requirement 58)
        if (!allow_small && block_size < MIN_BLOCK_SIZE) {
            return false;
        }
        
        // Check maximum
        if (block_size > MAX_BLOCK_SIZE) {
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate sample rate
     * 
     * Per RFC 9639 and Requirement 27:
     * - 0 means "get from STREAMINFO" (valid)
     * - Maximum 1048575 Hz (20-bit field)
     * 
     * @param sample_rate Sample rate to validate
     * @return true if valid, false otherwise
     */
    static inline bool validateSampleRate(uint32_t sample_rate) {
        // 0 is valid (means use STREAMINFO)
        if (sample_rate == 0) {
            return true;
        }
        
        // Check maximum
        if (sample_rate > MAX_SAMPLE_RATE) {
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate bit depth
     * 
     * Per RFC 9639 and Requirement 9:
     * - 0 means "get from STREAMINFO" (valid)
     * - Range 4-32 bits
     * 
     * @param bit_depth Bit depth to validate
     * @return true if valid, false otherwise
     */
    static inline bool validateBitDepth(uint32_t bit_depth) {
        // 0 is valid (means use STREAMINFO)
        if (bit_depth == 0) {
            return true;
        }
        
        // Check range
        if (bit_depth < MIN_BIT_DEPTH || bit_depth > MAX_BIT_DEPTH) {
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate channel count
     * 
     * Per RFC 9639 and Requirement 26:
     * - Range 1-8 channels
     * 
     * @param channels Channel count to validate
     * @return true if valid, false otherwise
     */
    static inline bool validateChannelCount(uint32_t channels) {
        return channels >= 1 && channels <= MAX_CHANNELS;
    }
    
    /**
     * @brief Validate partition order
     * 
     * Per RFC 9639 and Requirement 29:
     * - Range 0-15
     * - Must result in valid partition sizes
     * 
     * @param partition_order Partition order to validate
     * @param block_size Block size
     * @param predictor_order Predictor order
     * @return true if valid, false otherwise
     */
    static inline bool validatePartitionOrder(uint32_t partition_order, 
                                              uint32_t block_size,
                                              uint32_t predictor_order) {
        // Check maximum
        if (partition_order > MAX_PARTITION_ORDER) {
            return false;
        }
        
        // Check that block size is evenly divisible by partition count
        uint32_t partition_count = 1U << partition_order;
        if ((block_size % partition_count) != 0) {
            return false;
        }
        
        // Check that first partition has enough samples
        uint32_t partition_size = block_size >> partition_order;
        if (partition_size <= predictor_order) {
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate LPC order
     * 
     * Per RFC 9639 and Requirement 5:
     * - Range 1-32
     * - Must not exceed block size
     * 
     * @param lpc_order LPC order to validate
     * @param block_size Block size
     * @return true if valid, false otherwise
     */
    static inline bool validateLPCOrder(uint32_t lpc_order, uint32_t block_size) {
        if (lpc_order == 0 || lpc_order > MAX_LPC_ORDER) {
            return false;
        }
        
        if (lpc_order >= block_size) {
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate FIXED predictor order
     * 
     * Per RFC 9639 and Requirement 4:
     * - Range 0-4
     * - Must not exceed block size
     * 
     * @param order FIXED order to validate
     * @param block_size Block size
     * @return true if valid, false otherwise
     */
    static inline bool validateFixedOrder(uint32_t order, uint32_t block_size) {
        if (order > 4) {
            return false;
        }
        
        if (order >= block_size) {
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Validate sample value range
     * 
     * Per RFC 9639 and Requirement 57:
     * - Sample must fit in bit depth range
     * - Range: [-2^(N-1), 2^(N-1)-1] for N-bit samples
     * 
     * @param sample Sample value to validate
     * @param bit_depth Bit depth
     * @return true if valid, false otherwise
     */
    static inline bool validateSampleValue(int32_t sample, uint32_t bit_depth) {
        if (bit_depth == 0 || bit_depth > 32) {
            return false;
        }
        
        // Calculate valid range
        int32_t max_value = (1 << (bit_depth - 1)) - 1;
        int32_t min_value = -(1 << (bit_depth - 1));
        
        return sample >= min_value && sample <= max_value;
    }
    
    /**
     * @brief Validate residual value
     * 
     * Per RFC 9639 and Requirement 37:
     * - Must fit in 32-bit signed range
     * - Most negative value (INT32_MIN) is forbidden
     * 
     * @param residual Residual value to validate
     * @return true if valid, false otherwise
     */
    static inline bool validateResidualValue(int32_t residual) {
        // Most negative 32-bit value is forbidden
        return residual != INT32_MIN;
    }
    
    /**
     * @brief Check for forbidden sample rate bits
     * 
     * Per RFC 9639 and Requirement 23:
     * - 0b1111 is forbidden
     * 
     * @param sample_rate_bits Sample rate bits from frame header
     * @return true if valid, false if forbidden
     */
    static inline bool checkForbiddenSampleRateBits(uint32_t sample_rate_bits) {
        return sample_rate_bits != 0b1111;
    }
    
    /**
     * @brief Check for forbidden predictor coefficient precision
     * 
     * Per RFC 9639 and Requirement 23:
     * - 0b1111 is forbidden
     * 
     * @param precision_bits Precision bits from LPC subframe
     * @return true if valid, false if forbidden
     */
    static inline bool checkForbiddenPredictorPrecision(uint32_t precision_bits) {
        return precision_bits != 0b1111;
    }
    
    /**
     * @brief Validate predictor right shift
     * 
     * Per RFC 9639 and Requirement 23:
     * - Negative shift is forbidden
     * - Range 0-31 (5-bit signed field)
     * 
     * @param shift Shift value (signed)
     * @return true if valid, false if forbidden
     */
    static inline bool validatePredictorShift(int32_t shift) {
        return shift >= 0 && shift <= 31;
    }
    
    /**
     * @brief Validate wasted bits count
     * 
     * Per RFC 9639 and Requirement 20:
     * - Must not result in zero or negative bit depth
     * - Must be less than bit depth
     * 
     * @param wasted_bits Wasted bits count
     * @param bit_depth Original bit depth
     * @return true if valid, false otherwise
     */
    static inline bool validateWastedBits(uint32_t wasted_bits, uint32_t bit_depth) {
        if (wasted_bits == 0) {
            return true;  // No wasted bits is valid
        }
        
        if (wasted_bits >= bit_depth) {
            return false;  // Would result in zero or negative bit depth
        }
        
        return true;
    }
    
    /**
     * @brief Validate metadata block length
     * 
     * Per RFC 9639 and Requirement 48:
     * - Maximum 16777215 bytes (24-bit field)
     * - Must be reasonable to prevent DoS
     * 
     * @param length Metadata block length
     * @return true if valid, false otherwise
     */
    static inline bool validateMetadataBlockLength(uint32_t length) {
        return length <= MAX_METADATA_BLOCK_LENGTH;
    }
    
    /**
     * @brief Validate STREAMINFO constraints
     * 
     * Per RFC 9639 and Requirement 58:
     * - Minimum block size >= 16
     * - Maximum block size >= 16
     * - Minimum block size <= maximum block size
     * 
     * @param min_block_size Minimum block size from STREAMINFO
     * @param max_block_size Maximum block size from STREAMINFO
     * @return true if valid, false otherwise
     */
    static inline bool validateStreamInfoBlockSizes(uint32_t min_block_size, 
                                                     uint32_t max_block_size) {
        // Both must be >= 16
        if (min_block_size < MIN_BLOCK_SIZE || max_block_size < MIN_BLOCK_SIZE) {
            return false;
        }
        
        // Min must not exceed max
        if (min_block_size > max_block_size) {
            return false;
        }
        
        // Max must not exceed limit
        if (max_block_size > MAX_BLOCK_SIZE) {
            return false;
        }
        
        return true;
    }
};

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // VALIDATION_UTILS_H
