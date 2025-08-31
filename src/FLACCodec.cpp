/*
 * FLACCodec.cpp - Container-agnostic FLAC audio codec implementation
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

#ifdef HAVE_FLAC

// ============================================================================
// FLACStreamDecoder Implementation
// ============================================================================

FLACStreamDecoder::FLACStreamDecoder(FLACCodec* parent)
    : m_parent(parent) {
    if (!m_parent) {
        throw std::invalid_argument("FLACStreamDecoder: parent codec cannot be null");
    }
    
    // Pre-allocate input buffer for performance
    m_input_buffer.reserve(INPUT_BUFFER_SIZE);
    
    Debug::log("flac_codec", "[FLACStreamDecoder] Created decoder for parent codec");
}

FLACStreamDecoder::~FLACStreamDecoder() {
    // Ensure decoder is properly finished before destruction
    if (get_state() != FLAC__STREAM_DECODER_UNINITIALIZED) {
        finish();
    }
    
    Debug::log("flac_codec", "[FLACStreamDecoder] Destroyed decoder");
}

bool FLACStreamDecoder::feedData(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        Debug::log("flac_codec", "[FLACStreamDecoder::feedData] Invalid input data");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_input_mutex);
    
    try {
        // Ensure buffer has enough capacity
        if (m_input_buffer.size() + size > m_input_buffer.capacity()) {
            m_input_buffer.reserve(m_input_buffer.size() + size + INPUT_BUFFER_SIZE);
        }
        
        // Append new data to input buffer
        m_input_buffer.insert(m_input_buffer.end(), data, data + size);
        
        Debug::log("flac_codec", "[FLACStreamDecoder::feedData] Fed ", size, 
                  " bytes, buffer now has ", m_input_buffer.size(), " bytes");
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACStreamDecoder::feedData] Exception: ", e.what());
        return false;
    }
}

void FLACStreamDecoder::clearInputBuffer() {
    std::lock_guard<std::mutex> lock(m_input_mutex);
    m_input_buffer.clear();
    m_buffer_position = 0;
    
    Debug::log("flac_codec", "[FLACStreamDecoder::clearInputBuffer] Input buffer cleared");
}

size_t FLACStreamDecoder::getInputBufferSize() const {
    std::lock_guard<std::mutex> lock(m_input_mutex);
    return m_input_buffer.size() - m_buffer_position;
}

bool FLACStreamDecoder::hasInputData() const {
    std::lock_guard<std::mutex> lock(m_input_mutex);
    return m_buffer_position < m_input_buffer.size();
}

void FLACStreamDecoder::clearError() {
    m_error_occurred = false;
    m_last_error = FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC;
}

// libFLAC callback implementations

FLAC__StreamDecoderReadStatus FLACStreamDecoder::read_callback(FLAC__byte buffer[], size_t *bytes) {
    if (!buffer || !bytes || *bytes == 0) {
        Debug::log("flac_codec", "[FLACStreamDecoder::read_callback] Invalid parameters");
        *bytes = 0;
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }
    
    std::lock_guard<std::mutex> lock(m_input_mutex);
    
    // Calculate available data
    size_t available = m_input_buffer.size() - m_buffer_position;
    if (available == 0) {
        *bytes = 0;
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }
    
    // Copy requested amount or available amount, whichever is smaller
    size_t to_copy = std::min(*bytes, available);
    std::memcpy(buffer, m_input_buffer.data() + m_buffer_position, to_copy);
    
    m_buffer_position += to_copy;
    *bytes = to_copy;
    
    Debug::log("flac_codec", "[FLACStreamDecoder::read_callback] Provided ", to_copy, 
              " bytes, position now ", m_buffer_position);
    
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus FLACStreamDecoder::write_callback(const FLAC__Frame *frame, 
                                                                const FLAC__int32 * const buffer[]) {
    if (!frame || !buffer || !m_parent) {
        Debug::log("flac_codec", "[FLACStreamDecoder::write_callback] Invalid parameters");
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    
    try {
        // Delegate to parent codec for processing
        m_parent->handleWriteCallback_unlocked(frame, buffer);
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACStreamDecoder::write_callback] Exception: ", e.what());
        m_error_occurred = true;
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
}

void FLACStreamDecoder::metadata_callback(const FLAC__StreamMetadata *metadata) {
    if (!metadata || !m_parent) {
        Debug::log("flac_codec", "[FLACStreamDecoder::metadata_callback] Invalid parameters");
        return;
    }
    
    try {
        // Delegate to parent codec for processing
        m_parent->handleMetadataCallback_unlocked(metadata);
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACStreamDecoder::metadata_callback] Exception: ", e.what());
        m_error_occurred = true;
    }
}

void FLACStreamDecoder::error_callback(FLAC__StreamDecoderErrorStatus status) {
    m_error_occurred = true;
    m_last_error = status;
    
    Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] libFLAC error: ", 
              FLAC__StreamDecoderErrorStatusString[status]);
    
    if (m_parent) {
        try {
            m_parent->handleErrorCallback_unlocked(status);
        } catch (const std::exception& e) {
            Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] Exception in parent handler: ", e.what());
        }
    }
}

// ============================================================================
// FLACCodec Implementation
// ============================================================================

FLACCodec::FLACCodec(const StreamInfo& stream_info)
    : AudioCodec(stream_info) {
    
    Debug::log("flac_codec", "[FLACCodec] Creating FLAC codec for stream: ", 
              stream_info.codec_name, ", ", stream_info.sample_rate, "Hz, ", 
              stream_info.channels, " channels, ", stream_info.bits_per_sample, " bits");
    
    // Initialize statistics
    m_stats = FLACCodecStats{};
    
    // Pre-allocate buffers for performance
    m_input_buffer.reserve(64 * 1024);  // 64KB input buffer
    m_decode_buffer.reserve(65535 * 8); // Maximum FLAC block size * max channels
    m_output_buffer.reserve(MAX_BUFFER_SAMPLES);
}

FLACCodec::~FLACCodec() {
    Debug::log("flac_codec", "[FLACCodec] Destroying FLAC codec");
    
    // Ensure proper cleanup with thread coordination
    {
        std::lock_guard<std::mutex> state_lock(m_state_mutex);
        cleanupFLAC_unlocked();
    }
    
    Debug::log("flac_codec", "[FLACCodec] Destroyed FLAC codec, decoded ", 
              m_stats.frames_decoded, " frames, ", m_stats.samples_decoded, " samples");
}

// Public AudioCodec interface implementations (thread-safe with RAII guards)

bool FLACCodec::initialize() {
    Debug::log("flac_codec", "[FLACCodec::initialize] [ENTRY] Acquiring state lock");
    std::lock_guard<std::mutex> lock(m_state_mutex);
    Debug::log("flac_codec", "[FLACCodec::initialize] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        bool result = initialize_unlocked();
        Debug::log("flac_codec", "[FLACCodec::initialize] [EXIT] Returning ", result ? "success" : "failure");
        return result;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::initialize] [EXCEPTION] ", e.what());
        setErrorState_unlocked(true);
        return false;
    }
}

AudioFrame FLACCodec::decode(const MediaChunk& chunk) {
    Debug::log("flac_codec", "[FLACCodec::decode] [ENTRY] Acquiring state lock for chunk with ", chunk.data.size(), " bytes");
    std::lock_guard<std::mutex> lock(m_state_mutex);
    Debug::log("flac_codec", "[FLACCodec::decode] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        AudioFrame result = decode_unlocked(chunk);
        Debug::log("flac_codec", "[FLACCodec::decode] [EXIT] Returning frame with ", 
                  result.getSampleFrameCount(), " sample frames");
        return result;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::decode] [EXCEPTION] ", e.what());
        setErrorState_unlocked(true);
        return createSilenceFrame_unlocked(1024);
    }
}

AudioFrame FLACCodec::flush() {
    Debug::log("flac_codec", "[FLACCodec::flush] [ENTRY] Acquiring state lock");
    std::lock_guard<std::mutex> lock(m_state_mutex);
    Debug::log("flac_codec", "[FLACCodec::flush] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        AudioFrame result = flush_unlocked();
        Debug::log("flac_codec", "[FLACCodec::flush] [EXIT] Returning frame with ", 
                  result.getSampleFrameCount(), " sample frames");
        return result;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::flush] [EXCEPTION] ", e.what());
        return AudioFrame();
    }
}

void FLACCodec::reset() {
    Debug::log("flac_codec", "[FLACCodec::reset] [ENTRY] Acquiring state lock");
    std::lock_guard<std::mutex> lock(m_state_mutex);
    Debug::log("flac_codec", "[FLACCodec::reset] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        reset_unlocked();
        Debug::log("flac_codec", "[FLACCodec::reset] [EXIT] Reset completed successfully");
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::reset] [EXCEPTION] ", e.what());
        setErrorState_unlocked(true);
    }
}

bool FLACCodec::canDecode(const StreamInfo& stream_info) const {
    Debug::log("flac_codec", "[FLACCodec::canDecode] [ENTRY] Acquiring state lock for codec: ", stream_info.codec_name);
    std::lock_guard<std::mutex> lock(m_state_mutex);
    Debug::log("flac_codec", "[FLACCodec::canDecode] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        bool result = canDecode_unlocked(stream_info);
        Debug::log("flac_codec", "[FLACCodec::canDecode] [EXIT] Returning ", result ? "can decode" : "cannot decode");
        return result;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::canDecode] [EXCEPTION] ", e.what());
        return false;
    }
}

// FLAC-specific public interface (thread-safe with RAII guards)

bool FLACCodec::supportsSeekReset() const {
    Debug::log("flac_codec", "[FLACCodec::supportsSeekReset] [ENTRY/EXIT] FLAC codec supports seeking through reset");
    return true; // FLAC codec supports seeking through reset
}

uint64_t FLACCodec::getCurrentSample() const {
    uint64_t current = m_current_sample.load();
    Debug::log("flac_codec", "[FLACCodec::getCurrentSample] [ENTRY/EXIT] Current sample position: ", current);
    return current;
}

FLACCodecStats FLACCodec::getStats() const {
    Debug::log("flac_codec", "[FLACCodec::getStats] [ENTRY] Acquiring state lock for statistics");
    std::lock_guard<std::mutex> lock(m_state_mutex);
    Debug::log("flac_codec", "[FLACCodec::getStats] [LOCKED] State lock acquired, returning statistics");
    
    try {
        FLACCodecStats stats_copy = m_stats;
        Debug::log("flac_codec", "[FLACCodec::getStats] [EXIT] Returning stats: ", 
                  stats_copy.frames_decoded, " frames, ", stats_copy.samples_decoded, " samples");
        return stats_copy;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::getStats] [EXCEPTION] ", e.what());
        return FLACCodecStats{}; // Return empty stats on error
    }
}

// Private implementation methods (assume locks are held)

bool FLACCodec::initialize_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::initialize_unlocked] Initializing FLAC codec");
    
    if (m_initialized) {
        Debug::log("flac_codec", "[FLACCodec::initialize_unlocked] Already initialized");
        return true;
    }
    
    try {
        // Configure codec from stream information
        if (!configureFromStreamInfo_unlocked(m_stream_info)) {
            Debug::log("flac_codec", "[FLACCodec::initialize_unlocked] Failed to configure from StreamInfo");
            return false;
        }
        
        // Validate configuration against RFC 9639
        if (!validateConfiguration_unlocked()) {
            Debug::log("flac_codec", "[FLACCodec::initialize_unlocked] Configuration validation failed");
            return false;
        }
        
        // Initialize libFLAC decoder
        if (!initializeFLACDecoder_unlocked()) {
            Debug::log("flac_codec", "[FLACCodec::initialize_unlocked] Failed to initialize libFLAC decoder");
            return false;
        }
        
        // Optimize buffer sizes for performance
        optimizeBufferSizes_unlocked();
        
        m_initialized = true;
        setErrorState_unlocked(false);
        
        Debug::log("flac_codec", "[FLACCodec::initialize_unlocked] FLAC codec initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::initialize_unlocked] Exception: ", e.what());
        setErrorState_unlocked(true);
        return false;
    }
}

AudioFrame FLACCodec::decode_unlocked(const MediaChunk& chunk) {
    Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Decoding chunk with ", chunk.data.size(), " bytes");
    
    // Performance monitoring
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Validate input chunk
    if (chunk.data.empty()) {
        Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Empty chunk received");
        return AudioFrame(); // Empty frame
    }
    
    // Check if codec is initialized
    if (!m_initialized || !m_decoder_initialized || !m_decoder) {
        Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Decoder not initialized");
        setErrorState_unlocked(true);
        return createSilenceFrame_unlocked(1024); // Fallback silence
    }
    
    // Check for error state
    if (m_error_state.load()) {
        Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Codec in error state");
        return createSilenceFrame_unlocked(1024);
    }
    
    try {
        // Clear previous output buffer efficiently
        {
            std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
            m_output_buffer.clear();
            m_buffer_read_position = 0;
        }
        
        // Feed frame data to libFLAC decoder
        if (!m_decoder->feedData(chunk.data.data(), chunk.data.size())) {
            Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Failed to feed data to decoder");
            return handleDecodingError_unlocked(chunk);
        }
        
        // Process the frame through libFLAC
        if (!m_decoder->process_single()) {
            Debug::log("flac_codec", "[FLACCodec::decode_unlocked] libFLAC processing failed, state: ", 
                      FLAC__StreamDecoderStateString[m_decoder->get_state()]);
            return handleDecodingError_unlocked(chunk);
        }
        
        // Extract decoded samples
        AudioFrame result = extractDecodedSamples_unlocked();
        
        // Update performance statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        updatePerformanceStats_unlocked(result.getSampleFrameCount(), duration.count());
        m_stats.total_bytes_processed += chunk.data.size();
        
        // Performance logging (debug builds only)
        if (duration.count() > 1000) { // Log if decoding takes >1ms
            Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Frame decoding took ", duration.count(), " μs");
        }
        
        Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Successfully decoded ", 
                  result.getSampleFrameCount(), " sample frames");
        
        return result;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Exception during decoding: ", e.what());
        setErrorState_unlocked(true);
        return createSilenceFrame_unlocked(1024);
    }
}

AudioFrame FLACCodec::flush_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::flush_unlocked] Flushing remaining samples");
    
    if (!m_initialized || !m_decoder_initialized || !m_decoder) {
        Debug::log("flac_codec", "[FLACCodec::flush_unlocked] Decoder not initialized");
        return AudioFrame();
    }
    
    try {
        // Process any remaining data in the decoder
        if (m_decoder->hasInputData()) {
            Debug::log("flac_codec", "[FLACCodec::flush_unlocked] Processing remaining input data");
            
            // Try to process remaining frames
            while (m_decoder->hasInputData() && m_decoder->get_state() == FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC) {
                if (!m_decoder->process_single()) {
                    break;
                }
            }
        }
        
        // Extract any remaining decoded samples
        AudioFrame result = extractDecodedSamples_unlocked();
        
        if (result.getSampleFrameCount() > 0) {
            Debug::log("flac_codec", "[FLACCodec::flush_unlocked] Flushed ", 
                      result.getSampleFrameCount(), " sample frames");
        } else {
            Debug::log("flac_codec", "[FLACCodec::flush_unlocked] No remaining samples to flush");
        }
        
        // Mark stream as finished
        m_stream_finished = true;
        
        return result;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::flush_unlocked] Exception during flush: ", e.what());
        return AudioFrame();
    }
}

void FLACCodec::reset_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::reset_unlocked] Resetting FLAC codec state");
    
    // Reset decoder state
    resetDecoderState_unlocked();
    
    // Clear buffers
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        m_output_buffer.clear();
        m_buffer_read_position = 0;
    }
    
    // Reset position tracking
    m_current_sample.store(0);
    m_last_block_size = 0;
    m_stream_finished = false;
    
    // Clear error state
    setErrorState_unlocked(false);
    
    Debug::log("flac_codec", "[FLACCodec::reset_unlocked] FLAC codec reset completed");
}

bool FLACCodec::canDecode_unlocked(const StreamInfo& stream_info) const {
    Debug::log("flac_codec", "[FLACCodec::canDecode_unlocked] Checking decode capability for: ", 
              stream_info.codec_name, ", ", stream_info.sample_rate, "Hz, ", 
              stream_info.channels, " channels, ", stream_info.bits_per_sample, " bits");
    
    // Check if this is a FLAC stream
    if (stream_info.codec_name != "flac") {
        Debug::log("flac_codec", "[FLACCodec::canDecode_unlocked] Not a FLAC stream: ", stream_info.codec_name);
        return false;
    }
    
    // RFC 9639 compliance validation - sample rate
    // Per RFC 9639: streamable subset supports 1-655350 Hz
    if (stream_info.sample_rate < 1 || stream_info.sample_rate > 655350) {
        Debug::log("flac_codec", "[FLACCodec::canDecode_unlocked] Invalid sample rate per RFC 9639: ", 
                  stream_info.sample_rate, " Hz (valid range: 1-655350 Hz)");
        return false;
    }
    
    // RFC 9639 compliance validation - channels
    // Per RFC 9639: 1-8 channels supported
    if (stream_info.channels < 1 || stream_info.channels > 8) {
        Debug::log("flac_codec", "[FLACCodec::canDecode_unlocked] Invalid channel count per RFC 9639: ", 
                  stream_info.channels, " channels (valid range: 1-8 channels)");
        return false;
    }
    
    // RFC 9639 compliance validation - bit depth
    // Per RFC 9639: 4-32 bits per sample supported
    if (stream_info.bits_per_sample < 4 || stream_info.bits_per_sample > 32) {
        Debug::log("flac_codec", "[FLACCodec::canDecode_unlocked] Invalid bit depth per RFC 9639: ", 
                  stream_info.bits_per_sample, " bits (valid range: 4-32 bits)");
        return false;
    }
    
    // Additional validation for practical decoding capability
    
    // Check for reasonable total samples if specified
    if (stream_info.duration_samples > 0) {
        // Maximum sample number is 36 bits per RFC 9639 Section 9.1.5
        uint64_t max_samples = (1ULL << 36) - 1;
        if (stream_info.duration_samples > max_samples) {
            Debug::log("flac_codec", "[FLACCodec::canDecode_unlocked] Total samples exceeds RFC 9639 36-bit limit: ", 
                      stream_info.duration_samples);
            return false;
        }
    }
    
    // Check for performance feasibility
    if (stream_info.sample_rate > 0 && stream_info.channels > 0) {
        // Calculate maximum uncompressed data rate
        uint64_t max_data_rate = static_cast<uint64_t>(stream_info.sample_rate) * 
                                 stream_info.channels * stream_info.bits_per_sample;
        
        // Reject if data rate is unreasonably high (>100 Mbps uncompressed)
        if (max_data_rate > 100000000) {
            Debug::log("flac_codec", "[FLACCodec::canDecode_unlocked] Data rate too high for practical decoding: ", 
                      max_data_rate / 1000000, " Mbps uncompressed");
            return false;
        }
    }
    
    Debug::log("flac_codec", "[FLACCodec::canDecode_unlocked] Stream is decodable (RFC 9639 compliant)");
    return true;
}

// Configuration and validation methods

bool FLACCodec::configureFromStreamInfo_unlocked(const StreamInfo& stream_info) {
    Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Configuring codec with RFC 9639 validation");
    
    // Extract basic parameters with RFC 9639 validation
    m_sample_rate = stream_info.sample_rate;
    m_channels = stream_info.channels;
    m_bits_per_sample = stream_info.bits_per_sample;
    m_total_samples = stream_info.duration_samples;
    
    // RFC 9639 compliance validation - sample rate
    // Per RFC 9639: FLAC can code for sample rates from 1 to 1048575 Hz
    // However, the streamable subset limits this to 655350 Hz
    if (m_sample_rate < 1 || m_sample_rate > 655350) {
        Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Invalid sample rate per RFC 9639: ", 
                  m_sample_rate, " Hz (valid range: 1-655350 Hz)");
        return false;
    }
    
    // RFC 9639 compliance validation - channels
    // Per RFC 9639: FLAC can code for 1 to 8 channels
    if (m_channels < 1 || m_channels > 8) {
        Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Invalid channel count per RFC 9639: ", 
                  m_channels, " channels (valid range: 1-8 channels)");
        return false;
    }
    
    // RFC 9639 compliance validation - bit depth
    // Per RFC 9639: FLAC can code for bit depths from 4 to 32 bits
    if (m_bits_per_sample < 4 || m_bits_per_sample > 32) {
        Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Invalid bit depth per RFC 9639: ", 
                  m_bits_per_sample, " bits (valid range: 4-32 bits)");
        return false;
    }
    
    // Validate total samples (should be reasonable)
    if (m_total_samples > 0) {
        // Calculate maximum reasonable duration (24 hours at highest sample rate)
        uint64_t max_reasonable_samples = static_cast<uint64_t>(655350) * 24 * 60 * 60;
        if (m_total_samples > max_reasonable_samples) {
            Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Warning: Very large sample count: ", 
                      m_total_samples, " samples (>24 hours at max sample rate)");
            // Don't fail, just warn - this could be a valid long recording
        }
    }
    
    // Performance optimization: Pre-calculate buffer sizes based on RFC 9639 limits
    // Maximum block size per RFC 9639 is 65535 samples
    size_t max_block_size = 65535;
    size_t required_buffer_size = max_block_size * m_channels;
    
    // Pre-allocate buffers to avoid runtime allocations
    try {
        m_output_buffer.reserve(required_buffer_size);
        m_decode_buffer.reserve(required_buffer_size);
        m_input_buffer.reserve(64 * 1024); // 64KB input buffer for frame data
        
        Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Pre-allocated buffers: ", 
                  "output=", required_buffer_size, " samples, input=64KB");
        
    } catch (const std::bad_alloc& e) {
        Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Failed to pre-allocate buffers: ", e.what());
        return false;
    }
    
    // Set up internal configuration for optimized bit depth conversion algorithms
    // Pre-calculate conversion parameters based on source bit depth
    switch (m_bits_per_sample) {
        case 8:
            Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Configured for 8-bit to 16-bit upscaling");
            break;
        case 16:
            Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Configured for 16-bit direct copy (no conversion)");
            break;
        case 24:
            Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Configured for 24-bit to 16-bit downscaling");
            break;
        case 32:
            Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Configured for 32-bit to 16-bit downscaling");
            break;
        default:
            Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Configured for generic bit depth conversion: ", 
                      m_bits_per_sample, " bits");
            break;
    }
    
    Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] Successfully configured: ", 
              m_sample_rate, "Hz, ", m_channels, " channels, ", 
              m_bits_per_sample, " bits, ", m_total_samples, " samples (RFC 9639 compliant)");
    
    return true;
}

bool FLACCodec::validateConfiguration_unlocked() const {
    Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] Performing comprehensive RFC 9639 validation");
    
    // RFC 9639 compliance validation - sample rate
    // Per RFC 9639 Section 1: sample rates from 1 to 1048575 Hz are supported
    // However, streamable subset limits to 655350 Hz for compatibility
    if (m_sample_rate < 1 || m_sample_rate > 655350) {
        Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] RFC 9639 violation - Invalid sample rate: ", 
                  m_sample_rate, " Hz (streamable subset range: 1-655350 Hz)");
        return false;
    }
    
    // Validate against common sample rates for better compatibility
    static const uint32_t common_rates[] = {
        8000, 11025, 16000, 22050, 32000, 44100, 48000, 
        88200, 96000, 176400, 192000, 352800, 384000
    };
    bool is_common_rate = false;
    for (size_t i = 0; i < sizeof(common_rates) / sizeof(common_rates[0]); ++i) {
        if (m_sample_rate == common_rates[i]) {
            is_common_rate = true;
            break;
        }
    }
    if (!is_common_rate) {
        Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] Note: Uncommon sample rate ", 
                  m_sample_rate, " Hz (may require special handling)");
    }
    
    // RFC 9639 compliance validation - channels
    // Per RFC 9639 Section 1: 1 to 8 channels are supported
    if (m_channels < 1 || m_channels > 8) {
        Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] RFC 9639 violation - Invalid channel count: ", 
                  m_channels, " channels (valid range: 1-8 channels)");
        return false;
    }
    
    // Validate channel configurations per RFC 9639 Section 9.1.3
    if (m_channels > 2) {
        Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] Multi-channel configuration: ", 
                  m_channels, " channels (stereo decorrelation not applicable)");
    }
    
    // RFC 9639 compliance validation - bit depth
    // Per RFC 9639 Section 1: bit depths from 4 to 32 bits are supported
    if (m_bits_per_sample < 4 || m_bits_per_sample > 32) {
        Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] RFC 9639 violation - Invalid bit depth: ", 
                  m_bits_per_sample, " bits (valid range: 4-32 bits)");
        return false;
    }
    
    // Validate against common bit depths for better compatibility
    static const uint16_t common_depths[] = { 8, 12, 16, 20, 24, 32 };
    bool is_common_depth = false;
    for (size_t i = 0; i < sizeof(common_depths) / sizeof(common_depths[0]); ++i) {
        if (m_bits_per_sample == common_depths[i]) {
            is_common_depth = true;
            break;
        }
    }
    if (!is_common_depth) {
        Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] Note: Uncommon bit depth ", 
                  m_bits_per_sample, " bits (may require special handling)");
    }
    
    // Validate total samples if specified
    if (m_total_samples > 0) {
        // Calculate maximum theoretical samples based on RFC 9639 limits
        // Maximum sample number is 36 bits per RFC 9639 Section 9.1.5
        uint64_t max_samples = (1ULL << 36) - 1;
        if (m_total_samples > max_samples) {
            Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] RFC 9639 violation - Total samples exceeds 36-bit limit: ", 
                      m_total_samples, " samples (max: ", max_samples, ")");
            return false;
        }
        
        // Validate reasonable duration (warn for very long streams)
        if (m_sample_rate > 0) {
            uint64_t duration_seconds = m_total_samples / m_sample_rate;
            if (duration_seconds > 24 * 60 * 60) { // More than 24 hours
                Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] Note: Very long stream duration: ", 
                          duration_seconds / 3600, " hours");
            }
        }
    }
    
    // Validate configuration consistency
    if (m_sample_rate == 0 && m_total_samples > 0) {
        Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] Warning: Total samples specified but sample rate is 0");
    }
    
    // Performance validation - ensure configuration is suitable for real-time decoding
    if (m_sample_rate > 0 && m_channels > 0) {
        // Calculate maximum data rate (samples per second * channels * bits per sample)
        uint64_t max_data_rate = static_cast<uint64_t>(m_sample_rate) * m_channels * m_bits_per_sample;
        
        // Warn if data rate is very high (may impact real-time performance)
        if (max_data_rate > 50000000) { // > 50 Mbps uncompressed
            Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] Note: High data rate configuration: ", 
                      max_data_rate / 1000000, " Mbps uncompressed (may impact performance)");
        }
    }
    
    Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] Configuration passes RFC 9639 validation: ", 
              m_sample_rate, "Hz, ", m_channels, " channels, ", m_bits_per_sample, " bits");
    
    return true;
}

bool FLACCodec::initializeFLACDecoder_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Initializing optimized libFLAC decoder");
    
    try {
        // Create optimized libFLAC decoder with callback integration
        m_decoder = std::make_unique<FLACStreamDecoder>(this);
        
        if (!m_decoder) {
            Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Failed to create FLACStreamDecoder instance");
            return false;
        }
        
        // Configure decoder for maximum performance
        Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Configuring performance optimizations");
        
        // Disable MD5 checking for performance (per RFC 9639, MD5 is optional)
        if (!m_decoder->set_md5_checking(false)) {
            Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Warning: Failed to disable MD5 checking");
            // Continue anyway - this is not critical
        } else {
            Debug::log("flac_codec", "[FLACCodec::initializeFLAC_unlocked] MD5 checking disabled for performance");
        }
        
        // Set metadata response for STREAMINFO only (we need this for configuration)
        if (!m_decoder->set_metadata_respond(FLAC__METADATA_TYPE_STREAMINFO)) {
            Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Warning: Failed to set STREAMINFO metadata response");
            // Continue anyway - we may already have configuration from StreamInfo
        }
        
        // Ignore other metadata types for performance
        m_decoder->set_metadata_ignore_all();
        m_decoder->set_metadata_respond(FLAC__METADATA_TYPE_STREAMINFO);
        
        Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Performance optimizations configured");
        
        // Initialize decoder state with comprehensive error checking
        Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Initializing decoder state");
        
        FLAC__StreamDecoderInitStatus init_status = m_decoder->init();
        
        // Detailed error reporting for initialization failures
        if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
            const char* error_string = FLAC__StreamDecoderInitStatusString[init_status];
            Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Initialization failed with status: ", 
                      init_status, " (", error_string, ")");
            
            // Provide specific guidance based on error type
            switch (init_status) {
                case FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER:
                    Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Error: Unsupported container format");
                    break;
                case FLAC__STREAM_DECODER_INIT_STATUS_INVALID_CALLBACKS:
                    Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Error: Invalid callback configuration");
                    break;
                case FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR:
                    Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Error: Memory allocation failed");
                    break;
                case FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE:
                    Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Error: File opening failed (not applicable for stream decoder)");
                    break;
                case FLAC__STREAM_DECODER_INIT_STATUS_ALREADY_INITIALIZED:
                    Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Error: Decoder already initialized");
                    break;
                default:
                    Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Error: Unknown initialization failure");
                    break;
            }
            
            // Clean up failed decoder
            m_decoder.reset();
            return false;
        }
        
        // Verify decoder state after initialization
        FLAC__StreamDecoderState decoder_state = m_decoder->get_state();
        if (decoder_state != FLAC__STREAM_DECODER_SEARCH_FOR_METADATA) {
            Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Warning: Unexpected initial decoder state: ", 
                      FLAC__StreamDecoderStateString[decoder_state]);
            // Continue anyway - decoder might still be functional
        }
        
        // Initialize decoder state and prepare for high-performance frame processing
        Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Preparing for high-performance frame processing");
        
        // Clear any previous error state
        if (m_decoder->hasError()) {
            Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Clearing previous decoder error state");
            m_decoder->clearError();
        }
        
        // Pre-allocate decoder internal buffers if possible
        // Note: libFLAC handles most internal buffer management, but we ensure our buffers are ready
        try {
            // Ensure our output buffer is ready for maximum block size
            size_t max_output_samples = 65535 * m_channels; // RFC 9639 max block size * channels
            {
                std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
                if (m_output_buffer.capacity() < max_output_samples) {
                    m_output_buffer.reserve(max_output_samples);
                }
            }
            
            Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Output buffer prepared for ", 
                      max_output_samples, " samples");
            
        } catch (const std::bad_alloc& e) {
            Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Warning: Failed to pre-allocate output buffer: ", e.what());
            // Continue anyway - buffers will be allocated as needed
        }
        
        m_decoder_initialized = true;
        
        Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] libFLAC decoder initialized successfully");
        Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Decoder ready for high-performance frame processing");
        Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Configuration: MD5=disabled, metadata=STREAMINFO only");
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Exception during initialization: ", e.what());
        
        // Clean up on exception
        if (m_decoder) {
            try {
                if (m_decoder_initialized) {
                    m_decoder->finish();
                    m_decoder_initialized = false;
                }
                m_decoder.reset();
            } catch (...) {
                Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Exception during cleanup");
            }
        }
        
        return false;
    } catch (...) {
        Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Unknown exception during initialization");
        
        // Clean up on unknown exception
        if (m_decoder) {
            try {
                if (m_decoder_initialized) {
                    m_decoder->finish();
                    m_decoder_initialized = false;
                }
                m_decoder.reset();
            } catch (...) {
                // Ignore cleanup exceptions
            }
        }
        
        return false;
    }
}

void FLACCodec::cleanupFLAC_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] Cleaning up libFLAC resources with graceful shutdown");
    
    if (m_decoder) {
        try {
            if (m_decoder_initialized) {
                Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] Finishing libFLAC decoder");
                
                // Check decoder state before finishing
                FLAC__StreamDecoderState state = m_decoder->get_state();
                Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] Decoder state before finish: ", 
                          FLAC__StreamDecoderStateString[state]);
                
                // Finish the decoder gracefully
                if (!m_decoder->finish()) {
                    Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] Warning: Decoder finish() returned false");
                } else {
                    Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] Decoder finished successfully");
                }
                
                m_decoder_initialized = false;
            } else {
                Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] Decoder was not initialized, skipping finish()");
            }
            
            // Clear any remaining input data
            if (m_decoder->hasInputData()) {
                Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] Clearing remaining input data");
                m_decoder->clearInputBuffer();
            }
            
            // Reset the decoder instance
            Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] Destroying decoder instance");
            m_decoder.reset();
            
        } catch (const std::exception& e) {
            Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] Exception during cleanup: ", e.what());
            
            // Force cleanup even on exception
            m_decoder_initialized = false;
            m_decoder.reset();
            
        } catch (...) {
            Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] Unknown exception during cleanup");
            
            // Force cleanup even on unknown exception
            m_decoder_initialized = false;
            m_decoder.reset();
        }
    } else {
        Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] No decoder to clean up");
    }
    
    // Clear any decoder-related state
    m_decoder_initialized = false;
    
    Debug::log("flac_codec", "[FLACCodec::cleanupFLAC_unlocked] libFLAC cleanup completed successfully");
}

// Frame processing methods

bool FLACCodec::processFrameData_unlocked(const uint8_t* data, size_t size) {
    if (!data || size == 0 || !m_decoder) {
        return false;
    }
    
    return m_decoder->feedData(data, size);
}

AudioFrame FLACCodec::extractDecodedSamples_unlocked() {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    if (m_output_buffer.empty()) {
        Debug::log("flac_codec", "[FLACCodec::extractDecodedSamples_unlocked] No samples in buffer");
        return AudioFrame();
    }
    
    // Create AudioFrame with optimal memory usage
    AudioFrame frame;
    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.timestamp_samples = m_current_sample.load();
    
    // Calculate sample frame count (samples per channel)
    size_t sample_frame_count = m_output_buffer.size() / m_channels;
    
    // Efficient sample data transfer
    frame.samples.reserve(m_output_buffer.size());
    frame.samples.assign(m_output_buffer.begin(), m_output_buffer.end());
    
    // Update position tracking
    m_current_sample.fetch_add(sample_frame_count);
    
    // Calculate timestamps
    if (m_sample_rate > 0) {
        frame.timestamp_ms = (frame.timestamp_samples * 1000ULL) / m_sample_rate;
    }
    
    Debug::log("flac_codec", "[FLACCodec::extractDecodedSamples_unlocked] Extracted ", 
              sample_frame_count, " sample frames (", m_output_buffer.size(), " samples) at ", 
              frame.timestamp_samples);
    
    return frame;
}

// Callback handler methods (called by FLACStreamDecoder)

void FLACCodec::handleWriteCallback_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    if (!frame || !buffer) {
        Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Invalid parameters");
        return;
    }
    
    Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Processing frame: ", 
              frame->header.blocksize, " samples, ", frame->header.channels, " channels, ", 
              frame->header.bits_per_sample, " bits, sample ", frame->header.number.sample_number);
    
    try {
        // Validate frame parameters against our configuration
        if (frame->header.channels != m_channels) {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Channel count mismatch: expected ", 
                      m_channels, ", got ", frame->header.channels);
            return;
        }
        
        if (frame->header.sample_rate != m_sample_rate) {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Sample rate mismatch: expected ", 
                      m_sample_rate, ", got ", frame->header.sample_rate);
            return;
        }
        
        // Update current block size
        m_last_block_size = frame->header.blocksize;
        
        // Process channel assignment and convert samples
        processChannelAssignment_unlocked(frame, buffer);
        
        Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Successfully processed frame");
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Exception: ", e.what());
        m_stats.error_count++;
    }
}

void FLACCodec::handleMetadataCallback_unlocked(const FLAC__StreamMetadata* metadata) {
    if (!metadata) {
        return;
    }
    
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        const FLAC__StreamMetadata_StreamInfo& info = metadata->data.stream_info;
        
        Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] STREAMINFO: ",
                  info.sample_rate, "Hz, ", info.channels, " channels, ", 
                  info.bits_per_sample, " bits, ", info.total_samples, " samples");
        
        // Update configuration from metadata if needed
        if (m_sample_rate == 0) m_sample_rate = info.sample_rate;
        if (m_channels == 0) m_channels = info.channels;
        if (m_bits_per_sample == 0) m_bits_per_sample = info.bits_per_sample;
        if (m_total_samples == 0) m_total_samples = info.total_samples;
    }
}

void FLACCodec::handleErrorCallback_unlocked(FLAC__StreamDecoderErrorStatus status) {
    m_stats.error_count++;
    m_stats.libflac_errors++;
    
    Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] libFLAC error: ", 
              FLAC__StreamDecoderErrorStatusString[status]);
    
    // Handle specific error types
    switch (status) {
        case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
            m_stats.sync_errors++;
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
        case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
            m_stats.crc_errors++;
            break;
        default:
            break;
    }
}

// Error handling methods

AudioFrame FLACCodec::handleDecodingError_unlocked(const MediaChunk& chunk) {
    m_stats.error_count++;
    
    Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Handling decoding error");
    
    // Try to recover from error
    if (recoverFromError_unlocked()) {
        Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Error recovery successful");
        // Return silence frame for this chunk
        return createSilenceFrame_unlocked(1024);
    } else {
        Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Error recovery failed");
        setErrorState_unlocked(true);
        return AudioFrame();
    }
}

bool FLACCodec::recoverFromError_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::recoverFromError_unlocked] Attempting error recovery");
    
    try {
        // Reset libFLAC decoder state
        resetDecoderState_unlocked();
        
        // Clear any error flags
        if (m_decoder) {
            m_decoder->clearError();
        }
        
        Debug::log("flac_codec", "[FLACCodec::recoverFromError_unlocked] Error recovery completed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::recoverFromError_unlocked] Recovery failed: ", e.what());
        return false;
    }
}

void FLACCodec::resetDecoderState_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] Resetting decoder state");
    
    if (m_decoder && m_decoder_initialized) {
        if (!m_decoder->reset()) {
            Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] libFLAC reset failed");
        }
        m_decoder->clearInputBuffer();
    }
}

AudioFrame FLACCodec::createSilenceFrame_unlocked(uint32_t block_size) {
    AudioFrame frame;
    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.timestamp_samples = m_current_sample.load();
    
    // Create silence samples
    size_t sample_count = static_cast<size_t>(block_size) * m_channels;
    frame.samples.resize(sample_count, 0); // Fill with silence (zeros)
    
    // Update position
    m_current_sample.fetch_add(block_size);
    
    Debug::log("flac_codec", "[FLACCodec::createSilenceFrame_unlocked] Created silence frame: ", 
              block_size, " samples");
    
    return frame;
}

void FLACCodec::setErrorState_unlocked(bool error_state) {
    m_error_state.store(error_state);
}

// Memory management methods

void FLACCodec::optimizeBufferSizes_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::optimizeBufferSizes_unlocked] Optimizing buffer sizes");
    
    // Pre-calculate buffer sizes based on configuration
    size_t max_block_size = 65535; // RFC 9639 maximum
    size_t required_buffer_size = max_block_size * m_channels;
    
    // Pre-allocate buffers to avoid runtime allocations
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        m_output_buffer.reserve(required_buffer_size);
    }
    
    m_decode_buffer.reserve(required_buffer_size);
    m_input_buffer.reserve(64 * 1024); // 64KB input buffer
    
    Debug::log("flac_codec", "[FLACCodec::optimizeBufferSizes_unlocked] Buffers optimized for ", 
              required_buffer_size, " samples");
}

void FLACCodec::ensureBufferCapacity_unlocked(size_t required_samples) {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    if (m_output_buffer.capacity() < required_samples) {
        m_output_buffer.reserve(required_samples * 2); // Over-allocate for future frames
        Debug::log("flac_codec", "[FLACCodec::ensureBufferCapacity_unlocked] Expanded buffer to ", 
                  m_output_buffer.capacity(), " samples");
    }
}

void FLACCodec::freeUnusedMemory_unlocked() {
    // Shrink buffers if they're significantly over-allocated
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        if (m_output_buffer.capacity() > m_output_buffer.size() * 4) {
            std::vector<int16_t>(m_output_buffer).swap(m_output_buffer);
        }
    }
    
    if (m_decode_buffer.capacity() > m_decode_buffer.size() * 4) {
        std::vector<FLAC__int32>(m_decode_buffer).swap(m_decode_buffer);
    }
}

// Performance monitoring methods

void FLACCodec::updatePerformanceStats_unlocked(uint32_t block_size, uint64_t decode_time_us) {
    m_stats.frames_decoded++;
    m_stats.samples_decoded += block_size;
    m_stats.total_decode_time_us += decode_time_us;
    
    // Update min/max decode times
    if (decode_time_us > m_stats.max_frame_decode_time_us) {
        m_stats.max_frame_decode_time_us = decode_time_us;
    }
    if (decode_time_us < m_stats.min_frame_decode_time_us) {
        m_stats.min_frame_decode_time_us = decode_time_us;
    }
    
    // Update average frame size (simple moving average)
    if (m_stats.frames_decoded > 0) {
        m_stats.average_frame_size = static_cast<double>(m_stats.total_bytes_processed) / m_stats.frames_decoded;
    }
}

void FLACCodec::logPerformanceMetrics_unlocked() const {
    Debug::log("flac_codec", "[FLACCodec] Performance metrics: ",
              m_stats.frames_decoded, " frames, ",
              m_stats.samples_decoded, " samples, ",
              m_stats.getAverageDecodeTimeUs(), " μs/frame avg, ",
              m_stats.getErrorRate(), "% error rate");
}

// Channel processing methods

void FLACCodec::processChannelAssignment_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    if (!frame || !buffer) {
        return;
    }
    
    // Handle different channel assignments per RFC 9639
    switch (frame->header.channel_assignment) {
        case FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT:
            processIndependentChannels_unlocked(frame, buffer);
            break;
        case FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE:
            processLeftSideStereo_unlocked(frame, buffer);
            break;
        case FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE:
            processRightSideStereo_unlocked(frame, buffer);
            break;
        case FLAC__CHANNEL_ASSIGNMENT_MID_SIDE:
            processMidSideStereo_unlocked(frame, buffer);
            break;
        default:
            Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] Unknown channel assignment: ", 
                      frame->header.channel_assignment);
            processIndependentChannels_unlocked(frame, buffer); // Fallback
            break;
    }
}

void FLACCodec::processIndependentChannels_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    uint32_t block_size = frame->header.blocksize;
    uint16_t channels = frame->header.channels;
    
    // Ensure output buffer has sufficient capacity
    size_t required_samples = static_cast<size_t>(block_size) * channels;
    if (m_output_buffer.capacity() < required_samples) {
        m_output_buffer.reserve(required_samples * 2);
    }
    m_output_buffer.resize(required_samples);
    
    // Convert and interleave samples
    for (uint32_t sample = 0; sample < block_size; ++sample) {
        for (uint16_t channel = 0; channel < channels; ++channel) {
            size_t output_index = sample * channels + channel;
            FLAC__int32 raw_sample = buffer[channel][sample];
            
            // Convert based on bit depth
            int16_t converted_sample;
            switch (frame->header.bits_per_sample) {
                case 8:
                    converted_sample = convert8BitTo16Bit(raw_sample);
                    break;
                case 16:
                    converted_sample = static_cast<int16_t>(raw_sample);
                    break;
                case 24:
                    converted_sample = convert24BitTo16Bit(raw_sample);
                    break;
                case 32:
                    converted_sample = convert32BitTo16Bit(raw_sample);
                    break;
                default:
                    // Generic conversion for unusual bit depths
                    if (frame->header.bits_per_sample < 16) {
                        converted_sample = static_cast<int16_t>(raw_sample << (16 - frame->header.bits_per_sample));
                    } else {
                        converted_sample = static_cast<int16_t>(raw_sample >> (frame->header.bits_per_sample - 16));
                    }
                    break;
            }
            
            m_output_buffer[output_index] = converted_sample;
        }
    }
    
    m_stats.conversion_operations++;
    
    Debug::log("flac_codec", "[FLACCodec::processIndependentChannels_unlocked] Processed ", 
              block_size, " samples, ", channels, " channels");
}

void FLACCodec::processLeftSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    uint32_t block_size = frame->header.blocksize;
    
    // Ensure we have exactly 2 channels for stereo processing
    if (frame->header.channels != 2) {
        Debug::log("flac_codec", "[FLACCodec::processLeftSideStereo_unlocked] Invalid channel count for left-side stereo: ", 
                  frame->header.channels);
        return;
    }
    
    size_t required_samples = static_cast<size_t>(block_size) * 2;
    if (m_output_buffer.capacity() < required_samples) {
        m_output_buffer.reserve(required_samples * 2);
    }
    m_output_buffer.resize(required_samples);
    
    // Left-Side stereo reconstruction: Left = buffer[0], Right = Left - buffer[1]
    for (uint32_t i = 0; i < block_size; ++i) {
        FLAC__int32 left = buffer[0][i];
        FLAC__int32 side = buffer[1][i];
        FLAC__int32 right = left - side;
        
        // Convert to 16-bit and store interleaved
        int16_t left_16, right_16;
        switch (frame->header.bits_per_sample) {
            case 8:
                left_16 = convert8BitTo16Bit(left);
                right_16 = convert8BitTo16Bit(right);
                break;
            case 16:
                left_16 = static_cast<int16_t>(left);
                right_16 = static_cast<int16_t>(right);
                break;
            case 24:
                left_16 = convert24BitTo16Bit(left);
                right_16 = convert24BitTo16Bit(right);
                break;
            case 32:
                left_16 = convert32BitTo16Bit(left);
                right_16 = convert32BitTo16Bit(right);
                break;
            default:
                left_16 = static_cast<int16_t>(left >> (frame->header.bits_per_sample - 16));
                right_16 = static_cast<int16_t>(right >> (frame->header.bits_per_sample - 16));
                break;
        }
        
        m_output_buffer[i * 2] = left_16;
        m_output_buffer[i * 2 + 1] = right_16;
    }
    
    m_stats.conversion_operations++;
    
    Debug::log("flac_codec", "[FLACCodec::processLeftSideStereo_unlocked] Processed ", 
              block_size, " left-side stereo samples");
}

void FLACCodec::processRightSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    uint32_t block_size = frame->header.blocksize;
    
    if (frame->header.channels != 2) {
        Debug::log("flac_codec", "[FLACCodec::processRightSideStereo_unlocked] Invalid channel count for right-side stereo: ", 
                  frame->header.channels);
        return;
    }
    
    size_t required_samples = static_cast<size_t>(block_size) * 2;
    if (m_output_buffer.capacity() < required_samples) {
        m_output_buffer.reserve(required_samples * 2);
    }
    m_output_buffer.resize(required_samples);
    
    // Right-Side stereo reconstruction: Right = buffer[1], Left = buffer[0] - Right
    for (uint32_t i = 0; i < block_size; ++i) {
        FLAC__int32 side = buffer[0][i];
        FLAC__int32 right = buffer[1][i];
        FLAC__int32 left = side - right;
        
        // Convert to 16-bit and store interleaved
        int16_t left_16, right_16;
        switch (frame->header.bits_per_sample) {
            case 8:
                left_16 = convert8BitTo16Bit(left);
                right_16 = convert8BitTo16Bit(right);
                break;
            case 16:
                left_16 = static_cast<int16_t>(left);
                right_16 = static_cast<int16_t>(right);
                break;
            case 24:
                left_16 = convert24BitTo16Bit(left);
                right_16 = convert24BitTo16Bit(right);
                break;
            case 32:
                left_16 = convert32BitTo16Bit(left);
                right_16 = convert32BitTo16Bit(right);
                break;
            default:
                left_16 = static_cast<int16_t>(left >> (frame->header.bits_per_sample - 16));
                right_16 = static_cast<int16_t>(right >> (frame->header.bits_per_sample - 16));
                break;
        }
        
        m_output_buffer[i * 2] = left_16;
        m_output_buffer[i * 2 + 1] = right_16;
    }
    
    m_stats.conversion_operations++;
    
    Debug::log("flac_codec", "[FLACCodec::processRightSideStereo_unlocked] Processed ", 
              block_size, " right-side stereo samples");
}

void FLACCodec::processMidSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    uint32_t block_size = frame->header.blocksize;
    
    if (frame->header.channels != 2) {
        Debug::log("flac_codec", "[FLACCodec::processMidSideStereo_unlocked] Invalid channel count for mid-side stereo: ", 
                  frame->header.channels);
        return;
    }
    
    size_t required_samples = static_cast<size_t>(block_size) * 2;
    if (m_output_buffer.capacity() < required_samples) {
        m_output_buffer.reserve(required_samples * 2);
    }
    m_output_buffer.resize(required_samples);
    
    // Mid-Side stereo reconstruction: Left = (Mid + Side) / 2, Right = (Mid - Side) / 2
    for (uint32_t i = 0; i < block_size; ++i) {
        FLAC__int32 mid = buffer[0][i];
        FLAC__int32 side = buffer[1][i];
        
        // Reconstruct left and right channels
        FLAC__int32 left = (mid + side) >> 1;   // Divide by 2 using bit shift
        FLAC__int32 right = (mid - side) >> 1;  // Divide by 2 using bit shift
        
        // Convert to 16-bit and store interleaved
        int16_t left_16, right_16;
        switch (frame->header.bits_per_sample) {
            case 8:
                left_16 = convert8BitTo16Bit(left);
                right_16 = convert8BitTo16Bit(right);
                break;
            case 16:
                left_16 = static_cast<int16_t>(left);
                right_16 = static_cast<int16_t>(right);
                break;
            case 24:
                left_16 = convert24BitTo16Bit(left);
                right_16 = convert24BitTo16Bit(right);
                break;
            case 32:
                left_16 = convert32BitTo16Bit(left);
                right_16 = convert32BitTo16Bit(right);
                break;
            default:
                left_16 = static_cast<int16_t>(left >> (frame->header.bits_per_sample - 16));
                right_16 = static_cast<int16_t>(right >> (frame->header.bits_per_sample - 16));
                break;
        }
        
        m_output_buffer[i * 2] = left_16;
        m_output_buffer[i * 2 + 1] = right_16;
    }
    
    m_stats.conversion_operations++;
    
    Debug::log("flac_codec", "[FLACCodec::processMidSideStereo_unlocked] Processed ", 
              block_size, " mid-side stereo samples");
}

// Bit depth conversion methods

int16_t FLACCodec::convert8BitTo16Bit(FLAC__int32 sample) const {
    // Scale 8-bit (-128 to 127) to 16-bit (-32768 to 32767)
    // Use bit shifting for maximum performance
    return static_cast<int16_t>(sample << 8);
}

int16_t FLACCodec::convert24BitTo16Bit(FLAC__int32 sample) const {
    // High-quality downscaling from 24-bit to 16-bit
    // Simple truncation for maximum performance (could add dithering later)
    return static_cast<int16_t>(sample >> 8);
}

int16_t FLACCodec::convert32BitTo16Bit(FLAC__int32 sample) const {
    // Scale down from 32-bit to 16-bit range with overflow protection
    // Use arithmetic right shift to preserve sign
    int32_t scaled = sample >> 16;
    
    // Clamp to 16-bit range to prevent overflow
    if (scaled > 32767) {
        return 32767;
    } else if (scaled < -32768) {
        return -32768;
    } else {
        return static_cast<int16_t>(scaled);
    }
}

void FLACCodec::convertSamplesGeneric_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Generic conversion for unusual bit depths
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    size_t required_samples = static_cast<size_t>(block_size) * m_channels;
    if (m_output_buffer.capacity() < required_samples) {
        m_output_buffer.reserve(required_samples * 2);
    }
    m_output_buffer.resize(required_samples);
    
    int shift_amount = 0;
    if (m_bits_per_sample < 16) {
        shift_amount = 16 - m_bits_per_sample; // Left shift for upscaling
    } else if (m_bits_per_sample > 16) {
        shift_amount = m_bits_per_sample - 16; // Right shift for downscaling
    }
    
    for (uint32_t sample = 0; sample < block_size; ++sample) {
        for (uint16_t channel = 0; channel < m_channels; ++channel) {
            size_t output_index = sample * m_channels + channel;
            FLAC__int32 raw_sample = buffer[channel][sample];
            
            int16_t converted_sample;
            if (m_bits_per_sample < 16) {
                converted_sample = static_cast<int16_t>(raw_sample << shift_amount);
            } else if (m_bits_per_sample > 16) {
                int32_t scaled = raw_sample >> shift_amount;
                converted_sample = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
            } else {
                converted_sample = static_cast<int16_t>(raw_sample);
            }
            
            m_output_buffer[output_index] = converted_sample;
        }
    }
    
    m_stats.conversion_operations++;
}

// ============================================================================
// FLACCodecSupport Implementation
// ============================================================================

namespace FLACCodecSupport {

void registerCodec() {
    Debug::log("flac_codec", "[FLACCodecSupport::registerCodec] Registering FLAC codec with AudioCodecFactory");
    
    AudioCodecFactory::registerCodec("flac", [](const StreamInfo& stream_info) -> std::unique_ptr<AudioCodec> {
        if (isFLACStream(stream_info)) {
            return std::make_unique<FLACCodec>(stream_info);
        }
        return nullptr;
    });
    
    Debug::log("flac_codec", "[FLACCodecSupport::registerCodec] FLAC codec registered successfully");
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
    return "FLAC Codec v1.0 - RFC 9639 compliant, container-agnostic FLAC decoder";
}

} // namespace FLACCodecSupport

#endif // HAVE_FLAC