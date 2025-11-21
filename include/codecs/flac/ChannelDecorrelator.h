// ChannelDecorrelator.h - FLAC channel decorrelation component
// Part of the Native FLAC Decoder implementation
// Handles stereo decorrelation (left-side, right-side, mid-side) and multi-channel support

#ifndef CHANNEL_DECORRELATOR_H
#define CHANNEL_DECORRELATOR_H

#include <cstdint>
#include <cstddef>

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

// Forward declaration - ChannelAssignment is defined in FrameParser.h
enum class ChannelAssignment;

/**
 * ChannelDecorrelator - Handles FLAC channel decorrelation
 * 
 * FLAC uses stereo decorrelation to improve compression by encoding
 * correlated channels. This class reverses the decorrelation to
 * reconstruct independent left and right channels.
 * 
 * Decorrelation modes:
 * - INDEPENDENT: No decorrelation, channels are independent
 * - LEFT_SIDE: Right = Left - Side
 * - RIGHT_SIDE: Left = Right + Side
 * - MID_SIDE: Left = Mid + (Side>>1), Right = Mid - (Side>>1)
 * 
 * Requirements: 7, 26
 * RFC 9639 Section: 9.1 (Channel Assignment)
 */
class ChannelDecorrelator {
public:
    /**
     * Constructor
     */
    ChannelDecorrelator();
    
    /**
     * Destructor
     */
    ~ChannelDecorrelator();
    
    /**
     * Decorrelate channels based on channel assignment mode
     * 
     * Applies the appropriate decorrelation algorithm to reconstruct
     * independent channels from correlated data.
     * 
     * @param channels Array of channel buffers (modified in-place)
     * @param block_size Number of samples per channel
     * @param channel_count Number of channels (1-8)
     * @param assignment Channel assignment mode
     * @return true if decorrelation succeeded, false on error
     * 
     * Requirements: 7.1-7.8, 26.1-26.8
     */
    bool decorrelate(int32_t** channels, uint32_t block_size,
                    uint32_t channel_count, ChannelAssignment assignment);
    
private:
    /**
     * Decorrelate left-side stereo
     * Right = Left - Side
     * 
     * @param left Left channel buffer (input)
     * @param side Side channel buffer (input, becomes right output)
     * @param count Number of samples
     * 
     * Requirements: 7.2
     */
    void decorrelateLeftSide(int32_t* left, int32_t* side, uint32_t count);
    
    /**
     * Decorrelate right-side stereo
     * Left = Right + Side
     * 
     * @param side Side channel buffer (input, becomes left output)
     * @param right Right channel buffer (input)
     * @param count Number of samples
     * 
     * Requirements: 7.3
     */
    void decorrelateRightSide(int32_t* side, int32_t* right, uint32_t count);
    
    /**
     * Decorrelate mid-side stereo
     * Left = Mid + (Side>>1)
     * Right = Mid - (Side>>1)
     * 
     * Handles proper rounding for odd side values per RFC 9639.
     * 
     * @param mid Mid channel buffer (input, becomes left output)
     * @param side Side channel buffer (input, becomes right output)
     * @param count Number of samples
     * 
     * Requirements: 7.4, 7.5
     */
    void decorrelateMidSide(int32_t* mid, int32_t* side, uint32_t count);
    
    /**
     * Validate channel count
     * 
     * @param channel_count Number of channels
     * @return true if valid (1-8), false otherwise
     * 
     * Requirements: 26.1-26.8
     */
    bool validateChannelCount(uint32_t channel_count) const;
    
    /**
     * Validate channel assignment for given channel count
     * 
     * @param channel_count Number of channels
     * @param assignment Channel assignment mode
     * @return true if valid combination, false otherwise
     * 
     * Requirements: 7.7, 7.8
     */
    bool validateChannelAssignment(uint32_t channel_count, 
                                   ChannelAssignment assignment) const;
};

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // CHANNEL_DECORRELATOR_H
