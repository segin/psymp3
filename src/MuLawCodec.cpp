/*
 * MuLawCodec.cpp - μ-law (G.711 μ-law) audio codec implementation
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

#ifdef ENABLE_MULAW_CODEC

// Static member initialization
bool MuLawCodec::s_table_initialized = false;

// μ-law to 16-bit PCM conversion lookup table
// ITU-T G.711 μ-law compliant values
const int16_t MuLawCodec::MULAW_TO_PCM[256] = {
    -32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
    -23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
    -15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
    -11900, -11388, -10876, -10364,  -9852,  -9340,  -8828,  -8316,
     -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
     -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
     -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
     -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
     -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
     -1372,  -1308,  -1244,  -1180,  -1116,  -1052,   -988,   -924,
      -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
      -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
      -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
      -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
      -120,   -112,   -104,    -96,    -88,    -80,    -72,    -64,
       -56,    -48,    -40,    -32,    -24,    -16,     -8,      0,
     32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
     23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
     15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
     11900,  11388,  10876,  10364,   9852,   9340,   8828,   8316,
      7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
      5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
      3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
      2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
      1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
      1372,   1308,   1244,   1180,   1116,   1052,    988,    924,
       876,    844,    812,    780,    748,    716,    684,    652,
       620,    588,    556,    524,    492,    460,    428,    396,
       372,    356,    340,    324,    308,    292,    276,    260,
       244,    228,    212,    196,    180,    164,    148,    132,
       120,    112,    104,     96,     88,     80,     72,     64,
        56,     48,     40,     32,     24,     16,      8,      0
};

MuLawCodec::MuLawCodec(const StreamInfo& stream_info) 
    : SimplePCMCodec(stream_info) {
    // Ensure lookup table is initialized
    if (!s_table_initialized) {
        initializeMuLawTable();
    }
}

bool MuLawCodec::canDecode(const StreamInfo& stream_info) const {
    // First check: Must be audio stream with μ-law codec name
    if (stream_info.codec_type != "audio") {
        return false;
    }
    
    // Accept various μ-law format identifiers
    bool is_mulaw_codec = (stream_info.codec_name == "mulaw" || 
                          stream_info.codec_name == "pcm_mulaw" ||
                          stream_info.codec_name == "g711_mulaw");
    
    if (!is_mulaw_codec) {
        return false;
    }
    
    // Validate μ-law specific parameters
    // μ-law uses 8-bit samples (1 byte per sample)
    if (stream_info.bits_per_sample != 0 && stream_info.bits_per_sample != 8) {
        Debug::log("MuLawCodec: Rejecting stream - μ-law requires 8 bits per sample, got %d", 
                   stream_info.bits_per_sample);
        return false;
    }
    
    // Validate sample rate - μ-law supports telephony rates and common audio rates
    if (stream_info.sample_rate != 0) {
        // Common telephony and audio sample rates for μ-law
        bool valid_sample_rate = (stream_info.sample_rate == 8000 ||   // Standard telephony
                                 stream_info.sample_rate == 16000 ||   // Wideband telephony
                                 stream_info.sample_rate == 32000 ||   // Super-wideband
                                 stream_info.sample_rate == 44100 ||   // CD quality
                                 stream_info.sample_rate == 48000);    // Professional audio
        
        if (!valid_sample_rate) {
            Debug::log("MuLawCodec: Warning - Unusual sample rate %d Hz for μ-law stream", 
                       stream_info.sample_rate);
            // Don't reject - allow unusual sample rates but log warning
        }
    }
    
    // Validate channel count - μ-law typically mono but can support stereo
    if (stream_info.channels != 0) {
        if (stream_info.channels > 2) {
            Debug::log("MuLawCodec: Rejecting stream - μ-law supports max 2 channels, got %d", 
                       stream_info.channels);
            return false;
        }
        
        if (stream_info.channels == 0) {
            Debug::log("MuLawCodec: Rejecting stream - Invalid channel count: ", 0);
            return false;
        }
    }
    
    // All validation passed
    return true;
}

std::string MuLawCodec::getCodecName() const {
    return "mulaw";
}

size_t MuLawCodec::convertSamples(const std::vector<uint8_t>& input_data, 
                                 std::vector<int16_t>& output_samples) {
    const size_t input_samples = input_data.size();
    
    // Ensure output vector has sufficient capacity
    output_samples.reserve(input_samples);
    output_samples.clear();
    
    // Convert each μ-law sample to 16-bit PCM using lookup table
    for (size_t i = 0; i < input_samples; ++i) {
        const uint8_t mulaw_sample = input_data[i];
        const int16_t pcm_sample = MULAW_TO_PCM[mulaw_sample];
        output_samples.push_back(pcm_sample);
    }
    
    return input_samples;
}

size_t MuLawCodec::getBytesPerInputSample() const {
    return 1; // μ-law uses 8-bit samples
}

void MuLawCodec::initializeMuLawTable() {
    // Table is statically initialized with ITU-T G.711 μ-law values
    // This method serves as a one-time initialization checkpoint
    
    if (s_table_initialized) {
        return; // Already initialized
    }
    
    // Validate critical values for ITU-T G.711 compliance
    // μ-law silence value (0xFF) should map to 0
    if (MULAW_TO_PCM[0xFF] != 0) {
        Debug::log("MuLawCodec: Warning - μ-law silence value (0xFF) does not map to ", 0);
    }
    
    // Validate sign bit handling - values 0x00-0x7F should be negative
    if (MULAW_TO_PCM[0x00] >= 0 || MULAW_TO_PCM[0x7F] >= 0) {
        Debug::log("MuLawCodec: Warning - μ-law sign bit handling may be ", "incorrect");
    }
    
    // Validate sign bit handling - values 0x80-0xFE should be positive  
    if (MULAW_TO_PCM[0x80] <= 0 || MULAW_TO_PCM[0xFE] <= 0) {
        Debug::log("MuLawCodec: Warning - μ-law sign bit handling may be ", "incorrect");
    }
    
    Debug::log("MuLawCodec: ITU-T G.711 μ-law lookup table initialized ", "successfully");
    s_table_initialized = true;
}

void registerMuLawCodec() {
    // Register μ-law codec with multiple format identifiers
    AudioCodecFactory::registerCodec("mulaw", [](const StreamInfo& stream_info) {
        return std::make_unique<MuLawCodec>(stream_info);
    });
    
    AudioCodecFactory::registerCodec("pcm_mulaw", [](const StreamInfo& stream_info) {
        return std::make_unique<MuLawCodec>(stream_info);
    });
    
    AudioCodecFactory::registerCodec("g711_mulaw", [](const StreamInfo& stream_info) {
        return std::make_unique<MuLawCodec>(stream_info);
    });
}

#endif // ENABLE_MULAW_CODEC