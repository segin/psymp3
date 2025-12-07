#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

/**
 * @file ResidualDecoder.cpp
 * @brief Implementation of FLAC residual decoder
 * 
 * Implements Rice/Golomb entropy decoding for FLAC residual samples.
 * Follows RFC 9639 Section 9 (Residual Coding).
 */

ResidualDecoder::ResidualDecoder(BitstreamReader* reader)
    : m_reader(reader)
    , m_last_error(nullptr)
{
}

bool ResidualDecoder::decodeResidual(int32_t* output, uint32_t block_size, uint32_t predictor_order)
{
    
    if (!output) {
        m_last_error = "Output buffer is null";
        return false;
    }
    
    if (block_size == 0) {
        m_last_error = "Block size is zero";
        return false;
    }
    
    if (predictor_order >= block_size) {
        m_last_error = "Predictor order >= block size";
        return false;
    }
    
    // Parse residual coding header
    CodingMethod method;
    uint32_t partition_order;
    if (!parseResidualHeader(method, partition_order)) {
        return false;
    }
    
    // Calculate number of residual samples (excludes warm-up samples)
    uint32_t residual_count = block_size - predictor_order;
    
    // Validate partition order
    // RFC 9639 Section 9.2: block_size must be evenly divisible by 2^partition_order
    uint32_t partition_count = 1u << partition_order;
    if (block_size % partition_count != 0) {
        m_last_error = "Block size not divisible by partition count";
        Debug::log("residual_decoder", "Block size ", block_size, " not divisible by ", partition_count);
        return false;
    }
    
    // Samples per partition is block_size / partition_count
    // Per RFC 9639: first partition has (block_size / partition_count) - predictor_order samples
    // Other partitions have (block_size / partition_count) samples
    uint32_t samples_per_partition = block_size / partition_count;
    
    // Decode each partition
    uint32_t output_offset = 0;
    for (uint32_t p = 0; p < partition_count; ++p) {
        PartitionInfo info;
        
        // First partition has fewer samples due to predictor warm-up samples
        if (p == 0) {
            info.sample_count = samples_per_partition - predictor_order;
        } else {
            info.sample_count = samples_per_partition;
        }
        
        // Determine Rice parameter bit width based on coding method
        uint32_t param_bits = (method == CodingMethod::RICE_4BIT) ? 4 : 5;
        uint32_t escape_code = (1u << param_bits) - 1;  // 0b1111 or 0b11111
        
        // Read Rice parameter
        uint32_t rice_param;
        if (!m_reader->readBits(rice_param, param_bits)) {
            m_last_error = "Failed to read Rice parameter";
            return false;
        }
        
        // Check for escape code
        if (rice_param == escape_code) {
            // Escaped partition: samples encoded directly
            info.is_escaped = true;
            if (!m_reader->readBits(info.escape_bits, 5)) {  // 5-bit escape bit width
                m_last_error = "Failed to read escape bit width";
                return false;
            }
            
            if (info.escape_bits == 0 || info.escape_bits > 32) {
                m_last_error = "Invalid escape bit width";
                return false;
            }
        } else {
            // Normal Rice-coded partition
            info.is_escaped = false;
            info.rice_parameter = rice_param;
        }
        
        // Decode partition samples
        if (!decodePartition(output + output_offset, info)) {
            return false;
        }
        
        output_offset += info.sample_count;
    }
    
    // Verify we decoded the expected number of samples
    if (output_offset != residual_count) {
        m_last_error = "Residual count mismatch";
        Debug::log("residual_decoder", "Residual count mismatch: got %u, expected %u", output_offset, residual_count);
        return false;
    }
    
    m_last_error = nullptr;
    return true;
}

bool ResidualDecoder::parseResidualHeader(CodingMethod& method, uint32_t& partition_order)
{
    // RFC 9639 Section 9.1: Residual coding method
    // 2 bits: coding method
    //   00 = RICE_4BIT (4-bit Rice parameter)
    //   01 = RICE_5BIT (5-bit Rice parameter)
    //   10, 11 = reserved (invalid)
    uint32_t method_bits;
    if (!m_reader->readBits(method_bits, 2)) {
        m_last_error = "Failed to read coding method";
        return false;
    }
    
    if (method_bits > 1) {
        m_last_error = "Invalid residual coding method";
        return false;
    }
    
    method = static_cast<CodingMethod>(method_bits);
    
    // RFC 9639 Section 9.2: Partition order
    // 4 bits: partition order (0-15)
    // Number of partitions = 2^partition_order
    if (!m_reader->readBits(partition_order, 4)) {
        m_last_error = "Failed to read partition order";
        return false;
    }
    
    return true;
}

bool ResidualDecoder::decodePartition(int32_t* output, const PartitionInfo& info)
{
    if (info.is_escaped) {
        return decodeEscapedPartition(output, info);
    }
    
    // Decode Rice-coded samples
    for (uint32_t i = 0; i < info.sample_count; ++i) {
        int32_t value;
        if (!decodeRiceCode(value, info.rice_parameter)) {
            Debug::log("residual_decoder", "Rice decode failed at sample ", i, " of ", info.sample_count);
            return false;
        }
        
        // Validate residual value
        if (!isValidResidual(value)) {
            m_last_error = "Invalid residual value (INT32_MIN forbidden)";
            return false;
        }
        
        output[i] = value;
    }
    
    return true;
}

bool ResidualDecoder::decodeRiceCode(int32_t& value, uint32_t rice_param)
{
    // RFC 9639 Section 9.3: Rice coding
    // 
    // Rice code consists of:
    // 1. Unary-coded quotient (count of leading zeros before a 1 bit)
    // 2. Binary-coded remainder (rice_param bits)
    // 
    // Folded value = (quotient << rice_param) | remainder
    // Signed value = unfoldSigned(folded)
    
    // Read unary quotient
    uint32_t quotient;
    if (!m_reader->readUnary(quotient)) {
        m_last_error = "Failed to read unary quotient";
        return false;
    }
    
    // Check for excessive quotient (potential DoS or corrupted data)
    if (quotient > 1024) {
        m_last_error = "Excessive unary quotient in Rice code";
        return false;
    }
    
    // Read binary remainder
    uint32_t remainder = 0;
    if (rice_param > 0) {
        if (!m_reader->readBits(remainder, rice_param)) {
            m_last_error = "Failed to read Rice remainder";
            return false;
        }
    }
    
    // Compute folded unsigned value
    uint32_t folded = (quotient << rice_param) | remainder;
    
    // Unfold to signed value using zigzag encoding
    value = unfoldSigned(folded);
    
    return true;
}

bool ResidualDecoder::decodeEscapedPartition(int32_t* output, const PartitionInfo& info)
{
    // RFC 9639 Section 9.4: Escaped partitions
    // 
    // When Rice coding is inefficient (e.g., for white noise),
    // the partition can be "escaped" by encoding samples directly
    // with a fixed bit width.
    
    for (uint32_t i = 0; i < info.sample_count; ++i) {
        // Read signed sample directly
        int32_t value;
        if (!m_reader->readBitsSigned(value, info.escape_bits)) {
            m_last_error = "Failed to read escaped sample";
            return false;
        }
        
        // Validate residual value
        if (!isValidResidual(value)) {
            m_last_error = "Invalid residual value in escaped partition";
            return false;
        }
        
        output[i] = value;
    }
    
    return true;
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3
