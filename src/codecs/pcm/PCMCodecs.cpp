/*
 * PCMCodecs.cpp - PCM and PCM-variant audio codec implementations
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#ifdef HAVE_MP3
#include "io/MemoryIOHandler.h"
#endif

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



#ifdef HAVE_MP3

using PsyMP3::IO::MemoryIOHandler;

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
    
    // 1. If we have data, feed it into the decoder or buffer it
    if (!chunk.data.empty()) {
        if (m_mp3_stream && m_io_handler) {
            // We have an active stream, feed it directly
            m_io_handler->write(chunk.data.data(), chunk.data.size());
        } else {
             // Buffer initially until we create stream
             m_buffer.insert(m_buffer.end(), chunk.data.begin(), chunk.data.end());
        }
    }

    // 2. Create MP3Stream if we haven't yet and have enough data
    if (!m_mp3_stream && m_buffer.size() >= 4) {
        // Create the handler
        auto handler = std::make_unique<MemoryIOHandler>();
        m_io_handler = handler.get();

        // Pre-fill with accumulated data
        if (!m_buffer.empty()) {
            handler->write(m_buffer.data(), m_buffer.size());
            // Clear initial buffer as data is now in the handler
            m_buffer.clear();
        }
        
        // Create MP3Stream using the handler
        // Note: Libmpg123 takes ownership of the handler
        try {
            m_mp3_stream = new PsyMP3::Codec::MP3::Libmpg123(std::move(handler));
        } catch (...) {
            m_io_handler = nullptr;
            throw;
        }
    }
    
    // 3. Decode frames from the stream
    if (m_mp3_stream) {
        // Feed data if we have it and haven't already (via initial buffer)
        // Note: if m_io_handler was just created, we already fed it.
        // But if m_mp3_stream already existed, we need to feed new chunk.
        // We handled feeding above in Step 1.

        // Try to decode frames until no more data or error
        // mpg123 usually returns one frame per read call if buffer is large enough
        // but our Libmpg123::getData wrapper behaves slightly differently.
        // It calls mpg123_read and fills the buffer.

        // We need a buffer to hold the decoded PCM
        size_t pcm_buffer_size = 4096 * 4; // Reasonable chunk size
        std::vector<uint8_t> pcm_data(pcm_buffer_size);

        try {
            // Read from the stream
            size_t bytes_read = m_mp3_stream->getData(pcm_buffer_size, pcm_data.data());

            if (bytes_read > 0) {
                // Resize to actual data read
                pcm_data.resize(bytes_read);

                // Set frame properties
                frame.sample_rate = m_mp3_stream->getRate();
                frame.channels = m_mp3_stream->getChannels();
                // Timestamp from chunk or estimated? For now use chunk's if first frame
                // Ideally track sample count.
                frame.timestamp_samples = chunk.timestamp_samples; // Approximate

                // Convert 16-bit PCM bytes to int16_t samples
                size_t num_samples = bytes_read / 2; // 16-bit
                frame.samples.resize(num_samples);
                std::memcpy(frame.samples.data(), pcm_data.data(), bytes_read);

                // Clean up consumed data from handler to keep memory usage low
                // Note: We can't know exactly how much input was consumed by mpg123
                // without querying mpg123 internals or tracking file position.
                // m_mp3_stream->getPosition() returns msec.
                // MemoryIOHandler::tell() returns current read position!
                // So we can discard up to tell().
                if (m_io_handler) {
                    // Use discardRead() to safely discard only what has been read from the buffer
                    m_io_handler->discardRead();
                }
            }
        } catch (const std::exception& e) {
            Debug::log("mp3_passthrough", "Decode error: ", e.what());
            // It's possible we just need more data
        }
    }
    
    return frame;
}

AudioFrame MP3PassthroughCodec::flush() {
    AudioFrame frame;
    // Flush any remaining data in the decoder if possible
    // Currently Libmpg123 doesn't expose a flush method that returns data,
    // but we can try reading until EOF.
    return frame;
}

void MP3PassthroughCodec::reset() {
    m_buffer.clear();
    m_header_written = false;
    
    if (m_mp3_stream) {
        delete m_mp3_stream;
        m_mp3_stream = nullptr;
    }
    m_io_handler = nullptr;
}

bool MP3PassthroughCodec::canDecode(const StreamInfo& stream_info) const {
    return stream_info.codec_name == "mp3";
}

} // namespace PCM
} // namespace Codec
} // namespace PsyMP3
#endif
