/*
 * test_alaw_minimal.cpp - Minimal A-law conversion test
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>
#include <vector>
#include <cstdint>
#include <cstdio>

// Minimal includes to avoid dependencies
struct StreamInfo {
    std::string codec_type;
    std::string codec_name;
    int sample_rate = 0;
    int channels = 0;
    int bits_per_sample = 0;
};

// Mock Debug class
class Debug {
public:
    template<typename... Args>
    static void log(const char* format, Args... args) {
        printf(format, args...);
        printf("\n");
    }
};

// Minimal SimplePCMCodec base class for testing
class SimplePCMCodec {
protected:
    StreamInfo m_stream_info;
    
public:
    explicit SimplePCMCodec(const StreamInfo& stream_info) : m_stream_info(stream_info) {}
    virtual ~SimplePCMCodec() = default;
    
    virtual size_t convertSamples(const std::vector<uint8_t>& input_data, 
                                  std::vector<int16_t>& output_samples) = 0;
    virtual size_t getBytesPerInputSample() const = 0;
};

// Include only the ALawCodec class definition and implementation
class ALawCodec : public SimplePCMCodec {
public:
    explicit ALawCodec(const StreamInfo& stream_info);
    bool canDecode(const StreamInfo& stream_info) const;
    std::string getCodecName() const;

protected:
    size_t convertSamples(const std::vector<uint8_t>& input_data, 
                         std::vector<int16_t>& output_samples) override;
    size_t getBytesPerInputSample() const override;

private:
    static const int16_t ALAW_TO_PCM[256];
    static void initializeALawTable();
    static bool s_table_initialized;
};

// Static member initialization
bool ALawCodec::s_table_initialized = false;

// A-law to 16-bit PCM conversion lookup table (ITU-T G.711 A-law compliant values)
const int16_t ALawCodec::ALAW_TO_PCM[256] = {
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

ALawCodec::ALawCodec(const StreamInfo& stream_info) 
    : SimplePCMCodec(stream_info) {
    if (!s_table_initialized) {
        initializeALawTable();
    }
}

bool ALawCodec::canDecode(const StreamInfo& stream_info) const {
    if (stream_info.codec_type != "audio") {
        return false;
    }
    
    bool is_alaw_codec = (stream_info.codec_name == "alaw" || 
                         stream_info.codec_name == "pcm_alaw" ||
                         stream_info.codec_name == "g711_alaw");
    
    if (!is_alaw_codec) {
        return false;
    }
    
    if (stream_info.bits_per_sample != 0 && stream_info.bits_per_sample != 8) {
        Debug::log("ALawCodec: Rejecting stream - A-law requires 8 bits per sample, got %d", 
                   stream_info.bits_per_sample);
        return false;
    }
    
    if (stream_info.sample_rate != 0) {
        bool valid_sample_rate = (stream_info.sample_rate == 8000 ||
                                 stream_info.sample_rate == 16000 ||
                                 stream_info.sample_rate == 32000 ||
                                 stream_info.sample_rate == 44100 ||
                                 stream_info.sample_rate == 48000);
        
        if (!valid_sample_rate) {
            Debug::log("ALawCodec: Warning - Unusual sample rate %d Hz for A-law stream", 
                       stream_info.sample_rate);
        }
    }
    
    if (stream_info.channels != 0) {
        if (stream_info.channels > 2) {
            Debug::log("ALawCodec: Rejecting stream - A-law supports max 2 channels, got %d", 
                       stream_info.channels);
            return false;
        }
        
        if (stream_info.channels == 0) {
            Debug::log("ALawCodec: Rejecting stream - Invalid channel count: %d", 0);
            return false;
        }
    }
    
    return true;
}

std::string ALawCodec::getCodecName() const {
    return "alaw";
}

size_t ALawCodec::convertSamples(const std::vector<uint8_t>& input_data, 
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

size_t ALawCodec::getBytesPerInputSample() const {
    return 1;
}

void ALawCodec::initializeALawTable() {
    if (s_table_initialized) {
        return;
    }
    
    if (ALAW_TO_PCM[0x55] != -8) {
        Debug::log("ALawCodec: Warning - A-law closest-to-silence value (0x55) should map to -8");
    }
    
    if (ALAW_TO_PCM[0x00] >= 0 || ALAW_TO_PCM[0x7F] >= 0) {
        Debug::log("ALawCodec: Warning - A-law sign bit handling may be incorrect");
    }
    
    if (ALAW_TO_PCM[0x80] <= 0 || ALAW_TO_PCM[0xFF] <= 0) {
        Debug::log("ALawCodec: Warning - A-law sign bit handling may be incorrect");
    }
    
    if ((ALAW_TO_PCM[0x54] >= 0) || (ALAW_TO_PCM[0x56] >= 0)) {
        Debug::log("ALawCodec: Warning - A-law even-bit inversion may be incorrect");
    }
    
    Debug::log("ALawCodec: ITU-T G.711 A-law lookup table initialized successfully");
    s_table_initialized = true;
}

// Test class to access protected convertSamples method
class TestALawCodec : public ALawCodec {
public:
    explicit TestALawCodec(const StreamInfo& stream_info) : ALawCodec(stream_info) {}
    
    size_t testConvertSamples(const std::vector<uint8_t>& input_data, 
                             std::vector<int16_t>& output_samples) {
        return convertSamples(input_data, output_samples);
    }
};

int main() {
    printf("A-law Sample Conversion Test:\n");
    
    StreamInfo stream_info;
    stream_info.codec_type = "audio";
    stream_info.codec_name = "alaw";
    stream_info.sample_rate = 8000;
    stream_info.channels = 1;
    stream_info.bits_per_sample = 8;
    
    TestALawCodec codec(stream_info);
    
    // Test data: A-law closest-to-silence (0x55) and some other values
    std::vector<uint8_t> input_data = {0x55, 0x00, 0x80, 0xFF, 0x54, 0x56};
    std::vector<int16_t> output_samples;
    
    size_t converted = codec.testConvertSamples(input_data, output_samples);
    
    printf("Input samples: %zu\n", input_data.size());
    printf("Converted samples: %zu\n", converted);
    printf("Output samples: %zu\n", output_samples.size());
    
    if (converted != input_data.size() || output_samples.size() != input_data.size()) {
        printf("ERROR: Size mismatch\n");
        return 1;
    }
    
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
    std::vector<size_t> chunk_sizes = {1, 8, 20, 160, 320};
    
    for (size_t chunk_size : chunk_sizes) {
        std::vector<uint8_t> chunk_input(chunk_size, 0x55); // Fill with closest-to-silence
        std::vector<int16_t> chunk_output;
        
        size_t chunk_converted = codec.testConvertSamples(chunk_input, chunk_output);
        
        printf("Chunk size %zu: converted %zu samples\n", chunk_size, chunk_converted);
        
        if (chunk_converted != chunk_size || chunk_output.size() != chunk_size) {
            printf("ERROR: Chunk size %zu processing failed\n", chunk_size);
            return 1;
        }
        
        for (size_t j = 0; j < chunk_output.size(); ++j) {
            if (chunk_output[j] != -8) {
                printf("ERROR: Expected closest-to-silence (-8) at index %zu, got %d\n", j, chunk_output[j]);
                return 1;
            }
        }
    }
    
    printf("\nAll A-law sample conversion tests passed!\n");
    return 0;
}