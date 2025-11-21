#ifndef SUBFRAME_DECODER_H
#define SUBFRAME_DECODER_H

#include <cstdint>
#include <cstddef>

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

// Forward declarations
class BitstreamReader;
class ResidualDecoder;

/**
 * SubframeDecoder - Decodes FLAC subframes (individual channel data)
 * 
 * Implements RFC 9639 subframe decoding including:
 * - CONSTANT subframes (single value replicated)
 * - VERBATIM subframes (uncompressed samples)
 * - FIXED predictors (orders 0-4)
 * - LPC predictors (orders 1-32)
 * 
 * Requirements: 3, 4, 5, 20, 28, 35, 36, 51, 52, 54
 */

/**
 * SubframeType - Type of subframe encoding
 */
enum class SubframeType {
    CONSTANT,   // Single constant value for all samples
    VERBATIM,   // Uncompressed samples
    FIXED,      // Fixed predictor (order 0-4)
    LPC,        // Linear predictive coding (order 1-32)
    RESERVED    // Reserved/invalid type
};

/**
 * SubframeHeader - Parsed subframe header information
 */
struct SubframeHeader {
    SubframeType type;           // Type of subframe
    uint32_t predictor_order;    // Predictor order (0 for CONSTANT/VERBATIM, 0-4 for FIXED, 1-32 for LPC)
    uint32_t wasted_bits;        // Number of wasted (zero) LSBs
    uint32_t bit_depth;          // Effective bit depth for this subframe
    
    SubframeHeader() 
        : type(SubframeType::RESERVED)
        , predictor_order(0)
        , wasted_bits(0)
        , bit_depth(0)
    {}
};

/**
 * SubframeDecoder - Decodes FLAC subframes
 */
class SubframeDecoder {
public:
    /**
     * Constructor
     * @param reader BitstreamReader for reading subframe data
     * @param residual ResidualDecoder for decoding residuals
     */
    SubframeDecoder(BitstreamReader* reader, ResidualDecoder* residual);
    
    /**
     * Destructor
     */
    ~SubframeDecoder();
    
    /**
     * Decode a subframe
     * @param output Output buffer for decoded samples (must have space for block_size samples)
     * @param block_size Number of samples in this block
     * @param bit_depth Frame bit depth (before wasted bits adjustment)
     * @param is_side_channel True if this is a side channel (needs +1 bit depth)
     * @return true on success, false on error
     * 
     * Requirements: 3, 36
     */
    bool decodeSubframe(int32_t* output, uint32_t block_size, 
                       uint32_t bit_depth, bool is_side_channel);
    
private:
    BitstreamReader* m_reader;      // Bitstream reader
    ResidualDecoder* m_residual;    // Residual decoder
    
    /**
     * Parse subframe header
     * @param header Output subframe header
     * @param frame_bit_depth Frame bit depth
     * @param is_side_channel True if side channel
     * @return true on success, false on error
     * 
     * Requirements: 3, 20, 36
     */
    bool parseSubframeHeader(SubframeHeader& header, uint32_t frame_bit_depth, 
                            bool is_side_channel);
    
    /**
     * Decode CONSTANT subframe
     * @param output Output buffer
     * @param block_size Number of samples
     * @param header Subframe header
     * @return true on success, false on error
     * 
     * Requirements: 3
     */
    bool decodeConstant(int32_t* output, uint32_t block_size, 
                       const SubframeHeader& header);
    
    /**
     * Decode VERBATIM subframe
     * @param output Output buffer
     * @param block_size Number of samples
     * @param header Subframe header
     * @return true on success, false on error
     * 
     * Requirements: 3
     */
    bool decodeVerbatim(int32_t* output, uint32_t block_size, 
                       const SubframeHeader& header);
    
    /**
     * Decode FIXED predictor subframe
     * @param output Output buffer
     * @param block_size Number of samples
     * @param header Subframe header
     * @return true on success, false on error
     * 
     * Requirements: 3, 4, 35, 54
     */
    bool decodeFixed(int32_t* output, uint32_t block_size, 
                    const SubframeHeader& header);
    
    /**
     * Decode LPC predictor subframe
     * @param output Output buffer
     * @param block_size Number of samples
     * @param header Subframe header
     * @return true on success, false on error
     * 
     * Requirements: 3, 5, 28, 35, 51, 52, 54
     */
    bool decodeLPC(int32_t* output, uint32_t block_size, 
                  const SubframeHeader& header);
    
    /**
     * Apply FIXED predictor
     * @param samples Sample buffer (warm-up samples at start, residuals follow)
     * @param residuals Residual values
     * @param count Number of residual samples to decode
     * @param order Predictor order (0-4)
     * 
     * Requirements: 4, 54
     */
    void applyFixedPredictor(int32_t* samples, const int32_t* residuals,
                            uint32_t count, uint32_t order);
    
    /**
     * Apply LPC predictor
     * @param samples Sample buffer (warm-up samples at start, residuals follow)
     * @param residuals Residual values
     * @param coeffs Predictor coefficients
     * @param count Number of residual samples to decode
     * @param order Predictor order (1-32)
     * @param shift Quantization level shift
     * 
     * Requirements: 5, 51, 52, 54
     */
    void applyLPCPredictor(int32_t* samples, const int32_t* residuals,
                          const int32_t* coeffs, uint32_t count, 
                          uint32_t order, int32_t shift);
};

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // SUBFRAME_DECODER_H
