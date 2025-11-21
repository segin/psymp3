/*
 * NativeFLACCodec.cpp - Native FLAC decoder implementation
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

#ifdef HAVE_NATIVE_FLAC

// ============================================================================
// FLACCodec Implementation
// ============================================================================

FLACCodec::FLACCodec(const StreamInfo& stream_info)
    : AudioCodec(stream_info)
    , m_stream_info(stream_info)
    , m_current_sample(0)
    , m_initialized(false) {
    
    Debug::log("flac_codec", "[NativeFLACCodec] Creating native FLAC codec for stream: ",
              stream_info.codec_name, ", ", stream_info.sample_rate, "Hz, ",
              stream_info.channels, " channels, ", stream_info.bits_per_sample, " bits");
    
    // Pre-allocate buffers for performance
    m_input_buffer.reserve(64 * 1024);  // 64KB input buffer
    m_decode_buffer.reserve(65535 * 8); // Maximum FLAC block size * max channels
    m_output_buffer.reserve(65535 * 8); // Maximum output samples
}

FLACCodec::~FLACCodec() {
    Debug::log("flac_codec", "[NativeFLACCodec] Destroying native FLAC codec");
    
    // Cleanup will be implemented in future tasks
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    m_initialized = false;
}

// Public AudioCodec interface implementations (thread-safe with RAII guards)

bool FLACCodec::initialize() {
    Debug::log("flac_codec", "[NativeFLACCodec::initialize] [ENTRY] Acquiring locks for initialization");
    
    // Acquire locks in documented order to prevent deadlocks
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    std::lock_guard<std::mutex> decoder_lock(m_decoder_mutex);
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    Debug::log("flac_codec", "[NativeFLACCodec::initialize] [LOCKED] All locks acquired, calling unlocked implementation");
    
    try {
        bool result = initialize_unlocked();
        Debug::log("flac_codec", "[NativeFLACCodec::initialize] [EXIT] Returning ", result ? "success" : "failure");
        return result;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize] [EXCEPTION] ", e.what());
        return false;
    }
}

AudioFrame FLACCodec::decode(const MediaChunk& chunk) {
    Debug::log("flac_codec", "[NativeFLACCodec::decode] [ENTRY] Acquiring state lock for chunk with ", chunk.data.size(), " bytes");
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    Debug::log("flac_codec", "[NativeFLACCodec::decode] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        AudioFrame result = decode_unlocked(chunk);
        Debug::log("flac_codec", "[NativeFLACCodec::decode] [EXIT] Returning frame with ",
                  result.getSampleFrameCount(), " sample frames");
        return result;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::decode] [EXCEPTION] ", e.what());
        return AudioFrame();
    }
}

AudioFrame FLACCodec::flush() {
    Debug::log("flac_codec", "[NativeFLACCodec::flush] [ENTRY] Acquiring state lock");
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    Debug::log("flac_codec", "[NativeFLACCodec::flush] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        AudioFrame result = flush_unlocked();
        Debug::log("flac_codec", "[NativeFLACCodec::flush] [EXIT] Returning frame with ",
                  result.getSampleFrameCount(), " sample frames");
        return result;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::flush] [EXCEPTION] ", e.what());
        return AudioFrame();
    }
}

void FLACCodec::reset() {
    Debug::log("flac_codec", "[NativeFLACCodec::reset] [ENTRY] Acquiring state lock");
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    Debug::log("flac_codec", "[NativeFLACCodec::reset] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        reset_unlocked();
        Debug::log("flac_codec", "[NativeFLACCodec::reset] [EXIT] Reset completed successfully");
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::reset] [EXCEPTION] ", e.what());
    }
}

bool FLACCodec::canDecode(const StreamInfo& stream_info) const {
    Debug::log("flac_codec", "[NativeFLACCodec::canDecode] [ENTRY] Acquiring state lock for codec: ", stream_info.codec_name);
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    Debug::log("flac_codec", "[NativeFLACCodec::canDecode] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        bool result = canDecode_unlocked(stream_info);
        Debug::log("flac_codec", "[NativeFLACCodec::canDecode] [EXIT] Returning ", result ? "can decode" : "cannot decode");
        return result;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::canDecode] [EXCEPTION] ", e.what());
        return false;
    }
}

// FLAC-specific public interface (thread-safe with RAII guards)

bool FLACCodec::supportsSeekReset() const {
    Debug::log("flac_codec", "[NativeFLACCodec::supportsSeekReset] [ENTRY/EXIT] Native FLAC codec supports seeking through reset");
    return true; // Native FLAC codec supports seeking through reset
}

uint64_t FLACCodec::getCurrentSample() const {
    uint64_t current = m_current_sample.load();
    Debug::log("flac_codec", "[NativeFLACCodec::getCurrentSample] [ENTRY/EXIT] Current sample position: ", current);
    return current;
}

// Private implementation methods (assume locks are already held)

bool FLACCodec::initialize_unlocked() {
    Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Initializing native FLAC decoder");
    
    // Validate stream parameters against RFC 9639 requirements
    if (m_stream_info.sample_rate < 1 || m_stream_info.sample_rate > 655350) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Invalid sample rate: ",
                  m_stream_info.sample_rate, " (RFC 9639 range: 1-655350 Hz)");
        return false;
    }
    
    if (m_stream_info.channels < 1 || m_stream_info.channels > 8) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Invalid channel count: ",
                  m_stream_info.channels, " (RFC 9639 range: 1-8)");
        return false;
    }
    
    if (m_stream_info.bits_per_sample < 4 || m_stream_info.bits_per_sample > 32) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Invalid bit depth: ",
                  m_stream_info.bits_per_sample, " (RFC 9639 range: 4-32 bits)");
        return false;
    }
    
    // Initialize decoder components (to be implemented in future tasks)
    // m_bitstream_reader = std::make_unique<BitstreamReader>();
    // m_frame_parser = std::make_unique<FrameParser>(m_bitstream_reader.get(), m_crc_validator.get());
    // ... etc
    
    m_current_sample.store(0);
    m_initialized = true;
    
    Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Native FLAC decoder initialized successfully");
    return true;
}

AudioFrame FLACCodec::decode_unlocked(const MediaChunk& chunk) {
    Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Decoding chunk with ", chunk.data.size(), " bytes");
    
    if (!m_initialized) {
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Decoder not initialized");
        return AudioFrame();
    }
    
    if (chunk.data.empty()) {
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Empty chunk");
        return AudioFrame();
    }
    
    // Decoding implementation will be added in future tasks
    // For now, return empty frame as placeholder
    Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Decoding not yet implemented - returning empty frame");
    
    return AudioFrame();
}

AudioFrame FLACCodec::flush_unlocked() {
    Debug::log("flac_codec", "[NativeFLACCodec::flush_unlocked] Flushing remaining samples");
    
    // Flush implementation will be added in future tasks
    return AudioFrame();
}

void FLACCodec::reset_unlocked() {
    Debug::log("flac_codec", "[NativeFLACCodec::reset_unlocked] Resetting decoder state");
    
    // Clear buffers
    m_input_buffer.clear();
    m_decode_buffer.clear();
    m_output_buffer.clear();
    
    // Reset sample position
    m_current_sample.store(0);
    
    // Reset decoder components (to be implemented in future tasks)
    // m_bitstream_reader->clearBuffer();
    // m_frame_parser->reset();
    // ... etc
    
    Debug::log("flac_codec", "[NativeFLACCodec::reset_unlocked] Decoder state reset complete");
}

bool FLACCodec::canDecode_unlocked(const StreamInfo& stream_info) const {
    // Check codec name (case-insensitive)
    std::string codec_lower = stream_info.codec_name;
    std::transform(codec_lower.begin(), codec_lower.end(), codec_lower.begin(), ::tolower);
    
    if (codec_lower != "flac") {
        Debug::log("flac_codec", "[NativeFLACCodec::canDecode_unlocked] Codec name mismatch: ", stream_info.codec_name);
        return false;
    }
    
    // Validate parameters against RFC 9639
    if (stream_info.sample_rate < 1 || stream_info.sample_rate > 655350) {
        Debug::log("flac_codec", "[NativeFLACCodec::canDecode_unlocked] Sample rate out of range: ", stream_info.sample_rate);
        return false;
    }
    
    if (stream_info.channels < 1 || stream_info.channels > 8) {
        Debug::log("flac_codec", "[NativeFLACCodec::canDecode_unlocked] Channel count out of range: ", stream_info.channels);
        return false;
    }
    
    if (stream_info.bits_per_sample < 4 || stream_info.bits_per_sample > 32) {
        Debug::log("flac_codec", "[NativeFLACCodec::canDecode_unlocked] Bit depth out of range: ", stream_info.bits_per_sample);
        return false;
    }
    
    Debug::log("flac_codec", "[NativeFLACCodec::canDecode_unlocked] Can decode FLAC stream: ",
              stream_info.sample_rate, "Hz, ", stream_info.channels, " channels, ",
              stream_info.bits_per_sample, " bits");
    
    return true;
}

// ============================================================================
// FLACCodecSupport Implementation
// ============================================================================

namespace FLACCodecSupport {

void registerCodec() {
    Debug::log("flac_codec", "[FLACCodecSupport::registerCodec] Registering native FLAC codec with AudioCodecFactory");
    
    AudioCodecFactory::registerCodec("flac", [](const StreamInfo& stream_info) -> std::unique_ptr<AudioCodec> {
        if (isFLACStream(stream_info)) {
            return std::make_unique<FLACCodec>(stream_info);
        }
        return nullptr;
    });
    
    Debug::log("flac_codec", "[FLACCodecSupport::registerCodec] Native FLAC codec registered successfully");
}

std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info) {
    if (isFLACStream(stream_info)) {
        return std::make_unique<FLACCodec>(stream_info);
    }
    return nullptr;
}

bool isFLACStream(const StreamInfo& stream_info) {
    return stream_info.codec_name == "flac" && 
           stream_info.codec_type == "audio";
}

std::string getCodecInfo() {
    return "Native FLAC Codec v1.0 - RFC 9639 compliant, pure C++ implementation without libFLAC dependency";
}

} // namespace FLACCodecSupport

#endif // HAVE_NATIVE_FLAC
