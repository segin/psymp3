/*
 * test_alaw_conversion_simple.cpp - Simple A-law conversion test
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef ENABLE_ALAW_CODEC

// Test class to access protected convertSamples method
class TestALawCodec : public ALawCodec {
public:
    explicit TestALawCodec(const StreamInfo& stream_info) : ALawCodec(stream_info) {}
    
    // Make convertSamples public for testing
    size_t testConvertSamples(const std::vector<uint8_t>& input_data, 
                             std::vector<int16_t>& output_samples) {
        return convertSamples(input_data, output_samples);
    }
};

int main() {
    printf("A-law Sample Conversion Test:\n");
    
    // Test basic A-law sample conversion
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.codec_name = "alaw";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    TestALawCodec codec(stream_info);
    
    // Test data: A-law silence (0x55) and some other values
    std::vector<uint8_t> input_data = {0x55, 0x00, 0x80, 0xFF, 0x54, 0x56};
    std::vector<int16_t> output_samples;
    
    size_t converted = codec.testConvertSamples(input_data, output_samples);
    
    printf("Input samples: %zu\n", input_data.size());
    printf("Converted samples: %zu\n", converted);
    printf("Output samples: %zu\n", output_samples.size());
    
    // Verify conversion results
    if (converted != input_data.size()) {
        printf("ERROR: Converted count mismatch\n");
        return 1;
    }
    
    if (output_samples.size() != input_data.size()) {
        printf("ERROR: Output size mismatch\n");
        return 1;
    }
    
    // Test specific values
    printf("\nSample conversions:\n");
    for (size_t i = 0; i < input_data.size(); ++i) {
        printf("A-law 0x%02X -> PCM %d\n", input_data[i], output_samples[i]);
    }
    
    // Test A-law closest-to-silence (0x55 should map to -8 per ITU-T G.711)
    if (output_samples[0] != -8) {
        printf("ERROR: A-law closest-to-silence (0x55) should map to -8, got %d\n", output_samples[0]);
        return 1;
    }
    
    // Test multi-channel processing (stereo)
    printf("\nTesting stereo (multi-channel) processing:\n");
    StreamInfo stereo_stream_info;
    stereo_stream_info.codec_type = "audio";
    stereo_stream_info.codec_name = "alaw";
    stereo_stream_info.sample_rate = 8000;
    stereo_stream_info.channels = 2;
    stereo_stream_info.bits_per_sample = 8;
    
    TestALawCodec stereo_codec(stereo_stream_info);
    
    // Stereo test data: L0, R0, L1, R1 (interleaved)
    std::vector<uint8_t> stereo_input = {0x55, 0x00, 0x80, 0xFF};
    std::vector<int16_t> stereo_output;
    
    size_t stereo_converted = stereo_codec.testConvertSamples(stereo_input, stereo_output);
    
    printf("Stereo input samples: %zu\n", stereo_input.size());
    printf("Stereo converted samples: %zu\n", stereo_converted);
    printf("Stereo output samples: %zu\n", stereo_output.size());
    
    // Verify stereo interleaving is preserved
    printf("Stereo sample pairs:\n");
    for (size_t i = 0; i < stereo_input.size(); i += 2) {
        if (i + 1 < stereo_input.size()) {
            printf("L: A-law 0x%02X -> PCM %d, R: A-law 0x%02X -> PCM %d\n",
                   stereo_input[i], stereo_output[i],
                   stereo_input[i+1], stereo_output[i+1]);
        }
    }
    
    // Test empty input handling
    printf("\nTesting empty input handling:\n");
    std::vector<uint8_t> empty_input;
    std::vector<int16_t> empty_output;
    
    size_t empty_converted = codec.testConvertSamples(empty_input, empty_output);
    
    if (empty_converted != 0 || !empty_output.empty()) {
        printf("ERROR: Empty input should produce empty output\n");
        return 1;
    }
    printf("Empty input handled correctly\n");
    
    // Test variable chunk sizes (VoIP packet simulation)
    printf("\nTesting variable chunk sizes (VoIP simulation):\n");
    std::vector<size_t> chunk_sizes = {1, 8, 20, 160, 320}; // Common VoIP packet sizes
    
    for (size_t chunk_size : chunk_sizes) {
        std::vector<uint8_t> chunk_input(chunk_size, 0x55); // Fill with closest-to-silence
        std::vector<int16_t> chunk_output;
        
        size_t chunk_converted = codec.testConvertSamples(chunk_input, chunk_output);
        
        printf("Chunk size %zu: converted %zu samples\n", chunk_size, chunk_converted);
        
        if (chunk_converted != chunk_size || chunk_output.size() != chunk_size) {
            printf("ERROR: Chunk size %zu processing failed\n", chunk_size);
            return 1;
        }
        
        // Verify all samples are closest-to-silence (-8)
        for (size_t j = 0; j < chunk_output.size(); ++j) {
            if (chunk_output[j] != -8) {
                printf("ERROR: Expected closest-to-silence (-8) at index %zu, got %d\n", j, chunk_output[j]);
                return 1;
            }
        }
    }
    
    // Test canDecode method
    printf("\nTesting canDecode method:\n");
    
    // Valid A-law formats
    StreamInfo valid_alaw;
    valid_alaw.codec_type = "audio";
    valid_alaw.codec_name = "alaw";
    valid_alaw.sample_rate = 8000;
    valid_alaw.channels = 1;
    valid_alaw.bits_per_sample = 8;
    
    if (!codec.canDecode(valid_alaw)) {
        printf("ERROR: Should accept valid A-law format\n");
        return 1;
    }
    printf("Valid A-law format accepted\n");
    
    // Invalid format (wrong codec name)
    StreamInfo invalid_codec;
    invalid_codec.codec_type = "audio";
    invalid_codec.codec_name = "mulaw";
    invalid_codec.sample_rate = 8000;
    invalid_codec.channels = 1;
    invalid_codec.bits_per_sample = 8;
    
    if (codec.canDecode(invalid_codec)) {
        printf("ERROR: Should reject μ-law format\n");
        return 1;
    }
    printf("μ-law format correctly rejected\n");
    
    // Invalid format (wrong bits per sample)
    StreamInfo invalid_bits;
    invalid_bits.codec_type = "audio";
    invalid_bits.codec_name = "alaw";
    invalid_bits.sample_rate = 8000;
    invalid_bits.channels = 1;
    invalid_bits.bits_per_sample = 16;
    
    if (codec.canDecode(invalid_bits)) {
        printf("ERROR: Should reject 16-bit format\n");
        return 1;
    }
    printf("16-bit format correctly rejected\n");
    
    printf("\nAll A-law sample conversion tests passed!\n");
    return 0;
}

#else

int main() {
    printf("A-law codec not enabled in build\n");
    return 0;
}

#endif // ENABLE_ALAW_CODEC