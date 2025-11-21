// ChannelDecorrelator.cpp - FLAC channel decorrelation implementation
// Part of the Native FLAC Decoder implementation

#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

ChannelDecorrelator::ChannelDecorrelator() {
    // No initialization needed
}

ChannelDecorrelator::~ChannelDecorrelator() {
    // No cleanup needed
}

bool ChannelDecorrelator::decorrelate(int32_t** channels, uint32_t block_size,
                                     uint32_t channel_count, ChannelAssignment assignment) {
    // Validate inputs
    if (!channels) {
        Debug::log("flac_codec", "ChannelDecorrelator: null channels pointer");
        return false;
    }
    
    if (block_size == 0) {
        Debug::log("flac_codec", "ChannelDecorrelator: zero block size");
        return false;
    }
    
    // Validate channel count (Requirements 26.1-26.8)
    if (!validateChannelCount(channel_count)) {
        Debug::log("flac_codec", "ChannelDecorrelator: invalid channel count %u", channel_count);
        return false;
    }
    
    // Validate channel assignment for this channel count
    if (!validateChannelAssignment(channel_count, assignment)) {
        Debug::log("flac_codec", "ChannelDecorrelator: invalid channel assignment %d for %u channels",
                  static_cast<int>(assignment), channel_count);
        return false;
    }
    
    // Apply decorrelation based on assignment mode
    switch (assignment) {
        case ChannelAssignment::INDEPENDENT:
            // No decorrelation needed (Requirement 7.1)
            Debug::log("flac_codec", "ChannelDecorrelator: independent channels, no decorrelation");
            return true;
            
        case ChannelAssignment::LEFT_SIDE:
            // Left-side decorrelation: Right = Left - Side (Requirement 7.2)
            if (channel_count != 2) {
                Debug::log("flac_codec", "ChannelDecorrelator: left-side requires 2 channels, got %u", 
                          channel_count);
                return false;
            }
            Debug::log("flac_codec", "ChannelDecorrelator: applying left-side decorrelation");
            decorrelateLeftSide(channels[0], channels[1], block_size);
            return true;
            
        case ChannelAssignment::RIGHT_SIDE:
            // Right-side decorrelation: Left = Right + Side (Requirement 7.3)
            if (channel_count != 2) {
                Debug::log("flac_codec", "ChannelDecorrelator: right-side requires 2 channels, got %u",
                          channel_count);
                return false;
            }
            Debug::log("flac_codec", "ChannelDecorrelator: applying right-side decorrelation");
            decorrelateRightSide(channels[0], channels[1], block_size);
            return true;
            
        case ChannelAssignment::MID_SIDE:
            // Mid-side decorrelation (Requirements 7.4, 7.5)
            if (channel_count != 2) {
                Debug::log("flac_codec", "ChannelDecorrelator: mid-side requires 2 channels, got %u",
                          channel_count);
                return false;
            }
            Debug::log("flac_codec", "ChannelDecorrelator: applying mid-side decorrelation");
            decorrelateMidSide(channels[0], channels[1], block_size);
            return true;
            
        default:
            Debug::log("flac_codec", "ChannelDecorrelator: unknown channel assignment %d",
                      static_cast<int>(assignment));
            return false;
    }
}

void ChannelDecorrelator::decorrelateLeftSide(int32_t* left, int32_t* side, uint32_t count) {
    // Left-side decorrelation: Right = Left - Side
    // The 'side' buffer contains the side channel and will be overwritten with right channel
    // Requirement 7.2
    
    for (uint32_t i = 0; i < count; ++i) {
        // Compute right channel: right = left - side
        // Note: side buffer is modified in-place to become right channel
        int32_t left_sample = left[i];
        int32_t side_sample = side[i];
        side[i] = left_sample - side_sample;  // side buffer now contains right channel
    }
    
    // After this operation:
    // - channels[0] (left) contains the left channel (unchanged)
    // - channels[1] (side) now contains the right channel
}

void ChannelDecorrelator::decorrelateRightSide(int32_t* side, int32_t* right, uint32_t count) {
    // Right-side decorrelation: Left = Right + Side
    // The 'side' buffer contains the side channel and will be overwritten with left channel
    // Requirement 7.3
    
    for (uint32_t i = 0; i < count; ++i) {
        // Compute left channel: left = right + side
        // Note: side buffer is modified in-place to become left channel
        int32_t right_sample = right[i];
        int32_t side_sample = side[i];
        side[i] = right_sample + side_sample;  // side buffer now contains left channel
    }
    
    // After this operation:
    // - channels[0] (side) now contains the left channel
    // - channels[1] (right) contains the right channel (unchanged)
}

void ChannelDecorrelator::decorrelateMidSide(int32_t* mid, int32_t* side, uint32_t count) {
    // Mid-side decorrelation with proper rounding for odd side values
    // Left = Mid + (Side>>1)
    // Right = Mid - (Side>>1)
    // Requirements 7.4, 7.5
    //
    // RFC 9639 specifies:
    // - The side channel has one extra bit of precision
    // - For odd side values, proper rounding must be applied
    // - The formulas ensure lossless reconstruction
    
    for (uint32_t i = 0; i < count; ++i) {
        int32_t mid_sample = mid[i];
        int32_t side_sample = side[i];
        
        // Arithmetic right shift for signed values
        // This handles both positive and negative side values correctly
        int32_t side_half = side_sample >> 1;
        
        // Compute left and right channels
        // Left = Mid + (Side>>1)
        int32_t left_sample = mid_sample + side_half;
        
        // Right = Mid - (Side>>1)
        // For odd side values, we need to handle rounding:
        // - If side is odd and positive, right gets the extra bit
        // - If side is odd and negative, left gets the extra bit
        // The arithmetic right shift handles this correctly
        int32_t right_sample = mid_sample - side_half;
        
        // Handle odd side values (Requirement 7.5)
        // When side is odd, we need to adjust right channel
        if (side_sample & 1) {
            // For odd side values, subtract 1 from right to maintain proper rounding
            // This ensures: left + right = 2 * mid (for even side)
            //              left + right = 2 * mid - 1 (for odd positive side)
            //              left + right = 2 * mid + 1 (for odd negative side)
            if (side_sample > 0) {
                right_sample--;
            } else {
                right_sample++;
            }
        }
        
        // Store results (in-place modification)
        mid[i] = left_sample;   // mid buffer now contains left channel
        side[i] = right_sample; // side buffer now contains right channel
    }
    
    // After this operation:
    // - channels[0] (mid) now contains the left channel
    // - channels[1] (side) now contains the right channel
}

bool ChannelDecorrelator::validateChannelCount(uint32_t channel_count) const {
    // RFC 9639 supports 1-8 channels (Requirements 26.1-26.8)
    return channel_count >= 1 && channel_count <= 8;
}

bool ChannelDecorrelator::validateChannelAssignment(uint32_t channel_count,
                                                   ChannelAssignment assignment) const {
    // Requirement 7.7: When channel count is 1, skip decorrelation
    if (channel_count == 1) {
        // Mono audio must use independent channel assignment
        return assignment == ChannelAssignment::INDEPENDENT;
    }
    
    // Stereo decorrelation modes require exactly 2 channels
    if (assignment != ChannelAssignment::INDEPENDENT) {
        // LEFT_SIDE, RIGHT_SIDE, and MID_SIDE all require 2 channels
        return channel_count == 2;
    }
    
    // Independent channel assignment is valid for any channel count
    return true;
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3
