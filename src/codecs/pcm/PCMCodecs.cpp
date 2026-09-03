/*
 * PCMCodecs.cpp - PCM and PCM-variant audio codec implementations
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {

// SimplePCMCodec implementation
SimplePCMCodec::SimplePCMCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
}

bool SimplePCMCodec::initialize() {
    m_initialized = true;
    return true;
}

AudioFrame SimplePCMCodec::decode(const MediaChunk& chunk) {
    AudioFrame frame;
    
    if (chunk.data.empty()) {
        return frame; // Empty frame
    }
    
    // Set frame properties
    frame.sample_rate = m_stream_info.sample_rate;
    frame.channels = m_stream_info.channels;
    frame.timestamp_samples = chunk.timestamp_samples;
    
    // Convert samples
    convertSamples(chunk.data, frame.samples);
    
    return frame;
}

AudioFrame SimplePCMCodec::flush() {
    // Simple PCM codecs don't buffer data
    return AudioFrame{}; // Empty frame
}

void SimplePCMCodec::reset() {
    // Simple PCM codecs don't have state to reset
}

} // namespace Codec

namespace Codec {
namespace PCM {

// PCMCodec implementation
PCMCodec::PCMCodec(const StreamInfo& stream_info) 
    : SimplePCMCodec(stream_info) {
    detectPCMFormat();
}

bool PCMCodec::canDecode(const StreamInfo& stream_info) const {
    if (stream_info.codec_name != "pcm") {
        return false;
    }
    
    // Check supported bit depths
    switch (stream_info.bits_per_sample) {
        case 8:
        case 16:
        case 24:
        case 32:
            return true;
        default:
            return false;
    }
}

size_t PCMCodec::convertSamples(const std::vector<uint8_t>& input_data, 
                               std::vector<AudioSample>& output_samples) {
    const uint8_t* input_ptr = input_data.data();
    size_t input_size = input_data.size();
    size_t bytes_per_sample = getBytesPerInputSample();
    
    // Truncate to aligned boundary to prevent over-reads on trailing bytes
    input_size -= input_size % bytes_per_sample;
    if (input_size == 0) {
        output_samples.clear();
        return 0;
    }
    
    size_t num_samples = input_size / bytes_per_sample;
    
    output_samples.resize(num_samples);
    
    // Everything lands as full-scale S32, so each depth is scaled up rather
    // than down. 24- and 32-bit sources used to be shifted DOWN into int16,
    // which threw away the extra resolution they exist to carry.
    switch (m_pcm_format) {
        case PCMFormat::PCM_8_UNSIGNED:
            for (size_t i = 0; i < num_samples; ++i) {
                // 8-bit unsigned -> signed, then up to full scale. Multiply
                // rather than shift: shifting a negative value left is UB.
                output_samples[i] =
                    static_cast<AudioSample>((static_cast<int32_t>(input_ptr[i]) - 128) * 16777216);
            }
            break;

        case PCMFormat::PCM_16_SIGNED:
            for (size_t i = 0; i < num_samples; ++i) {
                int16_t sample16;
                std::memcpy(&sample16, &input_ptr[i * 2], sizeof(int16_t));
                output_samples[i] = static_cast<AudioSample>(sample16) * 65536;
            }
            break;

        case PCMFormat::PCM_24_SIGNED:
            for (size_t i = 0; i < num_samples; ++i) {
                // Build the 24-bit value (little-endian) unsigned, then
                // sign-extend from bit 23 and scale to full range.
                uint32_t raw = (static_cast<uint32_t>(input_ptr[i*3 + 2]) << 16) |
                               (static_cast<uint32_t>(input_ptr[i*3 + 1]) << 8) |
                                static_cast<uint32_t>(input_ptr[i*3]);
                int32_t sample24 = (raw & 0x800000u) ? static_cast<int32_t>(raw | 0xFF000000u)
                                                     : static_cast<int32_t>(raw);
                output_samples[i] = static_cast<AudioSample>(sample24) * 256;
            }
            break;

        case PCMFormat::PCM_32_SIGNED:
            for (size_t i = 0; i < num_samples; ++i) {
                int32_t sample32;
                std::memcpy(&sample32, &input_ptr[i*4], sizeof(int32_t));
                output_samples[i] = static_cast<AudioSample>(sample32); // already full scale
            }
            break;

        case PCMFormat::PCM_32_FLOAT:
            for (size_t i = 0; i < num_samples; ++i) {
                float sample_float;
                std::memcpy(&sample_float, &input_ptr[i*4], sizeof(float));
                sample_float = std::clamp(sample_float, -1.0f, 1.0f);
                output_samples[i] = static_cast<AudioSample>(sample_float * 2147483520.0f);
            }
            break;
    }

    return num_samples;
}

size_t PCMCodec::getBytesPerInputSample() const {
    return m_stream_info.bits_per_sample / 8;
}

void PCMCodec::detectPCMFormat() {
    switch (m_stream_info.bits_per_sample) {
        case 8:
            m_pcm_format = PCMFormat::PCM_8_UNSIGNED;
            break;
        case 16:
            m_pcm_format = PCMFormat::PCM_16_SIGNED;
            break;
        case 24:
            m_pcm_format = PCMFormat::PCM_24_SIGNED;
            break;
        case 32:
            // Check codec tag to distinguish between int32 and float32
            if (m_stream_info.codec_tag == 0x0003) { // IEEE_FLOAT
                m_pcm_format = PCMFormat::PCM_32_FLOAT;
            } else {
                m_pcm_format = PCMFormat::PCM_32_SIGNED;
            }
            break;
        default:
            m_pcm_format = PCMFormat::PCM_16_SIGNED; // Default fallback
            break;
    }
}
} // namespace PCM
} // namespace Codec
} // namespace PsyMP3
