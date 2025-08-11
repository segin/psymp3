/*
 * ALawCodec.cpp - A-law (G.711 A-law) audio codec implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"

#ifdef ENABLE_ALAW_CODEC

// Static member initialization
bool ALawCodec::s_table_initialized = false;

// A-law to 16-bit PCM conversion lookup table
// ITU-T G.711 A-law compliant values
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
    // Ensure lookup table is initialized
    if (!s_table_initialized) {
        initializeALawTable();
    }
}

bool ALawCodec::canDecode(const StreamInfo& stream_info) const {
    // First check: Must be audio stream with A-law codec name
    if (stream_info.codec_type != "audio") {
        return false;
    }
    
    // Accept various A-law format identifiers
    bool is_alaw_codec = (stream_info.codec_name == "alaw" || 
                         stream_info.codec_name == "pcm_alaw" ||
                         stream_info.codec_name == "g711_alaw");
    
    if (!is_alaw_codec) {
        return false;
    }
    
    // Validate A-law specific parameters
    // A-law uses 8-bit samples (1 byte per sample)
    if (stream_info.bits_per_sample != 0 && stream_info.bits_per_sample != 8) {
        Debug::log("ALawCodec", "Rejecting stream - A-law requires 8 bits per sample, got ", stream_info.bits_per_sample);
        return false;
    }
    
    // Validate sample rate - A-law supports telephony rates and common audio rates
    if (stream_info.sample_rate != 0) {
        // Common telephony and audio sample rates for A-law
        bool valid_sample_rate = (stream_info.sample_rate == 8000 ||   // Standard telephony
                                 stream_info.sample_rate == 16000 ||   // Wideband telephony
                                 stream_info.sample_rate == 32000 ||   // Super-wideband
                                 stream_info.sample_rate == 44100 ||   // CD quality
                                 stream_info.sample_rate == 48000);    // Professional audio
        
        if (!valid_sample_rate) {
            Debug::log("ALawCodec", "Warning - Unusual sample rate ", stream_info.sample_rate, " Hz for A-law stream");
            // Don't reject - allow unusual sample rates but log warning
        }
    }
    
    // Validate channel count - A-law typically mono but can support stereo
    if (stream_info.channels != 0) {
        if (stream_info.channels > 2) {
            Debug::log("ALawCodec", "Rejecting stream - A-law supports max 2 channels, got ", stream_info.channels);
            return false;
        }
        
        if (stream_info.channels == 0) {
            Debug::log("ALawCodec", "Rejecting stream - Invalid channel count: ", stream_info.channels);
            return false;
        }
    }
    
    // All validation passed
    return true;
}

std::string ALawCodec::getCodecName() const {
    return "alaw";
}

size_t ALawCodec::convertSamples(const std::vector<uint8_t>& input_data, 
                                std::vector<int16_t>& output_samples) {
    const size_t input_samples = input_data.size();
    
    // Handle empty input gracefully
    if (input_samples == 0) {
        output_samples.clear();
        return 0;
    }
    
    // Ensure output vector has sufficient capacity for optimal performance
    output_samples.reserve(input_samples);
    output_samples.clear();
    
    // Convert each A-law sample to 16-bit PCM using lookup table
    // For multi-channel audio, samples are processed in interleaved order:
    // Mono: [sample0, sample1, sample2, ...]
    // Stereo: [L0, R0, L1, R1, L2, R2, ...]
    // This maintains proper channel interleaving in the output
    for (size_t i = 0; i < input_samples; ++i) {
        const uint8_t alaw_sample = input_data[i];
        const int16_t pcm_sample = ALAW_TO_PCM[alaw_sample];
        output_samples.push_back(pcm_sample);
    }
    
    return input_samples;
}

size_t ALawCodec::getBytesPerInputSample() const {
    return 1; // A-law uses 8-bit samples
}

void ALawCodec::initializeALawTable() {
    // Table is statically initialized with ITU-T G.711 A-law values
    // This method serves as a one-time initialization checkpoint
    
    if (s_table_initialized) {
        return; // Already initialized
    }
    
    // Validate critical values for ITU-T G.711 compliance
    // A-law closest-to-silence value (0x55) should map to -8 (ITU-T G.711 compliant)
    if (ALAW_TO_PCM[0x55] != -8) {
        Debug::log("ALawCodec", "Warning - A-law closest-to-silence value (0x55) should map to -8, got ", ALAW_TO_PCM[0x55]);
    }
    
    // Validate sign bit handling - values with bit 7 clear (0x00-0x7F) should be negative
    if (ALAW_TO_PCM[0x00] >= 0 || ALAW_TO_PCM[0x7F] >= 0) {
        Debug::log("ALawCodec", "Warning - A-law sign bit handling may be incorrect");
    }
    
    // Validate sign bit handling - values with bit 7 set (0x80-0xFF) should be positive
    if (ALAW_TO_PCM[0x80] <= 0 || ALAW_TO_PCM[0xFF] <= 0) {
        Debug::log("ALawCodec", "Warning - A-law sign bit handling may be incorrect");
    }
    
    // Validate even-bit inversion characteristic of A-law
    // A-law inverts even bits during encoding, verify adjacent values
    if (ALAW_TO_PCM[0x54] != -24 || ALAW_TO_PCM[0x56] != -56) {
        Debug::log("ALawCodec", "Warning - A-law even-bit inversion values may be incorrect");
    }
    
    Debug::log("ALawCodec", "ITU-T G.711 A-law lookup table initialized successfully");
    s_table_initialized = true;
}

void registerALawCodec() {
    // Register A-law codec with multiple format identifiers
    AudioCodecFactory::registerCodec("alaw", [](const StreamInfo& stream_info) {
        return std::make_unique<ALawCodec>(stream_info);
    });
    
    AudioCodecFactory::registerCodec("pcm_alaw", [](const StreamInfo& stream_info) {
        return std::make_unique<ALawCodec>(stream_info);
    });
    
    AudioCodecFactory::registerCodec("g711_alaw", [](const StreamInfo& stream_info) {
        return std::make_unique<ALawCodec>(stream_info);
    });
}

#endif // ENABLE_ALAW_CODEC