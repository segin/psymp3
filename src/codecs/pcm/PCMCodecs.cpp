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
                               std::vector<int16_t>& output_samples) {
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
    
    switch (m_pcm_format) {
        case PCMFormat::PCM_8_UNSIGNED:
            for (size_t i = 0; i < num_samples; ++i) {
                // Convert 8-bit unsigned to 16-bit signed
                output_samples[i] = (static_cast<int16_t>(input_ptr[i]) - 128) << 8;
            }
            break;
            
        case PCMFormat::PCM_16_SIGNED:
            // Direct copy (little-endian assumed)
            std::memcpy(output_samples.data(), input_ptr, num_samples * sizeof(int16_t));
            break;
            
        case PCMFormat::PCM_24_SIGNED:
            for (size_t i = 0; i < num_samples; ++i) {
                // Convert 24-bit to 16-bit (little-endian)
                int32_t sample24 = (static_cast<int8_t>(input_ptr[i*3 + 2]) << 16) |
                                  (static_cast<uint8_t>(input_ptr[i*3 + 1]) << 8) |
                                  static_cast<uint8_t>(input_ptr[i*3]);
                output_samples[i] = static_cast<int16_t>(sample24 >> 8);
            }
            break;
            
        case PCMFormat::PCM_32_SIGNED:
            for (size_t i = 0; i < num_samples; ++i) {
                // Convert 32-bit to 16-bit
                int32_t sample32;
                std::memcpy(&sample32, &input_ptr[i*4], sizeof(int32_t));
                output_samples[i] = static_cast<int16_t>(sample32 >> 16);
            }
            break;
            
        case PCMFormat::PCM_32_FLOAT:
            for (size_t i = 0; i < num_samples; ++i) {
                // Convert 32-bit float to 16-bit signed
                float sample_float;
                std::memcpy(&sample_float, &input_ptr[i*4], sizeof(float));
                sample_float = std::clamp(sample_float, -1.0f, 1.0f);
                output_samples[i] = static_cast<int16_t>(sample_float * 32767.0f);
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
