#ifndef PSYMP3_CODECS_FLAC_SAMPLERECONSTRUCTOR_H
#define PSYMP3_CODECS_FLAC_SAMPLERECONSTRUCTOR_H

#include <cstdint>
#include <cstddef>

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

/**
 * SampleReconstructor - Converts decoded samples to output PCM format
 * 
 * This class handles bit depth conversion from various FLAC bit depths (4-32 bits)
 * to the standard 16-bit PCM output format. It also handles channel interleaving
 * for stereo and multi-channel audio output.
 * 
 * Requirements: 9, 10
 */
class SampleReconstructor {
public:
    SampleReconstructor();
    ~SampleReconstructor();
    
    /**
     * Reconstruct samples from decoded channels to interleaved 16-bit PCM output
     * 
     * @param output Output buffer for interleaved 16-bit PCM samples
     * @param channels Array of channel buffers containing decoded 32-bit samples
     * @param block_size Number of samples per channel
     * @param channel_count Number of channels
     * @param source_bit_depth Source bit depth (4-32 bits)
     * 
     * Requirements: 9, 10
     */
    void reconstructSamples(int16_t* output, 
                          int32_t** channels,
                          uint32_t block_size, 
                          uint32_t channel_count,
                          uint32_t source_bit_depth);
    
private:
    /**
     * Convert a single sample from source bit depth to 16-bit
     * 
     * @param sample Input sample at source bit depth
     * @param source_bit_depth Source bit depth (4-32 bits)
     * @return Converted 16-bit sample
     * 
     * Requirements: 9
     */
    int16_t convertTo16Bit(int32_t sample, uint32_t source_bit_depth);
    
    /**
     * Upscale 8-bit sample to 16-bit
     * Left-shift by 8 bits to scale to 16-bit range
     * 
     * Requirements: 9.2
     */
    int16_t upscale8To16(int32_t sample);
    
    /**
     * Downscale 24-bit sample to 16-bit with rounding
     * Right-shift by 8 bits with 0.5 LSB rounding
     * 
     * Requirements: 9.3
     */
    int16_t downscale24To16(int32_t sample);
    
    /**
     * Downscale 32-bit sample to 16-bit with rounding
     * Right-shift by 16 bits with 0.5 LSB rounding
     * 
     * Requirements: 9.4
     */
    int16_t downscale32To16(int32_t sample);
    
    /**
     * Downscale 20-bit sample to 16-bit with rounding
     * Right-shift by 4 bits with 0.5 LSB rounding
     * 
     * Requirements: 9.6
     */
    int16_t downscale20To16(int32_t sample);
    
    /**
     * Upscale samples from 4-12 bit depths to 16-bit
     * Left-shift to 16-bit range with proper scaling
     * 
     * Requirements: 9.5
     */
    int16_t upscaleTo16(int32_t sample, uint32_t source_bit_depth);
    
    /**
     * Validate sample is within 16-bit range and prevent clipping
     * 
     * Requirements: 10.5, 57
     */
    int16_t validateAndClip(int32_t sample);
};

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_FLAC_SAMPLERECONSTRUCTOR_H
