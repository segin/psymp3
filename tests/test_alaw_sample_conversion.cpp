/*
 * test_alaw_sample_conversion.cpp - Test A-law sample conversion method
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef ENABLE_ALAW_CODEC

int main() {
    // Test basic A-law sample conversion through decode method
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.codec_name = "alaw";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    ALawCodec codec(stream_info);
    
    if (!codec.initialize()) {
        printf("ERROR: Failed to initialize A-law codec\n");
        return 1;
    }
    
    // Test data: A-law silence (0x55) and some other values
    std::vector<uint8_t> input_data = {0x55, 0x00, 0x80, 0xFF, 0x54, 0x56};
    
    MediaChunk chunk;
    chunk.data = input_data;
    chunk.timestamp_samples = 0;
    
    AudioFrame frame = codec.decode(chunk);
    
    printf("A-law Sample Conversion Test:\n");
    printf("Input samples: %zu\n", input_data.size());
    printf("Output samples: %zu\n", frame.samples.size());
    printf("Frame sample rate: %d\n", frame.sample_rate);
    printf("Frame channels: %d\n", frame.channels);
    
    // Verify conversion results
    if (frame.samples.size() != input_data.size()) {
        printf("ERROR: Output size mismatch\n");
        return 1;
    }
    
    // Test specific values
    printf("\nSample conversions:\n");
    for (size_t i = 0; i < input_data.size(); ++i) {
        printf("A-law 0x%02X -> PCM %d\n", input_data[i], frame.samples[i]);
    }
    
    // Test A-law closest-to-silence (0x55 should map to -8 per ITU-T G.711)
    if (frame.samples[0] != -8) {
        printf("ERROR: A-law closest-to-silence (0x55) should map to -8, got %d\n", frame.samples[0]);
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
    
    ALawCodec stereo_codec(stereo_stream_info);
    
    if (!stereo_codec.initialize()) {
        printf("ERROR: Failed to initialize stereo A-law codec\n");
        return 1;
    }
    
    // Stereo test data: L0, R0, L1, R1 (interleaved)
    std::vector<uint8_t> stereo_input = {0x55, 0x00, 0x80, 0xFF};
    
    MediaChunk stereo_chunk;
    stereo_chunk.data = stereo_input;
    stereo_chunk.timestamp_samples = 0;
    
    AudioFrame stereo_frame = stereo_codec.decode(stereo_chunk);
    
    printf("Stereo input samples: %zu\n", stereo_input.size());
    printf("Stereo output samples: %zu\n", stereo_frame.samples.size());
    printf("Stereo frame channels: %d\n", stereo_frame.channels);
    
    // Verify stereo interleaving is preserved
    printf("Stereo sample pairs:\n");
    for (size_t i = 0; i < stereo_input.size(); i += 2) {
        if (i + 1 < stereo_input.size()) {
            printf("L: A-law 0x%02X -> PCM %d, R: A-law 0x%02X -> PCM %d\n",
                   stereo_input[i], stereo_frame.samples[i],
                   stereo_input[i+1], stereo_frame.samples[i+1]);
        }
    }
    
    // Test empty input handling
    printf("\nTesting empty input handling:\n");
    MediaChunk empty_chunk;
    empty_chunk.data.clear();
    empty_chunk.timestamp_samples = 0;
    
    AudioFrame empty_frame = codec.decode(empty_chunk);
    
    if (!empty_frame.samples.empty()) {
        printf("ERROR: Empty input should produce empty output\n");
        return 1;
    }
    printf("Empty input handled correctly\n");
    
    // Test variable chunk sizes (VoIP packet simulation)
    printf("\nTesting variable chunk sizes (VoIP simulation):\n");
    std::vector<size_t> chunk_sizes = {1, 8, 20, 160, 320}; // Common VoIP packet sizes
    
    for (size_t chunk_size : chunk_sizes) {
        std::vector<uint8_t> chunk_input(chunk_size, 0x55); // Fill with silence
        
        MediaChunk voip_chunk;
        voip_chunk.data = chunk_input;
        voip_chunk.timestamp_samples = 0;
        
        AudioFrame voip_frame = codec.decode(voip_chunk);
        
        printf("Chunk size %zu: output samples %zu\n", chunk_size, voip_frame.samples.size());
        
        if (voip_frame.samples.size() != chunk_size) {
            printf("ERROR: Chunk size %zu processing failed\n", chunk_size);
            return 1;
        }
    }
    
    printf("\nAll A-law sample conversion tests passed!\n");
    return 0;
}

#else

int main() {
    printf("A-law codec not enabled in build\n");
    return 0;
}

#endif // ENABLE_ALAW_CODEC