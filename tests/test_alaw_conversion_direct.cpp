/*
 * test_alaw_conversion_direct.cpp - Direct test of A-law sample conversion
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>
#include <vector>
#include <cstdint>

// Minimal test - directly test the conversion logic without full framework
// A-law to 16-bit PCM conversion lookup table (ITU-T G.711 compliant)
static const int16_t ALAW_TO_PCM[256] = {
    -5504, -5248, -6016, -5760, -4480, -4224, -4992, -4736,
    -7552, -7296, -8064, -7808, -6528, -6272, -7040, -6784,
    -2752, -2624, -3008, -2880, -2240, -2112, -2496, -2368,
    -3776, -3648, -4032, -3904, -3264, -3136, -3520, -3392,
    -22016,-20992,-24064,-23040,-17920,-16896,-19968,-18944,
    -30208,-29184,-32256,-31232,-26112,-25088,-28160,-27136,
    -11008,-10496,-12032,-11520, -8960, -8448, -9984, -9472,
    -15104,-14592,-16128,-15616,-13056,-12544,-14080,-13568,
    -344,  -328,  -376,  -360,  -280,  -264,  -312,  -296,
    -472,  -456,  -504,  -488,  -408,  -392,  -440,  -424,
    -88,   -72,   -120,  -104,  -24,   -8,    -56,   -40,
    -216,  -200,  -248,  -232,  -152,  -136,  -184,  -168,
    -1376, -1312, -1504, -1440, -1120, -1056, -1248, -1184,
    -1888, -1824, -2016, -1952, -1632, -1568, -1760, -1696,
    -688,  -656,  -752,  -720,  -560,  -528,  -624,  -592,
    -944,  -912,  -1008, -976,  -816,  -784,  -880,  -848,
     5504,  5248,  6016,  5760,  4480,  4224,  4992,  4736,
     7552,  7296,  8064,  7808,  6528,  6272,  7040,  6784,
     2752,  2624,  3008,  2880,  2240,  2112,  2496,  2368,
     3776,  3648,  4032,  3904,  3264,  3136,  3520,  3392,
     22016, 20992, 24064, 23040, 17920, 16896, 19968, 18944,
     30208, 29184, 32256, 31232, 26112, 25088, 28160, 27136,
     11008, 10496, 12032, 11520,  8960,  8448,  9984,  9472,
     15104, 14592, 16128, 15616, 13056, 12544, 14080, 13568,
      344,   328,   376,   360,   280,   264,   312,   296,
      472,   456,   504,   488,   408,   392,   440,   424,
       88,    72,   120,   104,    24,     8,    56,    40,
      216,   200,   248,   232,   152,   136,   184,   168,
     1376,  1312,  1504,  1440,  1120,  1056,  1248,  1184,
     1888,  1824,  2016,  1952,  1632,  1568,  1760,  1696,
      688,   656,   752,   720,   560,   528,   624,   592,
      944,   912,  1008,   976,   816,   784,   880,   848
};

// Direct conversion function
size_t convertALawSamples(const std::vector<uint8_t>& input_data, 
                         std::vector<int16_t>& output_samples) {
    const size_t input_samples = input_data.size();
    
    if (input_samples == 0) {
        output_samples.clear();
        return 0;
    }
    
    output_samples.reserve(input_samples);
    output_samples.clear();
    
    for (size_t i = 0; i < input_samples; ++i) {
        const uint8_t alaw_sample = input_data[i];
        const int16_t pcm_sample = ALAW_TO_PCM[alaw_sample];
        output_samples.push_back(pcm_sample);
    }
    
    return input_samples;
}

int main() {
    std::cout << "A-law Direct Sample Conversion Test:\n";
    
    // Test data: A-law closest-to-silence (0x55) and some other values
    std::vector<uint8_t> input_data = {0x55, 0x00, 0x80, 0xFF, 0x54, 0x56};
    std::vector<int16_t> output_samples;
    
    size_t converted = convertALawSamples(input_data, output_samples);
    
    std::cout << "Input samples: " << input_data.size() << "\n";
    std::cout << "Output samples: " << output_samples.size() << "\n";
    std::cout << "Converted samples: " << converted << "\n";
    
    // Verify conversion results
    if (output_samples.size() != input_data.size()) {
        std::cout << "ERROR: Output size mismatch\n";
        return 1;
    }
    
    // Test specific values
    std::cout << "\nSample conversions:\n";
    for (size_t i = 0; i < input_data.size(); ++i) {
        std::cout << "A-law 0x" << std::hex << (int)input_data[i] 
                  << " -> PCM " << std::dec << output_samples[i] << "\n";
    }
    
    // Test A-law closest-to-silence (0x55 should map to -8 per ITU-T G.711)
    if (output_samples[0] != -8) {
        std::cout << "ERROR: A-law closest-to-silence (0x55) should map to -8, got " 
                  << output_samples[0] << "\n";
        return 1;
    }
    
    // Test multi-channel processing (stereo simulation)
    std::cout << "\nTesting stereo (multi-channel) processing:\n";
    std::vector<uint8_t> stereo_input = {0x55, 0x00, 0x80, 0xFF}; // L0, R0, L1, R1
    std::vector<int16_t> stereo_output;
    
    size_t stereo_converted = convertALawSamples(stereo_input, stereo_output);
    
    std::cout << "Stereo input samples: " << stereo_input.size() << "\n";
    std::cout << "Stereo output samples: " << stereo_output.size() << "\n";
    std::cout << "Stereo converted: " << stereo_converted << "\n";
    
    // Verify stereo interleaving is preserved
    std::cout << "Stereo sample pairs:\n";
    for (size_t i = 0; i < stereo_input.size(); i += 2) {
        if (i + 1 < stereo_input.size()) {
            std::cout << "L: A-law 0x" << std::hex << (int)stereo_input[i] 
                      << " -> PCM " << std::dec << stereo_output[i]
                      << ", R: A-law 0x" << std::hex << (int)stereo_input[i+1] 
                      << " -> PCM " << std::dec << stereo_output[i+1] << "\n";
        }
    }
    
    // Test empty input handling
    std::cout << "\nTesting empty input handling:\n";
    std::vector<uint8_t> empty_input;
    std::vector<int16_t> empty_output;
    
    size_t empty_converted = convertALawSamples(empty_input, empty_output);
    
    if (!empty_output.empty() || empty_converted != 0) {
        std::cout << "ERROR: Empty input should produce empty output\n";
        return 1;
    }
    std::cout << "Empty input handled correctly\n";
    
    // Test variable chunk sizes (VoIP packet simulation)
    std::cout << "\nTesting variable chunk sizes (VoIP simulation):\n";
    std::vector<size_t> chunk_sizes = {1, 8, 20, 160, 320}; // Common VoIP packet sizes
    
    for (size_t chunk_size : chunk_sizes) {
        std::vector<uint8_t> chunk_input(chunk_size, 0x55); // Fill with closest-to-silence
        std::vector<int16_t> chunk_output;
        
        size_t chunk_converted = convertALawSamples(chunk_input, chunk_output);
        
        std::cout << "Chunk size " << chunk_size << ": output samples " 
                  << chunk_output.size() << ", converted " << chunk_converted << "\n";
        
        if (chunk_output.size() != chunk_size || chunk_converted != chunk_size) {
            std::cout << "ERROR: Chunk size " << chunk_size << " processing failed\n";
            return 1;
        }
    }
    
    // Test all 256 A-law values for completeness
    std::cout << "\nTesting all 256 A-law values:\n";
    std::vector<uint8_t> all_values(256);
    for (int i = 0; i < 256; ++i) {
        all_values[i] = static_cast<uint8_t>(i);
    }
    
    std::vector<int16_t> all_outputs;
    size_t all_converted = convertALawSamples(all_values, all_outputs);
    
    if (all_outputs.size() != 256 || all_converted != 256) {
        std::cout << "ERROR: All values test failed\n";
        return 1;
    }
    
    // Verify some key values
    if (all_outputs[0x55] != -8) {
        std::cout << "ERROR: A-law 0x55 should map to -8, got " << all_outputs[0x55] << "\n";
        return 1;
    }
    
    if (all_outputs[0x00] != -5504) {
        std::cout << "ERROR: A-law 0x00 should map to -5504, got " << all_outputs[0x00] << "\n";
        return 1;
    }
    
    if (all_outputs[0x80] != 5504) {
        std::cout << "ERROR: A-law 0x80 should map to 5504, got " << all_outputs[0x80] << "\n";
        return 1;
    }
    
    std::cout << "All 256 A-law values converted successfully\n";
    
    std::cout << "\nAll A-law sample conversion tests passed!\n";
    return 0;
}