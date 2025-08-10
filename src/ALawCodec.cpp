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
    // Accept various A-law format identifiers
    return stream_info.codec_name == "alaw" || 
           stream_info.codec_name == "pcm_alaw" ||
           stream_info.codec_name == "g711_alaw";
}

std::string ALawCodec::getCodecName() const {
    return "alaw";
}

size_t ALawCodec::convertSamples(const std::vector<uint8_t>& input_data, 
                                std::vector<int16_t>& output_samples) {
    const size_t input_samples = input_data.size();
    
    // Ensure output vector has sufficient capacity
    output_samples.reserve(input_samples);
    output_samples.clear();
    
    // Convert each A-law sample to 16-bit PCM using lookup table
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
    // A-law silence value (0x55) should map to 0
    if (ALAW_TO_PCM[0x55] != 0) {
        Debug::log("ALawCodec: Warning - A-law silence value (0x55) does not map to ", 0);
    }
    
    // Validate sign bit handling - values with bit 7 clear (0x00-0x7F) should be negative
    if (ALAW_TO_PCM[0x00] >= 0 || ALAW_TO_PCM[0x7F] >= 0) {
        Debug::log("ALawCodec: Warning - A-law sign bit handling may be ", "incorrect");
    }
    
    // Validate sign bit handling - values with bit 7 set (0x80-0xFF) should be positive
    if (ALAW_TO_PCM[0x80] <= 0 || ALAW_TO_PCM[0xFF] <= 0) {
        Debug::log("ALawCodec: Warning - A-law sign bit handling may be ", "incorrect");
    }
    
    // Validate even-bit inversion characteristic of A-law
    // A-law inverts even bits, so 0x54 and 0x56 should have different signs than 0x55
    if ((ALAW_TO_PCM[0x54] >= 0) || (ALAW_TO_PCM[0x56] >= 0)) {
        Debug::log("ALawCodec: Warning - A-law even-bit inversion may be ", "incorrect");
    }
    
    Debug::log("ALawCodec: ITU-T G.711 A-law lookup table initialized ", "successfully");
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