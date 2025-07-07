/*
 * PCMCodecs.cpp - PCM and PCM-variant audio codec implementations
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "PCMCodecs.h"
#include "libmpg123w.h" // For MP3PassthroughCodec
#include <algorithm>
#include <cstring>

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

// ALawCodec implementation
ALawCodec::ALawCodec(const StreamInfo& stream_info) 
    : SimplePCMCodec(stream_info) {
}

bool ALawCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "alaw" && stream_info.bits_per_sample == 8;
}

size_t ALawCodec::convertSamples(const std::vector<uint8_t>& input_data, 
                                std::vector<int16_t>& output_samples) {
    size_t num_samples = input_data.size();
    output_samples.resize(num_samples);
    
    for (size_t i = 0; i < num_samples; ++i) {
        output_samples[i] = alaw2linear(input_data[i]);
    }
    
    return num_samples;
}

int16_t ALawCodec::alaw2linear(uint8_t alaw_sample) {
    int16_t sign, exponent, mantissa, sample;
    
    alaw_sample ^= 0x55;
    sign = (alaw_sample & 0x80);
    exponent = (alaw_sample >> 4) & 0x07;
    mantissa = alaw_sample & 0x0F;
    
    if (exponent == 0) {
        sample = mantissa << 4;
    } else {
        sample = ((mantissa << 4) | 0x100) << (exponent - 1);
    }
    
    if (sign == 0) {
        sample = -sample;
    }
    
    return sample;
}

// MuLawCodec implementation
MuLawCodec::MuLawCodec(const StreamInfo& stream_info) 
    : SimplePCMCodec(stream_info) {
}

bool MuLawCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "mulaw" && stream_info.bits_per_sample == 8;
}

size_t MuLawCodec::convertSamples(const std::vector<uint8_t>& input_data, 
                                 std::vector<int16_t>& output_samples) {
    size_t num_samples = input_data.size();
    output_samples.resize(num_samples);
    
    for (size_t i = 0; i < num_samples; ++i) {
        output_samples[i] = mulaw2linear(input_data[i]);
    }
    
    return num_samples;
}

int16_t MuLawCodec::mulaw2linear(uint8_t mulaw_sample) {
    static int exp_lut[8] = {0, 132, 396, 924, 1980, 4092, 8316, 16764};
    int sign, exponent, mantissa, sample;
    
    mulaw_sample = ~mulaw_sample;
    sign = (mulaw_sample & 0x80);
    exponent = (mulaw_sample >> 4) & 0x07;
    mantissa = mulaw_sample & 0x0F;
    sample = exp_lut[exponent] + (mantissa << (exponent + 3));
    
    return sign ? -sample : sample;
}

// MP3PassthroughCodec implementation
MP3PassthroughCodec::MP3PassthroughCodec(const StreamInfo& stream_info) 
    : AudioCodec(stream_info) {
}

MP3PassthroughCodec::~MP3PassthroughCodec() {
    delete m_mp3_stream;
}

bool MP3PassthroughCodec::initialize() {
    // We'll create the MP3Stream when we get the first chunk
    m_initialized = true;
    return true;
}

AudioFrame MP3PassthroughCodec::decode(const MediaChunk& chunk) {
    AudioFrame frame;
    
    if (chunk.data.empty()) {
        return frame;
    }
    
    // Accumulate data in buffer
    m_buffer.insert(m_buffer.end(), chunk.data.begin(), chunk.data.end());
    
    // Create MP3Stream if we haven't yet
    if (!m_mp3_stream && m_buffer.size() >= 4) {
        // Try to create from buffered data
        // Note: This is a simplified approach - in practice, we might need
        // to write the data to a temporary file or implement a memory-based stream
        
        // For now, return empty frame - full implementation would require
        // significant changes to the existing MP3Stream class
        return frame;
    }
    
    // TODO: Implement proper MP3 passthrough
    // This would require either:
    // 1. Modifying MP3Stream to accept memory buffers
    // 2. Creating a temporary file
    // 3. Implementing a memory-based IOHandler
    
    return frame;
}

AudioFrame MP3PassthroughCodec::flush() {
    return AudioFrame{};
}

void MP3PassthroughCodec::reset() {
    m_buffer.clear();
    m_header_written = false;
    
    if (m_mp3_stream) {
        delete m_mp3_stream;
        m_mp3_stream = nullptr;
    }
}

bool MP3PassthroughCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "mp3";
}