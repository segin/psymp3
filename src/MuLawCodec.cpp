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
// Will be populated by initializeMuLawTable()
const int16_t MuLawCodec::MULAW_TO_PCM[256] = {0}; // Placeholder - will be initialized

MuLawCodec::MuLawCodec(const StreamInfo& stream_info) 
    : SimplePCMCodec(stream_info) {
    // Ensure lookup table is initialized
    if (!s_table_initialized) {
        initializeMuLawTable();
    }
}

bool MuLawCodec::canDecode(const StreamInfo& stream_info) const {
    // Accept various μ-law format identifiers
    return stream_info.codec_name == "mulaw" || 
           stream_info.codec_name == "pcm_mulaw" ||
           stream_info.codec_name == "g711_mulaw";
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
    // TODO: Implement ITU-T G.711 μ-law lookup table initialization
    // This will be implemented in task 3
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