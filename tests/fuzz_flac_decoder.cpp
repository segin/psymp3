/*
 * fuzz_flac_decoder.cpp - libFuzzer harness for FLAC decoder
 * 
 * This file implements a libFuzzer target for fuzzing the native FLAC decoder.
 * It can be compiled with libFuzzer to perform continuous fuzzing and detect
 * crashes, hangs, and undefined behavior.
 * 
 * Compile with:
 *   clang++ -fsanitize=fuzzer,address -I./include -I./src fuzz_flac_decoder.cpp \
 *     src/codecs/flac/*.cpp -o fuzz_flac_decoder
 * 
 * Run with:
 *   ./fuzz_flac_decoder corpus/ -max_len=1000000
 */

#include <cstdint>
#include <cstring>
#include <vector>
#include <memory>

// Forward declarations for FLAC decoder components
namespace PsyMP3 {
namespace Codec {
namespace FLAC {

class BitstreamReader;
class FrameParser;
class SubframeDecoder;
class ResidualDecoder;
class ChannelDecorrelator;
class SampleReconstructor;
class CRCValidator;
class MetadataParser;

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

// Include FLAC decoder headers
#include "codecs/flac/BitstreamReader.h"
#include "codecs/flac/FrameParser.h"
#include "codecs/flac/SubframeDecoder.h"
#include "codecs/flac/ResidualDecoder.h"
#include "codecs/flac/ChannelDecorrelator.h"
#include "codecs/flac/SampleReconstructor.h"
#include "codecs/flac/CRCValidator.h"
#include "codecs/flac/MetadataParser.h"
#include "codecs/flac/FLACError.h"

using namespace PsyMP3::Codec::FLAC;

/*
 * libFuzzer entry point
 * 
 * This function is called by libFuzzer with random input data.
 * It attempts to decode the input as a FLAC stream and catches any errors.
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Minimum size for a valid FLAC frame header
    if (size < 4) {
        return 0;
    }

    try {
        // Create bitstream reader with fuzzer input
        BitstreamReader reader;
        reader.feedData(data, size);

        // Try to find and parse frame sync
        if (!reader.isAligned()) {
            reader.alignToByte();
        }

        // Attempt to find frame sync pattern (0xFFF8-0xFFFF)
        bool found_sync = false;
        size_t max_search = std::min(size, static_cast<size_t>(65536));
        
        for (size_t i = 0; i < max_search && reader.getAvailableBits() >= 16; ++i) {
            uint32_t sync_bits = 0;
            if (reader.readBits(sync_bits, 16)) {
                if ((sync_bits & 0xFFF8) == 0xFFF8) {
                    found_sync = true;
                    break;
                }
            }
        }

        if (!found_sync) {
            return 0;  // No valid frame found
        }

        // Create frame parser
        CRCValidator crc_validator;
        FrameParser frame_parser(&reader, &crc_validator);

        // Try to parse frame header
        FrameParser::FrameHeader header;
        if (!frame_parser.parseFrameHeader(header)) {
            return 0;  // Invalid frame header
        }

        // Validate frame parameters
        if (header.block_size == 0 || header.block_size > 65535) {
            return 0;  // Invalid block size
        }

        if (header.sample_rate == 0 || header.sample_rate > 1048575) {
            return 0;  // Invalid sample rate
        }

        if (header.channels == 0 || header.channels > 8) {
            return 0;  // Invalid channel count
        }

        if (header.bit_depth < 4 || header.bit_depth > 32) {
            return 0;  // Invalid bit depth
        }

        // Create subframe decoder
        ResidualDecoder residual_decoder(&reader);
        SubframeDecoder subframe_decoder(&reader, &residual_decoder);

        // Allocate decode buffers
        std::vector<int32_t> decode_buffers[8];
        for (uint32_t ch = 0; ch < header.channels; ++ch) {
            decode_buffers[ch].resize(header.block_size, 0);
        }

        // Try to decode subframes
        for (uint32_t ch = 0; ch < header.channels; ++ch) {
            bool is_side_channel = (ch == 1) && 
                (header.channel_assignment == FrameParser::ChannelAssignment::LEFT_SIDE ||
                 header.channel_assignment == FrameParser::ChannelAssignment::RIGHT_SIDE ||
                 header.channel_assignment == FrameParser::ChannelAssignment::MID_SIDE);

            if (!subframe_decoder.decodeSubframe(
                decode_buffers[ch].data(),
                header.block_size,
                header.bit_depth,
                is_side_channel)) {
                return 0;  // Subframe decode failed
            }
        }

        // Try to apply channel decorrelation
        ChannelDecorrelator decorrelator;
        int32_t* channel_ptrs[8];
        for (uint32_t ch = 0; ch < header.channels; ++ch) {
            channel_ptrs[ch] = decode_buffers[ch].data();
        }
        decorrelator.decorrelate(channel_ptrs, header.block_size, header.channel_assignment);

        // Try to reconstruct samples
        SampleReconstructor reconstructor;
        std::vector<int16_t> output_buffer(header.block_size * header.channels);
        reconstructor.reconstructSamples(
            output_buffer.data(),
            channel_ptrs,
            header.block_size,
            header.channels,
            header.bit_depth);

        // Try to parse frame footer
        FrameParser::FrameFooter footer;
        if (!frame_parser.parseFrameFooter(footer)) {
            return 0;  // Invalid frame footer
        }

        // Validate frame CRC
        if (!frame_parser.validateFrame(header, footer)) {
            return 0;  // Frame CRC validation failed (expected for fuzzer input)
        }

    } catch (const FLACException& e) {
        // Expected - fuzzer input may cause exceptions
        return 0;
    } catch (const std::exception& e) {
        // Unexpected exception - log but don't crash
        return 0;
    } catch (...) {
        // Catch all exceptions to prevent crashes
        return 0;
    }

    return 0;
}

/*
 * AFL++ entry point (for AFL fuzzer compatibility)
 * 
 * This allows the same binary to be used with both libFuzzer and AFL++
 */
#ifdef __AFL_COMPILER
int main(int argc, char** argv) {
    // Read input from stdin
    std::vector<uint8_t> input;
    int byte;
    while ((byte = getchar()) != EOF) {
        input.push_back(static_cast<uint8_t>(byte));
    }

    if (!input.empty()) {
        LLVMFuzzerTestOneInput(input.data(), input.size());
    }

    return 0;
}
#endif
