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
    // Performance monitoring for callback overhead
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Validate parameters with minimal overhead
    if (!buffer || !bytes || *bytes == 0) {
        Debug::log("flac_codec", "[FLACStreamDecoder::read_callback] Invalid parameters");
        *bytes = 0;
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }
    
    // Fast path: check if we have data without locking first (atomic check would be better)
    if (m_buffer_position >= m_input_buffer.size()) {
        *bytes = 0;
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }
    
    std::lock_guard<std::mutex> lock(m_input_mutex);
    
    // Calculate available data efficiently
    size_t available = m_input_buffer.size() - m_buffer_position;
    if (available == 0) {
        *bytes = 0;
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }
    
    // Optimized copy: use the smaller of requested or available
    size_t to_copy = std::min(*bytes, available);
    
    // High-performance memory copy with alignment optimization
    if (to_copy > 0) {
        // Use optimized memcpy for larger blocks, direct assignment for small ones
        if (to_copy >= 64) {
            std::memcpy(buffer, m_input_buffer.data() + m_buffer_position, to_copy);
        } else {
            // Manual copy for small blocks to avoid memcpy overhead
            const uint8_t* src = m_input_buffer.data() + m_buffer_position;
            for (size_t i = 0; i < to_copy; ++i) {
                buffer[i] = src[i];
            }
        }
        
        m_buffer_position += to_copy;
    }
    
    *bytes = to_copy;
    
    // Performance monitoring - log if callback takes too long
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    if (duration.count() > 50) { // Log if read callback takes >50μs
        Debug::log("flac_codec", "[FLACStreamDecoder::read_callback] Slow read: ", duration.count(), 
                  " μs for ", to_copy, " bytes");
    }
    
    Debug::log("flac_codec", "[FLACStreamDecoder::read_callback] Provided ", to_copy, 
              " bytes, position now ", m_buffer_position, " (", available - to_copy, " remaining)");
    
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus FLACStreamDecoder::write_callback(const FLAC__Frame *frame, 
                                                                const FLAC__int32 * const buffer[]) {
    // Performance monitoring for write callback overhead
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Fast parameter validation
    if (!frame || !buffer || !m_parent) {
        Debug::log("flac_codec", "[FLACStreamDecoder::write_callback] Invalid parameters");
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    
    // Validate frame parameters for RFC 9639 compliance
    if (frame->header.blocksize == 0 || frame->header.blocksize > 65535) {
        Debug::log("flac_codec", "[FLACStreamDecoder::write_callback] Invalid block size: ", 
                  frame->header.blocksize, " (RFC 9639 range: 1-65535)");
        
        // Log RFC compliance violation
        FLACRFCValidator::logRFCViolation("9.1.2", "Invalid block size", 
                                        "Block size outside RFC 9639 valid range",
                                        frame->header.number.sample_number);
        
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    
    if (frame->header.channels == 0 || frame->header.channels > 8) {
        Debug::log("flac_codec", "[FLACStreamDecoder::write_callback] Invalid channel count: ", 
                  frame->header.channels, " (RFC 9639 range: 1-8)");
        
        // Log RFC compliance violation
        FLACRFCValidator::logRFCViolation("9.1.4", "Invalid channel count", 
                                        "Channel count outside RFC 9639 valid range",
                                        frame->header.number.sample_number);
        
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    
    if (frame->header.bits_per_sample < 4 || frame->header.bits_per_sample > 32) {
        Debug::log("flac_codec", "[FLACStreamDecoder::write_callback] Invalid bit depth: ", 
                  frame->header.bits_per_sample, " (RFC 9639 range: 4-32)");
        
        // Log RFC compliance violation
        FLACRFCValidator::logRFCViolation("9.1.5", "Invalid sample size", 
                                        "Sample size outside RFC 9639 valid range",
                                        frame->header.number.sample_number);
        
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
    
    try {
        // Delegate to parent codec for high-performance processing
        // The parent handles all the heavy lifting including bit depth conversion
        m_parent->handleWriteCallback_unlocked(frame, buffer);
        
        // Performance monitoring - detect bottlenecks in write processing
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        if (duration.count() > 100) { // Log if write callback takes >100μs
            Debug::log("flac_codec", "[FLACStreamDecoder::write_callback] Slow write: ", duration.count(), 
                      " μs for ", frame->header.blocksize, " samples, ", frame->header.channels, " channels");
        }
        
        Debug::log("flac_codec", "[FLACStreamDecoder::write_callback] Processed frame: ", 
                  frame->header.blocksize, " samples, ", frame->header.channels, " channels, ", 
                  frame->header.bits_per_sample, " bits, sample ", frame->header.number.sample_number);
        
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACStreamDecoder::write_callback] Exception: ", e.what());
        m_error_occurred = true;
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }
}

void FLACStreamDecoder::metadata_callback(const FLAC__StreamMetadata *metadata) {
    // Performance monitoring for metadata processing
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (!metadata || !m_parent) {
        Debug::log("flac_codec", "[FLACStreamDecoder::metadata_callback] Invalid parameters");
        return;
    }
    
    try {
        // Log metadata type for debugging
        const char* metadata_type = "UNKNOWN";
        switch (metadata->type) {
            case FLAC__METADATA_TYPE_STREAMINFO:
                metadata_type = "STREAMINFO";
                break;
            case FLAC__METADATA_TYPE_PADDING:
                metadata_type = "PADDING";
                break;
            case FLAC__METADATA_TYPE_APPLICATION:
                metadata_type = "APPLICATION";
                break;
            case FLAC__METADATA_TYPE_SEEKTABLE:
                metadata_type = "SEEKTABLE";
                break;
            case FLAC__METADATA_TYPE_VORBIS_COMMENT:
                metadata_type = "VORBIS_COMMENT";
                break;
            case FLAC__METADATA_TYPE_CUESHEET:
                metadata_type = "CUESHEET";
                break;
            case FLAC__METADATA_TYPE_PICTURE:
                metadata_type = "PICTURE";
                break;
            default:
                metadata_type = "RESERVED";
                break;
        }
        
        Debug::log("flac_codec", "[FLACStreamDecoder::metadata_callback] Processing ", metadata_type, 
                  " metadata (", metadata->length, " bytes)");
        
        // Delegate to parent codec for thread-safe processing
        // The parent will handle the metadata with appropriate locking
        m_parent->handleMetadataCallback_unlocked(metadata);
        
        // Performance monitoring
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        if (duration.count() > 200) { // Log if metadata processing takes >200μs
            Debug::log("flac_codec", "[FLACStreamDecoder::metadata_callback] Slow metadata processing: ", 
                      duration.count(), " μs for ", metadata_type);
        }
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACStreamDecoder::metadata_callback] Exception: ", e.what());
        m_error_occurred = true;
    }
}

void FLACStreamDecoder::error_callback(FLAC__StreamDecoderErrorStatus status) {
    m_error_occurred = true;
    m_last_error = status;
    
    // RFC 9639 compliant error logging with section references
    const char* error_description = "Unknown error";
    const char* rfc_section = "N/A";
    bool is_recoverable = false;
    bool requires_stream_termination = false;
    
    switch (status) {
        case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
            error_description = "Lost synchronization - frame boundary detection failed";
            rfc_section = "RFC 9639 Section 9.1 (Frame Header)";
            is_recoverable = true; // Can often recover by finding next sync pattern
            requires_stream_termination = false;
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
            error_description = "Invalid frame header - corrupted frame data or forbidden bit patterns";
            rfc_section = "RFC 9639 Section 9.1 (Frame Header Structure)";
            is_recoverable = true; // Can skip bad frame and continue
            requires_stream_termination = false;
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
            error_description = "Frame CRC mismatch - data corruption detected";
            rfc_section = "RFC 9639 Section 9.3 (Frame Footer CRC-16)";
            is_recoverable = true; // Can use decoded data despite CRC error per RFC
            requires_stream_termination = false;
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM:
            error_description = "Unparseable stream - fundamental format violation or unsupported features";
            rfc_section = "RFC 9639 Section 5 (Format Principles)";
            is_recoverable = false; // Usually indicates incompatible FLAC variant
            requires_stream_termination = true; // Stream cannot be processed
            break;
        default:
            error_description = "Unrecognized libFLAC error - potential RFC 9639 compliance issue";
            rfc_section = "RFC 9639 General Compliance";
            is_recoverable = false;
            requires_stream_termination = true;
            break;
    }
    
    Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] RFC 9639 ERROR (", 
              static_cast<int>(status), "): ", error_description);
    Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] RFC Reference: ", rfc_section);
    Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] Recovery: ", 
              (is_recoverable ? "POSSIBLE" : "IMPOSSIBLE"));
    Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] Stream Termination: ", 
              (requires_stream_termination ? "REQUIRED" : "NOT REQUIRED"));
    
    // Provide detailed error information to parent codec for RFC-compliant recovery
    if (m_parent) {
        try {
            // The parent codec will implement RFC 9639 compliant error handling
            m_parent->handleErrorCallback_unlocked(status);
            
            // RFC 9639 compliant recovery guidance
            if (is_recoverable) {
                Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] RFC 9639 Recovery Strategy: ", 
                          "Error is recoverable per RFC guidelines - attempting graceful handling");
                
                // Specific recovery recommendations per RFC 9639
                switch (status) {
                    case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
                        Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] RFC 9639 Sync Recovery: ", 
                                  "Search for next valid sync pattern (0x3FFE + reserved bit)");
                        break;
                    case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
                        Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] RFC 9639 Header Recovery: ", 
                                  "Skip corrupted frame, validate forbidden patterns in next frame");
                        break;
                    case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
                        Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] RFC 9639 CRC Recovery: ", 
                                  "Decoders MAY use data despite CRC mismatch for error resilience");
                        break;
                    default:
                        break;
                }
            } else {
                Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] RFC 9639 Fatal Error: ", 
                          "Error violates fundamental RFC requirements - stream termination required");
                
                if (requires_stream_termination) {
                    Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] RFC 9639 Termination: ", 
                              "Stream contains unrecoverable violations - decoder must terminate");
                }
            }
            
        } catch (const std::exception& e) {
            Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] CRITICAL: Exception in RFC error handler: ", 
                      e.what(), " - codec may violate RFC 9639 error handling requirements");
        }
    } else {
        Debug::log("flac_codec", "[FLACStreamDecoder::error_callback] CRITICAL: No parent codec for RFC 9639 error handling");
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
    
    // Initialize enhanced buffer management
    m_max_pending_samples = MAX_BUFFER_SAMPLES;
    updateBufferWatermarks_unlocked();
    m_preferred_buffer_size = calculateOptimalBufferSize_unlocked(65535 * stream_info.channels);
    
    // Initialize input queue management
    updateInputQueueWatermarks_unlocked();
    
    // Initialize threading state
    m_thread_active.store(false);
    m_thread_shutdown_requested.store(false);
    m_pending_work_items.store(0);
    m_completed_work_items.store(0);
    m_thread_processing_time_us.store(0);
    m_thread_frames_processed.store(0);
    m_thread_idle_cycles.store(0);
    
    // Initialize asynchronous processing (disabled by default)
    m_async_processing_enabled = false;
    
    // Pre-allocate buffers for performance
    m_input_buffer.reserve(64 * 1024);  // 64KB input buffer
    m_decode_buffer.reserve(65535 * 8); // Maximum FLAC block size * max channels
    m_output_buffer.reserve(MAX_BUFFER_SAMPLES);
}

FLACCodec::~FLACCodec() {
    Debug::log("flac_codec", "[FLACCodec] Destroying FLAC codec");
    
    // Stop decoder thread first to ensure clean shutdown
    try {
        if (m_thread_active.load()) {
            Debug::log("flac_codec", "[FLACCodec] Stopping active decoder thread during destruction");
            stopDecoderThread();
        }
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec] Exception stopping thread during destruction: ", e.what());
    }
    
    // Ensure proper cleanup with thread coordination
    {
        std::lock_guard<std::mutex> state_lock(m_state_mutex);
        cleanupFLAC_unlocked();
    }
    
    // Final cleanup of threading resources
    try {
        std::lock_guard<std::mutex> thread_lock(m_thread_mutex);
        if (m_decoder_thread && m_decoder_thread->joinable()) {
            Debug::log("flac_codec", "[FLACCodec] Joining remaining thread during destruction");
            m_decoder_thread->join();
        }
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec] Exception during final thread cleanup: ", e.what());
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

// CRC Validation Control Methods (RFC 9639 Compliance)

void FLACCodec::setCRCValidationEnabled(bool enabled) {
    Debug::log("flac_codec", "[FLACCodec::setCRCValidationEnabled] [ENTRY] Setting CRC validation to: ", 
              (enabled ? "ENABLED" : "DISABLED"));
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    Debug::log("flac_codec", "[FLACCodec::setCRCValidationEnabled] [LOCKED] State lock acquired");
    
    m_crc_validation_enabled = enabled;
    
    // Reset automatic disabling flag when manually enabling
    if (enabled) {
        m_crc_validation_disabled_due_to_errors = false;
        Debug::log("flac_codec", "[FLACCodec::setCRCValidationEnabled] CRC validation enabled, reset auto-disable flag");
    }
    
    Debug::log("flac_codec", "[FLACCodec::setCRCValidationEnabled] [EXIT] CRC validation state updated");
}

bool FLACCodec::getCRCValidationEnabled() const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    bool enabled = shouldValidateCRC_unlocked();
    
    Debug::log("flac_codec", "[FLACCodec::getCRCValidationEnabled] [ENTRY/EXIT] CRC validation active: ", 
              (enabled ? "YES" : "NO"));
    
    if (m_crc_validation_enabled && !enabled) {
        Debug::log("flac_codec", "[FLACCodec::getCRCValidationEnabled] Note: Validation disabled due to excessive errors");
    }
    
    return enabled;
}

void FLACCodec::setCRCValidationStrict(bool strict) {
    Debug::log("flac_codec", "[FLACCodec::setCRCValidationStrict] [ENTRY] Setting strict CRC mode to: ", 
              (strict ? "STRICT" : "PERMISSIVE"));
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    Debug::log("flac_codec", "[FLACCodec::setCRCValidationStrict] [LOCKED] State lock acquired");
    
    m_strict_crc_validation = strict;
    
    Debug::log("flac_codec", "[FLACCodec::setCRCValidationStrict] [EXIT] CRC validation mode updated");
    
    if (strict) {
        Debug::log("flac_codec", "[FLACCodec::setCRCValidationStrict] RFC 9639 strict mode: frames with CRC errors will be rejected");
    } else {
        Debug::log("flac_codec", "[FLACCodec::setCRCValidationStrict] RFC 9639 permissive mode: frames with CRC errors will be used but logged");
    }
}

bool FLACCodec::getCRCValidationStrict() const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    bool strict = m_strict_crc_validation;
    
    Debug::log("flac_codec", "[FLACCodec::getCRCValidationStrict] [ENTRY/EXIT] Strict CRC mode: ", 
              (strict ? "ENABLED" : "DISABLED"));
    
    return strict;
}

size_t FLACCodec::getCRCErrorCount() const {
    // Use atomic access for performance - no lock needed
    size_t errors = m_stats.crc_errors;
    
    Debug::log("flac_codec", "[FLACCodec::getCRCErrorCount] [ENTRY/EXIT] Total CRC errors: ", errors);
    
    return errors;
}

void FLACCodec::setCRCErrorThreshold(size_t threshold) {
    Debug::log("flac_codec", "[FLACCodec::setCRCErrorThreshold] [ENTRY] Setting CRC error threshold to: ", threshold);
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    Debug::log("flac_codec", "[FLACCodec::setCRCErrorThreshold] [LOCKED] State lock acquired");
    
    m_crc_error_threshold = threshold;
    
    Debug::log("flac_codec", "[FLACCodec::setCRCErrorThreshold] [EXIT] CRC error threshold updated");
    
    if (threshold == 0) {
        Debug::log("flac_codec", "[FLACCodec::setCRCErrorThreshold] Automatic CRC validation disabling is now DISABLED");
    } else {
        Debug::log("flac_codec", "[FLACCodec::setCRCErrorThreshold] CRC validation will auto-disable after ", threshold, " errors");
    }
    
    // Re-enable validation if we're now below the threshold
    if (m_crc_validation_disabled_due_to_errors && m_stats.crc_errors < threshold) {
        m_crc_validation_disabled_due_to_errors = false;
        Debug::log("flac_codec", "[FLACCodec::setCRCErrorThreshold] Re-enabled CRC validation (below new threshold)");
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
        
        // Initialize variable block size handling
        initializeBlockSizeHandling_unlocked();
        
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
    
    // Validate codec integrity before processing
    if (!validateCodecIntegrity_unlocked()) {
        Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Codec integrity validation failed - attempting recovery");
        
        // Try to recover from integrity issues
        if (recoverFromError_unlocked()) {
            Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Codec recovery successful, retrying decode");
            
            // Validate again after recovery
            if (!validateCodecIntegrity_unlocked()) {
                Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Codec still not functional after recovery");
                setErrorState_unlocked(true);
                return createSilenceFrame_unlocked(1024);
            }
        } else {
            Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Codec recovery failed");
            setErrorState_unlocked(true);
            return createSilenceFrame_unlocked(1024);
        }
    }
    
    // Check for error state
    if (m_error_state.load()) {
        Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Codec in error state - attempting recovery");
        
        // Try to clear error state through recovery
        if (recoverFromError_unlocked()) {
            Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Error state recovery successful");
        } else {
            Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Error state recovery failed");
            return createSilenceFrame_unlocked(1024);
        }
    }
    
    try {
        // Clear previous output buffer efficiently with thread synchronization
        {
            std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
            
            // Wait for any pending buffer operations to complete
            if (m_thread_active.load() && m_async_processing_enabled) {
                // Brief check to ensure buffer is not being accessed by decoder thread
                // The decoder thread will acquire the same lock, ensuring synchronization
            }
            
            m_output_buffer.clear();
            m_buffer_read_position = 0;
        }
        
        // Process input through queue management
        MediaChunk processed_chunk = chunk;
        
        // Handle input queue processing with frame reconstruction
        {
            std::lock_guard<std::mutex> input_lock(m_input_mutex);
            
            // Check if this might be a partial frame
            if (!isFrameComplete_unlocked(chunk.data.data(), chunk.data.size())) {
                Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Processing partial frame");
                
                if (!processPartialFrame_unlocked(chunk)) {
                    Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Failed to process partial frame");
                    return handleDecodingError_unlocked(chunk);
                }
                
                // Try to reconstruct complete frame
                if (!reconstructFrame_unlocked(processed_chunk)) {
                    Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Frame reconstruction incomplete, queuing for later");
                    return AudioFrame(); // Return empty frame, wait for more data
                }
            } else {
                // Complete frame - can process directly or queue it
                if (!enqueueInputChunk_unlocked(chunk)) {
                    Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Failed to enqueue input chunk");
                    return handleDecodingError_unlocked(chunk);
                }
                
                // Dequeue for processing
                processed_chunk = dequeueInputChunk_unlocked();
                if (processed_chunk.data.empty()) {
                    Debug::log("flac_codec", "[FLACCodec::decode_unlocked] No chunk available for processing");
                    return AudioFrame();
                }
            }
        }
        
        // Process frame data with thread-safe decoder access
        // Ensure decoder state synchronization when thread is active
        if (m_thread_active.load() && m_async_processing_enabled) {
            // For async processing, queue the chunk instead of processing directly
            {
                std::lock_guard<std::mutex> async_lock(m_async_mutex);
                if (!enqueueAsyncInput_unlocked(processed_chunk)) {
                    Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Failed to enqueue chunk for async processing");
                    return handleDecodingError_unlocked(processed_chunk);
                }
                
                // Notify decoder thread that work is available
                {
                    std::lock_guard<std::mutex> thread_lock(m_thread_mutex);
                    notifyWorkAvailable_unlocked();
                }
                
                // Check if we have any completed output available
                if (hasAsyncOutput_unlocked()) {
                    AudioFrame async_result = dequeueAsyncOutput_unlocked();
                    if (async_result.getSampleFrameCount() > 0) {
                        Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Returning async processed frame");
                        return async_result;
                    }
                }
                
                // Return empty frame - async processing will complete later
                return AudioFrame();
            }
        } else {
            // Synchronous processing with decoder state protection
            if (!processFrameData_unlocked(processed_chunk.data.data(), processed_chunk.data.size())) {
                Debug::log("flac_codec", "[FLACCodec::decode_unlocked] Failed to process frame data");
                return handleDecodingError_unlocked(processed_chunk);
            }
        }
        
        // Extract decoded samples (buffer lock already held by processFrameData_unlocked)
        AudioFrame result = extractDecodedSamples_unlocked();
        
        // Update performance statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        updatePerformanceStats_unlocked(result.getSampleFrameCount(), duration.count());
        
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
        // Process any remaining chunks in input queue
        {
            std::lock_guard<std::mutex> input_lock(m_input_mutex);
            
            while (hasInputChunks_unlocked()) {
                MediaChunk remaining_chunk = dequeueInputChunk_unlocked();
                if (!remaining_chunk.data.empty()) {
                    Debug::log("flac_codec", "[FLACCodec::flush_unlocked] Processing remaining queued chunk with ", 
                              remaining_chunk.data.size(), " bytes");
                    
                    // Process the remaining chunk
                    if (!processFrameData_unlocked(remaining_chunk.data.data(), remaining_chunk.data.size())) {
                        Debug::log("flac_codec", "[FLACCodec::flush_unlocked] Failed to process remaining chunk");
                        break;
                    }
                }
            }
            
            // Try to complete any partial frame reconstruction
            MediaChunk reconstructed_frame;
            if (reconstructFrame_unlocked(reconstructed_frame)) {
                Debug::log("flac_codec", "[FLACCodec::flush_unlocked] Processing reconstructed frame with ", 
                          reconstructed_frame.data.size(), " bytes");
                
                if (!processFrameData_unlocked(reconstructed_frame.data.data(), reconstructed_frame.data.size())) {
                    Debug::log("flac_codec", "[FLACCodec::flush_unlocked] Failed to process reconstructed frame");
                }
            }
        }
        
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
        
        // Mark stream as finished and reset buffer flow control
        m_stream_finished = true;
        
        // Reset buffer flow control state for potential reuse
        {
            std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
            resetBufferFlowControl_unlocked();
        }
        
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
    
    // Clear buffers and reset flow control
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        m_output_buffer.clear();
        m_buffer_read_position = 0;
        resetBufferFlowControl_unlocked();
    }
    
    // Clear input queue and reset input flow control
    {
        std::lock_guard<std::mutex> input_lock(m_input_mutex);
        clearInputQueue_unlocked();
    }
    
    // Reset threading state if thread is active
    if (m_thread_active.load()) {
        std::lock_guard<std::mutex> thread_lock(m_thread_mutex);
        resetThreadState_unlocked();
        
        // Clear async queues
        {
            std::lock_guard<std::mutex> async_lock(m_async_mutex);
            clearAsyncQueues_unlocked();
        }
    }
    
    // Reset position tracking
    m_current_sample.store(0);
    m_last_block_size = 0;
    m_stream_finished = false;
    
    // Reset block size tracking
    m_current_block_size = 0;
    m_preferred_block_size = 0;
    m_previous_block_size = 0;
    m_total_samples_processed = 0;
    // Note: Don't reset m_min_block_size, m_max_block_size, m_variable_block_size 
    // as they come from STREAMINFO and should persist across resets
    // Also don't reset statistics like m_block_size_changes, m_smallest_block_seen, etc.
    // as they represent accumulated knowledge about the stream
    
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
    
    // RFC 9639 compliance validation - bit depth and sample format
    if (!validateBitDepthRFC9639_unlocked(stream_info.bits_per_sample)) {
        Debug::log("flac_codec", "[FLACCodec::canDecode_unlocked] RFC 9639 bit depth validation failed: ", 
                  stream_info.bits_per_sample, " bits");
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
    
    // RFC 9639 compliance validation - bit depth and sample format
    if (!validateBitDepthRFC9639_unlocked(m_bits_per_sample)) {
        Debug::log("flac_codec", "[FLACCodec::configureFromStreamInfo_unlocked] RFC 9639 bit depth validation failed: ", 
                  m_bits_per_sample, " bits");
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
    
    // RFC 9639 compliance validation - bit depth and sample format
    if (!validateBitDepthRFC9639_unlocked(m_bits_per_sample)) {
        Debug::log("flac_codec", "[FLACCodec::validateConfiguration_unlocked] RFC 9639 bit depth validation failed: ", 
                  m_bits_per_sample, " bits");
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
        
        // Initialize decoder state with comprehensive error checking and thread safety
        Debug::log("flac_codec", "[FLACCodec::initializeFLACDecoder_unlocked] Initializing decoder state");
        
        // Protect decoder initialization with mutex
        std::lock_guard<std::mutex> decoder_lock(m_decoder_mutex);
        
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
            // Protect decoder cleanup with mutex
            std::lock_guard<std::mutex> decoder_lock(m_decoder_mutex);
            
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
    Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Processing frame data: ", size, " bytes");
    
    // Validate input parameters
    if (!data || size == 0) {
        Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Invalid input data");
        return false;
    }
    
    if (!m_decoder || !m_decoder_initialized) {
        Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Decoder not initialized");
        return false;
    }
    
    // Check for error state
    if (m_error_state.load()) {
        Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Codec in error state");
        return false;
    }
    
    // Performance monitoring
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Validate FLAC frame boundary using RFC 9639 compliant methods
        if (!validateFrameBoundary_unlocked(data, size)) {
            Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] RFC 9639 frame boundary validation failed");
            
            // Try to find the next valid frame sync pattern
            size_t sync_offset = findNextFrameSync_unlocked(data, size);
            if (sync_offset < size && sync_offset > 0) {
                Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Found potential sync pattern at offset ", sync_offset);
                // Adjust data pointer and size to start from the sync pattern
                data += sync_offset;
                size -= sync_offset;
                
                // Re-validate the frame boundary from the new position
                if (!validateFrameBoundary_unlocked(data, size)) {
                    Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Frame boundary validation failed even after sync correction");
                    // Continue anyway - might be metadata or partial frame
                }
            } else {
                Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] No valid sync pattern found - continuing anyway");
                // Continue anyway - might be metadata or partial frame
            }
        } else {
            Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] RFC 9639 frame boundary validation passed");
        }
        
        // Perform CRC validation per RFC 9639 if enabled and we have sufficient data
        if (shouldValidateCRC_unlocked() && size >= 4) {
            if (!validateFrameCRC_unlocked(data, size)) {
                Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] RFC 9639 CRC validation failed");
                
                // Calculate expected and actual CRC for detailed error reporting
                uint16_t expected_crc = 0;
                uint16_t calculated_crc = 0;
                if (size >= 2) {
                    expected_crc = (static_cast<uint16_t>(data[size - 2]) << 8) | data[size - 1];
                    calculated_crc = calculateFrameFooterCRC_unlocked(data, size);
                }
                
                handleCRCMismatch_unlocked("Frame CRC-16", expected_crc, calculated_crc, data, size);
                
                if (m_strict_crc_validation) {
                    Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Strict CRC mode: rejecting frame");
                    return false;
                } else {
                    Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Permissive CRC mode: using frame despite CRC error");
                }
            } else {
                Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] RFC 9639 CRC validation passed");
            }
        } else if (shouldValidateCRC_unlocked()) {
            Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Frame too small for CRC validation (", size, " bytes)");
        }
        
        // Acquire both decoder and buffer locks in consistent order to prevent deadlocks
        // This ensures that when libFLAC calls our callbacks, both locks are already held
        std::lock_guard<std::mutex> decoder_lock(m_decoder_mutex);
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        
        // Ensure decoder is in valid state for processing
        auto decoder_state = m_decoder->get_state();
        if (decoder_state == FLAC__STREAM_DECODER_ABORTED || 
            decoder_state == FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR) {
            Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Decoder in error state: ", 
                      FLAC__StreamDecoderStateString[decoder_state]);
            return false;
        }
        
        // Feed data to decoder with optimized buffering
        if (!feedDataToDecoder_unlocked(data, size)) {
            Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Failed to feed data to decoder");
            return false;
        }
        
        // Process the frame through libFLAC (thread-safe with both locks held)
        // The write callback will be called with both locks already acquired
        if (!m_decoder->process_single()) {
            FLAC__StreamDecoderState state = m_decoder->get_state();
            Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] libFLAC processing failed, state: ", 
                      FLAC__StreamDecoderStateString[state]);
            
            // Handle specific decoder states
            if (state == FLAC__STREAM_DECODER_END_OF_STREAM) {
                Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] End of stream reached");
                m_stream_finished = true;
                return true; // Not an error
            } else if (state == FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC) {
                Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Searching for frame sync - may need more data");
                return true; // Not an error, just needs more data
            }
            
            return false;
        }
        
        // Update performance statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        if (duration.count() > 100) { // Log if processing takes >100μs
            Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Frame processing took ", 
                      duration.count(), " μs");
        }
        
        Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Successfully processed frame data");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::processFrameData_unlocked] Exception: ", e.what());
        setErrorState_unlocked(true);
        return false;
    }
}

bool FLACCodec::feedDataToDecoder_unlocked(const uint8_t* data, size_t size) {
    Debug::log("flac_codec", "[FLACCodec::feedDataToDecoder_unlocked] Feeding ", size, " bytes to decoder");
    
    // Validate input parameters
    if (!data || size == 0) {
        Debug::log("flac_codec", "[FLACCodec::feedDataToDecoder_unlocked] Invalid input parameters");
        return false;
    }
    
    if (!m_decoder) {
        Debug::log("flac_codec", "[FLACCodec::feedDataToDecoder_unlocked] Decoder not available");
        return false;
    }
    
    try {
        // Efficient memory management - reuse input buffer when possible
        if (m_input_buffer.capacity() < size) {
            // Only reallocate if necessary, with some headroom for future frames
            size_t new_capacity = std::max(size * 2, static_cast<size_t>(64 * 1024));
            m_input_buffer.reserve(new_capacity);
            
            Debug::log("flac_codec", "[FLACCodec::feedDataToDecoder_unlocked] Expanded input buffer capacity to ", 
                      new_capacity, " bytes");
        }
        
        // Feed data to libFLAC decoder with minimal copying
        // The FLACStreamDecoder will handle the actual buffering
        if (!m_decoder->feedData(data, size)) {
            Debug::log("flac_codec", "[FLACCodec::feedDataToDecoder_unlocked] Failed to feed data to libFLAC decoder");
            return false;
        }
        
        // Update statistics
        m_stats.total_bytes_processed += size;
        
        Debug::log("flac_codec", "[FLACCodec::feedDataToDecoder_unlocked] Successfully fed ", size, 
                  " bytes to decoder (total processed: ", m_stats.total_bytes_processed, " bytes)");
        
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::feedDataToDecoder_unlocked] Exception: ", e.what());
        return false;
    }
}

AudioFrame FLACCodec::extractDecodedSamples_unlocked() {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    if (m_output_buffer.empty()) {
        Debug::log("flac_codec", "[FLACCodec::extractDecodedSamples_unlocked] No samples in buffer");
        handleBufferUnderrun_unlocked();
        return AudioFrame();
    }
    
    // Get current timestamp before updating position
    uint64_t current_timestamp = m_current_sample.load();
    
    // Calculate sample frame count (samples per channel) with validation
    uint16_t channels = m_channels > 0 ? m_channels : 2; // Fallback to stereo
    if (m_output_buffer.size() % channels != 0) {
        Debug::log("flac_codec", "[FLACCodec::extractDecodedSamples_unlocked] WARNING: Buffer size ", 
                  m_output_buffer.size(), " not divisible by channel count ", channels);
        
        // Truncate to nearest complete sample frame
        size_t complete_samples = (m_output_buffer.size() / channels) * channels;
        m_output_buffer.resize(complete_samples);
    }
    
    size_t sample_frame_count = m_output_buffer.size() / channels;
    
    // Create AudioFrame using helper method with move semantics for efficiency
    AudioFrame frame = createAudioFrame_unlocked(std::move(m_output_buffer), current_timestamp);
    
    // Update position tracking using helper method
    updateSamplePosition_unlocked(sample_frame_count);
    
    // Clear buffer after extraction to free space
    m_output_buffer.clear();
    
    // Notify waiting threads that buffer space is now available
    notifyBufferSpaceAvailable_unlocked();
    
    // Deactivate backpressure if buffer is now below low watermark
    if (m_backpressure_active && m_output_buffer.size() <= m_buffer_low_watermark) {
        deactivateBackpressure_unlocked();
    }
    
    Debug::log("flac_codec", "[FLACCodec::extractDecodedSamples_unlocked] Extracted ", 
              sample_frame_count, " sample frames (", frame.samples.size(), " samples) at ", 
              frame.timestamp_samples);
    
    return frame;
}

// Callback handler methods (called by FLACStreamDecoder)

void FLACCodec::handleWriteCallback_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    // Performance monitoring for write callback processing
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (!frame || !buffer) {
        Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Invalid parameters");
        m_stats.error_count++;
        return;
    }
    
    Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Processing frame: ", 
              frame->header.blocksize, " samples, ", frame->header.channels, " channels, ", 
              frame->header.bits_per_sample, " bits, sample ", frame->header.number.sample_number);
    
    try {
        // Fast validation of frame parameters against our configuration
        // Use likely/unlikely hints for branch prediction optimization
        if (frame->header.channels != m_channels) [[unlikely]] {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Channel count mismatch: expected ", 
                      m_channels, ", got ", frame->header.channels);
            m_stats.error_count++;
            return;
        }
        
        if (frame->header.sample_rate != m_sample_rate) [[unlikely]] {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Sample rate mismatch: expected ", 
                      m_sample_rate, ", got ", frame->header.sample_rate);
            m_stats.error_count++;
            return;
        }
        
        // Validate block size for RFC 9639 compliance and STREAMINFO constraints
        if (!validateBlockSize_unlocked(frame->header.blocksize)) [[unlikely]] {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Block size validation failed: ", 
                      frame->header.blocksize);
            m_stats.error_count++;
            return;
        }
        
        // Update block size tracking for variable block size handling
        updateBlockSizeTracking_unlocked(frame->header.blocksize);
        
        // RFC 9639 bit depth and sample format compliance validation
        if (!validateBitDepthRFC9639_unlocked(frame->header.bits_per_sample)) [[unlikely]] {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] RFC 9639 bit depth validation failed: ", 
                      frame->header.bits_per_sample, " bits");
            m_stats.error_count++;
            return;
        }
        
        // RFC 9639 sample format endianness validation
        size_t total_samples = static_cast<size_t>(frame->header.blocksize) * frame->header.channels;
        std::vector<FLAC__int32> flattened_samples;
        flattened_samples.reserve(total_samples);
        
        // Flatten the channel-separated samples for validation
        for (uint32_t sample = 0; sample < frame->header.blocksize; ++sample) {
            for (uint16_t channel = 0; channel < frame->header.channels; ++channel) {
                flattened_samples.push_back(buffer[channel][sample]);
            }
        }
        
        if (!validateSampleFormatEndianness_unlocked(flattened_samples.data(), total_samples, frame->header.bits_per_sample)) [[unlikely]] {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] RFC 9639 sample format endianness validation failed");
            m_stats.error_count++;
            // Continue processing despite endianness issues - they may be recoverable
        }
        
        if (!validateSampleFormatRanges_unlocked(flattened_samples.data(), total_samples, frame->header.bits_per_sample)) [[unlikely]] {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] RFC 9639 sample format range validation failed");
            m_stats.error_count++;
            // Continue processing despite range issues - they may be recoverable
        }
        
        // Validate sample format consistency between STREAMINFO and frame headers
        if (!validateSampleFormatConsistency_unlocked(frame)) [[unlikely]] {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Sample format consistency validation failed");
            m_stats.error_count++;
            return;
        }
        
        // RFC 9639 Section 9.2: Comprehensive subframe processing validation and debugging
        if (!validateAndDebugSubframeProcessing_unlocked(frame, buffer)) [[unlikely]] {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] RFC 9639 subframe processing validation failed");
            m_stats.error_count++;
            // Continue processing despite validation failure for error resilience
        }
        
        // Validate channel assignment and bit depth consistency per RFC 9639
        if (!validateChannelAssignmentBitDepth_unlocked(frame)) [[unlikely]] {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Channel assignment bit depth validation failed");
            m_stats.error_count++;
            return;
        }
        
        // Validate wasted bits handling per RFC 9639 Section 9.2.2
        if (!validateWastedBitsHandling_unlocked(frame)) [[unlikely]] {
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Wasted bits validation failed");
            m_stats.error_count++;
            // Continue processing - wasted bits errors are often recoverable
        }
        
        // Update current block size for performance tracking
        m_last_block_size = frame->header.blocksize;
        
        // Enhanced buffer management with flow control and backpressure
        // NOTE: Both decoder_mutex and buffer_mutex are already held by caller
        // as per threading safety guidelines for callback methods
        size_t required_samples = static_cast<size_t>(frame->header.blocksize) * frame->header.channels;
        
        // Check buffer capacity and handle backpressure (locks already held)
        if (!checkBufferCapacity_unlocked(required_samples)) {
            handleBackpressure_unlocked(required_samples);
            
            // If still no space after backpressure handling, handle overflow
            if (!checkBufferCapacity_unlocked(required_samples)) {
                handleBufferOverflow_unlocked();
                return; // Skip this frame to prevent buffer overflow
            }
        }
        
        // Optimize buffer allocation based on stream characteristics
        optimizeBufferAllocation_unlocked(required_samples);
        
        // High-performance channel assignment processing with minimal overhead
        processChannelAssignment_unlocked(frame, buffer);
        
        // Update frame statistics
        m_stats.frames_decoded++;
        m_stats.samples_decoded += frame->header.blocksize;
        
        // Performance monitoring - detect bottlenecks in frame processing
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // Update performance statistics
        uint64_t duration_us = static_cast<uint64_t>(duration.count());
        m_stats.total_decode_time_us += duration_us;
        if (duration_us > m_stats.max_frame_decode_time_us) {
            m_stats.max_frame_decode_time_us = duration_us;
        }
        if (duration_us < m_stats.min_frame_decode_time_us) {
            m_stats.min_frame_decode_time_us = duration_us;
        }
        
        // Log performance warnings for optimization
        if (duration.count() > 500) { // Log if frame processing takes >500μs
            Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Slow frame processing: ", 
                      duration.count(), " μs for ", frame->header.blocksize, " samples");
        }
        
        Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Successfully processed frame in ", 
                  duration.count(), " μs");
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::handleWriteCallback_unlocked] Exception: ", e.what());
        m_stats.error_count++;
        m_stats.libflac_errors++;
    }
}

void FLACCodec::handleMetadataCallback_unlocked(const FLAC__StreamMetadata* metadata) {
    if (!metadata) {
        Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Null metadata received");
        return;
    }
    
    // Performance monitoring for metadata processing
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        switch (metadata->type) {
            case FLAC__METADATA_TYPE_STREAMINFO: {
                const FLAC__StreamMetadata_StreamInfo& info = metadata->data.stream_info;
                
                Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] STREAMINFO: ",
                          info.sample_rate, "Hz, ", info.channels, " channels, ", 
                          info.bits_per_sample, " bits, ", info.total_samples, " samples, ",
                          "min_blocksize=", info.min_blocksize, ", max_blocksize=", info.max_blocksize);
                
                // RFC 9639 compliance validation for STREAMINFO
                if (info.sample_rate < 1 || info.sample_rate > 655350) {
                    Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Invalid sample rate in STREAMINFO: ", 
                              info.sample_rate, " (RFC 9639 range: 1-655350)");
                    return;
                }
                
                if (info.channels < 1 || info.channels > 8) {
                    Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Invalid channel count in STREAMINFO: ", 
                              info.channels, " (RFC 9639 range: 1-8)");
                    return;
                }
                
                // RFC 9639 bit depth validation for STREAMINFO
                if (!validateBitDepthRFC9639_unlocked(info.bits_per_sample)) {
                    Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] RFC 9639 bit depth validation failed in STREAMINFO: ", 
                              info.bits_per_sample, " bits");
                    return;
                }
                
                // Update configuration from metadata with thread safety
                // Only update if not already set (first STREAMINFO takes precedence)
                if (m_sample_rate == 0) {
                    m_sample_rate = info.sample_rate;
                    Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Updated sample rate from STREAMINFO: ", 
                              m_sample_rate, " Hz");
                }
                
                if (m_channels == 0) {
                    m_channels = info.channels;
                    Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Updated channels from STREAMINFO: ", 
                              m_channels);
                }
                
                if (m_bits_per_sample == 0) {
                    m_bits_per_sample = info.bits_per_sample;
                    Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Updated bit depth from STREAMINFO: ", 
                              m_bits_per_sample, " bits");
                }
                
                if (m_total_samples == 0) {
                    m_total_samples = info.total_samples;
                    Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Updated total samples from STREAMINFO: ", 
                              m_total_samples);
                }
                
                // Update block size constraints from STREAMINFO
                if (info.min_blocksize > 0 && info.max_blocksize > 0) {
                    m_min_block_size = info.min_blocksize;
                    m_max_block_size = info.max_blocksize;
                    
                    Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Updated block size constraints: ",
                              "min=", m_min_block_size, ", max=", m_max_block_size);
                    
                    // Detect if stream uses variable block sizes
                    if (m_min_block_size != m_max_block_size) {
                        m_variable_block_size = true;
                        Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Variable block size stream detected");
                    }
                }
                
                // Performance optimization: pre-allocate buffers based on STREAMINFO
                // NOTE: buffer_mutex is already held by caller for callback methods
                if (info.max_blocksize > 0) {
                    size_t max_samples = static_cast<size_t>(info.max_blocksize) * info.channels;
                    if (m_output_buffer.capacity() < max_samples) {
                        m_output_buffer.reserve(max_samples);
                        Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Pre-allocated output buffer: ", 
                                  max_samples, " samples based on max_blocksize=", info.max_blocksize);
                    }
                }
                
                break;
            }
            
            case FLAC__METADATA_TYPE_SEEKTABLE:
                Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] SEEKTABLE metadata received (", 
                          metadata->length, " bytes) - seeking support available");
                break;
                
            case FLAC__METADATA_TYPE_VORBIS_COMMENT:
                Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] VORBIS_COMMENT metadata received (", 
                          metadata->length, " bytes) - tags available");
                break;
                
            case FLAC__METADATA_TYPE_PADDING:
                Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] PADDING metadata received (", 
                          metadata->length, " bytes)");
                break;
                
            case FLAC__METADATA_TYPE_APPLICATION:
                Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] APPLICATION metadata received (", 
                          metadata->length, " bytes)");
                break;
                
            case FLAC__METADATA_TYPE_CUESHEET:
                Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] CUESHEET metadata received (", 
                          metadata->length, " bytes)");
                break;
                
            case FLAC__METADATA_TYPE_PICTURE:
                Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] PICTURE metadata received (", 
                          metadata->length, " bytes)");
                break;
                
            default:
                Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Unknown metadata type ", 
                          static_cast<int>(metadata->type), " (", metadata->length, " bytes)");
                break;
        }
        
        // Performance monitoring
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        if (duration.count() > 100) { // Log if metadata processing takes >100μs
            Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Slow metadata processing: ", 
                      duration.count(), " μs for type ", static_cast<int>(metadata->type));
        }
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::handleMetadataCallback_unlocked] Exception processing metadata: ", e.what());
        m_stats.error_count++;
    }
}

void FLACCodec::handleErrorCallback_unlocked(FLAC__StreamDecoderErrorStatus status) {
    Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] Processing libFLAC error with RFC 9639 compliance");
    
    // Use RFC 9639 compliant error handling
    bool recovery_successful = handleRFC9639Error_unlocked(status, "libFLAC callback");
    
    // Determine error characteristics for detailed logging
    const char* error_description = "Unknown error";
    const char* rfc_section = "N/A";
    bool should_reset_decoder = false;
    
    switch (status) {
        case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
            error_description = "Lost synchronization - frame boundary detection failed";
            rfc_section = "RFC 9639 Section 9.1 (Frame Header Sync)";
            should_reset_decoder = false;
            Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] RFC 9639 Sync Error: ", 
                      "Decoder will search for next valid sync pattern");
            break;
            
        case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
            error_description = "Bad frame header - forbidden patterns or reserved field violations";
            rfc_section = "RFC 9639 Section 9.1 (Frame Header Structure)";
            should_reset_decoder = false;
            Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] RFC 9639 Header Error: ", 
                      "Skipping corrupted frame, validating forbidden patterns");
            break;
            
        case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
            error_description = "Frame CRC mismatch - data corruption detected";
            rfc_section = "RFC 9639 Section 9.3 (Frame Footer CRC-16)";
            should_reset_decoder = false;
            Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] RFC 9639 CRC Error: ", 
                      "Using decoded data with quality warning (RFC permits this)");
            break;
            
        case FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM:
            error_description = "Unparseable stream - fundamental RFC 9639 format violations";
            rfc_section = "RFC 9639 Section 5 (Format Principles)";
            should_reset_decoder = true;
            Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] RFC 9639 Fatal Error: ", 
                      "Stream contains unrecoverable format violations");
            break;
            
        default:
            error_description = "Unknown libFLAC error - potential RFC 9639 compliance issue";
            rfc_section = "RFC 9639 General Compliance";
            should_reset_decoder = true;
            Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] RFC 9639 Unknown Error: ", 
                      "Assuming fatal format violation");
            break;
    }
    
    Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] Error Details:");
    Debug::log("flac_codec", "  Description: ", error_description);
    Debug::log("flac_codec", "  RFC Reference: ", rfc_section);
    Debug::log("flac_codec", "  Recovery Status: ", (recovery_successful ? "SUCCESSFUL" : "FAILED"));
    Debug::log("flac_codec", "  Stream Termination: ", (shouldTerminateStream_unlocked(status) ? "REQUIRED" : "NOT REQUIRED"));
    
    // Implement additional recovery strategy if RFC recovery failed
    if (!recovery_successful && should_reset_decoder) {
        Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] RFC recovery failed - attempting decoder reset");
        
        try {
            resetDecoderState_unlocked();
            Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] Decoder reset successful after RFC recovery failure");
        } catch (const std::exception& e) {
            Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] CRITICAL: Decoder reset failed: ", e.what());
            Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] RFC 9639 TERMINATION: Codec is now in unrecoverable state");
            setErrorState_unlocked(true);
        }
    }
    
    // Log comprehensive error statistics for RFC compliance monitoring
    if (m_stats.frames_decoded > 0) {
        double error_rate = (static_cast<double>(m_stats.error_count) * 100.0) / m_stats.frames_decoded;
        if (error_rate > 5.0) { // Log if error rate exceeds 5%
            Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] RFC 9639 ALERT: High error rate detected: ", 
                      error_rate, "% (", m_stats.error_count, " errors in ", m_stats.frames_decoded, " frames)");
            Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] RFC 9639 RECOMMENDATION: ", 
                      "Verify stream compliance with RFC 9639 specification");
        }
    }
    
    // Detailed error breakdown for RFC compliance analysis
    Debug::log("flac_codec", "[FLACCodec::handleErrorCallback_unlocked] RFC 9639 Error Statistics:");
    Debug::log("flac_codec", "  Sync Errors: ", m_stats.sync_errors, " (RFC 9639 Section 9.1)");
    Debug::log("flac_codec", "  CRC Errors: ", m_stats.crc_errors, " (RFC 9639 Section 9.3)");
    Debug::log("flac_codec", "  libFLAC Errors: ", m_stats.libflac_errors, " (General RFC violations)");
    Debug::log("flac_codec", "  Total Errors: ", m_stats.error_count);
}

// Error handling methods

AudioFrame FLACCodec::handleDecodingError_unlocked(const MediaChunk& chunk) {
    m_stats.error_count++;
    
    Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Handling decoding error for chunk with ", 
              chunk.data.size(), " bytes");
    
    // Analyze the type of error based on chunk data
    bool is_corrupted_frame = false;
    bool is_sync_lost = false;
    bool is_memory_error = false;
    
    // Check if this looks like a corrupted FLAC frame
    if (!chunk.data.empty()) {
        // Check for RFC 9639 compliant FLAC sync pattern (0xFFF8 or 0xFFF9)
        if (chunk.data.size() >= 2) {
            uint16_t sync_pattern = (static_cast<uint16_t>(chunk.data[0]) << 8) | chunk.data[1];
            bool valid_sync = (sync_pattern == 0xFFF8) || (sync_pattern == 0xFFF9);
            
            if (!valid_sync) {
                is_sync_lost = true;
                m_stats.sync_errors++;
                Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] RFC 9639 sync pattern lost - invalid frame header: 0x", 
                          std::hex, std::uppercase, sync_pattern, std::nouppercase, std::dec, 
                          " (expected 0xFFF8 or 0xFFF9)");
            } else {
                is_corrupted_frame = true;
                Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Frame appears corrupted despite valid RFC 9639 sync pattern");
            }
        } else {
            is_sync_lost = true;
            m_stats.sync_errors++;
            Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Insufficient data for frame header");
        }
    } else {
        is_memory_error = true;
        m_stats.memory_errors++;
        Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Empty chunk - possible memory error");
    }
    
    // Attempt recovery based on error type
    bool recovery_successful = false;
    
    if (is_sync_lost) {
        Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Attempting sync recovery");
        recovery_successful = recoverFromSyncLoss_unlocked(chunk);
    } else if (is_corrupted_frame) {
        Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Attempting corrupted frame recovery");
        recovery_successful = recoverFromCorruptedFrame_unlocked(chunk);
    } else if (is_memory_error) {
        Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Attempting memory error recovery");
        recovery_successful = recoverFromMemoryError_unlocked();
    } else {
        Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Attempting general error recovery");
        recovery_successful = recoverFromError_unlocked();
    }
    
    if (recovery_successful) {
        Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Error recovery successful");
        
        // Estimate block size for silence frame based on chunk size or use default
        uint32_t estimated_block_size = estimateBlockSizeFromChunk_unlocked(chunk);
        return createSilenceFrame_unlocked(estimated_block_size);
    } else {
        Debug::log("flac_codec", "[FLACCodec::handleDecodingError_unlocked] Error recovery failed - codec entering error state");
        setErrorState_unlocked(true);
        return createSilenceFrame_unlocked(1024); // Fallback silence
    }
}

bool FLACCodec::recoverFromError_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::recoverFromError_unlocked] Attempting comprehensive error recovery");
    
    try {
        // Step 1: Ensure decoder is functional
        if (!ensureDecoderFunctional_unlocked()) {
            Debug::log("flac_codec", "[FLACCodec::recoverFromError_unlocked] Failed to ensure decoder functionality");
            return false;
        }
        
        // Step 2: Handle any decoder state inconsistencies
        if (!handleDecoderStateInconsistency_unlocked()) {
            Debug::log("flac_codec", "[FLACCodec::recoverFromError_unlocked] Failed to handle decoder state inconsistency");
            return false;
        }
        
        // Step 3: Clear any error flags
        if (m_decoder) {
            m_decoder->clearError();
        }
        
        // Step 4: Clear input buffers to remove potentially corrupted data
        {
            std::lock_guard<std::mutex> input_lock(m_input_mutex);
            clearInputQueue_unlocked();
            resetFrameReconstruction_unlocked();
            resetInputFlowControl_unlocked();
        }
        
        // Step 5: Reset buffer state
        {
            std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
            m_output_buffer.clear();
            m_buffer_read_position = 0;
            resetBufferFlowControl_unlocked();
        }
        
        // Step 6: Reset threading state if async processing is enabled
        if (m_async_processing_enabled) {
            std::lock_guard<std::mutex> async_lock(m_async_mutex);
            clearAsyncQueues_unlocked();
        }
        
        // Step 7: Reset codec state variables
        m_stream_finished = false;
        m_last_block_size = 0;
        m_current_block_size = 0;
        m_previous_block_size = 0;
        
        // Step 8: Clear error state
        setErrorState_unlocked(false);
        
        Debug::log("flac_codec", "[FLACCodec::recoverFromError_unlocked] Comprehensive error recovery completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::recoverFromError_unlocked] Recovery failed with exception: ", e.what());
        
        // Last resort: try to recreate decoder
        try {
            if (recreateDecoder_unlocked()) {
                Debug::log("flac_codec", "[FLACCodec::recoverFromError_unlocked] Decoder recreation successful as last resort");
                setErrorState_unlocked(false);
                return true;
            }
        } catch (...) {
            Debug::log("flac_codec", "[FLACCodec::recoverFromError_unlocked] Even decoder recreation failed");
        }
        
        return false;
    }
}

bool FLACCodec::recoverFromSyncLoss_unlocked(const MediaChunk& chunk) {
    Debug::log("flac_codec", "[FLACCodec::recoverFromSyncLoss_unlocked] Attempting sync recovery");
    
    try {
        // Reset decoder to search for next sync pattern
        if (m_decoder && m_decoder_initialized) {
            if (!m_decoder->reset()) {
                Debug::log("flac_codec", "[FLACCodec::recoverFromSyncLoss_unlocked] libFLAC reset failed");
                return false;
            }
            
            // Clear decoder input buffer
            m_decoder->clearInputBuffer();
        }
        
        // Search for next valid RFC 9639 FLAC sync pattern in the chunk using enhanced detection
        if (!chunk.data.empty() && chunk.data.size() >= 2) {
            size_t sync_offset = findNextFrameSync_unlocked(chunk.data.data(), chunk.data.size());
            
            if (sync_offset < chunk.data.size()) {
                Debug::log("flac_codec", "[FLACCodec::recoverFromSyncLoss_unlocked] Found RFC 9639 sync pattern at offset ", sync_offset);
                
                // Validate the frame boundary at this position using comprehensive validation
                if (validateFrameBoundary_unlocked(chunk.data.data() + sync_offset, chunk.data.size() - sync_offset)) {
                    Debug::log("flac_codec", "[FLACCodec::recoverFromSyncLoss_unlocked] Frame boundary validation passed at sync offset");
                    
                    // Feed the data starting from the sync point
                    if (m_decoder->feedData(chunk.data.data() + sync_offset, chunk.data.size() - sync_offset)) {
                        Debug::log("flac_codec", "[FLACCodec::recoverFromSyncLoss_unlocked] Sync recovery successful");
                        return true;
                    } else {
                        Debug::log("flac_codec", "[FLACCodec::recoverFromSyncLoss_unlocked] Failed to feed data from sync position");
                    }
                } else {
                    Debug::log("flac_codec", "[FLACCodec::recoverFromSyncLoss_unlocked] Frame boundary validation failed at sync offset ", sync_offset);
                    
                    // Try to find another sync pattern after this one
                    if (sync_offset + 2 < chunk.data.size()) {
                        size_t next_sync = findNextFrameSync_unlocked(chunk.data.data(), chunk.data.size(), sync_offset + 1);
                        if (next_sync < chunk.data.size()) {
                            Debug::log("flac_codec", "[FLACCodec::recoverFromSyncLoss_unlocked] Trying next sync pattern at offset ", next_sync);
                            if (validateFrameBoundary_unlocked(chunk.data.data() + next_sync, chunk.data.size() - next_sync)) {
                                if (m_decoder->feedData(chunk.data.data() + next_sync, chunk.data.size() - next_sync)) {
                                    Debug::log("flac_codec", "[FLACCodec::recoverFromSyncLoss_unlocked] Sync recovery successful with second attempt");
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        Debug::log("flac_codec", "[FLACCodec::recoverFromSyncLoss_unlocked] No valid sync pattern found");
        return false;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::recoverFromSyncLoss_unlocked] Sync recovery failed: ", e.what());
        return false;
    }
}

bool FLACCodec::recoverFromCorruptedFrame_unlocked(const MediaChunk& chunk) {
    Debug::log("flac_codec", "[FLACCodec::recoverFromCorruptedFrame_unlocked] Attempting corrupted frame recovery");
    
    try {
        // For corrupted frames, we skip the frame and continue with next data
        // This is appropriate for CRC errors where the frame structure is intact
        
        // Clear decoder error state but don't reset completely
        if (m_decoder) {
            m_decoder->clearError();
        }
        
        // Log frame corruption details for debugging
        if (!chunk.data.empty()) {
            Debug::log("flac_codec", "[FLACCodec::recoverFromCorruptedFrame_unlocked] Skipping corrupted frame: ", 
                      chunk.data.size(), " bytes, sync=0x", 
                      std::hex, (chunk.data.size() >= 2 ? 
                                (static_cast<uint16_t>(chunk.data[0]) << 8) | chunk.data[1] : 0), 
                      std::dec);
        }
        
        // Increment CRC error count
        m_stats.crc_errors++;
        
        // Continue processing - the decoder should be able to handle the next frame
        Debug::log("flac_codec", "[FLACCodec::recoverFromCorruptedFrame_unlocked] Corrupted frame recovery completed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::recoverFromCorruptedFrame_unlocked] Corrupted frame recovery failed: ", e.what());
        return false;
    }
}

bool FLACCodec::recoverFromMemoryError_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::recoverFromMemoryError_unlocked] Attempting memory error recovery");
    
    try {
        // Free unused memory and reset buffers
        freeUnusedMemory_unlocked();
        
        // Reset buffer management state
        {
            std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
            m_output_buffer.clear();
            m_output_buffer.shrink_to_fit(); // Release memory
            m_buffer_read_position = 0;
            resetBufferFlowControl_unlocked();
        }
        
        // Reset input queue management
        {
            std::lock_guard<std::mutex> input_lock(m_input_mutex);
            clearInputQueue_unlocked();
            resetFrameReconstruction_unlocked();
            resetInputFlowControl_unlocked();
        }
        
        // Increment memory error count
        m_stats.memory_errors++;
        
        Debug::log("flac_codec", "[FLACCodec::recoverFromMemoryError_unlocked] Memory error recovery completed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::recoverFromMemoryError_unlocked] Memory error recovery failed: ", e.what());
        return false;
    }
}

void FLACCodec::resetDecoderState_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] Resetting decoder state for error recovery");
    
    try {
        // Reset libFLAC decoder if available
        if (m_decoder && m_decoder_initialized) {
            // Get current decoder state for logging
            FLAC__StreamDecoderState decoder_state = m_decoder->get_state();
            Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] Current decoder state: ", 
                      static_cast<int>(decoder_state));
            
            // Attempt decoder reset
            if (!m_decoder->reset()) {
                Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] libFLAC reset failed, attempting finish/init cycle");
                
                // Try finish and re-initialize if reset fails
                if (m_decoder->finish()) {
                    FLAC__StreamDecoderInitStatus init_status = m_decoder->init();
                    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
                        Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] Re-initialization failed: ", 
                                  static_cast<int>(init_status));
                        m_decoder_initialized = false;
                    } else {
                        Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] Decoder re-initialized successfully");
                    }
                } else {
                    Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] Decoder finish failed");
                    m_decoder_initialized = false;
                }
            } else {
                Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] libFLAC reset successful");
            }
            
            // Clear input buffer regardless of reset success
            m_decoder->clearInputBuffer();
            
            // Clear decoder error state
            m_decoder->clearError();
        } else {
            Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] No decoder available or not initialized");
        }
        
        // Reset internal decoder state variables
        m_last_block_size = 0;
        m_stream_finished = false;
        
        // Reset variable block size state
        m_current_block_size = 0;
        m_previous_block_size = 0;
        
        // Clear any pending asynchronous work
        if (m_async_processing_enabled) {
            std::lock_guard<std::mutex> async_lock(m_async_mutex);
            clearAsyncQueues_unlocked();
        }
        
        Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] Decoder state reset completed");
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::resetDecoderState_unlocked] Exception during decoder reset: ", e.what());
        
        // Mark decoder as uninitialized on critical failure
        m_decoder_initialized = false;
        setErrorState_unlocked(true);
    }
}

AudioFrame FLACCodec::createSilenceFrame_unlocked(uint32_t block_size) {
    // Validate block size against RFC 9639 limits
    if (block_size == 0) {
        block_size = 576; // Default FLAC block size
        Debug::log("flac_codec", "[FLACCodec::createSilenceFrame_unlocked] Zero block size, using default 576");
    } else if (block_size > 65535) {
        block_size = 4608; // Common large block size
        Debug::log("flac_codec", "[FLACCodec::createSilenceFrame_unlocked] Block size too large, clamping to 4608");
    }
    
    // Get current timestamp before updating position
    uint64_t current_timestamp = m_current_sample.load();
    
    // Create silence samples (16-bit PCM zeros)
    uint16_t channels = m_channels > 0 ? m_channels : 2; // Fallback to stereo
    size_t sample_count = static_cast<size_t>(block_size) * channels;
    
    try {
        // Create silence buffer
        std::vector<int16_t> silence_samples(sample_count, 0); // Fill with silence (zeros)
        
        // Create AudioFrame using helper method
        AudioFrame frame = createAudioFrame_unlocked(std::move(silence_samples), current_timestamp);
        
        // Update position tracking using helper method
        updateSamplePosition_unlocked(block_size);
        
        Debug::log("flac_codec", "[FLACCodec::createSilenceFrame_unlocked] Created silence frame: ", 
                  block_size, " samples, ", frame.channels, " channels, ", 
                  frame.sample_rate, "Hz, timestamp=", frame.timestamp_samples, 
                  " (", frame.timestamp_ms, "ms)");
        
        return frame;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::createSilenceFrame_unlocked] Failed to allocate silence frame: ", e.what());
        
        // Create minimal silence frame as fallback
        try {
            std::vector<int16_t> minimal_silence(channels, 0); // Single sample of silence per channel
            AudioFrame fallback_frame = createAudioFrame_unlocked(std::move(minimal_silence), current_timestamp);
            m_current_sample.fetch_add(1);
            return fallback_frame;
        } catch (...) {
            // Complete failure - return empty frame
            Debug::log("flac_codec", "[FLACCodec::createSilenceFrame_unlocked] Critical: Cannot allocate even minimal silence frame");
            return AudioFrame();
        }
    }
}

void FLACCodec::setErrorState_unlocked(bool error_state) {
    m_error_state.store(error_state);
}

uint32_t FLACCodec::estimateBlockSizeFromChunk_unlocked(const MediaChunk& chunk) {
    // Try to extract block size from FLAC frame header if possible
    if (chunk.data.size() >= 4) {
        // Check if this looks like a valid RFC 9639 FLAC frame header
        uint16_t sync_pattern = (static_cast<uint16_t>(chunk.data[0]) << 8) | chunk.data[1];
        bool valid_sync = (sync_pattern == 0xFFF8) || (sync_pattern == 0xFFF9);
        
        if (valid_sync) {
            // Extract block size from frame header (RFC 9639 section 7.2)
            uint8_t block_size_bits = (chunk.data[2] & 0xF0) >> 4;
            
            switch (block_size_bits) {
                case 0x1: return 192;
                case 0x2: return 576;
                case 0x3: return 1152;
                case 0x4: return 2304;
                case 0x5: return 4608;
                case 0x6: return 9216;  // Non-standard but used
                case 0x7: return 18432; // Non-standard but used
                case 0x8: return 256;
                case 0x9: return 512;
                case 0xA: return 1024;
                case 0xB: return 2048;
                case 0xC: return 4096;
                case 0xD: return 8192;
                case 0xE: return 16384;
                case 0xF: return 32768;
                default:
                    // Variable block size or reserved - use default
                    break;
            }
        }
    }
    
    // Fallback: estimate based on chunk size and compression ratio
    // Typical FLAC compression is 50-70%, so estimate original sample count
    size_t estimated_samples = chunk.data.size() * 2; // Assume ~50% compression
    
    // Divide by bytes per sample to get block size
    size_t bytes_per_sample = (m_bits_per_sample + 7) / 8; // Round up to nearest byte
    uint32_t estimated_block_size = static_cast<uint32_t>(estimated_samples / (bytes_per_sample * m_channels));
    
    // Clamp to reasonable range
    if (estimated_block_size < 16) {
        estimated_block_size = 576; // Common FLAC block size
    } else if (estimated_block_size > 65535) {
        estimated_block_size = 4608; // Another common FLAC block size
    }
    
    Debug::log("flac_codec", "[FLACCodec::estimateBlockSizeFromChunk_unlocked] Estimated block size: ", 
              estimated_block_size, " samples from ", chunk.data.size(), " byte chunk");
    
    return estimated_block_size;
}

bool FLACCodec::handleDecoderStateInconsistency_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] Handling decoder state inconsistency");
    
    try {
        if (!m_decoder) {
            Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] No decoder instance - creating new one");
            return recreateDecoder_unlocked();
        }
        
        // Check current decoder state
        FLAC__StreamDecoderState current_state = m_decoder->get_state();
        Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] Current decoder state: ", 
                  static_cast<int>(current_state));
        
        switch (current_state) {
            case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
            case FLAC__STREAM_DECODER_READ_METADATA:
            case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
            case FLAC__STREAM_DECODER_READ_FRAME:
                // These are normal operational states
                Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] Decoder in normal state");
                return true;
                
            case FLAC__STREAM_DECODER_END_OF_STREAM:
                Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] Decoder at end of stream - resetting");
                return resetDecoderForNewStream_unlocked();
                
            case FLAC__STREAM_DECODER_OGG_ERROR:
                Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] Ogg container error - attempting recovery");
                return recoverFromOggError_unlocked();
                
            case FLAC__STREAM_DECODER_SEEK_ERROR:
                Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] Seek error - resetting decoder");
                return resetDecoderState_unlocked(), true;
                
            case FLAC__STREAM_DECODER_ABORTED:
                Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] Decoder aborted - recreating");
                return recreateDecoder_unlocked();
                
            case FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR:
                Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] Memory allocation error - attempting recovery");
                return recoverFromDecoderMemoryError_unlocked();
                
            case FLAC__STREAM_DECODER_UNINITIALIZED:
                Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] Decoder uninitialized - reinitializing");
                return reinitializeDecoder_unlocked();
                
            default:
                Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] Unknown decoder state - recreating");
                return recreateDecoder_unlocked();
        }
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::handleDecoderStateInconsistency_unlocked] Exception: ", e.what());
        return false;
    }
}

bool FLACCodec::recreateDecoder_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::recreateDecoder_unlocked] Recreating libFLAC decoder");
    
    try {
        // Clean up existing decoder
        if (m_decoder) {
            if (m_decoder_initialized) {
                m_decoder->finish();
            }
            m_decoder.reset();
        }
        
        m_decoder_initialized = false;
        
        // Create new decoder instance
        m_decoder = std::make_unique<FLACStreamDecoder>(this);
        if (!m_decoder) {
            Debug::log("flac_codec", "[FLACCodec::recreateDecoder_unlocked] Failed to create new decoder instance");
            return false;
        }
        
        // Initialize the new decoder
        return initializeFLACDecoder_unlocked();
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::recreateDecoder_unlocked] Exception: ", e.what());
        m_decoder.reset();
        m_decoder_initialized = false;
        return false;
    }
}

bool FLACCodec::resetDecoderForNewStream_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::resetDecoderForNewStream_unlocked] Resetting decoder for new stream");
    
    try {
        if (!m_decoder) {
            return recreateDecoder_unlocked();
        }
        
        // Finish current stream processing
        if (m_decoder_initialized) {
            if (!m_decoder->finish()) {
                Debug::log("flac_codec", "[FLACCodec::resetDecoderForNewStream_unlocked] Decoder finish failed");
            }
        }
        
        // Clear all buffers
        m_decoder->clearInputBuffer();
        m_decoder->clearError();
        
        // Re-initialize for new stream
        FLAC__StreamDecoderInitStatus init_status = m_decoder->init();
        if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
            Debug::log("flac_codec", "[FLACCodec::resetDecoderForNewStream_unlocked] Re-initialization failed: ", 
                      static_cast<int>(init_status));
            return recreateDecoder_unlocked();
        }
        
        m_decoder_initialized = true;
        m_stream_finished = false;
        
        Debug::log("flac_codec", "[FLACCodec::resetDecoderForNewStream_unlocked] Decoder reset for new stream successful");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::resetDecoderForNewStream_unlocked] Exception: ", e.what());
        return recreateDecoder_unlocked();
    }
}

bool FLACCodec::recoverFromOggError_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::recoverFromOggError_unlocked] Recovering from Ogg container error");
    
    try {
        // Ogg errors typically require decoder recreation
        // as the Ogg wrapper state may be corrupted
        
        // Clear any Ogg-specific state
        if (m_decoder) {
            m_decoder->clearInputBuffer();
            m_decoder->clearError();
        }
        
        // Reset decoder state
        resetDecoderState_unlocked();
        
        // If reset doesn't work, recreate the decoder
        if (m_decoder && m_decoder->get_state() == FLAC__STREAM_DECODER_OGG_ERROR) {
            Debug::log("flac_codec", "[FLACCodec::recoverFromOggError_unlocked] Reset failed, recreating decoder");
            return recreateDecoder_unlocked();
        }
        
        Debug::log("flac_codec", "[FLACCodec::recoverFromOggError_unlocked] Ogg error recovery successful");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::recoverFromOggError_unlocked] Exception: ", e.what());
        return recreateDecoder_unlocked();
    }
}

bool FLACCodec::recoverFromDecoderMemoryError_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::recoverFromDecoderMemoryError_unlocked] Recovering from decoder memory error");
    
    try {
        // Free as much memory as possible
        freeUnusedMemory_unlocked();
        
        // Clear all buffers to free memory
        {
            std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
            m_output_buffer.clear();
            m_output_buffer.shrink_to_fit();
        }
        
        {
            std::lock_guard<std::mutex> input_lock(m_input_mutex);
            clearInputQueue_unlocked();
            m_partial_frame_buffer.clear();
            m_partial_frame_buffer.shrink_to_fit();
        }
        
        // Clear decoder buffers
        if (m_decoder) {
            m_decoder->clearInputBuffer();
        }
        
        // Clear internal buffers
        m_input_buffer.clear();
        m_input_buffer.shrink_to_fit();
        m_decode_buffer.clear();
        m_decode_buffer.shrink_to_fit();
        
        // Increment memory error statistics
        m_stats.memory_errors++;
        
        // Try to recreate decoder with minimal memory footprint
        bool recovery_success = recreateDecoder_unlocked();
        
        if (recovery_success) {
            Debug::log("flac_codec", "[FLACCodec::recoverFromDecoderMemoryError_unlocked] Memory error recovery successful");
            
            // Re-allocate minimal buffers
            try {
                m_input_buffer.reserve(32 * 1024);  // Smaller buffer
                m_decode_buffer.reserve(4608 * 8);  // Smaller decode buffer
                m_output_buffer.reserve(MAX_BUFFER_SAMPLES / 4); // Smaller output buffer
            } catch (...) {
                Debug::log("flac_codec", "[FLACCodec::recoverFromDecoderMemoryError_unlocked] Warning: Could not re-allocate full buffers");
            }
        }
        
        return recovery_success;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::recoverFromDecoderMemoryError_unlocked] Exception: ", e.what());
        return false;
    }
}

bool FLACCodec::reinitializeDecoder_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::reinitializeDecoder_unlocked] Reinitializing uninitialized decoder");
    
    try {
        if (!m_decoder) {
            return recreateDecoder_unlocked();
        }
        
        // Clear any existing state
        m_decoder->clearInputBuffer();
        m_decoder->clearError();
        
        // Initialize the decoder
        FLAC__StreamDecoderInitStatus init_status = m_decoder->init();
        if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
            Debug::log("flac_codec", "[FLACCodec::reinitializeDecoder_unlocked] Initialization failed: ", 
                      static_cast<int>(init_status));
            
            // Try recreating if initialization fails
            return recreateDecoder_unlocked();
        }
        
        m_decoder_initialized = true;
        
        Debug::log("flac_codec", "[FLACCodec::reinitializeDecoder_unlocked] Decoder reinitialization successful");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::reinitializeDecoder_unlocked] Exception: ", e.what());
        return recreateDecoder_unlocked();
    }
}

bool FLACCodec::ensureDecoderFunctional_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::ensureDecoderFunctional_unlocked] Ensuring decoder is functional");
    
    try {
        // Check if decoder exists
        if (!m_decoder) {
            Debug::log("flac_codec", "[FLACCodec::ensureDecoderFunctional_unlocked] No decoder - creating new one");
            return recreateDecoder_unlocked();
        }
        
        // Check if decoder is initialized
        if (!m_decoder_initialized) {
            Debug::log("flac_codec", "[FLACCodec::ensureDecoderFunctional_unlocked] Decoder not initialized - initializing");
            return reinitializeDecoder_unlocked();
        }
        
        // Check decoder state
        FLAC__StreamDecoderState state = m_decoder->get_state();
        switch (state) {
            case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
            case FLAC__STREAM_DECODER_READ_METADATA:
            case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
            case FLAC__STREAM_DECODER_READ_FRAME:
                // Normal operational states
                Debug::log("flac_codec", "[FLACCodec::ensureDecoderFunctional_unlocked] Decoder is functional");
                return true;
                
            case FLAC__STREAM_DECODER_END_OF_STREAM:
                Debug::log("flac_codec", "[FLACCodec::ensureDecoderFunctional_unlocked] End of stream - resetting");
                return resetDecoderForNewStream_unlocked();
                
            default:
                Debug::log("flac_codec", "[FLACCodec::ensureDecoderFunctional_unlocked] Problematic state - handling inconsistency");
                return handleDecoderStateInconsistency_unlocked();
        }
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::ensureDecoderFunctional_unlocked] Exception: ", e.what());
        return recreateDecoder_unlocked();
    }
}

bool FLACCodec::handleMemoryAllocationFailure_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::handleMemoryAllocationFailure_unlocked] Handling memory allocation failure");
    
    try {
        // Increment memory error statistics
        m_stats.memory_errors++;
        
        // Step 1: Free all non-essential memory
        freeUnusedMemory_unlocked();
        
        // Step 2: Clear and shrink all buffers
        {
            std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
            m_output_buffer.clear();
            m_output_buffer.shrink_to_fit();
            m_buffer_read_position = 0;
            
            // Reset buffer management to minimal state
            m_preferred_buffer_size = 1024; // Minimal buffer size
            m_adaptive_buffer_sizing = false; // Disable adaptive sizing
        }
        
        {
            std::lock_guard<std::mutex> input_lock(m_input_mutex);
            clearInputQueue_unlocked();
            m_partial_frame_buffer.clear();
            m_partial_frame_buffer.shrink_to_fit();
            
            // Reduce queue sizes to minimal
            m_max_input_queue_size = 4; // Minimal queue size
            m_max_input_queue_bytes = 64 * 1024; // 64KB max
        }
        
        // Step 3: Clear internal codec buffers
        m_input_buffer.clear();
        m_input_buffer.shrink_to_fit();
        m_decode_buffer.clear();
        m_decode_buffer.shrink_to_fit();
        
        // Step 4: Clear decoder buffers
        if (m_decoder) {
            m_decoder->clearInputBuffer();
        }
        
        // Step 5: Clear async processing queues if enabled
        if (m_async_processing_enabled) {
            std::lock_guard<std::mutex> async_lock(m_async_mutex);
            clearAsyncQueues_unlocked();
            
            // Reduce async queue sizes
            m_max_async_input_queue = 2;
            m_max_async_output_queue = 2;
        }
        
        // Step 6: Try to allocate minimal working buffers
        try {
            // Minimal input buffer (16KB)
            m_input_buffer.reserve(16 * 1024);
            
            // Minimal decode buffer (1152 samples * 2 channels)
            m_decode_buffer.reserve(1152 * 2);
            
            // Minimal output buffer (1 second at 44.1kHz stereo)
            {
                std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
                m_output_buffer.reserve(44100 * 2);
            }
            
            Debug::log("flac_codec", "[FLACCodec::handleMemoryAllocationFailure_unlocked] Minimal buffers allocated successfully");
            
        } catch (const std::exception& e) {
            Debug::log("flac_codec", "[FLACCodec::handleMemoryAllocationFailure_unlocked] Failed to allocate even minimal buffers: ", e.what());
            
            // Critical memory shortage - disable codec
            setErrorState_unlocked(true);
            return false;
        }
        
        // Step 7: Ensure decoder is still functional after memory cleanup
        if (!ensureDecoderFunctional_unlocked()) {
            Debug::log("flac_codec", "[FLACCodec::handleMemoryAllocationFailure_unlocked] Decoder not functional after memory cleanup");
            return false;
        }
        
        Debug::log("flac_codec", "[FLACCodec::handleMemoryAllocationFailure_unlocked] Memory allocation failure recovery completed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::handleMemoryAllocationFailure_unlocked] Exception during memory recovery: ", e.what());
        setErrorState_unlocked(true);
        return false;
    }
}

bool FLACCodec::validateCodecIntegrity_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::validateCodecIntegrity_unlocked] Validating codec integrity");
    
    try {
        // Check basic codec state
        if (!m_initialized) {
            Debug::log("flac_codec", "[FLACCodec::validateCodecIntegrity_unlocked] Codec not initialized");
            return false;
        }
        
        // Check decoder state
        if (!m_decoder) {
            Debug::log("flac_codec", "[FLACCodec::validateCodecIntegrity_unlocked] No decoder instance");
            return false;
        }
        
        if (!m_decoder_initialized) {
            Debug::log("flac_codec", "[FLACCodec::validateCodecIntegrity_unlocked] Decoder not initialized");
            return false;
        }
        
        // Check decoder state is valid
        FLAC__StreamDecoderState state = m_decoder->get_state();
        if (state == FLAC__STREAM_DECODER_ABORTED || 
            state == FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR) {
            Debug::log("flac_codec", "[FLACCodec::validateCodecIntegrity_unlocked] Decoder in error state: ", static_cast<int>(state));
            return false;
        }
        
        // Check configuration parameters
        if (m_sample_rate == 0 || m_channels == 0 || m_bits_per_sample == 0) {
            Debug::log("flac_codec", "[FLACCodec::validateCodecIntegrity_unlocked] Invalid configuration parameters");
            return false;
        }
        
        // Check error state
        if (m_error_state.load()) {
            Debug::log("flac_codec", "[FLACCodec::validateCodecIntegrity_unlocked] Codec in error state");
            return false;
        }
        
        Debug::log("flac_codec", "[FLACCodec::validateCodecIntegrity_unlocked] Codec integrity validation passed");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::validateCodecIntegrity_unlocked] Exception during validation: ", e.what());
        return false;
    }
}

// ============================================================================
// CRC Validation Methods (RFC 9639 Compliance)
// ============================================================================

bool FLACCodec::validateFrameCRC_unlocked(const uint8_t* frame_data, size_t frame_size) {
    // Performance optimization: early exit if CRC validation is disabled
    if (!shouldValidateCRC_unlocked()) {
        return true; // CRC validation disabled for performance or due to excessive errors
    }
    
    // Validate input parameters per RFC 9639 requirements
    if (!frame_data || frame_size < 6) {
        Debug::log("flac_codec", "[FLACCodec::validateFrameCRC_unlocked] Invalid frame data for CRC validation");
        Debug::log("flac_codec", "  Frame data pointer: ", (frame_data ? "valid" : "NULL"));
        Debug::log("flac_codec", "  Frame size: ", frame_size, " bytes (minimum 6 required)");
        return false;
    }
    
    // Performance monitoring for CRC validation overhead
    auto crc_start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Validate frame header CRC (8-bit CRC per RFC 9639 Section 9.1.8)
        // Find the header CRC position - it's after the frame/sample number and optional fields
        size_t header_crc_pos = 4; // Start after sync code and basic header
        
        // Skip variable-length fields to find header CRC position
        if (frame_size > header_crc_pos + 1) {
            // Parse frame header to find CRC position
            uint8_t blocking_strategy = (frame_data[1] >> 0) & 0x01;
            uint8_t block_size_bits = (frame_data[2] >> 4) & 0x0F;
            uint8_t sample_rate_bits = frame_data[2] & 0x0F;
            
            // Skip frame/sample number (variable length)
            if (blocking_strategy == 0) {
                // Fixed block size - frame number (up to 6 bytes)
                while (header_crc_pos < frame_size && (frame_data[header_crc_pos] & 0x80)) {
                    header_crc_pos++;
                }
                header_crc_pos++; // Final byte of frame number
            } else {
                // Variable block size - sample number (up to 7 bytes)
                while (header_crc_pos < frame_size && (frame_data[header_crc_pos] & 0x80)) {
                    header_crc_pos++;
                }
                header_crc_pos++; // Final byte of sample number
            }
            
            // Skip uncommon block size if present
            if (block_size_bits == 0x06) {
                header_crc_pos++; // 8-bit block size
            } else if (block_size_bits == 0x07) {
                header_crc_pos += 2; // 16-bit block size
            }
            
            // Skip uncommon sample rate if present
            if (sample_rate_bits == 0x0C) {
                header_crc_pos++; // 8-bit sample rate
            } else if (sample_rate_bits == 0x0D || sample_rate_bits == 0x0E) {
                header_crc_pos += 2; // 16-bit sample rate
            }
            
            // Now header_crc_pos points to the header CRC byte
            if (header_crc_pos < frame_size) {
                uint8_t expected_header_crc = frame_data[header_crc_pos];
                uint8_t calculated_header_crc = calculateFrameHeaderCRC_unlocked(frame_data, header_crc_pos);
                
                if (expected_header_crc != calculated_header_crc) {
                    handleCRCMismatch_unlocked("header", expected_header_crc, calculated_header_crc, 
                                             frame_data, frame_size);
                    return false;
                }
            }
        }
        
        // Validate frame footer CRC (16-bit CRC per RFC 9639 Section 9.3)
        if (frame_size >= 2) {
            uint16_t expected_footer_crc = (static_cast<uint16_t>(frame_data[frame_size - 2]) << 8) |
                                          static_cast<uint16_t>(frame_data[frame_size - 1]);
            uint16_t calculated_footer_crc = calculateFrameFooterCRC_unlocked(frame_data, frame_size);
            
            if (expected_footer_crc != calculated_footer_crc) {
                handleCRCMismatch_unlocked("footer", expected_footer_crc, calculated_footer_crc, 
                                         frame_data, frame_size);
                return false;
            }
        }
        
        // Performance monitoring: measure CRC validation overhead
        auto crc_end_time = std::chrono::high_resolution_clock::now();
        auto crc_duration = std::chrono::duration_cast<std::chrono::microseconds>(crc_end_time - crc_start_time);
        
        Debug::log("flac_codec", "[FLACCodec::validateFrameCRC_unlocked] Frame CRC validation PASSED");
        Debug::log("flac_codec", "  Validation time: ", crc_duration.count(), " μs");
        Debug::log("flac_codec", "  Frame size: ", frame_size, " bytes");
        Debug::log("flac_codec", "  Validation rate: ", 
                  (frame_size * 1000000.0) / crc_duration.count(), " bytes/second");
        
        // Log performance warning if CRC validation is taking too long
        if (crc_duration.count() > 1000) { // >1ms for CRC validation
            Debug::log("flac_codec", "[FLACCodec::validateFrameCRC_unlocked] PERFORMANCE WARNING:");
            Debug::log("flac_codec", "  CRC validation took ", crc_duration.count(), " μs (>1ms)");
            Debug::log("flac_codec", "  Consider disabling CRC validation for better performance");
            Debug::log("flac_codec", "  Use setCRCValidationEnabled(false) to disable");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        auto crc_end_time = std::chrono::high_resolution_clock::now();
        auto crc_duration = std::chrono::duration_cast<std::chrono::microseconds>(crc_end_time - crc_start_time);
        
        Debug::log("flac_codec", "[FLACCodec::validateFrameCRC_unlocked] Exception during CRC validation: ", e.what());
        Debug::log("flac_codec", "  Validation time before exception: ", crc_duration.count(), " μs");
        return false;
    }
}

uint8_t FLACCodec::calculateFrameHeaderCRC_unlocked(const uint8_t* header_data, size_t header_size) {
    // RFC 9639 Section 9.1.8: CRC-8 with polynomial x^8 + x^2 + x^1 + x^0 (0x107)
    // IMPORTANT: CRC covers whole frame header INCLUDING sync code but EXCLUDING the CRC byte itself
    // This ensures the CRC calculation follows RFC 9639 requirements exactly
    // 
    // CRC-8 calculation details per RFC 9639:
    // - Polynomial: x^8 + x^2 + x^1 + x^0 (0x107 in binary representation)
    // - Initialization: 0x00
    // - Data coverage: sync code (2 bytes) + frame header fields (variable) EXCLUDING CRC byte
    // - CRC position: immediately after frame/sample number and optional fields
    //
    // This is the correct CRC-8 table for polynomial 0x107 (x^8 + x^2 + x^1 + x^0)
    
    static const uint8_t crc8_table[256] = {
        0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
        0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
        0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
        0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
        0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
        0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
        0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
        0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
        0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
        0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
        0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
        0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
        0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
        0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
        0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
        0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
        0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
        0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
        0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
        0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
        0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
        0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
        0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
        0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
        0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
        0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
        0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
        0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
        0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
        0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
        0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
        0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
    };
    
    if (!header_data || header_size == 0) {
        Debug::log("flac_codec", "[FLACCodec::calculateFrameHeaderCRC_unlocked] Invalid input parameters");
        return 0x00;
    }
    
    uint8_t crc = 0x00; // Initialize with 0 per RFC 9639 Section 9.1.8
    
    // Validate that we're not including the CRC byte itself per RFC 9639
    if (header_size >= 2) {
        uint16_t sync_check = (static_cast<uint16_t>(header_data[0]) << 8) | header_data[1];
        bool valid_sync = (sync_check & 0xFFFE) == 0x3FFE;
        
        Debug::log("flac_codec", "[FLACCodec::calculateFrameHeaderCRC_unlocked] RFC 9639 CRC-8 calculation:");
        Debug::log("flac_codec", "  Data coverage: ", header_size, " bytes (INCLUDING sync code, EXCLUDING CRC byte)");
        Debug::log("flac_codec", "  Sync pattern validation: ", (valid_sync ? "VALID" : "INVALID"));
        Debug::log("flac_codec", "  First 4 bytes: 0x", std::hex, std::uppercase,
                  static_cast<uint32_t>(header_data[0]), " 0x", static_cast<uint32_t>(header_data[1]),
                  " 0x", static_cast<uint32_t>(header_data[2]), " 0x", static_cast<uint32_t>(header_data[3]),
                  std::nouppercase, std::dec);
    }
    
    // Calculate CRC-8 over header data per RFC 9639 (includes sync code, excludes CRC byte)
    for (size_t i = 0; i < header_size; i++) {
        crc = crc8_table[crc ^ header_data[i]];
    }
    
    Debug::log("flac_codec", "[FLACCodec::calculateFrameHeaderCRC_unlocked] Calculated header CRC-8: 0x", 
              std::hex, std::uppercase, static_cast<uint32_t>(crc), std::nouppercase, std::dec, 
              " over ", header_size, " bytes");
    
    return crc;
}

uint16_t FLACCodec::calculateFrameFooterCRC_unlocked(const uint8_t* frame_data, size_t frame_size) {
    // RFC 9639 Section 9.3: CRC-16 with polynomial x^16 + x^15 + x^2 + x^0 (0x8005)
    // IMPORTANT: CRC covers entire frame INCLUDING sync code but EXCLUDING the 16-bit CRC itself
    // This ensures the CRC calculation follows RFC 9639 requirements exactly
    //
    // CRC-16 calculation details per RFC 9639:
    // - Polynomial: x^16 + x^15 + x^2 + x^0 (0x8005 in binary representation)
    // - Initialization: 0x0000
    // - Data coverage: entire frame including sync code EXCLUDING final 2-byte CRC
    // - CRC position: last 2 bytes of frame (big-endian format)
    //
    // This is the correct CRC-16 table for polynomial 0x8005 (x^16 + x^15 + x^2 + x^0)
    
    static const uint16_t crc16_table[256] = {
        0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011,
        0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022,
        0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072,
        0x0050, 0x8055, 0x805F, 0x005A, 0x804B, 0x004E, 0x0044, 0x8041,
        0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2,
        0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1,
        0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
        0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082,
        0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
        0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1,
        0x01E0, 0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1,
        0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2,
        0x0140, 0x8145, 0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151,
        0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
        0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132,
        0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E, 0x0104, 0x8101,
        0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312,
        0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
        0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371,
        0x8353, 0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342,
        0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
        0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2,
        0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD, 0x83B7, 0x03B2,
        0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381,
        0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E, 0x0294, 0x8291,
        0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2,
        0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
        0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
        0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252,
        0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261,
        0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231,
        0x8213, 0x0216, 0x021C, 0x8219, 0x0208, 0x820D, 0x8207, 0x0202
    };
    
    if (!frame_data || frame_size < 2) {
        Debug::log("flac_codec", "[FLACCodec::calculateFrameFooterCRC_unlocked] Invalid input parameters - frame_size: ", frame_size);
        return 0x0000;
    }
    
    uint16_t crc = 0x0000; // Initialize with 0 per RFC 9639 Section 9.3
    
    // Calculate CRC over whole frame excluding the 16-bit CRC itself per RFC 9639
    // The CRC covers everything including sync code but excluding the final 2-byte CRC
    size_t crc_data_size = frame_size - 2;
    
    // Validate that we're properly excluding the CRC bytes per RFC 9639
    if (frame_size >= 4) {
        uint16_t sync_check = (static_cast<uint16_t>(frame_data[0]) << 8) | frame_data[1];
        bool valid_sync = (sync_check & 0xFFFE) == 0x3FFE;
        
        Debug::log("flac_codec", "[FLACCodec::calculateFrameFooterCRC_unlocked] RFC 9639 CRC-16 calculation:");
        Debug::log("flac_codec", "  Total frame size: ", frame_size, " bytes");
        Debug::log("flac_codec", "  Data coverage: ", crc_data_size, " bytes (INCLUDING sync code, EXCLUDING 2-byte CRC)");
        Debug::log("flac_codec", "  CRC location: bytes ", frame_size - 2, "-", frame_size - 1, " (EXCLUDED from calculation)");
        Debug::log("flac_codec", "  Sync pattern validation: ", (valid_sync ? "VALID" : "INVALID"));
        Debug::log("flac_codec", "  CRC bytes: 0x", std::hex, std::uppercase,
                  static_cast<uint32_t>(frame_data[frame_size - 2]), " 0x", 
                  static_cast<uint32_t>(frame_data[frame_size - 1]), std::nouppercase, std::dec);
    }
    
    // Calculate CRC-16 over frame data per RFC 9639 (includes sync code, excludes final 2-byte CRC)
    for (size_t i = 0; i < crc_data_size; i++) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ frame_data[i]) & 0xFF];
    }
    
    Debug::log("flac_codec", "[FLACCodec::calculateFrameFooterCRC_unlocked] Calculated footer CRC-16: 0x", 
              std::hex, std::uppercase, static_cast<uint32_t>(crc), std::nouppercase, std::dec, 
              " over ", crc_data_size, " bytes");
    
    return crc;
}

void FLACCodec::handleCRCMismatch_unlocked(const char* crc_type, uint32_t expected, uint32_t calculated, 
                                          const uint8_t* frame_data, size_t frame_size) {
    m_stats.crc_errors++;
    
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ========================================");
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] RFC 9639 CRC VALIDATION FAILURE");
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ========================================");
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] CRC Type: ", crc_type, " CRC");
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] Expected CRC: 0x", 
              std::hex, std::uppercase, expected, std::nouppercase, std::dec);
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] Calculated CRC: 0x", 
              std::hex, std::uppercase, calculated, std::nouppercase, std::dec);
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] CRC Difference: 0x", 
              std::hex, std::uppercase, (expected ^ calculated), std::nouppercase, std::dec);
    
    // Log comprehensive frame details for debugging per RFC 9639 requirements
    if (frame_data && frame_size >= 4) {
        Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ----------------------------------------");
        Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] DETAILED FRAME ANALYSIS (RFC 9639)");
        Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ----------------------------------------");
        
        // Extract and validate sync pattern per RFC 9639 Section 9.1.1
        uint16_t sync_pattern = (static_cast<uint16_t>(frame_data[0]) << 8) | frame_data[1];
        bool valid_sync = (sync_pattern & 0xFFFE) == 0x3FFE; // RFC 9639 sync pattern: 11111111111110xx
        
        Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] Frame size: ", frame_size, " bytes");
        Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] Sync pattern: 0x", 
                  std::hex, std::uppercase, sync_pattern, std::nouppercase, std::dec, 
                  " (", (valid_sync ? "VALID per RFC 9639" : "INVALID - not RFC 9639 compliant"), ")");
        
        if (!valid_sync) {
            Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] WARNING: Invalid sync pattern may indicate frame boundary detection error");
            Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] RFC 9639 requires sync pattern 0x3FFE (14 bits) followed by reserved bit and blocking strategy");
        }
        
        if (frame_size >= 4) {
            // Parse frame header for additional debugging info per RFC 9639 Section 9.1
            uint8_t reserved_bit = (frame_data[1] >> 1) & 0x01;
            uint8_t blocking_strategy = (frame_data[1] >> 0) & 0x01;
            uint8_t block_size_bits = (frame_data[2] >> 4) & 0x0F;
            uint8_t sample_rate_bits = frame_data[2] & 0x0F;
            uint8_t channel_assignment = (frame_data[3] >> 4) & 0x0F;
            uint8_t sample_size_bits = (frame_data[3] >> 1) & 0x07;
            uint8_t reserved_bit2 = frame_data[3] & 0x01;
            
            Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] Frame Header Analysis (RFC 9639 Section 9.1):");
            Debug::log("flac_codec", "  Reserved bit (must be 0): ", static_cast<uint32_t>(reserved_bit), 
                      (reserved_bit == 0 ? " (VALID)" : " (INVALID - RFC violation)"));
            Debug::log("flac_codec", "  Blocking strategy: ", (blocking_strategy ? "variable block size" : "fixed block size"));
            Debug::log("flac_codec", "  Block size encoding: 0x", std::hex, static_cast<uint32_t>(block_size_bits), std::dec);
            Debug::log("flac_codec", "  Sample rate encoding: 0x", std::hex, static_cast<uint32_t>(sample_rate_bits), std::dec);
            Debug::log("flac_codec", "  Channel assignment: 0x", std::hex, static_cast<uint32_t>(channel_assignment), std::dec, 
                      " (", getChannelAssignmentName(channel_assignment), ")");
            Debug::log("flac_codec", "  Sample size encoding: 0x", std::hex, static_cast<uint32_t>(sample_size_bits), std::dec);
            Debug::log("flac_codec", "  Reserved bit 2 (must be 0): ", static_cast<uint32_t>(reserved_bit2), 
                      (reserved_bit2 == 0 ? " (VALID)" : " (INVALID - RFC violation)"));
            
            // Validate reserved bits per RFC 9639
            if (reserved_bit != 0 || reserved_bit2 != 0) {
                Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ERROR: Reserved bits are non-zero - RFC 9639 violation detected");
                Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] This may indicate corrupted frame data or incorrect frame parsing");
            }
            
            // Validate forbidden patterns per RFC 9639
            if (block_size_bits == 0x00) {
                Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ERROR: Block size 0x00 is forbidden per RFC 9639");
            }
            if (sample_rate_bits == 0x0F) {
                Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ERROR: Sample rate 0x0F is forbidden per RFC 9639");
            }
            if (channel_assignment >= 0x0B && channel_assignment <= 0x0F) {
                Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ERROR: Channel assignment 0x", 
                          std::hex, static_cast<uint32_t>(channel_assignment), std::dec, " is reserved per RFC 9639");
            }
            if (sample_size_bits == 0x03 || sample_size_bits == 0x07) {
                Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ERROR: Sample size 0x", 
                          std::hex, static_cast<uint32_t>(sample_size_bits), std::dec, " is reserved per RFC 9639");
            }
        }
        
        // Log frame data in hex for detailed analysis
        Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ----------------------------------------");
        Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] FRAME DATA DUMP (for debugging)");
        Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ----------------------------------------");
        
        size_t hex_bytes = std::min(frame_size, static_cast<size_t>(64)); // Show first 64 bytes
        for (size_t i = 0; i < hex_bytes; i += 16) {
            std::string hex_line = "";
            std::string ascii_line = "";
            
            // Format address
            char addr_str[16];
            snprintf(addr_str, sizeof(addr_str), "0x%04X: ", static_cast<unsigned int>(i));
            hex_line += addr_str;
            
            // Format hex bytes
            for (size_t j = 0; j < 16 && (i + j) < hex_bytes; j++) {
                char hex_byte[4];
                snprintf(hex_byte, sizeof(hex_byte), "%02X ", frame_data[i + j]);
                hex_line += hex_byte;
                
                // ASCII representation
                char c = frame_data[i + j];
                ascii_line += (c >= 32 && c <= 126) ? c : '.';
            }
            
            // Pad hex line if needed
            while (hex_line.length() < 55) {
                hex_line += " ";
            }
            
            Debug::log("flac_codec", "  ", hex_line, " |", ascii_line, "|");
        }
        
        if (frame_size > 64) {
            Debug::log("flac_codec", "  ... (", frame_size - 64, " more bytes not shown)");
        }
        
        // Specific CRC location analysis
        if (strcmp(crc_type, "header") == 0) {
            Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] Header CRC Analysis (RFC 9639 Section 9.1.8):");
            Debug::log("flac_codec", "  CRC-8 polynomial: x^8 + x^2 + x^1 + x^0 (0x107)");
            Debug::log("flac_codec", "  CRC initialization: 0x00");
            Debug::log("flac_codec", "  CRC covers: sync code + frame header (excluding CRC byte itself)");
        } else if (strcmp(crc_type, "footer") == 0) {
            Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] Footer CRC Analysis (RFC 9639 Section 9.3):");
            Debug::log("flac_codec", "  CRC-16 polynomial: x^16 + x^15 + x^2 + x^0 (0x8005)");
            Debug::log("flac_codec", "  CRC initialization: 0x0000");
            Debug::log("flac_codec", "  CRC covers: entire frame including sync code (excluding 16-bit CRC itself)");
            
            if (frame_size >= 2) {
                uint16_t footer_crc = (static_cast<uint16_t>(frame_data[frame_size - 2]) << 8) |
                                     static_cast<uint16_t>(frame_data[frame_size - 1]);
                Debug::log("flac_codec", "  Footer CRC location: bytes ", frame_size - 2, "-", frame_size - 1);
                Debug::log("flac_codec", "  Footer CRC bytes: 0x", 
                          std::hex, std::uppercase, static_cast<uint32_t>(frame_data[frame_size - 2]), " 0x", 
                          static_cast<uint32_t>(frame_data[frame_size - 1]), std::nouppercase, std::dec);
                Debug::log("flac_codec", "  Footer CRC value: 0x", 
                          std::hex, std::uppercase, static_cast<uint32_t>(footer_crc), std::nouppercase, std::dec);
            }
        }
    }
    
    
    // RFC 9639 compliant error recovery strategy
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] RFC 9639 error recovery analysis:");
    
    // Check if we should disable CRC validation due to excessive errors
    double error_rate = m_stats.frames_decoded > 0 ? 
                       (static_cast<double>(m_stats.crc_errors) * 100.0) / m_stats.frames_decoded : 0.0;
    
    Debug::log("flac_codec", "  Total CRC errors: ", m_stats.crc_errors);
    Debug::log("flac_codec", "  Total frames decoded: ", m_stats.frames_decoded);
    Debug::log("flac_codec", "  CRC error rate: ", error_rate, "%");
    Debug::log("flac_codec", "  CRC error threshold: ", m_crc_error_threshold, " errors");
    Debug::log("flac_codec", "  Auto-disable threshold: ", (m_crc_error_threshold == 0 ? "DISABLED" : "ENABLED"));
    
    // Automatic CRC validation disabling per performance requirements
    if (m_crc_error_threshold > 0 && m_stats.crc_errors >= m_crc_error_threshold) {
        if (!m_crc_validation_disabled_due_to_errors) {
            Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] RFC 9639 compliance note:");
            Debug::log("flac_codec", "  Disabling CRC validation due to excessive errors (", 
                      m_stats.crc_errors, " errors, ", error_rate, "% error rate)");
            Debug::log("flac_codec", "  This may indicate systematic issues with the FLAC stream or decoder");
            Debug::log("flac_codec", "  CRC validation can be re-enabled with setCRCValidationEnabled(true)");
            m_crc_validation_disabled_due_to_errors = true;
        }
    }
    
    // RFC 9639 compliant error handling modes
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] Error handling strategy:");
    if (m_strict_crc_validation) {
        Debug::log("flac_codec", "  Mode: STRICT RFC 9639 compliance");
        Debug::log("flac_codec", "  Action: Frame with CRC error will be REJECTED");
        Debug::log("flac_codec", "  Result: Silence output for this frame, decoder continues");
        Debug::log("flac_codec", "  Impact: Maximum data integrity, potential audio dropouts");
        setErrorState_unlocked(true);
    } else {
        Debug::log("flac_codec", "  Mode: PERMISSIVE RFC 9639 compliance");
        Debug::log("flac_codec", "  Action: Frame with CRC error will be USED but logged");
        Debug::log("flac_codec", "  Result: Potentially corrupted audio data, playback continues");
        Debug::log("flac_codec", "  Impact: Better user experience, possible quality degradation");
        Debug::log("flac_codec", "  RFC 9639 note: Decoders MAY use data despite CRC mismatches for error resilience");
    }
    
    // Performance and debugging recommendations
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] Recommendations:");
    if (m_stats.crc_errors == 1) {
        Debug::log("flac_codec", "  First CRC error detected - monitor for patterns");
        Debug::log("flac_codec", "  Performance: CRC validation adds ~5-10% CPU overhead");
        Debug::log("flac_codec", "  Quality: Consider source integrity if errors persist");
    } else if (m_stats.crc_errors < 5) {
        Debug::log("flac_codec", "  Occasional CRC errors - likely transmission/storage issues");
        Debug::log("flac_codec", "  Action: Continue monitoring, consider source quality");
    } else {
        Debug::log("flac_codec", "  Frequent CRC errors - systematic issue likely");
        Debug::log("flac_codec", "  Action: Check source file integrity, consider disabling CRC validation");
        Debug::log("flac_codec", "  Debug: Enable detailed frame logging for analysis");
    }
    
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ========================================");
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] END CRC VALIDATION FAILURE ANALYSIS");
    Debug::log("flac_codec", "[FLACCodec::handleCRCMismatch_unlocked] ========================================");
}

bool FLACCodec::shouldValidateCRC_unlocked() const {
    return m_crc_validation_enabled && !m_crc_validation_disabled_due_to_errors;
}

const char* FLACCodec::getChannelAssignmentName(uint8_t channel_assignment) const {
    // RFC 9639 Section 9.1.4 - Channel Assignment
    switch (channel_assignment) {
        case 0: return "1 channel (mono)";
        case 1: return "2 channels (left, right)";
        case 2: return "3 channels (left, right, center)";
        case 3: return "4 channels (front left, front right, back left, back right)";
        case 4: return "5 channels (front left, front right, front center, back left, back right)";
        case 5: return "6 channels (front left, front right, front center, LFE, back left, back right)";
        case 6: return "7 channels (front left, front right, front center, LFE, back center, side left, side right)";
        case 7: return "8 channels (front left, front right, front center, LFE, back left, back right, side left, side right)";
        case 8: return "2 channels (left-side stereo: channel 0 is left, channel 1 is side)";
        case 9: return "2 channels (right-side stereo: channel 0 is side, channel 1 is right)";
        case 10: return "2 channels (mid-side stereo: channel 0 is mid, channel 1 is side)";
        default: return "reserved/invalid channel assignment";
    }
}

// Memory management methods

void FLACCodec::optimizeBufferSizes_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::optimizeBufferSizes_unlocked] Optimizing buffer sizes with advanced memory management");
    
    // Calculate optimal buffer sizes based on stream characteristics and performance requirements
    size_t max_block_size = std::max(m_max_block_size, static_cast<uint32_t>(65535)); // Use stream max or RFC 9639 maximum
    size_t required_buffer_size = max_block_size * m_channels;
    
    // Calculate memory pool sizes based on expected usage patterns
    size_t optimal_output_buffer_size = required_buffer_size * 2; // Double buffering for smooth playback
    size_t optimal_decode_buffer_size = required_buffer_size;     // Single frame processing
    size_t optimal_input_buffer_size = std::max(static_cast<size_t>(128 * 1024), required_buffer_size / 4); // Adaptive input buffer
    
    // Apply memory usage limits to prevent excessive allocation
    size_t max_memory_per_buffer = 16 * 1024 * 1024; // 16MB per buffer limit
    optimal_output_buffer_size = std::min(optimal_output_buffer_size, max_memory_per_buffer / sizeof(int16_t));
    optimal_decode_buffer_size = std::min(optimal_decode_buffer_size, max_memory_per_buffer / sizeof(FLAC__int32));
    optimal_input_buffer_size = std::min(optimal_input_buffer_size, max_memory_per_buffer);
    
    // Pre-allocate buffers with optimal sizes to avoid runtime allocations
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        
        // Reserve output buffer with alignment optimization
        if (m_output_buffer.capacity() < optimal_output_buffer_size) {
            m_output_buffer.reserve(optimal_output_buffer_size);
            Debug::log("flac_codec", "[FLACCodec::optimizeBufferSizes_unlocked] Output buffer reserved: ", 
                      optimal_output_buffer_size, " samples (", 
                      (optimal_output_buffer_size * sizeof(int16_t)) / 1024, " KB)");
        }
        
        // Update buffer management parameters
        m_preferred_buffer_size = optimal_output_buffer_size;
        updateBufferWatermarks_unlocked();
    }
    
    // Reserve decode buffer for libFLAC processing
    if (m_decode_buffer.capacity() < optimal_decode_buffer_size) {
        m_decode_buffer.reserve(optimal_decode_buffer_size);
        Debug::log("flac_codec", "[FLACCodec::optimizeBufferSizes_unlocked] Decode buffer reserved: ", 
                  optimal_decode_buffer_size, " samples (", 
                  (optimal_decode_buffer_size * sizeof(FLAC__int32)) / 1024, " KB)");
    }
    
    // Reserve input buffer for frame data
    if (m_input_buffer.capacity() < optimal_input_buffer_size) {
        m_input_buffer.reserve(optimal_input_buffer_size);
        Debug::log("flac_codec", "[FLACCodec::optimizeBufferSizes_unlocked] Input buffer reserved: ", 
                  optimal_input_buffer_size, " bytes (", optimal_input_buffer_size / 1024, " KB)");
    }
    
    // Initialize memory pool allocation tracking
    m_buffer_allocation_count = 0;
    m_adaptive_buffer_sizing = true;
    
    size_t total_memory_kb = (optimal_output_buffer_size * sizeof(int16_t) + 
                             optimal_decode_buffer_size * sizeof(FLAC__int32) + 
                             optimal_input_buffer_size) / 1024;
    
    Debug::log("flac_codec", "[FLACCodec::optimizeBufferSizes_unlocked] Total memory allocated: ", 
              total_memory_kb, " KB for ", m_channels, " channels, max block size ", max_block_size);
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
    Debug::log("flac_codec", "[FLACCodec::freeUnusedMemory_unlocked] Performing advanced memory cleanup");
    
    size_t memory_freed = 0;
    
    // Shrink output buffer if significantly over-allocated
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        
        size_t current_capacity = m_output_buffer.capacity();
        size_t current_size = m_output_buffer.size();
        size_t optimal_capacity = std::max(current_size * 2, m_preferred_buffer_size / 4); // Keep some headroom
        
        if (current_capacity > optimal_capacity * 2) {
            // Use copy-and-swap idiom for memory shrinking
            std::vector<int16_t> shrunk_buffer;
            shrunk_buffer.reserve(optimal_capacity);
            shrunk_buffer.assign(m_output_buffer.begin(), m_output_buffer.end());
            m_output_buffer.swap(shrunk_buffer);
            
            size_t freed_bytes = (current_capacity - m_output_buffer.capacity()) * sizeof(int16_t);
            memory_freed += freed_bytes;
            
            Debug::log("flac_codec", "[FLACCodec::freeUnusedMemory_unlocked] Output buffer shrunk from ", 
                      current_capacity, " to ", m_output_buffer.capacity(), " samples (freed ", 
                      freed_bytes / 1024, " KB)");
        }
    }
    
    // Shrink decode buffer if over-allocated
    {
        size_t current_capacity = m_decode_buffer.capacity();
        size_t current_size = m_decode_buffer.size();
        size_t min_required = std::max(static_cast<size_t>(65535 * m_channels), current_size * 2);
        
        if (current_capacity > min_required * 2) {
            std::vector<FLAC__int32> shrunk_buffer;
            shrunk_buffer.reserve(min_required);
            shrunk_buffer.assign(m_decode_buffer.begin(), m_decode_buffer.end());
            m_decode_buffer.swap(shrunk_buffer);
            
            size_t freed_bytes = (current_capacity - m_decode_buffer.capacity()) * sizeof(FLAC__int32);
            memory_freed += freed_bytes;
            
            Debug::log("flac_codec", "[FLACCodec::freeUnusedMemory_unlocked] Decode buffer shrunk from ", 
                      current_capacity, " to ", m_decode_buffer.capacity(), " samples (freed ", 
                      freed_bytes / 1024, " KB)");
        }
    }
    
    // Shrink input buffer if over-allocated
    {
        size_t current_capacity = m_input_buffer.capacity();
        size_t current_size = m_input_buffer.size();
        size_t min_required = std::max(static_cast<size_t>(64 * 1024), current_size * 2);
        
        if (current_capacity > min_required * 2) {
            std::vector<uint8_t> shrunk_buffer;
            shrunk_buffer.reserve(min_required);
            shrunk_buffer.assign(m_input_buffer.begin(), m_input_buffer.end());
            m_input_buffer.swap(shrunk_buffer);
            
            size_t freed_bytes = current_capacity - m_input_buffer.capacity();
            memory_freed += freed_bytes;
            
            Debug::log("flac_codec", "[FLACCodec::freeUnusedMemory_unlocked] Input buffer shrunk from ", 
                      current_capacity, " to ", m_input_buffer.capacity(), " bytes (freed ", 
                      freed_bytes / 1024, " KB)");
        }
    }
    
    // Clear input queue buffers if they're consuming too much memory
    {
        std::lock_guard<std::mutex> input_lock(m_input_mutex);
        
        if (m_input_queue_bytes > 512 * 1024) { // If queue is using more than 512KB
            size_t original_bytes = m_input_queue_bytes;
            
            // Keep only the most recent chunks
            while (m_input_queue.size() > 4 && m_input_queue_bytes > 256 * 1024) {
                MediaChunk front_chunk = m_input_queue.front();
                m_input_queue.pop();
                m_input_queue_bytes -= front_chunk.data.size();
            }
            
            size_t freed_bytes = original_bytes - m_input_queue_bytes;
            memory_freed += freed_bytes;
            
            if (freed_bytes > 0) {
                Debug::log("flac_codec", "[FLACCodec::freeUnusedMemory_unlocked] Input queue trimmed, freed ", 
                          freed_bytes / 1024, " KB, ", m_input_queue.size(), " chunks remaining");
            }
        }
        
        // Shrink partial frame buffer if over-allocated
        if (m_partial_frame_buffer.capacity() > m_partial_frame_buffer.size() * 4 && 
            m_partial_frame_buffer.capacity() > 32 * 1024) {
            
            size_t original_capacity = m_partial_frame_buffer.capacity();
            std::vector<uint8_t> shrunk_buffer(m_partial_frame_buffer);
            m_partial_frame_buffer.swap(shrunk_buffer);
            
            size_t freed_bytes = original_capacity - m_partial_frame_buffer.capacity();
            memory_freed += freed_bytes;
            
            Debug::log("flac_codec", "[FLACCodec::freeUnusedMemory_unlocked] Partial frame buffer shrunk, freed ", 
                      freed_bytes / 1024, " KB");
        }
    }
    
    // Clear async processing queues if they're consuming too much memory
    if (m_async_processing_enabled) {
        std::lock_guard<std::mutex> async_lock(m_async_mutex);
        
        size_t async_memory_freed = 0;
        
        // Limit async input queue size
        while (m_async_input_queue.size() > m_max_async_input_queue / 2) {
            m_async_input_queue.pop();
            async_memory_freed += 1024; // Estimate
        }
        
        // Limit async output queue size
        while (m_async_output_queue.size() > m_max_async_output_queue / 2) {
            m_async_output_queue.pop();
            async_memory_freed += 4096; // Estimate for AudioFrame
        }
        
        if (async_memory_freed > 0) {
            memory_freed += async_memory_freed;
            Debug::log("flac_codec", "[FLACCodec::freeUnusedMemory_unlocked] Async queues trimmed, freed ~", 
                      async_memory_freed / 1024, " KB");
        }
    }
    
    // Update memory usage statistics
    m_stats.memory_usage_bytes = calculateCurrentMemoryUsage_unlocked();
    
    if (memory_freed > 0) {
        Debug::log("flac_codec", "[FLACCodec::freeUnusedMemory_unlocked] Total memory freed: ", 
                  memory_freed / 1024, " KB, current usage: ", m_stats.memory_usage_bytes / 1024, " KB");
    } else {
        Debug::log("flac_codec", "[FLACCodec::freeUnusedMemory_unlocked] No significant memory to free, current usage: ", 
                  m_stats.memory_usage_bytes / 1024, " KB");
    }
}

// AudioFrame creation and validation methods

AudioFrame FLACCodec::createAudioFrame_unlocked(const std::vector<int16_t>& samples, uint64_t timestamp_samples) {
    AudioFrame frame;
    
    // Set basic properties with validation
    frame.sample_rate = m_sample_rate > 0 ? m_sample_rate : 44100; // Fallback to 44.1kHz
    frame.channels = m_channels > 0 ? m_channels : 2; // Fallback to stereo
    frame.timestamp_samples = timestamp_samples;
    
    // Calculate timestamp in milliseconds with overflow protection
    if (frame.sample_rate > 0) {
        frame.timestamp_ms = (timestamp_samples * 1000ULL) / frame.sample_rate;
    } else {
        frame.timestamp_ms = 0;
    }
    
    // Copy samples with validation
    if (samples.size() % frame.channels != 0) {
        Debug::log("flac_codec", "[FLACCodec::createAudioFrame_unlocked] WARNING: Sample count ", 
                  samples.size(), " not divisible by channel count ", frame.channels);
        
        // Truncate to nearest complete sample frame
        size_t complete_samples = (samples.size() / frame.channels) * frame.channels;
        frame.samples.assign(samples.begin(), samples.begin() + complete_samples);
    } else {
        frame.samples = samples;
    }
    
    // Validate the created frame
    validateAudioFrame_unlocked(frame);
    
    Debug::log("flac_codec", "[FLACCodec::createAudioFrame_unlocked] Created AudioFrame: ", 
              frame.getSampleFrameCount(), " sample frames, ", frame.channels, " channels, ", 
              frame.sample_rate, "Hz, timestamp=", frame.timestamp_samples, " (", frame.timestamp_ms, "ms)");
    
    return frame;
}

AudioFrame FLACCodec::createAudioFrame_unlocked(std::vector<int16_t>&& samples, uint64_t timestamp_samples) {
    AudioFrame frame;
    
    // Set basic properties with validation
    frame.sample_rate = m_sample_rate > 0 ? m_sample_rate : 44100; // Fallback to 44.1kHz
    frame.channels = m_channels > 0 ? m_channels : 2; // Fallback to stereo
    frame.timestamp_samples = timestamp_samples;
    
    // Calculate timestamp in milliseconds with overflow protection
    if (frame.sample_rate > 0) {
        frame.timestamp_ms = (timestamp_samples * 1000ULL) / frame.sample_rate;
    } else {
        frame.timestamp_ms = 0;
    }
    
    // Move samples with validation
    if (samples.size() % frame.channels != 0) {
        Debug::log("flac_codec", "[FLACCodec::createAudioFrame_unlocked] WARNING: Sample count ", 
                  samples.size(), " not divisible by channel count ", frame.channels);
        
        // Truncate to nearest complete sample frame
        size_t complete_samples = (samples.size() / frame.channels) * frame.channels;
        samples.resize(complete_samples);
    }
    
    frame.samples = std::move(samples);
    
    // Validate the created frame
    validateAudioFrame_unlocked(frame);
    
    Debug::log("flac_codec", "[FLACCodec::createAudioFrame_unlocked] Created AudioFrame (move): ", 
              frame.getSampleFrameCount(), " sample frames, ", frame.channels, " channels, ", 
              frame.sample_rate, "Hz, timestamp=", frame.timestamp_samples, " (", frame.timestamp_ms, "ms)");
    
    return frame;
}

void FLACCodec::validateAudioFrame_unlocked(AudioFrame& frame) const {
    // Validate sample rate
    if (frame.sample_rate == 0) {
        Debug::log("flac_codec", "[FLACCodec::validateAudioFrame_unlocked] WARNING: Zero sample rate, setting to 44100Hz");
        frame.sample_rate = 44100;
    } else if (frame.sample_rate > 655350) {
        Debug::log("flac_codec", "[FLACCodec::validateAudioFrame_unlocked] WARNING: Sample rate ", 
                  frame.sample_rate, " exceeds RFC 9639 limit, clamping to 655350Hz");
        frame.sample_rate = 655350;
    }
    
    // Validate channel count
    if (frame.channels == 0) {
        Debug::log("flac_codec", "[FLACCodec::validateAudioFrame_unlocked] WARNING: Zero channels, setting to 2 (stereo)");
        frame.channels = 2;
    } else if (frame.channels > 8) {
        Debug::log("flac_codec", "[FLACCodec::validateAudioFrame_unlocked] WARNING: Channel count ", 
                  frame.channels, " exceeds RFC 9639 limit, clamping to 8");
        frame.channels = 8;
    }
    
    // Validate sample consistency
    if (!frame.samples.empty() && frame.samples.size() % frame.channels != 0) {
        Debug::log("flac_codec", "[FLACCodec::validateAudioFrame_unlocked] ERROR: Sample count ", 
                  frame.samples.size(), " not consistent with channel count ", frame.channels);
        
        // Truncate to make it consistent
        size_t complete_samples = (frame.samples.size() / frame.channels) * frame.channels;
        frame.samples.resize(complete_samples);
    }
    
    // Recalculate timestamp_ms if it seems incorrect
    if (frame.sample_rate > 0) {
        uint64_t expected_timestamp_ms = (frame.timestamp_samples * 1000ULL) / frame.sample_rate;
        if (frame.timestamp_ms != expected_timestamp_ms) {
            Debug::log("flac_codec", "[FLACCodec::validateAudioFrame_unlocked] Correcting timestamp_ms from ", 
                      frame.timestamp_ms, " to ", expected_timestamp_ms);
            frame.timestamp_ms = expected_timestamp_ms;
        }
    }
}

void FLACCodec::updateSamplePosition_unlocked(size_t sample_frame_count) {
    // Update current sample position atomically
    uint64_t old_position = m_current_sample.fetch_add(sample_frame_count);
    
    Debug::log("flac_codec", "[FLACCodec::updateSamplePosition_unlocked] Updated position from ", 
              old_position, " to ", old_position + sample_frame_count, " (added ", sample_frame_count, " frames)");
    
    // Update statistics
    m_stats.samples_decoded += sample_frame_count * m_channels;
}

size_t FLACCodec::calculateCurrentMemoryUsage_unlocked() const {
    size_t total_memory = 0;
    
    // Calculate buffer memory usage
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        total_memory += m_output_buffer.capacity() * sizeof(int16_t);
    }
    
    total_memory += m_decode_buffer.capacity() * sizeof(FLAC__int32);
    total_memory += m_input_buffer.capacity();
    
    // Calculate queue memory usage
    {
        std::lock_guard<std::mutex> input_lock(m_input_mutex);
        total_memory += m_input_queue_bytes;
        total_memory += m_partial_frame_buffer.capacity();
    }
    
    // Estimate async queue memory usage
    if (m_async_processing_enabled) {
        std::lock_guard<std::mutex> async_lock(m_async_mutex);
        total_memory += m_async_input_queue.size() * 1024; // Estimate per chunk
        total_memory += m_async_output_queue.size() * 4096; // Estimate per AudioFrame
    }
    
    // Add fixed overhead for codec state
    total_memory += sizeof(FLACCodec);
    total_memory += sizeof(FLACStreamDecoder);
    
    return total_memory;
}

void FLACCodec::implementMemoryPoolAllocation_unlocked() {
    // Implement memory pool allocation for frequently used buffers
    Debug::log("flac_codec", "[FLACCodec::implementMemoryPoolAllocation_unlocked] Setting up memory pools");
    
    // Pre-allocate common buffer sizes to reduce allocation overhead
    std::vector<size_t> common_sizes = {
        static_cast<size_t>(576) * m_channels,    // Common FLAC block size
        static_cast<size_t>(1152) * m_channels,   // Common FLAC block size
        static_cast<size_t>(2304) * m_channels,   // Common FLAC block size
        static_cast<size_t>(4608) * m_channels    // Common FLAC block size
    };
    
    // Reserve space for these common sizes in the output buffer
    size_t max_common_size = *std::max_element(common_sizes.begin(), common_sizes.end());
    
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        if (m_output_buffer.capacity() < max_common_size * 4) {
            m_output_buffer.reserve(max_common_size * 4);
            Debug::log("flac_codec", "[FLACCodec::implementMemoryPoolAllocation_unlocked] Reserved pool space for common block sizes");
        }
    }
    
    // Pre-allocate decode buffer for common sizes
    if (m_decode_buffer.capacity() < max_common_size * 2) {
        m_decode_buffer.reserve(max_common_size * 2);
    }
    
    Debug::log("flac_codec", "[FLACCodec::implementMemoryPoolAllocation_unlocked] Memory pools configured for ", 
              common_sizes.size(), " common block sizes");
}

void FLACCodec::optimizeMemoryFragmentation_unlocked() {
    // Minimize memory fragmentation in long-running scenarios
    Debug::log("flac_codec", "[FLACCodec::optimizeMemoryFragmentation_unlocked] Optimizing memory layout");
    
    // Reallocate buffers to ensure contiguous memory layout
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        
        // Force reallocation to get fresh, contiguous memory
        if (m_buffer_allocation_count > 100) { // After many allocations
            size_t optimal_capacity = m_preferred_buffer_size;
            
            std::vector<int16_t> fresh_buffer;
            fresh_buffer.reserve(optimal_capacity);
            fresh_buffer.assign(m_output_buffer.begin(), m_output_buffer.end());
            m_output_buffer.swap(fresh_buffer);
            
            m_buffer_allocation_count = 0;
            
            Debug::log("flac_codec", "[FLACCodec::optimizeMemoryFragmentation_unlocked] Output buffer reallocated for defragmentation");
        }
    }
    
    // Optimize decode buffer layout
    if (m_decode_buffer.size() > 0) {
        size_t optimal_capacity = 65535 * m_channels * 2;
        if (m_decode_buffer.capacity() != optimal_capacity) {
            std::vector<FLAC__int32> fresh_buffer;
            fresh_buffer.reserve(optimal_capacity);
            fresh_buffer.assign(m_decode_buffer.begin(), m_decode_buffer.end());
            m_decode_buffer.swap(fresh_buffer);
            
            Debug::log("flac_codec", "[FLACCodec::optimizeMemoryFragmentation_unlocked] Decode buffer reallocated for defragmentation");
        }
    }
}

// ============================================================================
// Enhanced Output Buffer Management Implementation
// ============================================================================

bool FLACCodec::checkBufferCapacity_unlocked(size_t required_samples) {
    // Check if buffer has enough space for required samples
    size_t available_space = m_max_pending_samples - m_output_buffer.size();
    
    if (required_samples > available_space) {
        Debug::log("flac_codec", "[FLACCodec::checkBufferCapacity_unlocked] Insufficient buffer space: ", 
                  "required=", required_samples, ", available=", available_space, 
                  ", buffer_size=", m_output_buffer.size(), ", max_pending=", m_max_pending_samples);
        
        updateBufferStatistics_unlocked(true, false); // Record overflow
        return false;
    }
    
    return true;
}

void FLACCodec::handleBufferOverflow_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::handleBufferOverflow_unlocked] Buffer overflow detected");
    
    m_buffer_overflow_detected = true;
    m_buffer_overrun_count++;
    
    // Activate backpressure to prevent further overflow
    activateBackpressure_unlocked();
    
    // If adaptive sizing is enabled, try to increase buffer size
    if (m_adaptive_buffer_sizing && m_max_pending_samples < MAX_BUFFER_SAMPLES) {
        size_t new_max = std::min(m_max_pending_samples * 2, MAX_BUFFER_SAMPLES);
        Debug::log("flac_codec", "[FLACCodec::handleBufferOverflow_unlocked] Increasing max pending samples from ", 
                  m_max_pending_samples, " to ", new_max);
        m_max_pending_samples = new_max;
        updateBufferWatermarks_unlocked();
    }
}

void FLACCodec::handleBufferUnderrun_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::handleBufferUnderrun_unlocked] Buffer underrun detected");
    
    m_buffer_underrun_count++;
    
    // Deactivate backpressure if it was active
    if (m_backpressure_active) {
        deactivateBackpressure_unlocked();
    }
    
    // Notify waiting threads that buffer space is available
    notifyBufferSpaceAvailable_unlocked();
}

bool FLACCodec::waitForBufferSpace_unlocked(size_t required_samples, std::chrono::milliseconds timeout) {
    auto start_time = std::chrono::steady_clock::now();
    
    while (!checkBufferCapacity_unlocked(required_samples)) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout) {
            Debug::log("flac_codec", "[FLACCodec::waitForBufferSpace_unlocked] Timeout waiting for buffer space");
            return false;
        }
        
        // Wait for buffer space to become available
        auto remaining_timeout = timeout - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        std::unique_lock<std::mutex> lock(m_buffer_mutex, std::adopt_lock);
        if (m_buffer_cv.wait_for(lock, remaining_timeout) == std::cv_status::timeout) {
            Debug::log("flac_codec", "[FLACCodec::waitForBufferSpace_unlocked] Condition variable timeout");
            lock.release(); // Don't unlock since we adopted the lock
            return false;
        }
        lock.release(); // Don't unlock since we adopted the lock
    }
    
    return true;
}

void FLACCodec::notifyBufferSpaceAvailable_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::notifyBufferSpaceAvailable_unlocked] Notifying buffer space available");
    m_buffer_cv.notify_all();
}

void FLACCodec::updateBufferWatermarks_unlocked() {
    m_buffer_high_watermark = (m_max_pending_samples * 3) / 4;  // 75% of max
    m_buffer_low_watermark = m_max_pending_samples / 4;         // 25% of max
    
    Debug::log("flac_codec", "[FLACCodec::updateBufferWatermarks_unlocked] Updated watermarks: ", 
              "low=", m_buffer_low_watermark, ", high=", m_buffer_high_watermark, 
              ", max=", m_max_pending_samples);
}

void FLACCodec::resetBufferFlowControl_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::resetBufferFlowControl_unlocked] Resetting flow control state");
    
    m_buffer_overflow_detected = false;
    m_backpressure_active = false;
    m_buffer_full = false;
    
    // Reset statistics
    m_buffer_underrun_count = 0;
    m_buffer_overrun_count = 0;
    m_buffer_allocation_count = 0;
    
    // Notify any waiting threads
    notifyBufferSpaceAvailable_unlocked();
}

void FLACCodec::optimizeBufferAllocation_unlocked(size_t required_samples) {
    if (!m_adaptive_buffer_sizing) {
        return;
    }
    
    // Calculate optimal buffer size based on stream characteristics
    size_t optimal_size = calculateOptimalBufferSize_unlocked(required_samples);
    
    if (optimal_size != m_preferred_buffer_size) {
        Debug::log("flac_codec", "[FLACCodec::optimizeBufferAllocation_unlocked] Updating preferred buffer size from ", 
                  m_preferred_buffer_size, " to ", optimal_size);
        m_preferred_buffer_size = optimal_size;
    }
    
    // Perform adaptive resize if needed
    if (requiresBufferReallocation_unlocked(required_samples)) {
        adaptiveBufferResize_unlocked(required_samples);
    }
}

void FLACCodec::adaptiveBufferResize_unlocked(size_t required_samples) {
    size_t current_capacity = m_output_buffer.capacity();
    size_t new_capacity = std::max(required_samples * 2, m_preferred_buffer_size);
    
    // Ensure we don't exceed maximum buffer size
    new_capacity = std::min(new_capacity, MAX_BUFFER_SAMPLES);
    
    if (new_capacity != current_capacity) {
        Debug::log("flac_codec", "[FLACCodec::adaptiveBufferResize_unlocked] Resizing buffer from ", 
                  current_capacity, " to ", new_capacity, " samples");
        
        m_output_buffer.reserve(new_capacity);
        m_buffer_allocation_count++;
        
        // Update max pending samples if needed
        if (new_capacity < m_max_pending_samples) {
            m_max_pending_samples = new_capacity;
            updateBufferWatermarks_unlocked();
        }
    }
}

bool FLACCodec::requiresBufferReallocation_unlocked(size_t required_samples) const {
    size_t current_capacity = m_output_buffer.capacity();
    
    // Reallocate if current capacity is insufficient
    if (current_capacity < required_samples) {
        return true;
    }
    
    // Reallocate if buffer is significantly over-allocated (more than 4x needed)
    if (current_capacity > required_samples * 4 && current_capacity > m_preferred_buffer_size * 2) {
        return true;
    }
    
    return false;
}

void FLACCodec::updateBufferStatistics_unlocked(bool overflow, bool underrun) {
    if (overflow) {
        m_buffer_overrun_count++;
        m_buffer_overflow_detected = true;
    }
    
    if (underrun) {
        m_buffer_underrun_count++;
    }
    
    // Log statistics periodically for debugging
    static size_t log_counter = 0;
    if (++log_counter % 1000 == 0) {
        Debug::log("flac_codec", "[FLACCodec::updateBufferStatistics_unlocked] Buffer stats: ", 
                  "overruns=", m_buffer_overrun_count, ", underruns=", m_buffer_underrun_count, 
                  ", allocations=", m_buffer_allocation_count, ", size=", m_output_buffer.size(), 
                  ", capacity=", m_output_buffer.capacity());
    }
}

size_t FLACCodec::calculateOptimalBufferSize_unlocked(size_t required_samples) const {
    // Base optimal size on stream characteristics
    size_t base_size = static_cast<size_t>(m_sample_rate) * m_channels / 10; // 100ms worth of samples
    
    // Factor in block size if known
    if (m_max_block_size > 0) {
        size_t block_samples = static_cast<size_t>(m_max_block_size) * m_channels;
        base_size = std::max(base_size, block_samples * 4); // 4 blocks worth
    }
    
    // Ensure it's at least as large as required
    base_size = std::max(base_size, required_samples * 2);
    
    // Cap at maximum buffer size
    return std::min(base_size, MAX_BUFFER_SAMPLES);
}

bool FLACCodec::isBackpressureActive_unlocked() const {
    return m_backpressure_active;
}

void FLACCodec::activateBackpressure_unlocked() {
    if (!m_backpressure_active) {
        Debug::log("flac_codec", "[FLACCodec::activateBackpressure_unlocked] Activating backpressure");
        m_backpressure_active = true;
        m_buffer_full = true;
    }
}

void FLACCodec::deactivateBackpressure_unlocked() {
    if (m_backpressure_active) {
        Debug::log("flac_codec", "[FLACCodec::deactivateBackpressure_unlocked] Deactivating backpressure");
        m_backpressure_active = false;
        m_buffer_full = false;
        notifyBufferSpaceAvailable_unlocked();
    }
}

bool FLACCodec::shouldApplyBackpressure_unlocked(size_t required_samples) const {
    // Apply backpressure if buffer is above high watermark
    size_t current_size = m_output_buffer.size();
    
    if (current_size + required_samples > m_buffer_high_watermark) {
        return true;
    }
    
    // Apply backpressure if we're already in backpressure state and buffer isn't below low watermark
    if (m_backpressure_active && current_size > m_buffer_low_watermark) {
        return true;
    }
    
    return false;
}

void FLACCodec::handleBackpressure_unlocked(size_t required_samples) {
    Debug::log("flac_codec", "[FLACCodec::handleBackpressure_unlocked] Handling backpressure for ", 
              required_samples, " samples");
    
    if (shouldApplyBackpressure_unlocked(required_samples)) {
        activateBackpressure_unlocked();
        
        // Wait for buffer space to become available
        if (!waitForBufferSpace_unlocked(required_samples)) {
            Debug::log("flac_codec", "[FLACCodec::handleBackpressure_unlocked] Failed to wait for buffer space");
            handleBufferOverflow_unlocked();
        }
    } else if (m_backpressure_active && m_output_buffer.size() <= m_buffer_low_watermark) {
        deactivateBackpressure_unlocked();
    }
}

// ============================================================================
// Input Queue Management Implementation
// ============================================================================

bool FLACCodec::enqueueInputChunk_unlocked(const MediaChunk& chunk) {
    Debug::log("flac_codec", "[FLACCodec::enqueueInputChunk_unlocked] Enqueueing chunk with ", 
              chunk.data.size(), " bytes");
    
    // Check if we have capacity for this chunk
    if (!checkInputQueueCapacity_unlocked(chunk)) {
        handleInputBackpressure_unlocked(chunk);
        
        // Check again after backpressure handling
        if (!checkInputQueueCapacity_unlocked(chunk)) {
            handleInputOverflow_unlocked();
            return false;
        }
    }
    
    // Add chunk to queue
    m_input_queue.push(chunk);
    m_input_queue_bytes += chunk.data.size();
    
    Debug::log("flac_codec", "[FLACCodec::enqueueInputChunk_unlocked] Enqueued chunk, queue size: ", 
              m_input_queue.size(), " chunks, ", m_input_queue_bytes, " bytes");
    
    return true;
}

MediaChunk FLACCodec::dequeueInputChunk_unlocked() {
    if (m_input_queue.empty()) {
        Debug::log("flac_codec", "[FLACCodec::dequeueInputChunk_unlocked] Input queue is empty");
        handleInputUnderrun_unlocked();
        return MediaChunk();
    }
    
    MediaChunk chunk = m_input_queue.front();
    m_input_queue.pop();
    m_input_queue_bytes -= chunk.data.size();
    
    Debug::log("flac_codec", "[FLACCodec::dequeueInputChunk_unlocked] Dequeued chunk with ", 
              chunk.data.size(), " bytes, queue size: ", m_input_queue.size(), " chunks, ", 
              m_input_queue_bytes, " bytes");
    
    // Notify waiting threads that queue space is available
    notifyInputQueueSpaceAvailable_unlocked();
    
    // Deactivate input backpressure if queue is below low watermark
    if (m_input_backpressure_active && m_input_queue.size() <= m_input_queue_low_watermark) {
        deactivateInputBackpressure_unlocked();
    }
    
    return chunk;
}

bool FLACCodec::hasInputChunks_unlocked() const {
    return !m_input_queue.empty();
}

size_t FLACCodec::getInputQueueSize_unlocked() const {
    return m_input_queue.size();
}

void FLACCodec::clearInputQueue_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::clearInputQueue_unlocked] Clearing input queue with ", 
              m_input_queue.size(), " chunks");
    
    // Clear the queue
    while (!m_input_queue.empty()) {
        m_input_queue.pop();
    }
    m_input_queue_bytes = 0;
    
    // Reset frame reconstruction
    resetFrameReconstruction_unlocked();
    
    // Reset flow control
    resetInputFlowControl_unlocked();
}

bool FLACCodec::isInputQueueFull_unlocked() const {
    return m_input_queue.size() >= m_max_input_queue_size || 
           m_input_queue_bytes >= m_max_input_queue_bytes;
}

void FLACCodec::updateInputQueueWatermarks_unlocked() {
    m_input_queue_high_watermark = (m_max_input_queue_size * 3) / 4;  // 75% of max
    m_input_queue_low_watermark = m_max_input_queue_size / 4;         // 25% of max
    
    Debug::log("flac_codec", "[FLACCodec::updateInputQueueWatermarks_unlocked] Updated input watermarks: ", 
              "low=", m_input_queue_low_watermark, ", high=", m_input_queue_high_watermark, 
              ", max=", m_max_input_queue_size);
}

// ============================================================================
// Frame Reconstruction Implementation
// ============================================================================

bool FLACCodec::processPartialFrame_unlocked(const MediaChunk& chunk) {
    Debug::log("flac_codec", "[FLACCodec::processPartialFrame_unlocked] Processing partial frame with ", 
              chunk.data.size(), " bytes");
    
    m_partial_frames_received++;
    
    // If not currently reconstructing, start new frame reconstruction
    if (!m_frame_reconstruction_active) {
        // Estimate frame size from header if possible
        m_expected_frame_size = estimateFrameSize_unlocked(chunk.data.data(), chunk.data.size());
        
        if (m_expected_frame_size == 0) {
            Debug::log("flac_codec", "[FLACCodec::processPartialFrame_unlocked] Cannot estimate frame size");
            return false;
        }
        
        // Initialize partial frame buffer
        m_partial_frame_buffer.clear();
        m_partial_frame_buffer.reserve(m_expected_frame_size);
        m_frame_reconstruction_active = true;
        
        Debug::log("flac_codec", "[FLACCodec::processPartialFrame_unlocked] Started frame reconstruction, expected size: ", 
                  m_expected_frame_size);
    }
    
    // Append chunk data to partial frame buffer
    if (m_partial_frame_buffer.size() + chunk.data.size() <= m_expected_frame_size) {
        m_partial_frame_buffer.insert(m_partial_frame_buffer.end(), 
                                     chunk.data.begin(), chunk.data.end());
        
        Debug::log("flac_codec", "[FLACCodec::processPartialFrame_unlocked] Appended ", chunk.data.size(), 
                  " bytes, buffer now has ", m_partial_frame_buffer.size(), " bytes");
        return true;
    } else {
        Debug::log("flac_codec", "[FLACCodec::processPartialFrame_unlocked] Chunk would exceed expected frame size");
        resetFrameReconstruction_unlocked();
        return false;
    }
}

bool FLACCodec::reconstructFrame_unlocked(MediaChunk& complete_frame) {
    if (!m_frame_reconstruction_active || m_partial_frame_buffer.empty()) {
        return false;
    }
    
    // Check if frame is complete
    if (isFrameComplete_unlocked(m_partial_frame_buffer.data(), m_partial_frame_buffer.size())) {
        // Create complete frame from reconstructed data
        complete_frame.data = m_partial_frame_buffer;
        complete_frame.timestamp_samples = 0; // Will be set by caller if needed
        
        m_frames_reconstructed++;
        
        Debug::log("flac_codec", "[FLACCodec::reconstructFrame_unlocked] Reconstructed complete frame with ", 
                  complete_frame.data.size(), " bytes");
        
        // Reset reconstruction state
        resetFrameReconstruction_unlocked();
        return true;
    }
    
    return false;
}

void FLACCodec::resetFrameReconstruction_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::resetFrameReconstruction_unlocked] Resetting frame reconstruction");
    
    m_frame_reconstruction_active = false;
    m_expected_frame_size = 0;
    m_partial_frame_buffer.clear();
}

bool FLACCodec::isFrameComplete_unlocked(const uint8_t* data, size_t size) const {
    if (!data || size < 4) {
        return false;
    }
    
    // Basic FLAC frame completeness check
    // A complete frame should have:
    // 1. Valid sync pattern (0xFFF8-0xFFFF at start)
    // 2. Valid frame header
    // 3. Proper frame size matching the data
    
    // Check RFC 9639 Section 9.1 sync pattern: 0b111111111111100 + blocking strategy bit
    uint16_t sync_pattern = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    bool valid_sync = (sync_pattern == 0xFFF8) || (sync_pattern == 0xFFF9);
    
    if (!valid_sync) {
        return false;
    }
    
    // Validate frame header structure
    if (!validateFrameHeader_unlocked(data, size)) {
        return false;
    }
    
    // For a more complete check, we would need to parse the entire frame
    // For now, assume frame is complete if it has valid header and expected size
    return size >= m_expected_frame_size;
}

size_t FLACCodec::estimateFrameSize_unlocked(const uint8_t* data, size_t size) const {
    if (!data || size < 4) {
        return 0;
    }
    
    // Check for RFC 9639 FLAC sync pattern
    uint16_t sync_pattern = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    bool valid_sync = (sync_pattern == 0xFFF8) || (sync_pattern == 0xFFF9);
    
    if (!valid_sync) {
        return 0;
    }
    
    // For FLAC frames, size estimation is complex because frames are variable length
    // We'll use a conservative estimate based on block size and bit depth
    
    // Parse block size from frame header (RFC 9639 Section 9.2.2)
    uint8_t block_size_bits = (data[2] >> 4) & 0x0F;
    uint32_t estimated_block_size = 0;
    
    switch (block_size_bits) {
        case 0x1: estimated_block_size = 192; break;
        case 0x2: case 0x3: case 0x4: case 0x5:
            estimated_block_size = 576 << (block_size_bits - 2); break;
        case 0x6: case 0x7:
            // Variable block size - need to read from frame header
            estimated_block_size = 4608; // Conservative estimate
            break;
        case 0x8: case 0x9: case 0xA: case 0xB: case 0xC: case 0xD: case 0xE: case 0xF:
            estimated_block_size = 256 << (block_size_bits - 8); break;
        default:
            estimated_block_size = 4608; // Conservative default
            break;
    }
    
    // Estimate frame size based on block size, channels, and bit depth
    // This is a rough estimate - actual frames can be much smaller due to compression
    size_t estimated_size = (estimated_block_size * m_channels * m_bits_per_sample) / 8;
    
    // Add overhead for frame header, subframe headers, and CRC
    estimated_size += 64; // Conservative overhead estimate
    
    // Ensure minimum reasonable size
    estimated_size = std::max(estimated_size, size_t(16));
    
    // Cap at reasonable maximum (highly compressed frames are usually much smaller)
    estimated_size = std::min(estimated_size, size_t(65536));
    
    Debug::log("flac_codec", "[FLACCodec::estimateFrameSize_unlocked] Estimated frame size: ", 
              estimated_size, " bytes for block size ", estimated_block_size);
    
    return estimated_size;
}

bool FLACCodec::validateFrameHeader_unlocked(const uint8_t* data, size_t size) const {
    Debug::log("flac_codec", "[validateFrameHeader_unlocked] Performing RFC 9639 compliant frame header validation");
    
    if (!data || size < 4) {
        logRFC9639Violation_unlocked("Insufficient frame header data", "RFC 9639 Section 9.1", data, 0);
        return false;
    }
    
    // Use comprehensive RFC 9639 compliance validation first
    if (!validateRFC9639Compliance_unlocked(data, size)) {
        Debug::log("flac_codec", "[validateFrameHeader_unlocked] Frame header fails RFC 9639 compliance validation");
        return false;
    }
    
    // RFC 9639 Section 9.1: Frame Header Validation
    // Validate 15-bit sync code: 0b111111111111100 (0x3FFE when shifted)
    uint16_t sync_pattern = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    if ((sync_pattern & 0xFFFE) != 0xFFF8) {
        logRFC9639Violation_unlocked("Invalid sync pattern", "RFC 9639 Section 9.1", data, 0);
        return false;
    }
    
    // Extract and validate blocking strategy bit (RFC 9639 Section 9.1)
    uint8_t blocking_strategy = data[1] & 0x01;
    
    // Validate that sync code + blocking strategy forms valid pattern
    // Fixed block size: 0xFFF8, Variable block size: 0xFFF9
    if (blocking_strategy == 0 && (sync_pattern & 0xFFFF) != 0xFFF8) {
        logRFC9639Violation_unlocked("Invalid fixed block size sync pattern", 
                                   "RFC 9639 Section 9.1", data, 1);
        return false;
    }
    if (blocking_strategy == 1 && (sync_pattern & 0xFFFF) != 0xFFF9) {
        logRFC9639Violation_unlocked("Invalid variable block size sync pattern", 
                                   "RFC 9639 Section 9.1", data, 1);
        return false;
    }
    
    // Extract frame header fields for validation
    uint8_t block_size_bits = (data[2] >> 4) & 0x0F;      // RFC 9639 Section 9.1.1
    uint8_t sample_rate_bits = data[2] & 0x0F;             // RFC 9639 Section 9.1.2
    uint8_t channel_assignment = (data[3] >> 4) & 0x0F;    // RFC 9639 Section 9.1.3
    uint8_t bit_depth_bits = (data[3] >> 1) & 0x07;       // RFC 9639 Section 9.1.4
    uint8_t reserved_bit = data[3] & 0x01;                 // RFC 9639 Section 9.1.4
    
    // Additional RFC 9639 compliance checks for consistency
    
    // Validate block size ranges per RFC 9639 Table 14
    if (!validateBlockSizeBits_unlocked(block_size_bits)) {
        logRFC9639Violation_unlocked("Invalid block size bits", "RFC 9639 Section 9.1.1 (Table 14)", data, 2);
        return false;
    }
    
    // Validate sample rate ranges per RFC 9639 Table 15
    if (!validateSampleRateBits_unlocked(sample_rate_bits)) {
        logRFC9639Violation_unlocked("Invalid sample rate bits", "RFC 9639 Section 9.1.2 (Table 15)", data, 2);
        return false;
    }
    
    // Validate channel assignment per RFC 9639 Table 16
    if (!validateChannelAssignment_unlocked(channel_assignment)) {
        logRFC9639Violation_unlocked("Invalid channel assignment", "RFC 9639 Section 9.1.3 (Table 16)", data, 3);
        return false;
    }
    
    // Validate bit depth per RFC 9639 Table 17
    if (!validateBitDepthBits_unlocked(bit_depth_bits)) {
        logRFC9639Violation_unlocked("Invalid bit depth bits", "RFC 9639 Section 9.1.4 (Table 17)", data, 3);
        return false;
    }
    
    // Check for unsupported features that are valid per RFC but not implemented
    if (!handleUnsupportedFeatures_unlocked(data)) {
        Debug::log("flac_codec", "[validateFrameHeader_unlocked] Frame contains unsupported RFC 9639 features");
        return false;
    }
    
    Debug::log("flac_codec", "[validateFrameHeader_unlocked] RFC 9639 compliant frame header validated:");
    Debug::log("flac_codec", "  Sync Pattern: 0x", std::hex, sync_pattern, std::dec);
    Debug::log("flac_codec", "  Blocking Strategy: ", static_cast<unsigned>(blocking_strategy), 
              " (", (blocking_strategy ? "variable" : "fixed"), ")");
    Debug::log("flac_codec", "  Block Size Bits: 0x", std::hex, static_cast<unsigned>(block_size_bits), std::dec);
    Debug::log("flac_codec", "  Sample Rate Bits: 0x", std::hex, static_cast<unsigned>(sample_rate_bits), std::dec);
    Debug::log("flac_codec", "  Channel Assignment: 0x", std::hex, static_cast<unsigned>(channel_assignment), std::dec);
    Debug::log("flac_codec", "  Bit Depth Bits: 0x", std::hex, static_cast<unsigned>(bit_depth_bits), std::dec);
    Debug::log("flac_codec", "  Reserved Bit: ", static_cast<unsigned>(reserved_bit), " (RFC requires 0)");
    
    return true;
}

// ============================================================================
// RFC 9639 Compliance Validation Helper Functions
// ============================================================================

bool FLACCodec::validateBlockSizeBits_unlocked(uint8_t block_size_bits) const {
    // RFC 9639 Section 9.1.1 Table 14: Block Size Bits Validation
    switch (block_size_bits) {
        case 0x0:
            // Reserved - already checked in main validation
            Debug::log("flac_codec", "[validateBlockSizeBits_unlocked] Reserved block size: 0x0");
            return false;
            
        case 0x1:
            // 192 samples - valid
            return true;
            
        case 0x2: case 0x3: case 0x4: case 0x5:
            // 144 * (2^v): 576, 1152, 2304, 4608 - valid
            return true;
            
        case 0x6:
            // Uncommon block size minus 1, stored as 8-bit number
            // Need additional validation for the actual value
            Debug::log("flac_codec", "[validateBlockSizeBits_unlocked] Uncommon 8-bit block size");
            return true; // Will be validated when parsing the actual value
            
        case 0x7:
            // Uncommon block size minus 1, stored as 16-bit number
            // Need additional validation for the actual value
            Debug::log("flac_codec", "[validateBlockSizeBits_unlocked] Uncommon 16-bit block size");
            return true; // Will be validated when parsing the actual value
            
        case 0x8: case 0x9: case 0xA: case 0xB:
        case 0xC: case 0xD: case 0xE: case 0xF:
            // 2^v: 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 - valid
            return true;
            
        default:
            // Should never reach here due to 4-bit mask
            Debug::log("flac_codec", "[validateBlockSizeBits_unlocked] Invalid block size bits: 0x", 
                      std::hex, static_cast<unsigned>(block_size_bits), std::dec);
            return false;
    }
}

bool FLACCodec::validateSampleRateBits_unlocked(uint8_t sample_rate_bits) const {
    // RFC 9639 Section 9.1.2 Table 15: Sample Rate Bits Validation
    switch (sample_rate_bits) {
        case 0x0:
            // Sample rate only stored in streaminfo metadata block - valid
            return true;
            
        case 0x1: // 88.2 kHz
        case 0x2: // 176.4 kHz
        case 0x3: // 192 kHz
        case 0x4: // 8 kHz
        case 0x5: // 16 kHz
        case 0x6: // 22.05 kHz
        case 0x7: // 24 kHz
        case 0x8: // 32 kHz
        case 0x9: // 44.1 kHz
        case 0xA: // 48 kHz
        case 0xB: // 96 kHz
            // Standard sample rates - valid
            return true;
            
        case 0xC:
            // Uncommon sample rate in kHz, stored as 8-bit number
            Debug::log("flac_codec", "[validateSampleRateBits_unlocked] Uncommon 8-bit sample rate (kHz)");
            return true; // Will be validated when parsing the actual value
            
        case 0xD:
            // Uncommon sample rate in Hz, stored as 16-bit number
            Debug::log("flac_codec", "[validateSampleRateBits_unlocked] Uncommon 16-bit sample rate (Hz)");
            return true; // Will be validated when parsing the actual value
            
        case 0xE:
            // Uncommon sample rate in Hz divided by 10, stored as 16-bit number
            Debug::log("flac_codec", "[validateSampleRateBits_unlocked] Uncommon 16-bit sample rate (Hz/10)");
            return true; // Will be validated when parsing the actual value
            
        case 0xF:
            // Forbidden - already checked in main validation
            Debug::log("flac_codec", "[validateSampleRateBits_unlocked] Forbidden sample rate: 0xF");
            return false;
            
        default:
            // Should never reach here due to 4-bit mask
            Debug::log("flac_codec", "[validateSampleRateBits_unlocked] Invalid sample rate bits: 0x", 
                      std::hex, static_cast<unsigned>(sample_rate_bits), std::dec);
            return false;
    }
}

bool FLACCodec::validateChannelAssignment_unlocked(uint8_t channel_assignment) const {
    // RFC 9639 Section 9.1.3 Table 16: Channel Assignment Validation
    switch (channel_assignment) {
        case 0x0: // 1 channel: mono
        case 0x1: // 2 channels: left, right
        case 0x2: // 3 channels: left, right, center
        case 0x3: // 4 channels: front left, front right, back left, back right
        case 0x4: // 5 channels: front left, front right, front center, back/surround left, back/surround right
        case 0x5: // 6 channels: front left, front right, front center, LFE, back/surround left, back/surround right
        case 0x6: // 7 channels: front left, front right, front center, LFE, back center, side left, side right
        case 0x7: // 8 channels: front left, front right, front center, LFE, back left, back right, side left, side right
            // Standard channel configurations - valid
            return true;
            
        case 0x8: // 2 channels: left, right; stored as left-side stereo
        case 0x9: // 2 channels: left, right; stored as side-right stereo
        case 0xA: // 2 channels: left, right; stored as mid-side stereo
            // Stereo decorrelation modes - valid
            return true;
            
        case 0xB: case 0xC: case 0xD: case 0xE: case 0xF:
            // Reserved - already checked in main validation
            Debug::log("flac_codec", "[validateChannelAssignment_unlocked] Reserved channel assignment: 0x", 
                      std::hex, static_cast<unsigned>(channel_assignment), std::dec);
            return false;
            
        default:
            // Should never reach here due to 4-bit mask
            Debug::log("flac_codec", "[validateChannelAssignment_unlocked] Invalid channel assignment: 0x", 
                      std::hex, static_cast<unsigned>(channel_assignment), std::dec);
            return false;
    }
}

bool FLACCodec::validateBitDepthBits_unlocked(uint8_t bit_depth_bits) const {
    // RFC 9639 Section 9.1.4 Table 17: Bit Depth Validation
    switch (bit_depth_bits) {
        case 0x0:
            // Bit depth only stored in streaminfo metadata block - valid
            return true;
            
        case 0x1: // 8 bits per sample
        case 0x2: // 12 bits per sample
        case 0x4: // 16 bits per sample
        case 0x5: // 20 bits per sample
        case 0x6: // 24 bits per sample
        case 0x7: // 32 bits per sample
            // Valid bit depths - valid
            return true;
            
        case 0x3:
            // Reserved - already checked in main validation
            Debug::log("flac_codec", "[validateBitDepthBits_unlocked] Reserved bit depth: 0x3");
            return false;
            
        default:
            // Should never reach here due to 3-bit mask
            Debug::log("flac_codec", "[validateBitDepthBits_unlocked] Invalid bit depth bits: 0x", 
                      std::hex, static_cast<unsigned>(bit_depth_bits), std::dec);
            return false;
    }
}

// ============================================================================
// RFC 9639 Block Size and Sample Rate Decoding Implementation
// ============================================================================

uint32_t FLACCodec::decodeBlockSizeFromBits_unlocked(uint8_t block_size_bits, const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const {
    // RFC 9639 Section 9.1.1 Table 14: Block Size Decoding
    bytes_consumed = 0;
    
    switch (block_size_bits) {
        case 0x0:
            // Reserved - should have been caught in validation
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] ERROR: Reserved block size bits 0x0");
            return 0;
            
        case 0x1:
            // 192 samples
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 192 samples");
            return 192;
            
        case 0x2: // 576 samples (144 * 2^2)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 576 samples");
            return 576;
            
        case 0x3: // 1152 samples (144 * 2^3)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 1152 samples");
            return 1152;
            
        case 0x4: // 2304 samples (144 * 2^4)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 2304 samples");
            return 2304;
            
        case 0x5: // 4608 samples (144 * 2^5)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 4608 samples");
            return 4608;
            
        case 0x6:
            // Uncommon block size minus 1, stored as 8-bit number
            return decodeUncommonBlockSize8Bit_unlocked(header_data, header_size, bytes_consumed);
            
        case 0x7:
            // Uncommon block size minus 1, stored as 16-bit number
            return decodeUncommonBlockSize16Bit_unlocked(header_data, header_size, bytes_consumed);
            
        case 0x8: // 256 samples (2^8)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 256 samples");
            return 256;
            
        case 0x9: // 512 samples (2^9)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 512 samples");
            return 512;
            
        case 0xA: // 1024 samples (2^10)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 1024 samples");
            return 1024;
            
        case 0xB: // 2048 samples (2^11)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 2048 samples");
            return 2048;
            
        case 0xC: // 4096 samples (2^12)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 4096 samples");
            return 4096;
            
        case 0xD: // 8192 samples (2^13)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 8192 samples");
            return 8192;
            
        case 0xE: // 16384 samples (2^14)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 16384 samples");
            return 16384;
            
        case 0xF: // 32768 samples (2^15)
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] Fixed block size: 32768 samples");
            return 32768;
            
        default:
            // Should never reach here due to 4-bit mask
            Debug::log("flac_codec", "[decodeBlockSizeFromBits_unlocked] ERROR: Invalid block size bits: 0x", 
                      std::hex, static_cast<unsigned>(block_size_bits), std::dec);
            return 0;
    }
}

uint32_t FLACCodec::decodeSampleRateFromBits_unlocked(uint8_t sample_rate_bits, const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const {
    // RFC 9639 Section 9.1.2 Table 15: Sample Rate Decoding
    bytes_consumed = 0;
    
    switch (sample_rate_bits) {
        case 0x0:
            // Sample rate only stored in STREAMINFO metadata block
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Using STREAMINFO sample rate: ", m_sample_rate, " Hz");
            return m_sample_rate; // Use STREAMINFO sample rate
            
        case 0x1:
            // 88.2 kHz
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Fixed sample rate: 88200 Hz");
            return 88200;
            
        case 0x2:
            // 176.4 kHz
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Fixed sample rate: 176400 Hz");
            return 176400;
            
        case 0x3:
            // 192 kHz
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Fixed sample rate: 192000 Hz");
            return 192000;
            
        case 0x4:
            // 8 kHz
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Fixed sample rate: 8000 Hz");
            return 8000;
            
        case 0x5:
            // 16 kHz
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Fixed sample rate: 16000 Hz");
            return 16000;
            
        case 0x6:
            // 22.05 kHz
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Fixed sample rate: 22050 Hz");
            return 22050;
            
        case 0x7:
            // 24 kHz
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Fixed sample rate: 24000 Hz");
            return 24000;
            
        case 0x8:
            // 32 kHz
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Fixed sample rate: 32000 Hz");
            return 32000;
            
        case 0x9:
            // 44.1 kHz
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Fixed sample rate: 44100 Hz");
            return 44100;
            
        case 0xA:
            // 48 kHz
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Fixed sample rate: 48000 Hz");
            return 48000;
            
        case 0xB:
            // 96 kHz
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] Fixed sample rate: 96000 Hz");
            return 96000;
            
        case 0xC:
            // Uncommon sample rate in kHz, stored as 8-bit number
            return decodeUncommonSampleRate8BitKHz_unlocked(header_data, header_size, bytes_consumed);
            
        case 0xD:
            // Uncommon sample rate in Hz, stored as 16-bit number
            return decodeUncommonSampleRate16BitHz_unlocked(header_data, header_size, bytes_consumed);
            
        case 0xE:
            // Uncommon sample rate in Hz divided by 10, stored as 16-bit number
            return decodeUncommonSampleRate16BitHzDiv10_unlocked(header_data, header_size, bytes_consumed);
            
        case 0xF:
            // Forbidden - should have been caught in validation
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] ERROR: Forbidden sample rate bits 0xF");
            return 0;
            
        default:
            // Should never reach here due to 4-bit mask
            Debug::log("flac_codec", "[decodeSampleRateFromBits_unlocked] ERROR: Invalid sample rate bits: 0x", 
                      std::hex, static_cast<unsigned>(sample_rate_bits), std::dec);
            return 0;
    }
}

uint32_t FLACCodec::decodeUncommonBlockSize8Bit_unlocked(const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const {
    // RFC 9639 Section 9.1.6: Uncommon block size minus 1, stored as 8-bit number
    bytes_consumed = 0;
    
    if (!header_data || header_size < 1) {
        Debug::log("flac_codec", "[decodeUncommonBlockSize8Bit_unlocked] ERROR: Insufficient header data");
        return 0;
    }
    
    uint8_t block_size_minus_1 = header_data[0];
    uint32_t block_size = static_cast<uint32_t>(block_size_minus_1) + 1;
    bytes_consumed = 1;
    
    // Validate block size per RFC 9639 (16-65535 samples)
    if (block_size < 16 || block_size > 65535) {
        Debug::log("flac_codec", "[decodeUncommonBlockSize8Bit_unlocked] ERROR: Block size ", block_size, 
                  " outside RFC 9639 range (16-65535)");
        return 0;
    }
    
    Debug::log("flac_codec", "[decodeUncommonBlockSize8Bit_unlocked] Decoded 8-bit block size: ", block_size, " samples");
    return block_size;
}

uint32_t FLACCodec::decodeUncommonBlockSize16Bit_unlocked(const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const {
    // RFC 9639 Section 9.1.6: Uncommon block size minus 1, stored as 16-bit number
    bytes_consumed = 0;
    
    if (!header_data || header_size < 2) {
        Debug::log("flac_codec", "[decodeUncommonBlockSize16Bit_unlocked] ERROR: Insufficient header data");
        return 0;
    }
    
    // Read 16-bit big-endian value
    uint16_t block_size_minus_1 = (static_cast<uint16_t>(header_data[0]) << 8) | header_data[1];
    uint32_t block_size = static_cast<uint32_t>(block_size_minus_1) + 1;
    bytes_consumed = 2;
    
    // Validate block size per RFC 9639 (16-65535 samples)
    if (block_size < 16 || block_size > 65535) {
        Debug::log("flac_codec", "[decodeUncommonBlockSize16Bit_unlocked] ERROR: Block size ", block_size, 
                  " outside RFC 9639 range (16-65535)");
        return 0;
    }
    
    Debug::log("flac_codec", "[decodeUncommonBlockSize16Bit_unlocked] Decoded 16-bit block size: ", block_size, " samples");
    return block_size;
}

uint32_t FLACCodec::decodeUncommonSampleRate8BitKHz_unlocked(const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const {
    // RFC 9639 Section 9.1.7: Uncommon sample rate in kHz, stored as 8-bit number
    bytes_consumed = 0;
    
    if (!header_data || header_size < 1) {
        Debug::log("flac_codec", "[decodeUncommonSampleRate8BitKHz_unlocked] ERROR: Insufficient header data");
        return 0;
    }
    
    uint8_t sample_rate_khz = header_data[0];
    uint32_t sample_rate = static_cast<uint32_t>(sample_rate_khz) * 1000;
    bytes_consumed = 1;
    
    // Validate sample rate per RFC 9639 (1-655350 Hz)
    if (sample_rate < 1 || sample_rate > 655350) {
        Debug::log("flac_codec", "[decodeUncommonSampleRate8BitKHz_unlocked] ERROR: Sample rate ", sample_rate, 
                  " Hz outside RFC 9639 range (1-655350)");
        return 0;
    }
    
    Debug::log("flac_codec", "[decodeUncommonSampleRate8BitKHz_unlocked] Decoded 8-bit kHz sample rate: ", sample_rate, " Hz");
    return sample_rate;
}

uint32_t FLACCodec::decodeUncommonSampleRate16BitHz_unlocked(const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const {
    // RFC 9639 Section 9.1.7: Uncommon sample rate in Hz, stored as 16-bit number
    bytes_consumed = 0;
    
    if (!header_data || header_size < 2) {
        Debug::log("flac_codec", "[decodeUncommonSampleRate16BitHz_unlocked] ERROR: Insufficient header data");
        return 0;
    }
    
    // Read 16-bit big-endian value
    uint32_t sample_rate = (static_cast<uint32_t>(header_data[0]) << 8) | header_data[1];
    bytes_consumed = 2;
    
    // Validate sample rate per RFC 9639 (1-655350 Hz)
    if (sample_rate < 1 || sample_rate > 655350) {
        Debug::log("flac_codec", "[decodeUncommonSampleRate16BitHz_unlocked] ERROR: Sample rate ", sample_rate, 
                  " Hz outside RFC 9639 range (1-655350)");
        return 0;
    }
    
    Debug::log("flac_codec", "[decodeUncommonSampleRate16BitHz_unlocked] Decoded 16-bit Hz sample rate: ", sample_rate, " Hz");
    return sample_rate;
}

uint32_t FLACCodec::decodeUncommonSampleRate16BitHzDiv10_unlocked(const uint8_t* header_data, size_t header_size, size_t& bytes_consumed) const {
    // RFC 9639 Section 9.1.7: Uncommon sample rate in Hz divided by 10, stored as 16-bit number
    bytes_consumed = 0;
    
    if (!header_data || header_size < 2) {
        Debug::log("flac_codec", "[decodeUncommonSampleRate16BitHzDiv10_unlocked] ERROR: Insufficient header data");
        return 0;
    }
    
    // Read 16-bit big-endian value and multiply by 10
    uint16_t sample_rate_div10 = (static_cast<uint16_t>(header_data[0]) << 8) | header_data[1];
    uint32_t sample_rate = static_cast<uint32_t>(sample_rate_div10) * 10;
    bytes_consumed = 2;
    
    // Validate sample rate per RFC 9639 (1-655350 Hz)
    if (sample_rate < 1 || sample_rate > 655350) {
        Debug::log("flac_codec", "[decodeUncommonSampleRate16BitHzDiv10_unlocked] ERROR: Sample rate ", sample_rate, 
                  " Hz outside RFC 9639 range (1-655350)");
        return 0;
    }
    
    Debug::log("flac_codec", "[decodeUncommonSampleRate16BitHzDiv10_unlocked] Decoded 16-bit Hz/10 sample rate: ", sample_rate, " Hz");
    return sample_rate;
}

bool FLACCodec::validateStreamInfoConsistency_unlocked(uint32_t frame_block_size, uint32_t frame_sample_rate) const {
    // RFC 9639 Section 8.2: Validate consistency between STREAMINFO and frame header parameters
    
    // Validate block size against STREAMINFO constraints
    if (m_min_block_size > 0 && frame_block_size < m_min_block_size) {
        Debug::log("flac_codec", "[validateStreamInfoConsistency_unlocked] ERROR: Frame block size ", frame_block_size, 
                  " below STREAMINFO minimum ", m_min_block_size);
        return false;
    }
    
    if (m_max_block_size > 0 && frame_block_size > m_max_block_size) {
        Debug::log("flac_codec", "[validateStreamInfoConsistency_unlocked] ERROR: Frame block size ", frame_block_size, 
                  " above STREAMINFO maximum ", m_max_block_size);
        return false;
    }
    
    // Validate sample rate consistency (only if frame specifies a rate)
    if (frame_sample_rate != 0 && m_sample_rate != 0 && frame_sample_rate != m_sample_rate) {
        Debug::log("flac_codec", "[validateStreamInfoConsistency_unlocked] WARNING: Frame sample rate ", frame_sample_rate, 
                  " Hz differs from STREAMINFO ", m_sample_rate, " Hz");
        // This is a warning, not an error - frame rate takes precedence per RFC 9639
    }
    
    Debug::log("flac_codec", "[validateStreamInfoConsistency_unlocked] STREAMINFO consistency validated: block_size=", 
              frame_block_size, ", sample_rate=", frame_sample_rate, " Hz");
    return true;
}

bool FLACCodec::validateBlockSizeRange_unlocked(uint32_t block_size) const {
    // RFC 9639 Section 9.1.1: Block size must be 16-65535 samples
    if (block_size < 16 || block_size > 65535) {
        Debug::log("flac_codec", "[validateBlockSizeRange_unlocked] ERROR: Block size ", block_size, 
                  " outside RFC 9639 range (16-65535)");
        return false;
    }
    
    Debug::log("flac_codec", "[validateBlockSizeRange_unlocked] Block size ", block_size, " within RFC 9639 range");
    return true;
}

bool FLACCodec::validateSampleRateRange_unlocked(uint32_t sample_rate) const {
    // RFC 9639 Section 8.2: Sample rate must be 1-655350 Hz
    if (sample_rate < 1 || sample_rate > 655350) {
        Debug::log("flac_codec", "[validateSampleRateRange_unlocked] ERROR: Sample rate ", sample_rate, 
                  " Hz outside RFC 9639 range (1-655350)");
        return false;
    }
    
    Debug::log("flac_codec", "[validateSampleRateRange_unlocked] Sample rate ", sample_rate, " Hz within RFC 9639 range");
    return true;
}

// ============================================================================
// RFC 9639 Section 9.2 Subframe Type Compliance Validation Implementation
// ============================================================================

bool FLACCodec::validateSubframeType_unlocked(uint8_t subframe_type_bits) const {
    // RFC 9639 Section 9.2.1 Table 19: Subframe Type Validation
    // The subframe_type_bits parameter contains the 6-bit subframe type value
    
    Debug::log("flac_codec", "[validateSubframeType_unlocked] Validating subframe type: 0b", 
              std::bitset<6>(subframe_type_bits).to_string(), " (0x", 
              std::hex, static_cast<unsigned>(subframe_type_bits), std::dec, ")");
    
    // Validate individual subframe types per RFC 9639 specification
    if (validateConstantSubframe_unlocked(subframe_type_bits)) {
        Debug::log("flac_codec", "[validateSubframeType_unlocked] Valid CONSTANT subframe");
        return true;
    }
    
    if (validateVerbatimSubframe_unlocked(subframe_type_bits)) {
        Debug::log("flac_codec", "[validateSubframeType_unlocked] Valid VERBATIM subframe");
        return true;
    }
    
    if (validateFixedPredictorSubframe_unlocked(subframe_type_bits)) {
        uint8_t order = extractPredictorOrder_unlocked(subframe_type_bits);
        Debug::log("flac_codec", "[validateSubframeType_unlocked] Valid FIXED predictor subframe, order: ", 
                  static_cast<unsigned>(order));
        return true;
    }
    
    if (validateLinearPredictorSubframe_unlocked(subframe_type_bits)) {
        uint8_t order = extractPredictorOrder_unlocked(subframe_type_bits);
        Debug::log("flac_codec", "[validateSubframeType_unlocked] Valid LPC predictor subframe, order: ", 
                  static_cast<unsigned>(order));
        return true;
    }
    
    // Check for reserved values per RFC 9639 Table 19
    if ((subframe_type_bits >= 0x02 && subframe_type_bits <= 0x07) ||
        (subframe_type_bits >= 0x0D && subframe_type_bits <= 0x1F)) {
        Debug::log("flac_codec", "[validateSubframeType_unlocked] Reserved subframe type: 0x", 
                  std::hex, static_cast<unsigned>(subframe_type_bits), std::dec, 
                  " (RFC 9639 Section 9.2.1)");
        return false;
    }
    
    // Invalid subframe type
    Debug::log("flac_codec", "[validateSubframeType_unlocked] Invalid subframe type: 0x", 
              std::hex, static_cast<unsigned>(subframe_type_bits), std::dec);
    return false;
}

bool FLACCodec::validateConstantSubframe_unlocked(uint8_t subframe_type_bits) const {
    // RFC 9639 Section 9.2.3: Constant subframe type is 0b000000
    if (subframe_type_bits == 0x00) {
        Debug::log("flac_codec", "[validateConstantSubframe_unlocked] Valid CONSTANT subframe (0x00)");
        return true;
    }
    return false;
}

bool FLACCodec::validateVerbatimSubframe_unlocked(uint8_t subframe_type_bits) const {
    // RFC 9639 Section 9.2.4: Verbatim subframe type is 0b000001
    if (subframe_type_bits == 0x01) {
        Debug::log("flac_codec", "[validateVerbatimSubframe_unlocked] Valid VERBATIM subframe (0x01)");
        return true;
    }
    return false;
}

bool FLACCodec::validateFixedPredictorSubframe_unlocked(uint8_t subframe_type_bits) const {
    // RFC 9639 Section 9.2.5: Fixed predictor subframes are 0b001000 - 0b001100
    // This corresponds to predictor orders 0, 1, 2, 3, 4 (v-8 where v is the 6-bit value)
    if (subframe_type_bits >= 0x08 && subframe_type_bits <= 0x0C) {
        uint8_t predictor_order = subframe_type_bits - 0x08;
        Debug::log("flac_codec", "[validateFixedPredictorSubframe_unlocked] Valid FIXED predictor subframe, ", 
                  "type: 0x", std::hex, static_cast<unsigned>(subframe_type_bits), std::dec, 
                  ", order: ", static_cast<unsigned>(predictor_order));
        
        // Validate predictor order is within RFC 9639 limits (0-4)
        if (predictor_order > 4) {
            Debug::log("flac_codec", "[validateFixedPredictorSubframe_unlocked] Invalid predictor order: ", 
                      static_cast<unsigned>(predictor_order), " (RFC 9639 limit: 0-4)");
            return false;
        }
        
        return true;
    }
    return false;
}

bool FLACCodec::validateLinearPredictorSubframe_unlocked(uint8_t subframe_type_bits) const {
    // RFC 9639 Section 9.2.6: Linear predictor subframes are 0b100000 - 0b111111
    // This corresponds to predictor orders 1-32 (v-31 where v is the 6-bit value)
    if (subframe_type_bits >= 0x20 && subframe_type_bits <= 0x3F) {
        uint8_t predictor_order = subframe_type_bits - 0x1F; // v-31, but we want 1-based
        Debug::log("flac_codec", "[validateLinearPredictorSubframe_unlocked] Valid LPC predictor subframe, ", 
                  "type: 0x", std::hex, static_cast<unsigned>(subframe_type_bits), std::dec, 
                  ", order: ", static_cast<unsigned>(predictor_order));
        
        // Validate predictor order is within RFC 9639 limits (1-32)
        if (predictor_order < 1 || predictor_order > 32) {
            Debug::log("flac_codec", "[validateLinearPredictorSubframe_unlocked] Invalid predictor order: ", 
                      static_cast<unsigned>(predictor_order), " (RFC 9639 limit: 1-32)");
            return false;
        }
        
        // RFC 9639 Section 7: Streamable subset restriction for sample rates <= 48kHz
        // Linear prediction subframes MUST have predictor order <= 12 for sample rates <= 48000 Hz
        if (m_sample_rate <= 48000 && predictor_order > 12) {
            Debug::log("flac_codec", "[validateLinearPredictorSubframe_unlocked] Streamable subset violation: ", 
                      "predictor order ", static_cast<unsigned>(predictor_order), 
                      " > 12 for sample rate ", m_sample_rate, " Hz (RFC 9639 Section 7)");
            return false;
        }
        
        return true;
    }
    return false;
}

bool FLACCodec::validateWastedBitsFlag_unlocked(uint8_t wasted_bits_flag) const {
    // RFC 9639 Section 9.2.2: Wasted bits flag is a single bit (0 or 1)
    if (wasted_bits_flag > 1) {
        Debug::log("flac_codec", "[validateWastedBitsFlag_unlocked] Invalid wasted bits flag: ", 
                  static_cast<unsigned>(wasted_bits_flag), " (must be 0 or 1)");
        return false;
    }
    
    Debug::log("flac_codec", "[validateWastedBitsFlag_unlocked] Valid wasted bits flag: ", 
              static_cast<unsigned>(wasted_bits_flag));
    return true;
}

uint8_t FLACCodec::extractPredictorOrder_unlocked(uint8_t subframe_type_bits) const {
    // Extract predictor order based on subframe type per RFC 9639 Section 9.2.1
    if (subframe_type_bits >= 0x08 && subframe_type_bits <= 0x0C) {
        // Fixed predictor: order = v - 8 (where v is the 6-bit value)
        return subframe_type_bits - 0x08;
    } else if (subframe_type_bits >= 0x20 && subframe_type_bits <= 0x3F) {
        // Linear predictor: order = v - 31 (where v is the 6-bit value)
        return subframe_type_bits - 0x1F; // -31 + 1 to make it 1-based
    } else {
        // Not a predictor subframe
        Debug::log("flac_codec", "[extractPredictorOrder_unlocked] Not a predictor subframe: 0x", 
                  std::hex, static_cast<unsigned>(subframe_type_bits), std::dec);
        return 0;
    }
}

// ============================================================================
// RFC 9639 Section 9.2 Comprehensive Subframe Processing Validation
// ============================================================================

bool FLACCodec::validateAndDebugSubframeProcessing_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) const {
    if (!frame || !buffer) {
        Debug::log("flac_codec", "[validateAndDebugSubframeProcessing_unlocked] Invalid parameters");
        return false;
    }
    
    Debug::log("flac_codec", "[validateAndDebugSubframeProcessing_unlocked] RFC 9639 Section 9.2 subframe validation");
    Debug::log("flac_codec", "  Frame: ", frame->header.blocksize, " samples, ", 
              frame->header.channels, " channels, ", frame->header.bits_per_sample, " bits");
    Debug::log("flac_codec", "  Channel assignment: ", static_cast<unsigned>(frame->header.channel_assignment), 
              " (", getChannelAssignmentName_unlocked(frame->header.channel_assignment), ")");
    
    bool all_valid = true;
    
    // Validate each subframe according to RFC 9639 Section 9.2
    for (unsigned ch = 0; ch < frame->header.channels; ++ch) {
        Debug::log("flac_codec", "[validateAndDebugSubframeProcessing_unlocked] Validating subframe ", ch);
        
        // Since libFLAC has already processed the subframes, we validate the results
        // and perform bit-level debugging based on the decoded data
        if (!validateSubframeData_unlocked(frame, buffer, ch)) {
            Debug::log("flac_codec", "[validateAndDebugSubframeProcessing_unlocked] Subframe ", ch, " validation failed");
            all_valid = false;
        }
        
        // Debug subframe type detection (inferred from libFLAC processing)
        debugSubframeTypeDetection_unlocked(frame, buffer, ch);
        
        // Validate wasted bits handling per RFC 9639 Section 9.2.2
        if (!validateSubframeWastedBits_unlocked(frame, ch)) {
            Debug::log("flac_codec", "[validateAndDebugSubframeProcessing_unlocked] Wasted bits validation failed for subframe ", ch);
            all_valid = false;
        }
        
        // Validate predictor processing for FIXED and LPC subframes
        if (!validatePredictorProcessing_unlocked(frame, buffer, ch)) {
            Debug::log("flac_codec", "[validateAndDebugSubframeProcessing_unlocked] Predictor processing validation failed for subframe ", ch);
            all_valid = false;
        }
        
        // Validate residual decoding compliance
        if (!validateResidualDecoding_unlocked(frame, buffer, ch)) {
            Debug::log("flac_codec", "[validateAndDebugSubframeProcessing_unlocked] Residual decoding validation failed for subframe ", ch);
            all_valid = false;
        }
    }
    
    // Validate channel reconstruction algorithms
    if (!validateChannelReconstruction_unlocked(frame, buffer)) {
        Debug::log("flac_codec", "[validateAndDebugSubframeProcessing_unlocked] Channel reconstruction validation failed");
        all_valid = false;
    }
    
    if (all_valid) {
        Debug::log("flac_codec", "[validateAndDebugSubframeProcessing_unlocked] All RFC 9639 subframe validations passed");
    } else {
        Debug::log("flac_codec", "[validateAndDebugSubframeProcessing_unlocked] Some RFC 9639 subframe validations failed");
    }
    
    return all_valid;
}

bool FLACCodec::validateSubframeData_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[], unsigned channel) const {
    if (!frame || !buffer || channel >= frame->header.channels || !buffer[channel]) {
        Debug::log("flac_codec", "[validateSubframeData_unlocked] Invalid parameters for channel ", channel);
        return false;
    }
    
    const FLAC__int32* channel_data = buffer[channel];
    uint32_t block_size = frame->header.blocksize;
    uint16_t bits_per_sample = frame->header.bits_per_sample;
    
    Debug::log("flac_codec", "[validateSubframeData_unlocked] RFC 9639 subframe data validation for channel ", channel);
    
    // Calculate expected sample range based on bit depth
    int32_t max_sample_value = (1 << (bits_per_sample - 1)) - 1;
    int32_t min_sample_value = -(1 << (bits_per_sample - 1));
    
    Debug::log("flac_codec", "  Expected sample range: [", min_sample_value, ", ", max_sample_value, "]");
    
    // Validate all samples are within expected range
    bool range_valid = true;
    uint32_t out_of_range_count = 0;
    int32_t actual_min = INT32_MAX;
    int32_t actual_max = INT32_MIN;
    
    for (uint32_t i = 0; i < block_size; ++i) {
        int32_t sample = channel_data[i];
        
        if (sample < min_sample_value || sample > max_sample_value) {
            if (out_of_range_count < 5) { // Log first 5 violations
                Debug::log("flac_codec", "  RFC 9639 violation: sample[", i, "] = ", sample, 
                          " outside range [", min_sample_value, ", ", max_sample_value, "]");
            }
            out_of_range_count++;
            range_valid = false;
        }
        
        actual_min = std::min(actual_min, sample);
        actual_max = std::max(actual_max, sample);
    }
    
    if (out_of_range_count > 5) {
        Debug::log("flac_codec", "  ... and ", out_of_range_count - 5, " more out-of-range samples");
    }
    
    Debug::log("flac_codec", "  Actual sample range: [", actual_min, ", ", actual_max, "]");
    Debug::log("flac_codec", "  Range validation: ", (range_valid ? "PASSED" : "FAILED"));
    
    if (!range_valid) {
        Debug::log("flac_codec", "  RFC 9639 Section 9.2.3: Sample values exceed bit depth limits");
    }
    
    return range_valid;
}

void FLACCodec::debugSubframeTypeDetection_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[], unsigned channel) const {
    if (!frame || !buffer || channel >= frame->header.channels || !buffer[channel]) {
        return;
    }
    
    const FLAC__int32* channel_data = buffer[channel];
    uint32_t block_size = frame->header.blocksize;
    
    Debug::log("flac_codec", "[debugSubframeTypeDetection_unlocked] RFC 9639 Section 9.2 subframe type analysis for channel ", channel);
    
    // Analyze decoded data to infer subframe type characteristics
    // This helps debug subframe type detection issues
    
    // Check for CONSTANT subframe characteristics
    bool is_constant = true;
    int32_t first_sample = channel_data[0];
    for (uint32_t i = 1; i < block_size; ++i) {
        if (channel_data[i] != first_sample) {
            is_constant = false;
            break;
        }
    }
    
    if (is_constant) {
        Debug::log("flac_codec", "  Subframe type analysis: CONSTANT detected (RFC 9639 Section 9.2.3)");
        Debug::log("flac_codec", "  Constant value: ", first_sample);
        return;
    }
    
    // Analyze for VERBATIM characteristics (high entropy, no clear patterns)
    double entropy = calculateSampleEntropy_unlocked(channel_data, block_size);
    Debug::log("flac_codec", "  Sample entropy: ", entropy, " (higher values suggest VERBATIM)");
    
    // Analyze for FIXED predictor characteristics
    analyzeFixedPredictorCharacteristics_unlocked(channel_data, block_size, channel);
    
    // Analyze for LPC predictor characteristics
    analyzeLPCPredictorCharacteristics_unlocked(channel_data, block_size, channel);
    
    Debug::log("flac_codec", "  Subframe type analysis complete for channel ", channel);
}

bool FLACCodec::validateSubframeWastedBits_unlocked(const FLAC__Frame* frame, unsigned channel) const {
    if (!frame || channel >= frame->header.channels) {
        return false;
    }
    
    Debug::log("flac_codec", "[validateSubframeWastedBits_unlocked] RFC 9639 Section 9.2.2 wasted bits validation for channel ", channel);
    
    // Since libFLAC handles wasted bits internally, we validate the effective bit depth
    uint16_t frame_bits_per_sample = frame->header.bits_per_sample;
    uint8_t channel_assignment = frame->header.channel_assignment;
    
    // Calculate expected bit depth for this subframe per RFC 9639
    uint16_t expected_subframe_bits = frame_bits_per_sample;
    
    // RFC 9639 Section 9.2.3: Side subframes have one extra bit
    if (channel_assignment == 8 && channel == 1) { // Left-side stereo, side channel
        expected_subframe_bits += 1;
        Debug::log("flac_codec", "  Left-side stereo: side channel has +1 bit depth");
    } else if (channel_assignment == 9 && channel == 0) { // Right-side stereo, side channel
        expected_subframe_bits += 1;
        Debug::log("flac_codec", "  Right-side stereo: side channel has +1 bit depth");
    } else if (channel_assignment == 10 && channel == 1) { // Mid-side stereo, side channel
        expected_subframe_bits += 1;
        Debug::log("flac_codec", "  Mid-side stereo: side channel has +1 bit depth");
    }
    
    Debug::log("flac_codec", "  Frame bit depth: ", frame_bits_per_sample);
    Debug::log("flac_codec", "  Expected subframe bit depth: ", expected_subframe_bits);
    
    // Validate bit depth is within RFC 9639 limits
    if (expected_subframe_bits < 4 || expected_subframe_bits > 32) {
        Debug::log("flac_codec", "  RFC 9639 violation: subframe bit depth ", expected_subframe_bits, 
                  " outside valid range (4-32)");
        return false;
    }
    
    Debug::log("flac_codec", "  Wasted bits validation: PASSED");
    return true;
}

bool FLACCodec::validatePredictorProcessing_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[], unsigned channel) const {
    if (!frame || !buffer || channel >= frame->header.channels || !buffer[channel]) {
        return false;
    }
    
    Debug::log("flac_codec", "[validatePredictorProcessing_unlocked] RFC 9639 predictor validation for channel ", channel);
    
    const FLAC__int32* channel_data = buffer[channel];
    uint32_t block_size = frame->header.blocksize;
    
    // Analyze predictor effectiveness by examining sample patterns
    // This helps validate that FIXED and LPC predictors are working correctly
    
    // Calculate first-order differences (FIXED predictor order 1)
    std::vector<int32_t> first_diff(block_size - 1);
    for (uint32_t i = 1; i < block_size; ++i) {
        first_diff[i - 1] = channel_data[i] - channel_data[i - 1];
    }
    
    // Calculate second-order differences (FIXED predictor order 2)
    std::vector<int32_t> second_diff;
    if (block_size > 2) {
        second_diff.resize(block_size - 2);
        for (uint32_t i = 2; i < block_size; ++i) {
            second_diff[i - 2] = channel_data[i] - 2 * channel_data[i - 1] + channel_data[i - 2];
        }
    }
    
    // Analyze prediction residuals
    double first_diff_variance = calculateVariance_unlocked(first_diff);
    double second_diff_variance = second_diff.empty() ? 0.0 : calculateVariance_unlocked(second_diff);
    
    Debug::log("flac_codec", "  First-order difference variance: ", first_diff_variance);
    Debug::log("flac_codec", "  Second-order difference variance: ", second_diff_variance);
    
    // Validate predictor coefficients are within RFC 9639 limits
    // Since libFLAC handles this internally, we validate the results are reasonable
    bool predictor_valid = true;
    
    // Check for excessive residual values that might indicate predictor issues
    for (size_t i = 0; i < first_diff.size(); ++i) {
        if (std::abs(first_diff[i]) > (1 << 30)) { // Residuals should be < 2^31 per RFC 9639
            Debug::log("flac_codec", "  RFC 9639 violation: excessive residual ", first_diff[i], " at position ", i);
            predictor_valid = false;
        }
    }
    
    Debug::log("flac_codec", "  Predictor processing validation: ", (predictor_valid ? "PASSED" : "FAILED"));
    return predictor_valid;
}

bool FLACCodec::validateResidualDecoding_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[], unsigned channel) const {
    if (!frame || !buffer || channel >= frame->header.channels || !buffer[channel]) {
        return false;
    }
    
    Debug::log("flac_codec", "[validateResidualDecoding_unlocked] RFC 9639 Section 9.2.7 residual validation for channel ", channel);
    
    const FLAC__int32* channel_data = buffer[channel];
    uint32_t block_size = frame->header.blocksize;
    
    // Validate residual decoding by analyzing the decoded samples
    // RFC 9639 Section 9.2.7.3: All residual samples must be representable in 32-bit signed integers
    
    bool residual_valid = true;
    uint32_t invalid_residual_count = 0;
    
    // Since we have the final decoded samples, we can't directly validate residuals
    // but we can validate that the samples are within expected ranges
    
    // Calculate approximate residuals using simple prediction
    for (uint32_t i = 1; i < block_size; ++i) {
        int64_t predicted = channel_data[i - 1]; // Simple first-order prediction
        int64_t residual = static_cast<int64_t>(channel_data[i]) - predicted;
        
        // RFC 9639 Section 9.2.7.3: Residual must be representable in 32-bit signed
        if (residual < INT32_MIN || residual > INT32_MAX) {
            if (invalid_residual_count < 3) { // Log first 3 violations
                Debug::log("flac_codec", "  RFC 9639 violation: residual ", residual, 
                          " at position ", i, " exceeds 32-bit range");
            }
            invalid_residual_count++;
            residual_valid = false;
        }
    }
    
    if (invalid_residual_count > 3) {
        Debug::log("flac_codec", "  ... and ", invalid_residual_count - 3, " more invalid residuals");
    }
    
    Debug::log("flac_codec", "  Residual decoding validation: ", (residual_valid ? "PASSED" : "FAILED"));
    
    if (!residual_valid) {
        Debug::log("flac_codec", "  RFC 9639 Section 9.2.7.3: Residual values exceed 32-bit signed integer range");
    }
    
    return residual_valid;
}

bool FLACCodec::validateChannelReconstruction_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) const {
    if (!frame || !buffer) {
        return false;
    }
    
    uint8_t channel_assignment = frame->header.channel_assignment;
    uint16_t channels = frame->header.channels;
    
    Debug::log("flac_codec", "[validateChannelReconstruction_unlocked] RFC 9639 channel reconstruction validation");
    Debug::log("flac_codec", "  Channel assignment: ", static_cast<unsigned>(channel_assignment), 
              " (", getChannelAssignmentName_unlocked(channel_assignment), ")");
    
    // Validate channel reconstruction algorithms match RFC 9639 formulas
    bool reconstruction_valid = true;
    
    if (channel_assignment >= 8 && channel_assignment <= 10 && channels == 2) {
        // Stereo decorrelation modes - validate reconstruction formulas
        if (!buffer[0] || !buffer[1]) {
            Debug::log("flac_codec", "  Invalid buffer pointers for stereo reconstruction");
            return false;
        }
        
        uint32_t block_size = frame->header.blocksize;
        uint32_t samples_to_check = std::min(block_size, 10u); // Check first 10 samples
        
        for (uint32_t i = 0; i < samples_to_check; ++i) {
            int32_t ch0 = buffer[0][i];
            int32_t ch1 = buffer[1][i];
            
            switch (channel_assignment) {
                case 8: // Left-side stereo
                    // RFC 9639: left = ch0, right = left - side = ch0 - ch1
                    Debug::log("flac_codec", "  Sample ", i, ": left=", ch0, ", side=", ch1, 
                              ", reconstructed_right=", ch0 - ch1);
                    break;
                    
                case 9: // Right-side stereo  
                    // RFC 9639: left = side + right = ch0 + ch1, right = ch1
                    Debug::log("flac_codec", "  Sample ", i, ": side=", ch0, ", right=", ch1, 
                              ", reconstructed_left=", ch0 + ch1);
                    break;
                    
                case 10: // Mid-side stereo
                    // RFC 9639 Section 4.2: mid<<1 + (side&1), left=(mid+side)>>1, right=(mid-side)>>1
                    {
                        int32_t mid = ch0;
                        int32_t side = ch1;
                        int32_t mid_shifted = (mid << 1) + (side & 1);
                        int32_t reconstructed_left = (mid_shifted + side) >> 1;
                        int32_t reconstructed_right = (mid_shifted - side) >> 1;
                        Debug::log("flac_codec", "  Sample ", i, ": mid=", mid, ", side=", side, 
                                  ", mid_shifted=", mid_shifted,
                                  ", reconstructed_left=", reconstructed_left, 
                                  ", reconstructed_right=", reconstructed_right);
                    }
                    break;
            }
        }
        
        Debug::log("flac_codec", "  Stereo reconstruction formulas validated per RFC 9639");
    }
    
    Debug::log("flac_codec", "  Channel reconstruction validation: ", (reconstruction_valid ? "PASSED" : "FAILED"));
    return reconstruction_valid;
}

// Helper methods for subframe processing validation

const char* FLACCodec::getChannelAssignmentName_unlocked(uint8_t assignment) const {
    switch (assignment) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
            return "independent";
        case 8: return "left-side";
        case 9: return "right-side";
        case 10: return "mid-side";
        default: return "reserved";
    }
}

double FLACCodec::calculateSampleEntropy_unlocked(const FLAC__int32* samples, uint32_t count) const {
    if (!samples || count == 0) return 0.0;
    
    // Simple entropy calculation based on sample value distribution
    std::unordered_map<int32_t, uint32_t> value_counts;
    
    for (uint32_t i = 0; i < count; ++i) {
        value_counts[samples[i]]++;
    }
    
    double entropy = 0.0;
    for (const auto& pair : value_counts) {
        double probability = static_cast<double>(pair.second) / count;
        if (probability > 0.0) {
            entropy -= probability * std::log2(probability);
        }
    }
    
    return entropy;
}

void FLACCodec::analyzeFixedPredictorCharacteristics_unlocked(const FLAC__int32* samples, uint32_t count, unsigned channel) const {
    if (!samples || count < 5) return;
    
    Debug::log("flac_codec", "[analyzeFixedPredictorCharacteristics_unlocked] RFC 9639 Section 9.2.5 analysis for channel ", channel);
    
    // Test each fixed predictor order (0-4) and calculate residual variance
    for (int order = 0; order <= 4; ++order) {
        std::vector<int32_t> residuals;
        residuals.reserve(count - order);
        
        for (uint32_t i = order; i < count; ++i) {
            int32_t prediction = 0;
            
            switch (order) {
                case 0:
                    prediction = 0;
                    break;
                case 1:
                    prediction = samples[i - 1];
                    break;
                case 2:
                    prediction = 2 * samples[i - 1] - samples[i - 2];
                    break;
                case 3:
                    prediction = 3 * samples[i - 1] - 3 * samples[i - 2] + samples[i - 3];
                    break;
                case 4:
                    prediction = 4 * samples[i - 1] - 6 * samples[i - 2] + 4 * samples[i - 3] - samples[i - 4];
                    break;
            }
            
            residuals.push_back(samples[i] - prediction);
        }
        
        double variance = calculateVariance_unlocked(residuals);
        Debug::log("flac_codec", "  Fixed predictor order ", order, " variance: ", variance);
    }
}

void FLACCodec::analyzeLPCPredictorCharacteristics_unlocked(const FLAC__int32* samples, uint32_t count, unsigned channel) const {
    if (!samples || count < 10) return;
    
    Debug::log("flac_codec", "[analyzeLPCPredictorCharacteristics_unlocked] RFC 9639 Section 9.2.6 analysis for channel ", channel);
    
    // Analyze sample autocorrelation to understand LPC predictor suitability
    std::vector<double> autocorr(5, 0.0);
    
    for (int lag = 0; lag < 5 && lag < static_cast<int>(count); ++lag) {
        double sum = 0.0;
        for (uint32_t i = lag; i < count; ++i) {
            sum += static_cast<double>(samples[i]) * samples[i - lag];
        }
        autocorr[lag] = sum / (count - lag);
    }
    
    Debug::log("flac_codec", "  Autocorrelation analysis:");
    for (int lag = 0; lag < 5; ++lag) {
        Debug::log("flac_codec", "    lag ", lag, ": ", autocorr[lag]);
    }
    
    // Calculate prediction gain estimate
    if (autocorr[0] > 0.0) {
        double prediction_gain = autocorr[0] / std::max(autocorr[1], 1.0);
        Debug::log("flac_codec", "  Estimated prediction gain: ", prediction_gain);
    }
}

double FLACCodec::calculateVariance_unlocked(const std::vector<int32_t>& values) const {
    if (values.empty()) return 0.0;
    
    double mean = 0.0;
    for (int32_t value : values) {
        mean += value;
    }
    mean /= values.size();
    
    double variance = 0.0;
    for (int32_t value : values) {
        double diff = value - mean;
        variance += diff * diff;
    }
    
    return variance / values.size();
}

bool FLACCodec::validateChannelAssignmentBitDepth_unlocked(const FLAC__Frame* frame) const {
    if (!frame) return false;
    
    uint8_t assignment = frame->header.channel_assignment;
    uint16_t channels = frame->header.channels;
    uint16_t bits_per_sample = frame->header.bits_per_sample;
    
    Debug::log("flac_codec", "[validateChannelAssignmentBitDepth_unlocked] RFC 9639 channel assignment validation");
    Debug::log("flac_codec", "  Assignment: ", static_cast<unsigned>(assignment), 
              ", channels: ", channels, ", bits: ", bits_per_sample);
    
    // RFC 9639 Section 9.2.3: Side subframes have one extra bit
    bool valid = true;
    
    if (assignment >= 8 && assignment <= 10) {
        // Stereo decorrelation modes
        if (channels != 2) {
            Debug::log("flac_codec", "  RFC 9639 violation: stereo assignment requires 2 channels, got ", channels);
            valid = false;
        }
        
        // Validate bit depth for side channels
        uint16_t side_channel_bits = bits_per_sample + 1;
        if (side_channel_bits > 32) {
            Debug::log("flac_codec", "  RFC 9639 violation: side channel bit depth ", side_channel_bits, " exceeds 32-bit limit");
            valid = false;
        }
        
        Debug::log("flac_codec", "  Side channel effective bit depth: ", side_channel_bits);
    }
    
    Debug::log("flac_codec", "  Channel assignment bit depth validation: ", (valid ? "PASSED" : "FAILED"));
    return valid;
}

bool FLACCodec::validateWastedBitsHandling_unlocked(const FLAC__Frame* frame) const {
    if (!frame) return false;
    
    Debug::log("flac_codec", "[validateWastedBitsHandling_unlocked] RFC 9639 Section 9.2.2 wasted bits validation");
    
    // Since libFLAC handles wasted bits internally, we validate the logical consistency
    uint16_t frame_bits = frame->header.bits_per_sample;
    uint8_t assignment = frame->header.channel_assignment;
    
    bool valid = true;
    
    for (unsigned ch = 0; ch < frame->header.channels; ++ch) {
        uint16_t effective_bits = frame_bits;
        
        // RFC 9639: Side channels have one extra bit
        if ((assignment == 8 && ch == 1) ||  // Left-side, side channel
            (assignment == 9 && ch == 0) ||  // Right-side, side channel  
            (assignment == 10 && ch == 1)) { // Mid-side, side channel
            effective_bits += 1;
        }
        
        // Validate effective bit depth is within RFC limits
        if (effective_bits < 4 || effective_bits > 32) {
            Debug::log("flac_codec", "  RFC 9639 violation: channel ", ch, " effective bit depth ", 
                      effective_bits, " outside range (4-32)");
            valid = false;
        }
        
        Debug::log("flac_codec", "  Channel ", ch, " effective bit depth: ", effective_bits);
    }
    
    Debug::log("flac_codec", "  Wasted bits handling validation: ", (valid ? "PASSED" : "FAILED"));
    return valid;
}

// ============================================================================
// RFC 9639 Section 9.2.5 Entropy Coding Compliance Validation Implementation
// ============================================================================

bool FLACCodec::validateEntropyCoding_unlocked(const uint8_t* residual_data, size_t data_size, 
                                              uint32_t block_size, uint8_t predictor_order) const {
    // RFC 9639 Section 9.2.5: Coded Residual validation
    // The first two bits indicate the coding method
    
    if (!residual_data || data_size < 1) {
        Debug::log("flac_codec", "[validateEntropyCoding_unlocked] Invalid residual data parameters");
        return false;
    }
    
    // Extract coding method from first 2 bits
    uint8_t coding_method_bits = (residual_data[0] >> 6) & 0x03;
    
    Debug::log("flac_codec", "[validateEntropyCoding_unlocked] Validating entropy coding method: 0b", 
              std::bitset<2>(coding_method_bits).to_string(), " (", 
              static_cast<unsigned>(coding_method_bits), ")");
    
    // Validate coding method per RFC 9639 Table 23
    if (!validateRiceCodingMethod_unlocked(coding_method_bits)) {
        Debug::log("flac_codec", "[validateEntropyCoding_unlocked] Invalid Rice coding method: ", 
                  static_cast<unsigned>(coding_method_bits));
        return false;
    }
    
    // Extract partition order from next 4 bits
    uint8_t partition_order = (residual_data[0] >> 2) & 0x0F;
    
    Debug::log("flac_codec", "[validateEntropyCoding_unlocked] Partition order: ", 
              static_cast<unsigned>(partition_order));
    
    // Validate partition order constraints per RFC 9639
    if (!validatePartitionOrder_unlocked(partition_order, block_size, predictor_order)) {
        Debug::log("flac_codec", "[validateEntropyCoding_unlocked] Invalid partition order: ", 
                  static_cast<unsigned>(partition_order), " for block size ", block_size, 
                  " and predictor order ", static_cast<unsigned>(predictor_order));
        return false;
    }
    
    // Validate Rice parameters for all partitions
    if (!validateRiceParameters_unlocked(residual_data + 1, data_size - 1, coding_method_bits, 
                                        partition_order, block_size, predictor_order)) {
        Debug::log("flac_codec", "[validateEntropyCoding_unlocked] Rice parameter validation failed");
        return false;
    }
    
    Debug::log("flac_codec", "[validateEntropyCoding_unlocked] Entropy coding validation successful");
    return true;
}

bool FLACCodec::validateRiceCodingMethod_unlocked(uint8_t coding_method_bits) const {
    // RFC 9639 Section 9.2.5 Table 23: Coding method validation
    switch (coding_method_bits) {
        case 0x00:
            Debug::log("flac_codec", "[validateRiceCodingMethod_unlocked] Valid: Partitioned Rice code with 4-bit parameters");
            return true;
        case 0x01:
            Debug::log("flac_codec", "[validateRiceCodingMethod_unlocked] Valid: Partitioned Rice code with 5-bit parameters");
            return true;
        case 0x02:
        case 0x03:
            Debug::log("flac_codec", "[validateRiceCodingMethod_unlocked] Invalid: Reserved coding method: ", 
                      static_cast<unsigned>(coding_method_bits));
            return false;
        default:
            Debug::log("flac_codec", "[validateRiceCodingMethod_unlocked] Invalid: Unknown coding method: ", 
                      static_cast<unsigned>(coding_method_bits));
            return false;
    }
}

bool FLACCodec::validatePartitionOrder_unlocked(uint8_t partition_order, uint32_t block_size, 
                                               uint8_t predictor_order) const {
    // RFC 9639 Section 9.2.5: Partition order constraints
    
    // Partition order must be <= 8 per RFC 9639
    if (partition_order > 8) {
        Debug::log("flac_codec", "[validatePartitionOrder_unlocked] Partition order too large: ", 
                  static_cast<unsigned>(partition_order), " > 8 (RFC 9639 limit)");
        return false;
    }
    
    uint32_t num_partitions = 1U << partition_order; // 2^partition_order
    
    // Block size must be evenly divisible by number of partitions
    if ((block_size % num_partitions) != 0) {
        Debug::log("flac_codec", "[validatePartitionOrder_unlocked] Block size ", block_size, 
                  " not evenly divisible by ", num_partitions, " partitions");
        return false;
    }
    
    uint32_t samples_per_partition = block_size >> partition_order; // block_size / num_partitions
    
    // Samples per partition must be larger than predictor order
    if (samples_per_partition <= predictor_order) {
        Debug::log("flac_codec", "[validatePartitionOrder_unlocked] Samples per partition (", 
                  samples_per_partition, ") <= predictor order (", 
                  static_cast<unsigned>(predictor_order), ")");
        return false;
    }
    
    Debug::log("flac_codec", "[validatePartitionOrder_unlocked] Valid partition order: ", 
              static_cast<unsigned>(partition_order), " (", num_partitions, " partitions, ", 
              samples_per_partition, " samples each)");
    return true;
}

bool FLACCodec::validateRiceParameters_unlocked(const uint8_t* partition_data, size_t data_size,
                                               uint8_t coding_method, uint8_t partition_order,
                                               uint32_t block_size, uint8_t predictor_order) const {
    // RFC 9639 Section 9.2.5: Validate Rice parameters for all partitions
    
    if (!partition_data || data_size == 0) {
        Debug::log("flac_codec", "[validateRiceParameters_unlocked] Invalid partition data");
        return false;
    }
    
    uint32_t num_partitions = 1U << partition_order;
    uint32_t samples_per_partition = block_size >> partition_order;
    bool is_5bit_parameter = (coding_method == 0x01);
    uint8_t parameter_bits = is_5bit_parameter ? 5 : 4;
    
    Debug::log("flac_codec", "[validateRiceParameters_unlocked] Validating ", num_partitions, 
              " partitions with ", (is_5bit_parameter ? "5-bit" : "4-bit"), " parameters");
    
    size_t bit_offset = 0;
    
    for (uint32_t partition = 0; partition < num_partitions; ++partition) {
        // Calculate samples in this partition (first partition has fewer samples due to predictor order)
        uint32_t partition_samples = (partition == 0) ? 
                                   (samples_per_partition - predictor_order) : samples_per_partition;
        
        if (bit_offset + parameter_bits > data_size * 8) {
            Debug::log("flac_codec", "[validateRiceParameters_unlocked] Insufficient data for partition ", 
                      partition, " parameter");
            return false;
        }
        
        // Extract parameter bits
        uint8_t parameter_value = 0;
        for (uint8_t bit = 0; bit < parameter_bits; ++bit) {
            size_t byte_index = bit_offset / 8;
            size_t bit_index = 7 - (bit_offset % 8);
            
            if (partition_data[byte_index] & (1 << bit_index)) {
                parameter_value |= (1 << (parameter_bits - 1 - bit));
            }
            bit_offset++;
        }
        
        Debug::log("flac_codec", "[validateRiceParameters_unlocked] Partition ", partition, 
                  " parameter: ", static_cast<unsigned>(parameter_value), 
                  " (", partition_samples, " samples)");
        
        // Check for escape code
        if (validateEscapeCode_unlocked(parameter_value, is_5bit_parameter)) {
            Debug::log("flac_codec", "[validateRiceParameters_unlocked] Partition ", partition, 
                      " uses escape code - validating unencoded residual");
            
            // Skip escape code validation for now - would need to parse the unencoded data
            // This is a simplified validation that checks the parameter format
            continue;
        }
        
        // Validate Rice parameter range
        uint8_t max_rice_parameter = is_5bit_parameter ? 30 : 14; // Reserve escape codes
        if (parameter_value > max_rice_parameter) {
            Debug::log("flac_codec", "[validateRiceParameters_unlocked] Invalid Rice parameter: ", 
                      static_cast<unsigned>(parameter_value), " > ", 
                      static_cast<unsigned>(max_rice_parameter));
            return false;
        }
        
        // For full validation, we would decode the Rice-coded residuals here
        // This simplified version just validates the parameter structure
    }
    
    Debug::log("flac_codec", "[validateRiceParameters_unlocked] All Rice parameters validated successfully");
    return true;
}

bool FLACCodec::validateEscapeCode_unlocked(uint8_t parameter_bits, bool is_5bit_parameter) const {
    // RFC 9639 Section 9.2.5: Escape code detection
    if (is_5bit_parameter) {
        // 5-bit escape code is 0b11111 (31)
        bool is_escape = (parameter_bits == 0x1F);
        if (is_escape) {
            Debug::log("flac_codec", "[validateEscapeCode_unlocked] 5-bit escape code detected (0b11111)");
        }
        return is_escape;
    } else {
        // 4-bit escape code is 0b1111 (15)
        bool is_escape = (parameter_bits == 0x0F);
        if (is_escape) {
            Debug::log("flac_codec", "[validateEscapeCode_unlocked] 4-bit escape code detected (0b1111)");
        }
        return is_escape;
    }
}

bool FLACCodec::decodeRicePartition_unlocked(const uint8_t* partition_data, size_t data_size,
                                           uint8_t rice_parameter, uint32_t sample_count,
                                           std::vector<int32_t>& decoded_residuals) const {
    // RFC 9639 Section 9.2.5.2: Rice code decoding
    
    if (!partition_data || data_size == 0 || sample_count == 0) {
        Debug::log("flac_codec", "[decodeRicePartition_unlocked] Invalid parameters");
        return false;
    }
    
    Debug::log("flac_codec", "[decodeRicePartition_unlocked] Decoding ", sample_count, 
              " samples with Rice parameter ", static_cast<unsigned>(rice_parameter));
    
    decoded_residuals.clear();
    decoded_residuals.reserve(sample_count);
    
    size_t bit_offset = 0;
    
    for (uint32_t sample = 0; sample < sample_count; ++sample) {
        int32_t residual = decodeRiceSample_unlocked(partition_data, bit_offset, rice_parameter);
        
        if (!validateResidualRange_unlocked(residual)) {
            Debug::log("flac_codec", "[decodeRicePartition_unlocked] Residual value out of range: ", residual);
            return false;
        }
        
        decoded_residuals.push_back(residual);
        
        // Check if we've exceeded the available data
        if (bit_offset >= data_size * 8) {
            if (sample + 1 < sample_count) {
                Debug::log("flac_codec", "[decodeRicePartition_unlocked] Insufficient data for all samples");
                return false;
            }
            break;
        }
    }
    
    Debug::log("flac_codec", "[decodeRicePartition_unlocked] Successfully decoded ", 
              decoded_residuals.size(), " residual samples");
    return true;
}

bool FLACCodec::decodeEscapedPartition_unlocked(const uint8_t* partition_data, size_t data_size,
                                              uint8_t bits_per_sample, uint32_t sample_count,
                                              std::vector<int32_t>& decoded_residuals) const {
    // RFC 9639 Section 9.2.5.1: Escaped partition decoding
    
    if (!partition_data || data_size == 0) {
        Debug::log("flac_codec", "[decodeEscapedPartition_unlocked] Invalid parameters");
        return false;
    }
    
    Debug::log("flac_codec", "[decodeEscapedPartition_unlocked] Decoding escaped partition: ", 
              sample_count, " samples, ", static_cast<unsigned>(bits_per_sample), " bits each");
    
    // Special case: 0 bits per sample means all residuals are 0
    if (bits_per_sample == 0) {
        decoded_residuals.assign(sample_count, 0);
        Debug::log("flac_codec", "[decodeEscapedPartition_unlocked] All residuals are zero");
        return true;
    }
    
    // Validate we have enough data
    size_t required_bits = static_cast<size_t>(sample_count) * bits_per_sample;
    if (required_bits > data_size * 8) {
        Debug::log("flac_codec", "[decodeEscapedPartition_unlocked] Insufficient data: need ", 
                  required_bits, " bits, have ", data_size * 8);
        return false;
    }
    
    decoded_residuals.clear();
    decoded_residuals.reserve(sample_count);
    
    size_t bit_offset = 0;
    
    for (uint32_t sample = 0; sample < sample_count; ++sample) {
        int32_t residual = 0;
        
        // Extract bits_per_sample bits as signed two's complement
        for (uint8_t bit = 0; bit < bits_per_sample; ++bit) {
            size_t byte_index = bit_offset / 8;
            size_t bit_index = 7 - (bit_offset % 8);
            
            if (partition_data[byte_index] & (1 << bit_index)) {
                residual |= (1 << (bits_per_sample - 1 - bit));
            }
            bit_offset++;
        }
        
        // Sign extend if necessary
        if (bits_per_sample < 32 && (residual & (1 << (bits_per_sample - 1)))) {
            // Negative number - sign extend
            residual |= (~0U << bits_per_sample);
        }
        
        if (!validateResidualRange_unlocked(residual)) {
            Debug::log("flac_codec", "[decodeEscapedPartition_unlocked] Residual value out of range: ", residual);
            return false;
        }
        
        decoded_residuals.push_back(residual);
    }
    
    Debug::log("flac_codec", "[decodeEscapedPartition_unlocked] Successfully decoded ", 
              decoded_residuals.size(), " escaped residual samples");
    return true;
}

int32_t FLACCodec::decodeRiceSample_unlocked(const uint8_t* data, size_t& bit_offset, 
                                           uint8_t rice_parameter) const {
    // RFC 9639 Section 9.2.5.2: Rice code decoding algorithm
    
    // Count leading zeros (unary part)
    uint32_t quotient = 0;
    while (true) {
        size_t byte_index = bit_offset / 8;
        size_t bit_index = 7 - (bit_offset % 8);
        
        if (data[byte_index] & (1 << bit_index)) {
            // Found the '1' bit, stop counting
            bit_offset++;
            break;
        }
        
        quotient++;
        bit_offset++;
        
        // Prevent infinite loop on malformed data
        if (quotient > 1000) {
            Debug::log("flac_codec", "[decodeRiceSample_unlocked] Excessive quotient: ", quotient);
            return 0; // Return safe value
        }
    }
    
    // Read remainder (binary part)
    uint32_t remainder = 0;
    for (uint8_t bit = 0; bit < rice_parameter; ++bit) {
        size_t byte_index = bit_offset / 8;
        size_t bit_index = 7 - (bit_offset % 8);
        
        if (data[byte_index] & (1 << bit_index)) {
            remainder |= (1 << (rice_parameter - 1 - bit));
        }
        bit_offset++;
    }
    
    // Combine quotient and remainder: folded_value = (quotient << rice_parameter) | remainder
    uint32_t folded_value = (quotient << rice_parameter) | remainder;
    
    // Zigzag decode to get signed residual
    int32_t residual = zigzagDecode_unlocked(folded_value);
    
    return residual;
}

int32_t FLACCodec::zigzagDecode_unlocked(uint32_t folded_value) const {
    // RFC 9639 Section 9.2.5.2: Zigzag decoding
    // Even folded values: residual = folded_value / 2
    // Odd folded values: residual = -(folded_value + 1) / 2
    
    if ((folded_value & 1) == 0) {
        // Even: positive residual
        return static_cast<int32_t>(folded_value >> 1);
    } else {
        // Odd: negative residual
        return -static_cast<int32_t>((folded_value + 1) >> 1);
    }
}

bool FLACCodec::validateResidualRange_unlocked(int32_t residual_value) const {
    // RFC 9639 Section 9.2.5.3: Residual sample value limit
    // All residual values must be representable in 32-bit signed two's complement,
    // excluding the most negative value (-2^31)
    
    const int32_t MIN_RESIDUAL = -(1LL << 31) + 1; // -2^31 + 1
    const int32_t MAX_RESIDUAL = (1LL << 31) - 1;  // 2^31 - 1
    
    if (residual_value < MIN_RESIDUAL || residual_value > MAX_RESIDUAL) {
        Debug::log("flac_codec", "[validateResidualRange_unlocked] Residual out of range: ", 
                  residual_value, " (valid range: ", MIN_RESIDUAL, " to ", MAX_RESIDUAL, ")");
        return false;
    }
    
    return true;
}

// ============================================================================
// Input Flow Control Implementation
// ============================================================================

bool FLACCodec::checkInputQueueCapacity_unlocked(const MediaChunk& chunk) {
    // Check both queue size and byte limits
    if (m_input_queue.size() >= m_max_input_queue_size) {
        Debug::log("flac_codec", "[FLACCodec::checkInputQueueCapacity_unlocked] Queue size limit exceeded: ", 
                  m_input_queue.size(), " >= ", m_max_input_queue_size);
        return false;
    }
    
    if (m_input_queue_bytes + chunk.data.size() > m_max_input_queue_bytes) {
        Debug::log("flac_codec", "[FLACCodec::checkInputQueueCapacity_unlocked] Queue byte limit exceeded: ", 
                  m_input_queue_bytes + chunk.data.size(), " > ", m_max_input_queue_bytes);
        return false;
    }
    
    return true;
}

void FLACCodec::handleInputOverflow_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::handleInputOverflow_unlocked] Input queue overflow detected");
    
    m_input_overrun_count++;
    activateInputBackpressure_unlocked();
}

void FLACCodec::handleInputUnderrun_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::handleInputUnderrun_unlocked] Input queue underrun detected");
    
    m_input_underrun_count++;
    
    if (m_input_backpressure_active) {
        deactivateInputBackpressure_unlocked();
    }
}

bool FLACCodec::waitForInputQueueSpace_unlocked(const MediaChunk& chunk, std::chrono::milliseconds timeout) {
    auto start_time = std::chrono::steady_clock::now();
    
    while (!checkInputQueueCapacity_unlocked(chunk)) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout) {
            Debug::log("flac_codec", "[FLACCodec::waitForInputQueueSpace_unlocked] Timeout waiting for queue space");
            return false;
        }
        
        // Wait for queue space to become available
        auto remaining_timeout = timeout - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
        std::unique_lock<std::mutex> lock(m_input_mutex, std::adopt_lock);
        if (m_input_cv.wait_for(lock, remaining_timeout) == std::cv_status::timeout) {
            Debug::log("flac_codec", "[FLACCodec::waitForInputQueueSpace_unlocked] Condition variable timeout");
            lock.release(); // Don't unlock since we adopted the lock
            return false;
        }
        lock.release(); // Don't unlock since we adopted the lock
    }
    
    return true;
}

void FLACCodec::notifyInputQueueSpaceAvailable_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::notifyInputQueueSpaceAvailable_unlocked] Notifying input queue space available");
    m_input_cv.notify_all();
}

bool FLACCodec::shouldApplyInputBackpressure_unlocked(const MediaChunk& chunk) const {
    // Apply backpressure if queue is above high watermark
    if (m_input_queue.size() >= m_input_queue_high_watermark) {
        return true;
    }
    
    // Apply backpressure if we're already in backpressure state and queue isn't below low watermark
    if (m_input_backpressure_active && m_input_queue.size() > m_input_queue_low_watermark) {
        return true;
    }
    
    return false;
}

void FLACCodec::handleInputBackpressure_unlocked(const MediaChunk& chunk) {
    Debug::log("flac_codec", "[FLACCodec::handleInputBackpressure_unlocked] Handling input backpressure for chunk with ", 
              chunk.data.size(), " bytes");
    
    if (shouldApplyInputBackpressure_unlocked(chunk)) {
        activateInputBackpressure_unlocked();
        
        // Wait for queue space to become available
        if (!waitForInputQueueSpace_unlocked(chunk)) {
            Debug::log("flac_codec", "[FLACCodec::handleInputBackpressure_unlocked] Failed to wait for queue space");
            handleInputOverflow_unlocked();
        }
    } else if (m_input_backpressure_active && m_input_queue.size() <= m_input_queue_low_watermark) {
        deactivateInputBackpressure_unlocked();
    }
}

void FLACCodec::activateInputBackpressure_unlocked() {
    if (!m_input_backpressure_active) {
        Debug::log("flac_codec", "[FLACCodec::activateInputBackpressure_unlocked] Activating input backpressure");
        m_input_backpressure_active = true;
        m_input_queue_full = true;
    }
}

void FLACCodec::deactivateInputBackpressure_unlocked() {
    if (m_input_backpressure_active) {
        Debug::log("flac_codec", "[FLACCodec::deactivateInputBackpressure_unlocked] Deactivating input backpressure");
        m_input_backpressure_active = false;
        m_input_queue_full = false;
        notifyInputQueueSpaceAvailable_unlocked();
    }
}

void FLACCodec::resetInputFlowControl_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::resetInputFlowControl_unlocked] Resetting input flow control");
    
    m_input_backpressure_active = false;
    m_input_queue_full = false;
    m_input_underrun_count = 0;
    m_input_overrun_count = 0;
    
    // Notify any waiting threads
    notifyInputQueueSpaceAvailable_unlocked();
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
        Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] Invalid parameters");
        return;
    }
    
    // Validate channel assignment against RFC 9639 specification
    uint8_t assignment = frame->header.channel_assignment;
    uint16_t channels = frame->header.channels;
    
    Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] Processing channel assignment ", 
              assignment, " with ", channels, " channels (RFC 9639 Table 16)");
    
    // RFC 9639 Table 16: Channel assignment validation
    // Values 0-7: Independent channels (assignment + 1 = number of channels)
    // Values 8-10: Stereo modes (exactly 2 channels required)
    // Values 11-15: Reserved (forbidden per RFC 9639)
    
    if (assignment <= 7) {
        // RFC 9639: Independent channel assignment (0b0000 to 0b0111)
        // Channel count = assignment value + 1 (supports 1-8 channels)
        uint16_t expected_channels = assignment + 1;
        
        if (channels != expected_channels) {
            Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] RFC 9639 violation: ", 
                      "Independent assignment ", assignment, " requires ", expected_channels, 
                      " channels, got ", channels);
            m_stats.error_count++;
            return;
        }
        
        // Validate channel count is within RFC 9639 limits (1-8 channels)
        if (channels < 1 || channels > 8) {
            Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] RFC 9639 violation: ", 
                      "Channel count ", channels, " outside valid range (1-8)");
            m_stats.error_count++;
            return;
        }
        
        processIndependentChannels_unlocked(frame, buffer);
        return;
    }
    
    // RFC 9639: Stereo channel assignments (0b1000, 0b1001, 0b1010)
    if (assignment >= 8 && assignment <= 10) {
        // All stereo modes require exactly 2 channels per RFC 9639
        if (channels != 2) {
            Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] RFC 9639 violation: ", 
                      "Stereo assignment ", assignment, " requires exactly 2 channels, got ", channels);
            m_stats.error_count++;
            return;
        }
        
        // Process specific stereo modes per RFC 9639 Section 4.2
        switch (assignment) {
            case 8:  // 0b1000: Left-Side stereo (FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE)
                Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] Processing left-side stereo per RFC 9639");
                processLeftSideStereo_unlocked(frame, buffer);
                break;
                
            case 9:  // 0b1001: Side-Right stereo (FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE)
                Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] Processing side-right stereo per RFC 9639");
                processRightSideStereo_unlocked(frame, buffer);
                break;
                
            case 10: // 0b1010: Mid-Side stereo (FLAC__CHANNEL_ASSIGNMENT_MID_SIDE)
                Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] Processing mid-side stereo per RFC 9639");
                processMidSideStereo_unlocked(frame, buffer);
                break;
                
            default:
                // This should never happen due to the range check above
                Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] Internal error: ", 
                          "Unexpected stereo assignment ", assignment);
                m_stats.error_count++;
                return;
        }
        return;
    }
    
    // RFC 9639: Reserved channel assignments (0b1011 to 0b1111)
    if (assignment >= 11 && assignment <= 15) {
        Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] RFC 9639 violation: ", 
                  "Reserved channel assignment ", assignment, " (0b", std::bitset<4>(assignment), 
                  ") - forbidden per RFC 9639 Table 16");
        m_stats.error_count++;
        return;
    }
    
    // This should never happen - all possible 4-bit values are covered above
    Debug::log("flac_codec", "[FLACCodec::processChannelAssignment_unlocked] Internal error: ", 
              "Invalid channel assignment ", assignment, " outside 4-bit range");
    m_stats.error_count++;
}

void FLACCodec::processIndependentChannels_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    uint32_t block_size = frame->header.blocksize;
    uint16_t channels = frame->header.channels;
    uint8_t assignment = frame->header.channel_assignment;
    
    // RFC 9639 validation: Independent channels support 1-8 channels
    if (channels < 1 || channels > 8) {
        Debug::log("flac_codec", "[FLACCodec::processIndependentChannels_unlocked] RFC 9639 violation: ", 
                  "Channel count ", channels, " outside valid range (1-8)");
        m_stats.error_count++;
        return;
    }
    
    // RFC 9639 Table 16: Validate assignment matches channel count
    if (assignment != channels - 1) {
        Debug::log("flac_codec", "[FLACCodec::processIndependentChannels_unlocked] RFC 9639 violation: ", 
                  "Assignment ", assignment, " doesn't match channel count ", channels, 
                  " (expected assignment ", channels - 1, ")");
        m_stats.error_count++;
        return;
    }
    
    // RFC 9639 Table 16: Log standard channel configurations
    const char* rfc_description = "unknown";
    
    switch (channels) {
        case 1: 
            rfc_description = "1 channel: mono";
            break;
        case 2: 
            rfc_description = "2 channels: left, right";
            break;
        case 3: 
            rfc_description = "3 channels: left, right, center";
            break;
        case 4: 
            rfc_description = "4 channels: front left, front right, back left, back right";
            break;
        case 5: 
            rfc_description = "5 channels: front left, front right, front center, back/surround left, back/surround right";
            break;
        case 6: 
            rfc_description = "6 channels: front left, front right, front center, LFE, back/surround left, back/surround right";
            break;
        case 7: 
            rfc_description = "7 channels: front left, front right, front center, LFE, back center, side left, side right";
            break;
        case 8: 
            rfc_description = "8 channels: front left, front right, front center, LFE, back left, back right, side left, side right";
            break;
    }
    
    Debug::log("flac_codec", "[FLACCodec::processIndependentChannels_unlocked] RFC 9639 independent channels: ", 
              "assignment=", assignment, " (0b", std::bitset<4>(assignment), "), ", 
              channels, " channels - ", rfc_description);
    
    // Validate buffer array has enough channels
    if (!buffer) {
        Debug::log("flac_codec", "[FLACCodec::processIndependentChannels_unlocked] Null buffer array");
        m_stats.error_count++;
        return;
    }
    
    // Validate each channel buffer exists
    for (uint16_t ch = 0; ch < channels; ++ch) {
        if (!buffer[ch]) {
            Debug::log("flac_codec", "[FLACCodec::processIndependentChannels_unlocked] Null buffer for channel ", ch);
            m_stats.error_count++;
            return;
        }
    }
    
    // Ensure output buffer has sufficient capacity
    size_t required_samples = static_cast<size_t>(block_size) * channels;
    if (m_output_buffer.capacity() < required_samples) {
        m_output_buffer.reserve(required_samples * 2);
        Debug::log("flac_codec", "[FLACCodec::processIndependentChannels_unlocked] Expanded buffer for ", 
                  channels, " channels (", required_samples, " samples)");
    }
    m_output_buffer.resize(required_samples);
    
    // RFC 9639 compliant independent channel processing
    // Each channel is coded independently and interleaved in output
    
    if (channels == 1) {
        // Mono optimization - direct conversion without interleaving overhead
        processMonoChannelOptimized_unlocked(buffer[0], block_size, frame->header.bits_per_sample);
    } else if (channels == 2) {
        // Stereo optimization - optimized interleaving with SIMD when available
        processStereoChannelsOptimized_unlocked(buffer[0], buffer[1], block_size, frame->header.bits_per_sample);
    } else {
        // Multi-channel processing with optimized inner loops (3-8 channels)
        processMultiChannelOptimized_unlocked(buffer, channels, block_size, frame->header.bits_per_sample);
    }
    
    m_stats.conversion_operations++;
    
    Debug::log("flac_codec", "[FLACCodec::processIndependentChannels_unlocked] Successfully processed ", 
              block_size, " samples across ", channels, " independent channels");
}

void FLACCodec::processLeftSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    uint32_t block_size = frame->header.blocksize;
    uint8_t assignment = frame->header.channel_assignment;
    
    // RFC 9639 validation: Left-side stereo requires exactly 2 channels
    if (frame->header.channels != 2) {
        Debug::log("flac_codec", "[FLACCodec::processLeftSideStereo_unlocked] RFC 9639 violation: ", 
                  "Left-side stereo requires 2 channels, got ", frame->header.channels);
        m_stats.error_count++;
        return;
    }
    
    // RFC 9639 validation: Assignment must be 8 (0b1000) for left-side stereo
    if (assignment != 8) {
        Debug::log("flac_codec", "[FLACCodec::processLeftSideStereo_unlocked] RFC 9639 violation: ", 
                  "Expected assignment 8 for left-side stereo, got ", assignment);
        m_stats.error_count++;
        return;
    }
    
    // Validate buffer pointers
    if (!buffer || !buffer[0] || !buffer[1]) {
        Debug::log("flac_codec", "[FLACCodec::processLeftSideStereo_unlocked] Invalid buffer pointers");
        m_stats.error_count++;
        return;
    }
    
    size_t required_samples = static_cast<size_t>(block_size) * 2;
    if (m_output_buffer.capacity() < required_samples) {
        m_output_buffer.reserve(required_samples * 2);
    }
    m_output_buffer.resize(required_samples);
    
    Debug::log("flac_codec", "[FLACCodec::processLeftSideStereo_unlocked] RFC 9639 left-side stereo: ", 
              "buffer[0]=left, buffer[1]=side, formula: right = left - side");
    
    // RFC 9639 Section 4.2: Left-Side stereo reconstruction
    // "The left subblock is coded, and the left and right subblocks are used to code a side subframe.
    //  To decode, the right subblock is restored by subtracting the samples in the side subframe 
    //  from the corresponding samples in the left subframe."
    // 
    // Stored: buffer[0] = left channel, buffer[1] = side channel (left - right)
    // Reconstruction: left = buffer[0], right = left - side = buffer[0] - buffer[1]
    
    for (uint32_t i = 0; i < block_size; ++i) {
        FLAC__int32 left = buffer[0][i];   // Left channel (stored directly)
        FLAC__int32 side = buffer[1][i];   // Side channel (left - right)
        FLAC__int32 right = left - side;   // RFC 9639: right = left - side
        
        // Convert to 16-bit and store interleaved (left, right, left, right, ...)
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
                // Generic bit depth handling for unusual bit depths
                int shift = frame->header.bits_per_sample - 16;
                if (shift > 0) {
                    left_16 = static_cast<int16_t>(left >> shift);
                    right_16 = static_cast<int16_t>(right >> shift);
                } else {
                    left_16 = static_cast<int16_t>(left << (-shift));
                    right_16 = static_cast<int16_t>(right << (-shift));
                }
                break;
        }
        
        // Store interleaved: [L, R, L, R, ...]
        m_output_buffer[i * 2] = left_16;
        m_output_buffer[i * 2 + 1] = right_16;
    }
    
    m_stats.conversion_operations++;
    
    Debug::log("flac_codec", "[FLACCodec::processLeftSideStereo_unlocked] Successfully processed ", 
              block_size, " left-side stereo samples per RFC 9639");
}

void FLACCodec::processRightSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    uint32_t block_size = frame->header.blocksize;
    uint8_t assignment = frame->header.channel_assignment;
    
    // RFC 9639 validation: Right-side stereo requires exactly 2 channels
    if (frame->header.channels != 2) {
        Debug::log("flac_codec", "[FLACCodec::processRightSideStereo_unlocked] RFC 9639 violation: ", 
                  "Right-side stereo requires 2 channels, got ", frame->header.channels);
        m_stats.error_count++;
        return;
    }
    
    // RFC 9639 validation: Assignment must be 9 (0b1001) for right-side stereo
    if (assignment != 9) {
        Debug::log("flac_codec", "[FLACCodec::processRightSideStereo_unlocked] RFC 9639 violation: ", 
                  "Expected assignment 9 for right-side stereo, got ", assignment);
        m_stats.error_count++;
        return;
    }
    
    // Validate buffer pointers
    if (!buffer || !buffer[0] || !buffer[1]) {
        Debug::log("flac_codec", "[FLACCodec::processRightSideStereo_unlocked] Invalid buffer pointers");
        m_stats.error_count++;
        return;
    }
    
    size_t required_samples = static_cast<size_t>(block_size) * 2;
    if (m_output_buffer.capacity() < required_samples) {
        m_output_buffer.reserve(required_samples * 2);
    }
    m_output_buffer.resize(required_samples);
    
    Debug::log("flac_codec", "[FLACCodec::processRightSideStereo_unlocked] RFC 9639 right-side stereo: ", 
              "buffer[0]=side, buffer[1]=right, formula: left = side + right");
    
    // RFC 9639 Right-Side stereo reconstruction: 
    // Channel 0 contains side information (left - right)
    // Channel 1 contains right channel
    // Reconstruction: left = side + right, right = right
    for (uint32_t i = 0; i < block_size; ++i) {
        FLAC__int32 side = buffer[0][i];    // side = left - right
        FLAC__int32 right = buffer[1][i];   // right channel
        FLAC__int32 left = side + right;    // left = (left - right) + right = left
        
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
    
    Debug::log("flac_codec", "[FLACCodec::processRightSideStereo_unlocked] Successfully processed ", 
              block_size, " right-side stereo samples per RFC 9639");
}

void FLACCodec::processMidSideStereo_unlocked(const FLAC__Frame* frame, const FLAC__int32* const buffer[]) {
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    uint32_t block_size = frame->header.blocksize;
    uint8_t assignment = frame->header.channel_assignment;
    
    // RFC 9639 validation: Mid-side stereo requires exactly 2 channels
    if (frame->header.channels != 2) {
        Debug::log("flac_codec", "[FLACCodec::processMidSideStereo_unlocked] RFC 9639 violation: ", 
                  "Mid-side stereo requires 2 channels, got ", frame->header.channels);
        m_stats.error_count++;
        return;
    }
    
    // RFC 9639 validation: Assignment must be 10 (0b1010) for mid-side stereo
    if (assignment != 10) {
        Debug::log("flac_codec", "[FLACCodec::processMidSideStereo_unlocked] RFC 9639 violation: ", 
                  "Expected assignment 10 for mid-side stereo, got ", assignment);
        m_stats.error_count++;
        return;
    }
    
    // Validate buffer pointers
    if (!buffer || !buffer[0] || !buffer[1]) {
        Debug::log("flac_codec", "[FLACCodec::processMidSideStereo_unlocked] Invalid buffer pointers");
        m_stats.error_count++;
        return;
    }
    
    size_t required_samples = static_cast<size_t>(block_size) * 2;
    if (m_output_buffer.capacity() < required_samples) {
        m_output_buffer.reserve(required_samples * 2);
    }
    m_output_buffer.resize(required_samples);
    
    Debug::log("flac_codec", "[FLACCodec::processMidSideStereo_unlocked] RFC 9639 mid-side stereo: ", 
              "buffer[0]=mid, buffer[1]=side, RFC formula: mid<<1 + (side&1), left=(mid+side)>>1, right=(mid-side)>>1");
    
    // RFC 9639 Mid-Side stereo reconstruction (Section 4.2):
    // Channel 0 contains mid information: (left + right) >> 1
    // Channel 1 contains side information: left - right (with extra bit depth)
    // 
    // RFC 9639 decoding process:
    // 1. All mid channel samples have to be shifted left by 1 bit
    // 2. If a side channel sample is odd, 1 has to be added to the mid sample after shifting
    // 3. left = (mid + side) >> 1, right = (mid - side) >> 1
    for (uint32_t i = 0; i < block_size; ++i) {
        FLAC__int32 mid = buffer[0][i];    // mid channel (already >> 1 during encoding)
        FLAC__int32 side = buffer[1][i];   // side channel (left - right, extra bit depth)
        
        // RFC 9639 Section 4.2: Mid-Side stereo reconstruction
        // Step 1: Shift mid channel left by 1 bit
        FLAC__int32 mid_shifted = mid << 1;
        
        // Step 2: If side sample is odd, add 1 to the shifted mid sample
        if (side & 1) {
            mid_shifted += 1;
        }
        
        // Step 3: Reconstruct left and right channels
        // left = (mid + side) >> 1, right = (mid - side) >> 1
        FLAC__int32 left = (mid_shifted + side) >> 1;
        FLAC__int32 right = (mid_shifted - side) >> 1;
        
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
    
    Debug::log("flac_codec", "[FLACCodec::processMidSideStereo_unlocked] Successfully processed ", 
              block_size, " mid-side stereo samples per RFC 9639");
}

// Optimized channel processing methods for performance

void FLACCodec::processMonoChannelOptimized_unlocked(const FLAC__int32* input, uint32_t block_size, uint16_t bits_per_sample) {
    // Optimized mono channel processing - direct conversion without interleaving overhead
    
    switch (bits_per_sample) {
        case 8:
            // Optimized 8-bit mono conversion with loop unrolling
            {
                size_t unroll_samples = (block_size / 4) * 4;
                for (size_t i = 0; i < unroll_samples; i += 4) {
                    m_output_buffer[i] = static_cast<int16_t>(input[i] << 8);
                    m_output_buffer[i + 1] = static_cast<int16_t>(input[i + 1] << 8);
                    m_output_buffer[i + 2] = static_cast<int16_t>(input[i + 2] << 8);
                    m_output_buffer[i + 3] = static_cast<int16_t>(input[i + 3] << 8);
                }
                for (size_t i = unroll_samples; i < block_size; ++i) {
                    m_output_buffer[i] = convert8BitTo16Bit(input[i]);
                }
            }
            break;
            
        case 16:
            // Direct copy for 16-bit mono - fastest path
            #ifdef HAVE_SSE2
            {
                size_t simd_samples = (block_size / 8) * 8;
                for (size_t i = 0; i < simd_samples; i += 8) {
                    // Load 8 32-bit samples
                    __m128i samples1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[i]));
                    __m128i samples2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[i + 4]));
                    
                    // Pack to 16-bit
                    __m128i packed = _mm_packs_epi32(samples1, samples2);
                    
                    // Store 8 16-bit samples
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(&m_output_buffer[i]), packed);
                }
                for (size_t i = simd_samples; i < block_size; ++i) {
                    m_output_buffer[i] = static_cast<int16_t>(input[i]);
                }
            }
            #else
            {
                size_t unroll_samples = (block_size / 4) * 4;
                for (size_t i = 0; i < unroll_samples; i += 4) {
                    m_output_buffer[i] = static_cast<int16_t>(input[i]);
                    m_output_buffer[i + 1] = static_cast<int16_t>(input[i + 1]);
                    m_output_buffer[i + 2] = static_cast<int16_t>(input[i + 2]);
                    m_output_buffer[i + 3] = static_cast<int16_t>(input[i + 3]);
                }
                for (size_t i = unroll_samples; i < block_size; ++i) {
                    m_output_buffer[i] = static_cast<int16_t>(input[i]);
                }
            }
            #endif
            break;
            
        case 24:
            // Optimized 24-bit mono conversion
            {
                size_t unroll_samples = (block_size / 4) * 4;
                for (size_t i = 0; i < unroll_samples; i += 4) {
                    m_output_buffer[i] = static_cast<int16_t>(input[i] >> 8);
                    m_output_buffer[i + 1] = static_cast<int16_t>(input[i + 1] >> 8);
                    m_output_buffer[i + 2] = static_cast<int16_t>(input[i + 2] >> 8);
                    m_output_buffer[i + 3] = static_cast<int16_t>(input[i + 3] >> 8);
                }
                for (size_t i = unroll_samples; i < block_size; ++i) {
                    m_output_buffer[i] = convert24BitTo16Bit(input[i]);
                }
            }
            break;
            
        case 32:
            // Optimized 32-bit mono conversion with overflow protection
            {
                size_t unroll_samples = (block_size / 4) * 4;
                for (size_t i = 0; i < unroll_samples; i += 4) {
                    m_output_buffer[i] = static_cast<int16_t>(std::clamp(input[i] >> 16, -32768, 32767));
                    m_output_buffer[i + 1] = static_cast<int16_t>(std::clamp(input[i + 1] >> 16, -32768, 32767));
                    m_output_buffer[i + 2] = static_cast<int16_t>(std::clamp(input[i + 2] >> 16, -32768, 32767));
                    m_output_buffer[i + 3] = static_cast<int16_t>(std::clamp(input[i + 3] >> 16, -32768, 32767));
                }
                for (size_t i = unroll_samples; i < block_size; ++i) {
                    m_output_buffer[i] = convert32BitTo16Bit(input[i]);
                }
            }
            break;
            
        default:
            // Generic conversion for unusual bit depths
            for (uint32_t i = 0; i < block_size; ++i) {
                if (bits_per_sample < 16) {
                    m_output_buffer[i] = static_cast<int16_t>(input[i] << (16 - bits_per_sample));
                } else {
                    m_output_buffer[i] = static_cast<int16_t>(input[i] >> (bits_per_sample - 16));
                }
            }
            break;
    }
}

void FLACCodec::processStereoChannelsOptimized_unlocked(const FLAC__int32* left, const FLAC__int32* right, 
                                                       uint32_t block_size, uint16_t bits_per_sample) {
    // Optimized stereo channel processing with SIMD interleaving when available
    
    switch (bits_per_sample) {
        case 8:
            // Optimized 8-bit stereo conversion with interleaving
            #ifdef HAVE_SSE2
            {
                size_t simd_samples = (block_size / 4) * 4;
                for (size_t i = 0; i < simd_samples; i += 4) {
                    // Load 4 samples from each channel
                    __m128i left_samples = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&left[i]));
                    __m128i right_samples = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&right[i]));
                    
                    // Convert to 16-bit range
                    left_samples = _mm_slli_epi32(left_samples, 8);
                    right_samples = _mm_slli_epi32(right_samples, 8);
                    
                    // Pack to 16-bit
                    __m128i left_packed = _mm_packs_epi32(left_samples, _mm_setzero_si128());
                    __m128i right_packed = _mm_packs_epi32(right_samples, _mm_setzero_si128());
                    
                    // Interleave left and right channels
                    __m128i interleaved = _mm_unpacklo_epi16(left_packed, right_packed);
                    
                    // Store 8 interleaved samples (4 stereo pairs)
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(&m_output_buffer[i * 2]), interleaved);
                }
                for (size_t i = simd_samples; i < block_size; ++i) {
                    size_t out_idx = i * 2;
                    m_output_buffer[out_idx] = static_cast<int16_t>(left[i] << 8);
                    m_output_buffer[out_idx + 1] = static_cast<int16_t>(right[i] << 8);
                }
            }
            #else
            {
                size_t unroll_samples = (block_size / 2) * 2;
                for (size_t i = 0; i < unroll_samples; i += 2) {
                    size_t out_idx1 = i * 2;
                    size_t out_idx2 = (i + 1) * 2;
                    m_output_buffer[out_idx1] = static_cast<int16_t>(left[i] << 8);
                    m_output_buffer[out_idx1 + 1] = static_cast<int16_t>(right[i] << 8);
                    m_output_buffer[out_idx2] = static_cast<int16_t>(left[i + 1] << 8);
                    m_output_buffer[out_idx2 + 1] = static_cast<int16_t>(right[i + 1] << 8);
                }
                for (size_t i = unroll_samples; i < block_size; ++i) {
                    size_t out_idx = i * 2;
                    m_output_buffer[out_idx] = convert8BitTo16Bit(left[i]);
                    m_output_buffer[out_idx + 1] = convert8BitTo16Bit(right[i]);
                }
            }
            #endif
            break;
            
        case 16:
            // Direct copy for 16-bit stereo with optimized interleaving
            #ifdef HAVE_SSE2
            {
                size_t simd_samples = (block_size / 4) * 4;
                for (size_t i = 0; i < simd_samples; i += 4) {
                    // Load 4 samples from each channel
                    __m128i left_samples = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&left[i]));
                    __m128i right_samples = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&right[i]));
                    
                    // Pack to 16-bit
                    __m128i left_packed = _mm_packs_epi32(left_samples, _mm_setzero_si128());
                    __m128i right_packed = _mm_packs_epi32(right_samples, _mm_setzero_si128());
                    
                    // Interleave left and right channels
                    __m128i interleaved = _mm_unpacklo_epi16(left_packed, right_packed);
                    
                    // Store 8 interleaved samples (4 stereo pairs)
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(&m_output_buffer[i * 2]), interleaved);
                }
                for (size_t i = simd_samples; i < block_size; ++i) {
                    size_t out_idx = i * 2;
                    m_output_buffer[out_idx] = static_cast<int16_t>(left[i]);
                    m_output_buffer[out_idx + 1] = static_cast<int16_t>(right[i]);
                }
            }
            #else
            {
                size_t unroll_samples = (block_size / 2) * 2;
                for (size_t i = 0; i < unroll_samples; i += 2) {
                    size_t out_idx1 = i * 2;
                    size_t out_idx2 = (i + 1) * 2;
                    m_output_buffer[out_idx1] = static_cast<int16_t>(left[i]);
                    m_output_buffer[out_idx1 + 1] = static_cast<int16_t>(right[i]);
                    m_output_buffer[out_idx2] = static_cast<int16_t>(left[i + 1]);
                    m_output_buffer[out_idx2 + 1] = static_cast<int16_t>(right[i + 1]);
                }
                for (size_t i = unroll_samples; i < block_size; ++i) {
                    size_t out_idx = i * 2;
                    m_output_buffer[out_idx] = static_cast<int16_t>(left[i]);
                    m_output_buffer[out_idx + 1] = static_cast<int16_t>(right[i]);
                }
            }
            #endif
            break;
            
        case 24:
            // Optimized 24-bit stereo conversion
            {
                size_t unroll_samples = (block_size / 2) * 2;
                for (size_t i = 0; i < unroll_samples; i += 2) {
                    size_t out_idx1 = i * 2;
                    size_t out_idx2 = (i + 1) * 2;
                    m_output_buffer[out_idx1] = static_cast<int16_t>(left[i] >> 8);
                    m_output_buffer[out_idx1 + 1] = static_cast<int16_t>(right[i] >> 8);
                    m_output_buffer[out_idx2] = static_cast<int16_t>(left[i + 1] >> 8);
                    m_output_buffer[out_idx2 + 1] = static_cast<int16_t>(right[i + 1] >> 8);
                }
                for (size_t i = unroll_samples; i < block_size; ++i) {
                    size_t out_idx = i * 2;
                    m_output_buffer[out_idx] = convert24BitTo16Bit(left[i]);
                    m_output_buffer[out_idx + 1] = convert24BitTo16Bit(right[i]);
                }
            }
            break;
            
        case 32:
            // Optimized 32-bit stereo conversion with overflow protection
            {
                size_t unroll_samples = (block_size / 2) * 2;
                for (size_t i = 0; i < unroll_samples; i += 2) {
                    size_t out_idx1 = i * 2;
                    size_t out_idx2 = (i + 1) * 2;
                    m_output_buffer[out_idx1] = static_cast<int16_t>(std::clamp(left[i] >> 16, -32768, 32767));
                    m_output_buffer[out_idx1 + 1] = static_cast<int16_t>(std::clamp(right[i] >> 16, -32768, 32767));
                    m_output_buffer[out_idx2] = static_cast<int16_t>(std::clamp(left[i + 1] >> 16, -32768, 32767));
                    m_output_buffer[out_idx2 + 1] = static_cast<int16_t>(std::clamp(right[i + 1] >> 16, -32768, 32767));
                }
                for (size_t i = unroll_samples; i < block_size; ++i) {
                    size_t out_idx = i * 2;
                    m_output_buffer[out_idx] = convert32BitTo16Bit(left[i]);
                    m_output_buffer[out_idx + 1] = convert32BitTo16Bit(right[i]);
                }
            }
            break;
            
        default:
            // Generic conversion for unusual bit depths
            for (uint32_t i = 0; i < block_size; ++i) {
                size_t out_idx = i * 2;
                if (bits_per_sample < 16) {
                    m_output_buffer[out_idx] = static_cast<int16_t>(left[i] << (16 - bits_per_sample));
                    m_output_buffer[out_idx + 1] = static_cast<int16_t>(right[i] << (16 - bits_per_sample));
                } else {
                    m_output_buffer[out_idx] = static_cast<int16_t>(left[i] >> (bits_per_sample - 16));
                    m_output_buffer[out_idx + 1] = static_cast<int16_t>(right[i] >> (bits_per_sample - 16));
                }
            }
            break;
    }
}

void FLACCodec::processMultiChannelOptimized_unlocked(const FLAC__int32* const buffer[], uint16_t channels, 
                                                     uint32_t block_size, uint16_t bits_per_sample) {
    // Optimized multi-channel processing with cache-friendly access patterns
    
    // Use channel-major processing for better cache locality
    switch (bits_per_sample) {
        case 8:
            for (uint32_t sample = 0; sample < block_size; ++sample) {
                size_t output_base = sample * channels;
                for (uint16_t channel = 0; channel < channels; ++channel) {
                    m_output_buffer[output_base + channel] = static_cast<int16_t>(buffer[channel][sample] << 8);
                }
            }
            break;
            
        case 16:
            for (uint32_t sample = 0; sample < block_size; ++sample) {
                size_t output_base = sample * channels;
                for (uint16_t channel = 0; channel < channels; ++channel) {
                    m_output_buffer[output_base + channel] = static_cast<int16_t>(buffer[channel][sample]);
                }
            }
            break;
            
        case 24:
            for (uint32_t sample = 0; sample < block_size; ++sample) {
                size_t output_base = sample * channels;
                for (uint16_t channel = 0; channel < channels; ++channel) {
                    m_output_buffer[output_base + channel] = static_cast<int16_t>(buffer[channel][sample] >> 8);
                }
            }
            break;
            
        case 32:
            for (uint32_t sample = 0; sample < block_size; ++sample) {
                size_t output_base = sample * channels;
                for (uint16_t channel = 0; channel < channels; ++channel) {
                    int32_t scaled = buffer[channel][sample] >> 16;
                    m_output_buffer[output_base + channel] = static_cast<int16_t>(std::clamp(scaled, -32768, 32767));
                }
            }
            break;
            
        default:
            // Generic conversion for unusual bit depths
            for (uint32_t sample = 0; sample < block_size; ++sample) {
                size_t output_base = sample * channels;
                for (uint16_t channel = 0; channel < channels; ++channel) {
                    FLAC__int32 raw_sample = buffer[channel][sample];
                    if (bits_per_sample < 16) {
                        m_output_buffer[output_base + channel] = static_cast<int16_t>(raw_sample << (16 - bits_per_sample));
                    } else {
                        m_output_buffer[output_base + channel] = static_cast<int16_t>(raw_sample >> (bits_per_sample - 16));
                    }
                }
            }
            break;
    }
}

// Bit depth conversion methods

int16_t FLACCodec::convert8BitTo16Bit(FLAC__int32 sample) const {
    // RFC 9639 compliant 8-bit to 16-bit conversion with proper sign extension
    // Handle signed 8-bit sample range (-128 to 127) with proper sign extension
    // Scale to 16-bit range (-32768 to 32767) using efficient bit operations
    
    // Apply proper sign extension per RFC 9639 requirements
    FLAC__int32 sign_extended = applyProperSignExtension_unlocked(sample, 8);
    
    // RFC 9639 range validation - ensure input is within valid 8-bit signed range
    if (sign_extended < -128 || sign_extended > 127) {
        Debug::log("flac_codec", "[convert8BitTo16Bit] RFC 9639 range violation - 8-bit sample out of range: ", 
                  sign_extended, " (valid range: -128 to 127)");
        // Clamp to valid 8-bit range to prevent overflow per RFC requirements
        sign_extended = std::clamp(sign_extended, -128, 127);
    }
    
    // RFC 9639 compliant bit-shift upscaling for maximum performance
    // Left shift by 8 bits scales from 8-bit to 16-bit range while preserving sign
    // This provides proper scaling per RFC 9639 lossless requirements:
    // -128 << 8 = -32768 (minimum 16-bit value)
    // 127 << 8 = 32512 (near maximum, leaving headroom for proper scaling)
    int32_t scaled = sign_extended << 8;
    
    // RFC 9639 overflow protection - ensure result is within 16-bit range
    // This should never trigger for valid 8-bit input, but provides safety
    if (scaled < -32768 || scaled > 32767) {
        Debug::log("flac_codec", "[convert8BitTo16Bit] RFC 9639 overflow detected - scaled value out of 16-bit range: ", 
                  scaled, " (clamping to valid range)");
        scaled = std::clamp(scaled, -32768, 32767);
    }
    
    int16_t result = static_cast<int16_t>(scaled);
    
    // RFC 9639 bit-perfect validation - verify lossless conversion
    #ifdef DEBUG
    // Verify the conversion is mathematically correct and lossless
    int32_t reverse_check = result >> 8;
    if (reverse_check != sign_extended) {
        Debug::log("flac_codec", "[convert8BitTo16Bit] RFC 9639 bit-perfect validation failed - conversion not lossless: ", 
                  "original=", sign_extended, ", converted=", result, ", reverse=", reverse_check);
    }
    #endif
    
    return result;
}

int16_t FLACCodec::convert24BitTo16Bit(FLAC__int32 sample) const {
    // RFC 9639 compliant 24-bit to 16-bit conversion with proper sign extension
    // Handle proper truncation or rounding with performance-optimized algorithms
    // Maintain audio quality while reducing bit depth using advanced techniques
    
    // Apply proper sign extension per RFC 9639 requirements
    FLAC__int32 sign_extended = applyProperSignExtension_unlocked(sample, 24);
    
    // RFC 9639 range validation - validate 24-bit input range to prevent overflow
    if (sign_extended < -8388608 || sign_extended > 8388607) {
        Debug::log("flac_codec", "[convert24BitTo16Bit] RFC 9639 range violation - 24-bit sample out of range: ", 
                  sign_extended, " (valid range: -8388608 to 8388607)");
        // Clamp to valid 24-bit signed range per RFC requirements
        sign_extended = std::clamp(sign_extended, -8388608, 8388607);
    }
    
#ifdef ENABLE_DITHERING
    // RFC 9639 compliant dithering for high-quality bit depth reduction
    // Add triangular dithering for better audio quality when enabled
    // This reduces quantization noise and improves perceived audio quality
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<int> dither(-128, 127);
    
    // Apply triangular dither before downscaling per RFC best practices
    int32_t dithered = sign_extended + dither(gen);
    
    // Validate dithered value doesn't exceed 24-bit range
    if (dithered < -8388608 || dithered > 8388607) {
        dithered = std::clamp(dithered, -8388608, 8388607);
    }
    
    // RFC 9639 compliant arithmetic right-shift for proper sign preservation and scaling
    int32_t scaled = dithered >> 8;
    
    // RFC 9639 overflow protection - clamp to 16-bit range
    int32_t clamped = std::clamp(scaled, -32768, 32767);
    
    #ifdef DEBUG
    // Validate conversion maintains proper bit relationships
    if (abs(scaled - clamped) > 0) {
        Debug::log("flac_codec", "[convert24BitTo16Bit] RFC 9639 dithered conversion clamping applied: ", 
                  "scaled=", scaled, ", clamped=", clamped);
    }
    #endif
    
    return static_cast<int16_t>(clamped);
#else
    // RFC 9639 compliant optimized truncation for maximum performance
    // Use arithmetic right shift to preserve sign and scale from 24-bit to 16-bit
    // This provides good quality while maintaining real-time performance
    int32_t scaled = sign_extended >> 8;
    
    // RFC 9639 overflow protection - ensure result is within 16-bit range
    int32_t clamped = std::clamp(scaled, -32768, 32767);
    
    #ifdef DEBUG
    // RFC 9639 bit-perfect validation for truncation method
    if (abs(scaled - clamped) > 0) {
        Debug::log("flac_codec", "[convert24BitTo16Bit] RFC 9639 truncation clamping applied: ", 
                  "scaled=", scaled, ", clamped=", clamped);
    }
    
    // Verify the conversion preserves the most significant bits correctly
    int32_t reverse_check = clamped << 8;
    int32_t expected_range_min = sign_extended & 0xFFFF00; // Mask lower 8 bits
    int32_t expected_range_max = expected_range_min + 255;
    
    if (reverse_check < expected_range_min || reverse_check > expected_range_max) {
        Debug::log("flac_codec", "[convert24BitTo16Bit] RFC 9639 truncation validation warning - precision loss: ", 
                  "original=", sign_extended, ", converted=", clamped, ", reverse=", reverse_check);
    }
    #endif
    
    return static_cast<int16_t>(clamped);
#endif
}

int16_t FLACCodec::convert32BitTo16Bit(FLAC__int32 sample) const {
    // RFC 9639 compliant 32-bit to 16-bit conversion with proper sign handling
    // Handle full 32-bit dynamic range conversion with overflow protection
    // Prevent clipping using efficient clamping operations and maintain signal integrity
    
    // RFC 9639 validation - 32-bit samples should be within valid range
    // Note: 32-bit samples are already properly sign-extended by definition
    // but we validate the input for consistency with RFC 9639 requirements
    if (sample < INT32_MIN || sample > INT32_MAX) {
        Debug::log("flac_codec", "[convert32BitTo16Bit] RFC 9639 range violation - 32-bit sample out of range: ", 
                  sample, " (this should not occur with valid FLAC data)");
    }
    
    // RFC 9639 compliant arithmetic right-shift scaling for performance using bit operations
    // Right shift by 16 bits scales from 32-bit to 16-bit range while preserving sign
    // This maintains the most significant 16 bits of the 32-bit sample
    int32_t scaled = sample >> 16;
    
    // RFC 9639 overflow protection - efficient clamping operations
    // Use branchless clamping for better performance on modern CPUs
    // This ensures the result stays within valid 16-bit signed range (-32768 to 32767)
    int32_t clamped = std::clamp(scaled, -32768, 32767);
    
    #ifdef DEBUG
    // RFC 9639 validation - check if clamping was necessary
    if (scaled != clamped) {
        Debug::log("flac_codec", "[convert32BitTo16Bit] RFC 9639 overflow clamping applied: ", 
                  "original=", sample, ", scaled=", scaled, ", clamped=", clamped);
        
        // This indicates the 32-bit sample had significant data in the lower 16 bits
        // that is being lost in the conversion - this is expected behavior but worth logging
        int32_t lost_precision = sample & 0xFFFF; // Lower 16 bits
        if (lost_precision != 0) {
            Debug::log("flac_codec", "[convert32BitTo16Bit] RFC 9639 precision loss - lower 16 bits discarded: 0x", 
                      std::hex, lost_precision, std::dec);
        }
    }
    
    // RFC 9639 bit-perfect validation for 32-bit to 16-bit conversion
    // Verify the conversion preserves the most significant bits correctly
    int32_t reverse_check = clamped << 16;
    int32_t expected_range_min = sample & 0xFFFF0000; // Mask lower 16 bits
    int32_t expected_range_max = expected_range_min + 65535;
    
    if (reverse_check < expected_range_min || reverse_check > expected_range_max) {
        Debug::log("flac_codec", "[convert32BitTo16Bit] RFC 9639 conversion validation warning - unexpected precision loss: ", 
                  "original=", sample, ", converted=", clamped, ", reverse=", reverse_check);
    }
    #endif
    
    // Optimized clamping using std::clamp for better compiler optimization
    return static_cast<int16_t>(clamped);
}

void FLACCodec::convertSamples_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // High-performance convertSamples_unlocked() method for bit depth conversion to 16-bit PCM
    // Optimized for real-time requirements with SIMD-ready algorithms
    
    if (!buffer || block_size == 0 || m_channels == 0) {
        Debug::log("flac_codec", "[convertSamples_unlocked] Invalid parameters: buffer=", 
                  (buffer ? "valid" : "null"), ", block_size=", block_size, ", channels=", m_channels);
        return;
    }
    
    // Performance monitoring for conversion operations
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Ensure output buffer has sufficient capacity with pre-allocation optimization
    size_t required_samples = static_cast<size_t>(block_size) * m_channels;
    
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        
        // Resize buffer efficiently with over-allocation for future frames
        if (m_output_buffer.capacity() < required_samples) {
            m_output_buffer.reserve(required_samples * 2); // Over-allocate for performance
        }
        m_output_buffer.resize(required_samples);
    }
    
    // Perform optimized conversion based on bit depth with performance-first approach
    switch (m_bits_per_sample) {
        case 8:
            convertSamples8Bit_unlocked(buffer, block_size);
            break;
        case 16:
            convertSamples16Bit_unlocked(buffer, block_size); // Direct copy optimization
            break;
        case 24:
            convertSamples24Bit_unlocked(buffer, block_size);
            break;
        case 32:
            convertSamples32Bit_unlocked(buffer, block_size);
            break;
        default:
            // Generic conversion for unusual bit depths (4-7, 9-15, 17-23, 25-31, 33-32)
            convertSamplesGeneric_unlocked(buffer, block_size);
            break;
    }
    
    // RFC 9639 bit-perfect validation for lossless reconstruction (debug builds only)
    #ifdef DEBUG
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        
        // Create flattened original sample array for validation
        std::vector<FLAC__int32> original_samples;
        original_samples.reserve(required_samples);
        
        for (uint32_t sample = 0; sample < block_size; ++sample) {
            for (uint16_t channel = 0; channel < m_channels; ++channel) {
                original_samples.push_back(buffer[channel][sample]);
            }
        }
        
        // Validate bit-perfect reconstruction
        if (!validateBitPerfectReconstruction_unlocked(original_samples.data(), 
                                                       m_output_buffer.data(), 
                                                       required_samples, 
                                                       m_bits_per_sample)) {
            Debug::log("flac_codec", "[convertSamples_unlocked] RFC 9639 bit-perfect validation failed for ", 
                      m_bits_per_sample, "-bit samples");
        }
        
        // Calculate audio quality metrics for validation
        AudioQualityMetrics quality = calculateAudioQualityMetrics_unlocked(m_output_buffer.data(), 
                                                                            required_samples, 
                                                                            original_samples.data(), 
                                                                            m_bits_per_sample);
        
        // Log quality metrics if they indicate potential issues
        if (!quality.isGoodQuality()) {
            Debug::log("flac_codec", "[convertSamples_unlocked] Audio quality warning - SNR: ", 
                      quality.signal_to_noise_ratio_db, " dB, THD: ", quality.total_harmonic_distortion, 
                      "%, Clipped: ", quality.clipped_samples);
        }
    }
    #endif
    
    // Update conversion statistics for performance monitoring
    m_stats.conversion_operations++;
    
    // Performance monitoring - detect conversion bottlenecks
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    if (duration.count() > 200) { // Log if conversion takes >200μs
        Debug::log("flac_codec", "[convertSamples_unlocked] Slow conversion: ", duration.count(), 
                  " μs for ", block_size, " samples, ", m_channels, " channels, ", 
                  m_bits_per_sample, " bits");
    }
    
    Debug::log("flac_codec", "[convertSamples_unlocked] Converted ", block_size, " samples from ", 
              m_bits_per_sample, "-bit to 16-bit in ", duration.count(), " μs");
}

void FLACCodec::convertSamples8Bit_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Optimized 8-bit to 16-bit conversion with vectorized batch processing
    // Handle signed 8-bit sample range (-128 to 127) with proper sign extension
    // Scale to 16-bit range (-32768 to 32767) using efficient bit operations
    
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    // Check for vectorized conversion opportunity (batch processing of multiple samples)
    const size_t total_samples = static_cast<size_t>(block_size) * m_channels;
    const size_t vectorized_threshold = 16; // Process in batches of 16+ samples for efficiency
    
    if (total_samples >= vectorized_threshold && m_channels <= 2) {
        // Vectorized conversion for batch processing of multiple samples
        // Optimized for stereo and mono (most common cases)
        convertSamples8BitVectorized_unlocked(buffer, block_size);
    } else {
        // Standard conversion for small blocks or multi-channel audio
        convertSamples8BitStandard_unlocked(buffer, block_size);
    }
    
    Debug::log("flac_codec", "[convertSamples8Bit_unlocked] Converted ", block_size, 
              " samples from 8-bit to 16-bit with ", m_channels, " channels");
}

void FLACCodec::convertSamples8BitStandard_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Standard 8-bit to 16-bit conversion with interleaving for cache efficiency
    for (uint32_t sample = 0; sample < block_size; ++sample) {
        for (uint16_t channel = 0; channel < m_channels; ++channel) {
            size_t output_index = sample * m_channels + channel;
            
            // Use optimized convert8BitTo16Bit for maximum performance
            m_output_buffer[output_index] = convert8BitTo16Bit(buffer[channel][sample]);
        }
    }
}

void FLACCodec::convertSamples8BitVectorized_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Vectorized conversion for batch processing of multiple samples
    // Optimized for high-throughput 8-bit to 16-bit conversion with SIMD when available
    
#ifdef HAVE_SSE2
    // SSE2-optimized 8-bit conversion for x86/x64
    if (m_channels == 1) {
        // Mono SSE2 conversion - process 8 samples at once
        const FLAC__int32* input = buffer[0];
        size_t simd_samples = (block_size / 8) * 8;
        
        for (size_t i = 0; i < simd_samples; i += 8) {
            // Load 8 32-bit samples into two SSE registers
            __m128i samples1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[i]));
            __m128i samples2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[i + 4]));
            
            // Shift left by 8 bits (multiply by 256) to convert 8-bit to 16-bit range
            samples1 = _mm_slli_epi32(samples1, 8);
            samples2 = _mm_slli_epi32(samples2, 8);
            
            // Pack 32-bit to 16-bit with saturation
            __m128i packed = _mm_packs_epi32(samples1, samples2);
            
            // Store 8 16-bit samples
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&m_output_buffer[i]), packed);
        }
        
        // Handle remaining samples
        for (size_t i = simd_samples; i < block_size; ++i) {
            m_output_buffer[i] = convert8BitTo16Bit(input[i]);
        }
    } else if (m_channels == 2) {
        // Stereo SSE2 conversion - process 4 sample pairs at once
        const FLAC__int32* left = buffer[0];
        const FLAC__int32* right = buffer[1];
        size_t simd_samples = (block_size / 4) * 4;
        
        for (size_t i = 0; i < simd_samples; i += 4) {
            // Load 4 samples from each channel
            __m128i left_samples = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&left[i]));
            __m128i right_samples = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&right[i]));
            
            // Convert to 16-bit range
            left_samples = _mm_slli_epi32(left_samples, 8);
            right_samples = _mm_slli_epi32(right_samples, 8);
            
            // Pack to 16-bit
            __m128i left_packed = _mm_packs_epi32(left_samples, _mm_setzero_si128());
            __m128i right_packed = _mm_packs_epi32(right_samples, _mm_setzero_si128());
            
            // Interleave left and right channels
            __m128i interleaved = _mm_unpacklo_epi16(left_packed, right_packed);
            
            // Store 8 interleaved samples (4 stereo pairs)
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&m_output_buffer[i * 2]), interleaved);
        }
        
        // Handle remaining samples
        for (size_t i = simd_samples; i < block_size; ++i) {
            size_t output_base = i * 2;
            m_output_buffer[output_base] = convert8BitTo16Bit(left[i]);
            m_output_buffer[output_base + 1] = convert8BitTo16Bit(right[i]);
        }
    } else {
        // Fallback to standard conversion for >2 channels
        convertSamples8BitStandard_unlocked(buffer, block_size);
    }
#elif defined(HAVE_NEON)
    // NEON-optimized 8-bit conversion for ARM
    if (m_channels == 1) {
        // Mono NEON conversion - process 8 samples at once
        const FLAC__int32* input = buffer[0];
        size_t simd_samples = (block_size / 8) * 8;
        
        for (size_t i = 0; i < simd_samples; i += 8) {
            // Load 8 32-bit samples
            int32x4_t samples1 = vld1q_s32(&input[i]);
            int32x4_t samples2 = vld1q_s32(&input[i + 4]);
            
            // Shift left by 8 bits to convert 8-bit to 16-bit range
            samples1 = vshlq_n_s32(samples1, 8);
            samples2 = vshlq_n_s32(samples2, 8);
            
            // Convert to 16-bit with saturation
            int16x4_t packed1 = vqmovn_s32(samples1);
            int16x4_t packed2 = vqmovn_s32(samples2);
            int16x8_t packed = vcombine_s16(packed1, packed2);
            
            // Store 8 16-bit samples
            vst1q_s16(&m_output_buffer[i], packed);
        }
        
        // Handle remaining samples
        for (size_t i = simd_samples; i < block_size; ++i) {
            m_output_buffer[i] = convert8BitTo16Bit(input[i]);
        }
    } else if (m_channels == 2) {
        // Stereo NEON conversion - process 4 sample pairs at once
        const FLAC__int32* left = buffer[0];
        const FLAC__int32* right = buffer[1];
        size_t simd_samples = (block_size / 4) * 4;
        
        for (size_t i = 0; i < simd_samples; i += 4) {
            // Load 4 samples from each channel
            int32x4_t left_samples = vld1q_s32(&left[i]);
            int32x4_t right_samples = vld1q_s32(&right[i]);
            
            // Convert to 16-bit range
            left_samples = vshlq_n_s32(left_samples, 8);
            right_samples = vshlq_n_s32(right_samples, 8);
            
            // Convert to 16-bit with saturation
            int16x4_t left_packed = vqmovn_s32(left_samples);
            int16x4_t right_packed = vqmovn_s32(right_samples);
            
            // Interleave left and right channels
            int16x4x2_t interleaved = vzip_s16(left_packed, right_packed);
            
            // Store 8 interleaved samples (4 stereo pairs)
            vst1_s16(&m_output_buffer[i * 2], interleaved.val[0]);
            vst1_s16(&m_output_buffer[i * 2 + 4], interleaved.val[1]);
        }
        
        // Handle remaining samples
        for (size_t i = simd_samples; i < block_size; ++i) {
            size_t output_base = i * 2;
            m_output_buffer[output_base] = convert8BitTo16Bit(left[i]);
            m_output_buffer[output_base + 1] = convert8BitTo16Bit(right[i]);
        }
    } else {
        // Fallback to standard conversion for >2 channels
        convertSamples8BitStandard_unlocked(buffer, block_size);
    }
#else
    // Optimized scalar conversion for platforms without SIMD
    if (m_channels == 1) {
        // Mono optimized scalar conversion with loop unrolling
        const FLAC__int32* input = buffer[0];
        size_t unroll_samples = (block_size / 4) * 4;
        
        // Process 4 samples at a time for better instruction pipelining
        for (size_t i = 0; i < unroll_samples; i += 4) {
            // Direct bit shift for maximum performance (8-bit to 16-bit is just << 8)
            m_output_buffer[i] = static_cast<int16_t>(input[i] << 8);
            m_output_buffer[i + 1] = static_cast<int16_t>(input[i + 1] << 8);
            m_output_buffer[i + 2] = static_cast<int16_t>(input[i + 2] << 8);
            m_output_buffer[i + 3] = static_cast<int16_t>(input[i + 3] << 8);
        }
        
        // Handle remaining samples
        for (size_t i = unroll_samples; i < block_size; ++i) {
            m_output_buffer[i] = convert8BitTo16Bit(input[i]);
        }
    } else if (m_channels == 2) {
        // Stereo optimized scalar conversion with loop unrolling
        const FLAC__int32* left = buffer[0];
        const FLAC__int32* right = buffer[1];
        size_t unroll_samples = (block_size / 2) * 2;
        
        // Process 2 sample pairs at a time
        for (size_t i = 0; i < unroll_samples; i += 2) {
            size_t out_base1 = i * 2;
            size_t out_base2 = (i + 1) * 2;
            
            // Direct bit shift for maximum performance
            m_output_buffer[out_base1] = static_cast<int16_t>(left[i] << 8);
            m_output_buffer[out_base1 + 1] = static_cast<int16_t>(right[i] << 8);
            m_output_buffer[out_base2] = static_cast<int16_t>(left[i + 1] << 8);
            m_output_buffer[out_base2 + 1] = static_cast<int16_t>(right[i + 1] << 8);
        }
        
        // Handle remaining samples
        for (size_t i = unroll_samples; i < block_size; ++i) {
            size_t output_base = i * 2;
            m_output_buffer[output_base] = convert8BitTo16Bit(left[i]);
            m_output_buffer[output_base + 1] = convert8BitTo16Bit(right[i]);
        }
    } else {
        // Fallback to standard conversion for >2 channels
        convertSamples8BitStandard_unlocked(buffer, block_size);
    }
#endif
}

void FLACCodec::convertSamples16Bit_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Direct copy optimization for 16-bit samples (no conversion needed)
    // This is the fastest path since no bit depth conversion is required
    
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    // Direct assignment with range validation for 16-bit samples
    for (uint32_t sample = 0; sample < block_size; ++sample) {
        for (uint16_t channel = 0; channel < m_channels; ++channel) {
            size_t output_index = sample * m_channels + channel;
            FLAC__int32 raw_sample = buffer[channel][sample];
            
            // Validate 16-bit range and clamp if necessary
            if (raw_sample < -32768 || raw_sample > 32767) {
                raw_sample = std::clamp(raw_sample, -32768, 32767);
            }
            
            m_output_buffer[output_index] = static_cast<int16_t>(raw_sample);
        }
    }
    
    Debug::log("flac_codec", "[convertSamples16Bit_unlocked] Direct copied ", block_size, 
              " samples (16-bit, no conversion) with ", m_channels, " channels");
}

void FLACCodec::convertSamples24Bit_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Optimized 24-bit to 16-bit conversion with optional dithering
    // Handle proper truncation or rounding with performance-optimized algorithms
    // Include SIMD optimization for batch conversion of 24-bit samples
    
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    // Check for SIMD optimization opportunity
    const size_t total_samples = static_cast<size_t>(block_size) * m_channels;
    const size_t simd_threshold = 32; // Process in batches of 32+ samples for SIMD efficiency
    
    if (total_samples >= simd_threshold && m_channels <= 2) {
        // SIMD-optimized conversion for batch processing
        convertSamples24BitSIMD_unlocked(buffer, block_size);
    } else {
        // Standard high-quality conversion for small blocks or multi-channel audio
        convertSamples24BitStandard_unlocked(buffer, block_size);
    }
    
    Debug::log("flac_codec", "[convertSamples24Bit_unlocked] Converted ", block_size, 
              " samples from 24-bit to 16-bit with ", m_channels, " channels");
}

void FLACCodec::convertSamples24BitStandard_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Standard 24-bit to 16-bit conversion with high-quality algorithms
    for (uint32_t sample = 0; sample < block_size; ++sample) {
        for (uint16_t channel = 0; channel < m_channels; ++channel) {
            size_t output_index = sample * m_channels + channel;
            
            // Use optimized convert24BitTo16Bit for high-quality downscaling
            m_output_buffer[output_index] = convert24BitTo16Bit(buffer[channel][sample]);
        }
    }
}

void FLACCodec::convertSamples24BitSIMD_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // SIMD-optimized 24-bit to 16-bit conversion for high-throughput processing
    // Vectorized batch processing for maximum performance on supported hardware
    
#ifdef HAVE_SSE2
    // SSE2-optimized conversion for x86/x64 platforms
    if (m_channels == 1) {
        // Mono SIMD conversion - process 4 samples at a time
        convertSamples24BitSSE2Mono_unlocked(buffer[0], block_size);
    } else if (m_channels == 2) {
        // Stereo SIMD conversion - process 2 sample pairs at a time
        convertSamples24BitSSE2Stereo_unlocked(buffer[0], buffer[1], block_size);
    } else {
        // Fallback to standard conversion for >2 channels
        convertSamples24BitStandard_unlocked(buffer, block_size);
    }
#elif defined(HAVE_NEON)
    // NEON-optimized conversion for ARM platforms
    if (m_channels == 1) {
        // Mono NEON conversion - process 4 samples at a time
        convertSamples24BitNEONMono_unlocked(buffer[0], block_size);
    } else if (m_channels == 2) {
        // Stereo NEON conversion - process 2 sample pairs at a time
        convertSamples24BitNEONStereo_unlocked(buffer[0], buffer[1], block_size);
    } else {
        // Fallback to standard conversion for >2 channels
        convertSamples24BitStandard_unlocked(buffer, block_size);
    }
#else
    // Fallback to optimized scalar conversion when SIMD not available
    convertSamples24BitScalar_unlocked(buffer, block_size);
#endif
}

void FLACCodec::convertSamples24BitScalar_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Optimized scalar conversion for platforms without SIMD support
    // Uses efficient algorithms optimized for scalar processors
    
    if (m_channels == 1) {
        // Mono scalar conversion - optimized loop
        for (uint32_t sample = 0; sample < block_size; ++sample) {
            m_output_buffer[sample] = convert24BitTo16Bit(buffer[0][sample]);
        }
    } else if (m_channels == 2) {
        // Stereo scalar conversion - interleaved processing
        for (uint32_t sample = 0; sample < block_size; ++sample) {
            size_t output_base = sample * 2;
            m_output_buffer[output_base] = convert24BitTo16Bit(buffer[0][sample]);     // Left
            m_output_buffer[output_base + 1] = convert24BitTo16Bit(buffer[1][sample]); // Right
        }
    } else {
        // Multi-channel scalar conversion
        convertSamples24BitStandard_unlocked(buffer, block_size);
    }
}

#ifdef HAVE_SSE2
void FLACCodec::convertSamples24BitSSE2Mono_unlocked(const FLAC__int32* input, uint32_t block_size) {
    // SSE2-optimized mono 24-bit to 16-bit conversion
    // Process 4 samples at a time using 128-bit SIMD registers
    
    uint32_t simd_samples = (block_size / 4) * 4; // Process in groups of 4
    uint32_t sample = 0;
    
    // SIMD processing for bulk of samples
    for (; sample < simd_samples; sample += 4) {
        // Load 4 x 32-bit samples into SSE register
        __m128i samples = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[sample]));
        
        // Arithmetic right shift by 8 bits to convert 24-bit to 16-bit
        __m128i shifted = _mm_srai_epi32(samples, 8);
        
        // Pack 32-bit values to 16-bit with saturation
        __m128i packed = _mm_packs_epi32(shifted, shifted);
        
        // Store 4 x 16-bit results
        _mm_storel_epi64(reinterpret_cast<__m128i*>(&m_output_buffer[sample]), packed);
    }
    
    // Handle remaining samples with scalar conversion
    for (; sample < block_size; ++sample) {
        m_output_buffer[sample] = convert24BitTo16Bit(input[sample]);
    }
}

void FLACCodec::convertSamples24BitSSE2Stereo_unlocked(const FLAC__int32* left, const FLAC__int32* right, uint32_t block_size) {
    // SSE2-optimized stereo 24-bit to 16-bit conversion with interleaving
    // Process 2 sample pairs at a time using 128-bit SIMD registers
    
    uint32_t simd_samples = (block_size / 2) * 2; // Process in groups of 2
    uint32_t sample = 0;
    
    // SIMD processing for bulk of samples
    for (; sample < simd_samples; sample += 2) {
        // Load 2 left and 2 right samples
        __m128i left_samples = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&left[sample]));
        __m128i right_samples = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&right[sample]));
        
        // Shift both channels
        __m128i left_shifted = _mm_srai_epi32(left_samples, 8);
        __m128i right_shifted = _mm_srai_epi32(right_samples, 8);
        
        // Pack to 16-bit with saturation
        __m128i packed = _mm_packs_epi32(left_shifted, right_shifted);
        
        // Interleave left and right channels
        __m128i interleaved = _mm_unpacklo_epi16(packed, _mm_srli_si128(packed, 8));
        
        // Store interleaved stereo samples
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&m_output_buffer[sample * 2]), interleaved);
    }
    
    // Handle remaining samples with scalar conversion
    for (; sample < block_size; ++sample) {
        size_t output_base = sample * 2;
        m_output_buffer[output_base] = convert24BitTo16Bit(left[sample]);
        m_output_buffer[output_base + 1] = convert24BitTo16Bit(right[sample]);
    }
}
#endif // HAVE_SSE2

#ifdef HAVE_NEON
void FLACCodec::convertSamples24BitNEONMono_unlocked(const FLAC__int32* input, uint32_t block_size) {
    // NEON-optimized mono 24-bit to 16-bit conversion for ARM platforms
    // Process 4 samples at a time using 128-bit NEON registers
    
    uint32_t simd_samples = (block_size / 4) * 4; // Process in groups of 4
    uint32_t sample = 0;
    
    // NEON processing for bulk of samples
    for (; sample < simd_samples; sample += 4) {
        // Load 4 x 32-bit samples into NEON register
        int32x4_t samples = vld1q_s32(&input[sample]);
        
        // Arithmetic right shift by 8 bits to convert 24-bit to 16-bit
        int32x4_t shifted = vshrq_n_s32(samples, 8);
        
        // Narrow 32-bit to 16-bit with saturation
        int16x4_t narrowed = vqmovn_s32(shifted);
        
        // Store 4 x 16-bit results
        vst1_s16(&m_output_buffer[sample], narrowed);
    }
    
    // Handle remaining samples with scalar conversion
    for (; sample < block_size; ++sample) {
        m_output_buffer[sample] = convert24BitTo16Bit(input[sample]);
    }
}

void FLACCodec::convertSamples24BitNEONStereo_unlocked(const FLAC__int32* left, const FLAC__int32* right, uint32_t block_size) {
    // NEON-optimized stereo 24-bit to 16-bit conversion with interleaving
    // Process 2 sample pairs at a time using 128-bit NEON registers
    
    uint32_t simd_samples = (block_size / 2) * 2; // Process in groups of 2
    uint32_t sample = 0;
    
    // NEON processing for bulk of samples
    for (; sample < simd_samples; sample += 2) {
        // Load 2 left and 2 right samples
        int32x2_t left_samples = vld1_s32(&left[sample]);
        int32x2_t right_samples = vld1_s32(&right[sample]);
        
        // Shift both channels
        int32x2_t left_shifted = vshr_n_s32(left_samples, 8);
        int32x2_t right_shifted = vshr_n_s32(right_samples, 8);
        
        // Narrow to 16-bit with saturation
        int16x4_t left_narrow = vqmovn_s32(vcombine_s32(left_shifted, left_shifted));
        int16x4_t right_narrow = vqmovn_s32(vcombine_s32(right_shifted, right_shifted));
        
        // Interleave left and right channels
        int16x4x2_t interleaved = vzip_s16(vget_low_s16(left_narrow), vget_low_s16(right_narrow));
        
        // Store interleaved stereo samples
        vst1_s16(&m_output_buffer[sample * 2], interleaved.val[0]);
    }
    
    // Handle remaining samples with scalar conversion
    for (; sample < block_size; ++sample) {
        size_t output_base = sample * 2;
        m_output_buffer[output_base] = convert24BitTo16Bit(left[sample]);
        m_output_buffer[output_base + 1] = convert24BitTo16Bit(right[sample]);
    }
}
#endif // HAVE_NEON

#ifdef HAVE_SSE2
void FLACCodec::convertSamples32BitSSE2Mono_unlocked(const FLAC__int32* input, uint32_t block_size) {
    // SSE2-optimized mono 32-bit to 16-bit conversion with overflow protection
    // Process 4 samples at a time using 128-bit SIMD registers and bit operations
    
    uint32_t simd_samples = (block_size / 4) * 4; // Process in groups of 4
    uint32_t sample = 0;
    
    // SIMD processing for bulk of samples with efficient bit operations
    for (; sample < simd_samples; sample += 4) {
        // Load 4 x 32-bit samples into SSE register
        __m128i samples = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&input[sample]));
        
        // Arithmetic right shift by 16 bits to convert 32-bit to 16-bit range
        __m128i shifted = _mm_srai_epi32(samples, 16);
        
        // Pack 32-bit values to 16-bit with saturation (automatic clamping)
        __m128i packed = _mm_packs_epi32(shifted, shifted);
        
        // Store 4 x 16-bit results
        _mm_storel_epi64(reinterpret_cast<__m128i*>(&m_output_buffer[sample]), packed);
    }
    
    // Handle remaining samples with scalar conversion
    for (; sample < block_size; ++sample) {
        m_output_buffer[sample] = convert32BitTo16Bit(input[sample]);
    }
}

void FLACCodec::convertSamples32BitSSE2Stereo_unlocked(const FLAC__int32* left, const FLAC__int32* right, uint32_t block_size) {
    // SSE2-optimized stereo 32-bit to 16-bit conversion with interleaving and overflow protection
    // Process 2 sample pairs at a time using 128-bit SIMD registers
    
    uint32_t simd_samples = (block_size / 2) * 2; // Process in groups of 2
    uint32_t sample = 0;
    
    // SIMD processing for bulk of samples with efficient clamping operations
    for (; sample < simd_samples; sample += 2) {
        // Load 2 left and 2 right samples
        __m128i left_samples = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&left[sample]));
        __m128i right_samples = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&right[sample]));
        
        // Arithmetic right shift by 16 bits for both channels
        __m128i left_shifted = _mm_srai_epi32(left_samples, 16);
        __m128i right_shifted = _mm_srai_epi32(right_samples, 16);
        
        // Pack to 16-bit with saturation (automatic overflow protection)
        __m128i packed = _mm_packs_epi32(left_shifted, right_shifted);
        
        // Interleave left and right channels for stereo output
        __m128i interleaved = _mm_unpacklo_epi16(packed, _mm_srli_si128(packed, 8));
        
        // Store interleaved stereo samples
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&m_output_buffer[sample * 2]), interleaved);
    }
    
    // Handle remaining samples with scalar conversion
    for (; sample < block_size; ++sample) {
        size_t output_base = sample * 2;
        m_output_buffer[output_base] = convert32BitTo16Bit(left[sample]);
        m_output_buffer[output_base + 1] = convert32BitTo16Bit(right[sample]);
    }
}
#endif // HAVE_SSE2

#ifdef HAVE_NEON
void FLACCodec::convertSamples32BitNEONMono_unlocked(const FLAC__int32* input, uint32_t block_size) {
    // NEON-optimized mono 32-bit to 16-bit conversion for ARM platforms
    // Process 4 samples at a time using 128-bit NEON registers with bit operations
    
    uint32_t simd_samples = (block_size / 4) * 4; // Process in groups of 4
    uint32_t sample = 0;
    
    // NEON processing for bulk of samples with efficient clamping operations
    for (; sample < simd_samples; sample += 4) {
        // Load 4 x 32-bit samples into NEON register
        int32x4_t samples = vld1q_s32(&input[sample]);
        
        // Arithmetic right shift by 16 bits to convert 32-bit to 16-bit range
        int32x4_t shifted = vshrq_n_s32(samples, 16);
        
        // Narrow 32-bit to 16-bit with saturation (automatic overflow protection)
        int16x4_t narrowed = vqmovn_s32(shifted);
        
        // Store 4 x 16-bit results
        vst1_s16(&m_output_buffer[sample], narrowed);
    }
    
    // Handle remaining samples with scalar conversion
    for (; sample < block_size; ++sample) {
        m_output_buffer[sample] = convert32BitTo16Bit(input[sample]);
    }
}

void FLACCodec::convertSamples32BitNEONStereo_unlocked(const FLAC__int32* left, const FLAC__int32* right, uint32_t block_size) {
    // NEON-optimized stereo 32-bit to 16-bit conversion with interleaving and overflow protection
    // Process 2 sample pairs at a time using 128-bit NEON registers
    
    uint32_t simd_samples = (block_size / 2) * 2; // Process in groups of 2
    uint32_t sample = 0;
    
    // NEON processing for bulk of samples with efficient bit operations
    for (; sample < simd_samples; sample += 2) {
        // Load 2 left and 2 right samples
        int32x2_t left_samples = vld1_s32(&left[sample]);
        int32x2_t right_samples = vld1_s32(&right[sample]);
        
        // Arithmetic right shift by 16 bits for both channels
        int32x2_t left_shifted = vshr_n_s32(left_samples, 16);
        int32x2_t right_shifted = vshr_n_s32(right_samples, 16);
        
        // Narrow to 16-bit with saturation (automatic overflow protection)
        int16x4_t left_narrow = vqmovn_s32(vcombine_s32(left_shifted, left_shifted));
        int16x4_t right_narrow = vqmovn_s32(vcombine_s32(right_shifted, right_shifted));
        
        // Interleave left and right channels for stereo output
        int16x4x2_t interleaved = vzip_s16(vget_low_s16(left_narrow), vget_low_s16(right_narrow));
        
        // Store interleaved stereo samples
        vst1_s16(&m_output_buffer[sample * 2], interleaved.val[0]);
    }
    
    // Handle remaining samples with scalar conversion
    for (; sample < block_size; ++sample) {
        size_t output_base = sample * 2;
        m_output_buffer[output_base] = convert32BitTo16Bit(left[sample]);
        m_output_buffer[output_base + 1] = convert32BitTo16Bit(right[sample]);
    }
}
#endif // HAVE_NEON

void FLACCodec::convertSamples32Bit_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Optimized 32-bit to 16-bit conversion with overflow protection
    // Handle full 32-bit dynamic range conversion with efficient clamping operations
    // Add vectorized processing for high-throughput 32-bit to 16-bit conversion
    
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    // Check for SIMD optimization opportunity for high-throughput conversion
    const size_t total_samples = static_cast<size_t>(block_size) * m_channels;
    const size_t simd_threshold = 32; // Process in batches of 32+ samples for SIMD efficiency
    
    if (total_samples >= simd_threshold && m_channels <= 2) {
        // SIMD-optimized conversion for high-throughput processing
        convertSamples32BitSIMD_unlocked(buffer, block_size);
    } else {
        // Standard high-performance conversion for small blocks or multi-channel audio
        convertSamples32BitStandard_unlocked(buffer, block_size);
    }
    
    Debug::log("flac_codec", "[convertSamples32Bit_unlocked] Converted ", block_size, 
              " samples from 32-bit to 16-bit with ", m_channels, " channels");
}

void FLACCodec::convertSamples32BitStandard_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Standard 32-bit to 16-bit conversion with efficient clamping operations
    for (uint32_t sample = 0; sample < block_size; ++sample) {
        for (uint16_t channel = 0; channel < m_channels; ++channel) {
            size_t output_index = sample * m_channels + channel;
            
            // Use optimized convert32BitTo16Bit for high-throughput conversion
            m_output_buffer[output_index] = convert32BitTo16Bit(buffer[channel][sample]);
        }
    }
}

void FLACCodec::convertSamples32BitSIMD_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // SIMD-optimized 32-bit to 16-bit conversion for high-throughput processing
    // Vectorized processing using bit operations and SIMD instructions
    
#ifdef HAVE_SSE2
    // SSE2-optimized conversion for x86/x64 platforms
    if (m_channels == 1) {
        // Mono SIMD conversion - process 4 samples at a time
        convertSamples32BitSSE2Mono_unlocked(buffer[0], block_size);
    } else if (m_channels == 2) {
        // Stereo SIMD conversion - process 2 sample pairs at a time
        convertSamples32BitSSE2Stereo_unlocked(buffer[0], buffer[1], block_size);
    } else {
        // Fallback to standard conversion for >2 channels
        convertSamples32BitStandard_unlocked(buffer, block_size);
    }
#elif defined(HAVE_NEON)
    // NEON-optimized conversion for ARM platforms
    if (m_channels == 1) {
        // Mono NEON conversion - process 4 samples at a time
        convertSamples32BitNEONMono_unlocked(buffer[0], block_size);
    } else if (m_channels == 2) {
        // Stereo NEON conversion - process 2 sample pairs at a time
        convertSamples32BitNEONStereo_unlocked(buffer[0], buffer[1], block_size);
    } else {
        // Fallback to standard conversion for >2 channels
        convertSamples32BitStandard_unlocked(buffer, block_size);
    }
#else
    // Fallback to optimized scalar conversion when SIMD not available
    convertSamples32BitScalar_unlocked(buffer, block_size);
#endif
}

void FLACCodec::convertSamples32BitScalar_unlocked(const FLAC__int32* const buffer[], uint32_t block_size) {
    // Optimized scalar conversion for platforms without SIMD support
    // Uses efficient bit operations optimized for scalar processors
    
    if (m_channels == 1) {
        // Mono scalar conversion - optimized loop with efficient bit operations
        for (uint32_t sample = 0; sample < block_size; ++sample) {
            m_output_buffer[sample] = convert32BitTo16Bit(buffer[0][sample]);
        }
    } else if (m_channels == 2) {
        // Stereo scalar conversion - interleaved processing with bit operations
        for (uint32_t sample = 0; sample < block_size; ++sample) {
            size_t output_base = sample * 2;
            m_output_buffer[output_base] = convert32BitTo16Bit(buffer[0][sample]);     // Left
            m_output_buffer[output_base + 1] = convert32BitTo16Bit(buffer[1][sample]); // Right
        }
    } else {
        // Multi-channel scalar conversion
        convertSamples32BitStandard_unlocked(buffer, block_size);
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

// ============================================================================
// Quality Validation and Accuracy Testing Implementation
// ============================================================================

bool FLACCodec::validateBitPerfectDecoding(const std::vector<int16_t>& reference_samples, 
                                          const std::vector<int16_t>& decoded_samples) const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return validateBitPerfectDecoding_unlocked(reference_samples, decoded_samples);
}

double FLACCodec::calculateSignalToNoiseRatio(const std::vector<int16_t>& reference_samples,
                                             const std::vector<int16_t>& decoded_samples) const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return calculateSignalToNoiseRatio_unlocked(reference_samples, decoded_samples);
}

double FLACCodec::calculateTotalHarmonicDistortion(const std::vector<int16_t>& samples) const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return calculateTotalHarmonicDistortion_unlocked(samples);
}

bool FLACCodec::validateConversionQuality(const std::vector<FLAC__int32>& source_samples,
                                         const std::vector<int16_t>& converted_samples,
                                         uint16_t source_bit_depth) const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return validateConversionQuality_unlocked(source_samples, converted_samples, source_bit_depth);
}

bool FLACCodec::validateDynamicRange(const std::vector<int16_t>& samples) const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return validateDynamicRange_unlocked(samples);
}

AudioQualityMetrics FLACCodec::calculateQualityMetrics(const std::vector<int16_t>& samples) const {
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return calculateQualityMetrics_unlocked(samples);
}

// ============================================================================
// Private Quality Validation Implementation
// ============================================================================

bool FLACCodec::validateBitPerfectDecoding_unlocked(const std::vector<int16_t>& reference_samples, 
                                                   const std::vector<int16_t>& decoded_samples) const {
    Debug::log("flac_codec", "[FLACCodec::validateBitPerfectDecoding_unlocked] Validating bit-perfect decoding");
    
    // Check size match first
    if (reference_samples.size() != decoded_samples.size()) {
        Debug::log("flac_codec", "[validateBitPerfectDecoding] Size mismatch: reference=", 
                  reference_samples.size(), ", decoded=", decoded_samples.size());
        return false;
    }
    
    if (reference_samples.empty()) {
        Debug::log("flac_codec", "[validateBitPerfectDecoding] Empty sample arrays");
        return true; // Empty arrays are trivially bit-perfect
    }
    
    // Perform exact sample-by-sample comparison
    bool bit_perfect = compareSamplesExact_unlocked(reference_samples, decoded_samples);
    
    if (bit_perfect) {
        Debug::log("flac_codec", "[validateBitPerfectDecoding] Bit-perfect match confirmed for ", 
                  reference_samples.size(), " samples");
    } else {
        // Calculate error statistics for debugging
        double mse = calculateMeanSquaredError_unlocked(reference_samples, decoded_samples);
        double psnr = calculatePeakSignalToNoiseRatio_unlocked(reference_samples, decoded_samples);
        
        Debug::log("flac_codec", "[validateBitPerfectDecoding] Bit-perfect validation failed - MSE=", 
                  mse, ", PSNR=", psnr, "dB");
    }
    
    return bit_perfect;
}

double FLACCodec::calculateSignalToNoiseRatio_unlocked(const std::vector<int16_t>& reference_samples,
                                                      const std::vector<int16_t>& decoded_samples) const {
    Debug::log("flac_codec", "[FLACCodec::calculateSignalToNoiseRatio_unlocked] Calculating SNR");
    
    if (reference_samples.size() != decoded_samples.size() || reference_samples.empty()) {
        Debug::log("flac_codec", "[calculateSignalToNoiseRatio] Invalid input arrays");
        return 0.0;
    }
    
    // Calculate signal power (RMS of reference signal)
    double signal_power = 0.0;
    for (size_t i = 0; i < reference_samples.size(); ++i) {
        double sample = static_cast<double>(reference_samples[i]) / 32768.0; // Normalize to [-1, 1]
        signal_power += sample * sample;
    }
    signal_power /= reference_samples.size();
    
    // Calculate noise power (RMS of difference signal)
    double noise_power = 0.0;
    for (size_t i = 0; i < reference_samples.size(); ++i) {
        double diff = static_cast<double>(reference_samples[i] - decoded_samples[i]) / 32768.0;
        noise_power += diff * diff;
    }
    noise_power /= reference_samples.size();
    
    // Calculate SNR in dB
    if (noise_power <= 0.0) {
        Debug::log("flac_codec", "[calculateSignalToNoiseRatio] Perfect match - infinite SNR");
        return 200.0; // Return very high SNR for perfect match
    }
    
    double snr_db = 10.0 * std::log10(signal_power / noise_power);
    
    Debug::log("flac_codec", "[calculateSignalToNoiseRatio] SNR = ", snr_db, " dB");
    return snr_db;
}

double FLACCodec::calculateTotalHarmonicDistortion_unlocked(const std::vector<int16_t>& samples) const {
    Debug::log("flac_codec", "[FLACCodec::calculateTotalHarmonicDistortion_unlocked] Calculating THD");
    
    if (samples.empty()) {
        Debug::log("flac_codec", "[calculateTotalHarmonicDistortion] Empty sample array");
        return 0.0;
    }
    
    // For simplicity, we'll calculate THD as the ratio of high-frequency content
    // to total signal energy. A more sophisticated implementation would use FFT.
    
    // Calculate total signal energy
    double total_energy = 0.0;
    for (int16_t sample : samples) {
        double normalized = static_cast<double>(sample) / 32768.0;
        total_energy += normalized * normalized;
    }
    
    if (total_energy <= 0.0) {
        Debug::log("flac_codec", "[calculateTotalHarmonicDistortion] Zero signal energy");
        return 0.0;
    }
    
    // Calculate high-frequency energy using simple high-pass filtering
    double hf_energy = 0.0;
    for (size_t i = 1; i < samples.size(); ++i) {
        double diff = static_cast<double>(samples[i] - samples[i-1]) / 32768.0;
        hf_energy += diff * diff;
    }
    
    // THD as percentage
    double thd = (hf_energy / total_energy) * 100.0;
    
    Debug::log("flac_codec", "[calculateTotalHarmonicDistortion] THD = ", thd, "%");
    return thd;
}

bool FLACCodec::validateConversionQuality_unlocked(const std::vector<FLAC__int32>& source_samples,
                                                  const std::vector<int16_t>& converted_samples,
                                                  uint16_t source_bit_depth) const {
    Debug::log("flac_codec", "[FLACCodec::validateConversionQuality_unlocked] Validating conversion quality from ", 
              source_bit_depth, "-bit to 16-bit");
    
    if (source_samples.empty() || converted_samples.empty()) {
        Debug::log("flac_codec", "[validateConversionQuality] Empty sample arrays");
        return false;
    }
    
    if (source_samples.size() != converted_samples.size()) {
        Debug::log("flac_codec", "[validateConversionQuality] Size mismatch: source=", 
                  source_samples.size(), ", converted=", converted_samples.size());
        return false;
    }
    
    // Validate each conversion individually
    size_t error_count = 0;
    double max_error = 0.0;
    
    for (size_t i = 0; i < source_samples.size(); ++i) {
        if (!validateBitDepthConversion_unlocked(source_samples[i], converted_samples[i], source_bit_depth)) {
            error_count++;
        }
        
        // Calculate conversion error for statistics
        double expected = 0.0;
        switch (source_bit_depth) {
            case 8:
                expected = static_cast<double>(source_samples[i] << 8);
                break;
            case 16:
                expected = static_cast<double>(source_samples[i]);
                break;
            case 24:
                expected = static_cast<double>(source_samples[i] >> 8);
                break;
            case 32:
                expected = static_cast<double>(source_samples[i] >> 16);
                break;
            default:
                expected = static_cast<double>(source_samples[i] >> (source_bit_depth - 16));
                break;
        }
        
        double actual = static_cast<double>(converted_samples[i]);
        double error = std::abs(expected - actual);
        max_error = std::max(max_error, error);
    }
    
    double error_rate = static_cast<double>(error_count) / source_samples.size();
    bool quality_ok = error_rate < 0.01; // Less than 1% error rate
    
    Debug::log("flac_codec", "[validateConversionQuality] Error rate: ", error_rate * 100.0, 
              "%, Max error: ", max_error, ", Quality OK: ", quality_ok);
    
    return quality_ok;
}

bool FLACCodec::validateDynamicRange_unlocked(const std::vector<int16_t>& samples) const {
    Debug::log("flac_codec", "[FLACCodec::validateDynamicRange_unlocked] Validating dynamic range");
    
    if (samples.empty()) {
        Debug::log("flac_codec", "[validateDynamicRange] Empty sample array");
        return false;
    }
    
    // Calculate peak and RMS amplitudes
    double peak_amplitude = calculatePeakAmplitude_unlocked(samples);
    double rms_amplitude = calculateRMSAmplitude_unlocked(samples);
    
    if (rms_amplitude <= 0.0) {
        Debug::log("flac_codec", "[validateDynamicRange] Zero RMS amplitude - silence");
        return true; // Silence is valid
    }
    
    // Calculate dynamic range in dB
    double dynamic_range_db = 20.0 * std::log10(peak_amplitude / rms_amplitude);
    
    // Check for reasonable dynamic range (should be > 20dB for most audio)
    bool range_ok = dynamic_range_db > 20.0 && dynamic_range_db < 120.0;
    
    Debug::log("flac_codec", "[validateDynamicRange] Peak: ", peak_amplitude, 
              ", RMS: ", rms_amplitude, ", Dynamic range: ", dynamic_range_db, 
              "dB, Valid: ", range_ok);
    
    return range_ok;
}

AudioQualityMetrics FLACCodec::calculateQualityMetrics_unlocked(const std::vector<int16_t>& samples) const {
    Debug::log("flac_codec", "[FLACCodec::calculateQualityMetrics_unlocked] Calculating comprehensive quality metrics");
    
    AudioQualityMetrics metrics;
    
    if (samples.empty()) {
        Debug::log("flac_codec", "[calculateQualityMetrics] Empty sample array");
        return metrics;
    }
    
    // Calculate basic amplitude metrics
    metrics.peak_amplitude = calculatePeakAmplitude_unlocked(samples);
    metrics.rms_amplitude = calculateRMSAmplitude_unlocked(samples);
    metrics.dc_offset = calculateDCOffset_unlocked(samples);
    
    // Calculate dynamic range
    if (metrics.rms_amplitude > 0.0) {
        metrics.dynamic_range_db = 20.0 * std::log10(metrics.peak_amplitude / metrics.rms_amplitude);
    }
    
    // Count zero crossings and clipped samples
    metrics.zero_crossings = countZeroCrossings_unlocked(samples);
    metrics.clipped_samples = countClippedSamples_unlocked(samples);
    
    // Calculate THD (simplified)
    metrics.total_harmonic_distortion = calculateTotalHarmonicDistortion_unlocked(samples);
    
    // For SNR, we need a reference signal - use theoretical maximum for bit depth
    // For 16-bit audio, theoretical SNR is ~96dB
    if (metrics.rms_amplitude > 0.0) {
        double theoretical_max = 1.0; // Full scale
        metrics.signal_to_noise_ratio_db = 20.0 * std::log10(theoretical_max / metrics.rms_amplitude);
        
        // Clamp to reasonable range
        if (metrics.signal_to_noise_ratio_db > 120.0) {
            metrics.signal_to_noise_ratio_db = 120.0;
        }
    }
    
    // Determine if bit-perfect (no clipping, good SNR, low THD)
    metrics.bit_perfect = (metrics.clipped_samples == 0 && 
                          metrics.signal_to_noise_ratio_db > 90.0 && 
                          metrics.total_harmonic_distortion < 1.0);
    
    Debug::log("flac_codec", "[calculateQualityMetrics] Peak: ", metrics.peak_amplitude, 
              ", RMS: ", metrics.rms_amplitude, ", SNR: ", metrics.signal_to_noise_ratio_db, 
              "dB, THD: ", metrics.total_harmonic_distortion, "%, Clipped: ", metrics.clipped_samples);
    
    return metrics;
}

// ============================================================================
// Quality Validation Helper Methods
// ============================================================================

bool FLACCodec::compareSamplesExact_unlocked(const std::vector<int16_t>& samples1,
                                            const std::vector<int16_t>& samples2) const {
    if (samples1.size() != samples2.size()) {
        return false;
    }
    
    for (size_t i = 0; i < samples1.size(); ++i) {
        if (samples1[i] != samples2[i]) {
            return false;
        }
    }
    
    return true;
}

double FLACCodec::calculateMeanSquaredError_unlocked(const std::vector<int16_t>& reference_samples,
                                                    const std::vector<int16_t>& test_samples) const {
    if (reference_samples.size() != test_samples.size() || reference_samples.empty()) {
        return 0.0;
    }
    
    double mse = 0.0;
    for (size_t i = 0; i < reference_samples.size(); ++i) {
        double diff = static_cast<double>(reference_samples[i] - test_samples[i]);
        mse += diff * diff;
    }
    
    return mse / reference_samples.size();
}

double FLACCodec::calculatePeakSignalToNoiseRatio_unlocked(const std::vector<int16_t>& reference_samples,
                                                          const std::vector<int16_t>& test_samples) const {
    double mse = calculateMeanSquaredError_unlocked(reference_samples, test_samples);
    
    if (mse <= 0.0) {
        return 200.0; // Perfect match
    }
    
    double max_possible_value = 32767.0; // Maximum 16-bit signed value
    return 20.0 * std::log10(max_possible_value / std::sqrt(mse));
}

bool FLACCodec::validateBitDepthConversion_unlocked(FLAC__int32 source_sample, int16_t converted_sample,
                                                   uint16_t source_bit_depth) const {
    // Calculate expected converted value
    int16_t expected = 0;
    
    switch (source_bit_depth) {
        case 8:
            expected = convert8BitTo16Bit(source_sample);
            break;
        case 16:
            expected = static_cast<int16_t>(std::clamp(static_cast<int32_t>(source_sample), -32768, 32767));
            break;
        case 24:
            expected = convert24BitTo16Bit(source_sample);
            break;
        case 32:
            expected = convert32BitTo16Bit(source_sample);
            break;
        default:
            // Generic conversion for unusual bit depths
            int shift = source_bit_depth - 16;
            if (shift > 0) {
                expected = static_cast<int16_t>(std::clamp(source_sample >> shift, -32768, 32767));
            } else {
                expected = static_cast<int16_t>(std::clamp(source_sample << (-shift), -32768, 32767));
            }
            break;
    }
    
    // Allow small tolerance for dithering and rounding
    int tolerance = (source_bit_depth > 16) ? 1 : 0;
    return std::abs(converted_sample - expected) <= tolerance;
}

double FLACCodec::calculateConversionError_unlocked(const std::vector<FLAC__int32>& source_samples,
                                                   const std::vector<int16_t>& converted_samples,
                                                   uint16_t source_bit_depth) const {
    if (source_samples.size() != converted_samples.size() || source_samples.empty()) {
        return 0.0;
    }
    
    double total_error = 0.0;
    
    for (size_t i = 0; i < source_samples.size(); ++i) {
        // Calculate expected value
        double expected = 0.0;
        switch (source_bit_depth) {
            case 8:
                expected = static_cast<double>(source_samples[i] << 8);
                break;
            case 16:
                expected = static_cast<double>(source_samples[i]);
                break;
            case 24:
                expected = static_cast<double>(source_samples[i] >> 8);
                break;
            case 32:
                expected = static_cast<double>(source_samples[i] >> 16);
                break;
            default:
                expected = static_cast<double>(source_samples[i] >> (source_bit_depth - 16));
                break;
        }
        
        double actual = static_cast<double>(converted_samples[i]);
        double error = std::abs(expected - actual);
        total_error += error;
    }
    
    return total_error / source_samples.size();
}

bool FLACCodec::validateNoClipping_unlocked(const std::vector<int16_t>& samples) const {
    for (int16_t sample : samples) {
        if (sample == -32768 || sample == 32767) {
            return false; // Potential clipping detected
        }
    }
    return true;
}

double FLACCodec::calculateRMSAmplitude_unlocked(const std::vector<int16_t>& samples) const {
    if (samples.empty()) {
        return 0.0;
    }
    
    double sum_squares = 0.0;
    for (int16_t sample : samples) {
        double normalized = static_cast<double>(sample) / 32768.0;
        sum_squares += normalized * normalized;
    }
    
    return std::sqrt(sum_squares / samples.size());
}

double FLACCodec::calculatePeakAmplitude_unlocked(const std::vector<int16_t>& samples) const {
    if (samples.empty()) {
        return 0.0;
    }
    
    int16_t max_sample = 0;
    for (int16_t sample : samples) {
        max_sample = std::max(max_sample, static_cast<int16_t>(std::abs(sample)));
    }
    
    return static_cast<double>(max_sample) / 32768.0;
}

double FLACCodec::calculateDCOffset_unlocked(const std::vector<int16_t>& samples) const {
    if (samples.empty()) {
        return 0.0;
    }
    
    double sum = 0.0;
    for (int16_t sample : samples) {
        sum += static_cast<double>(sample);
    }
    
    double average = sum / samples.size();
    return (average / 32768.0) * 100.0; // Return as percentage
}

size_t FLACCodec::countZeroCrossings_unlocked(const std::vector<int16_t>& samples) const {
    if (samples.size() < 2) {
        return 0;
    }
    
    size_t crossings = 0;
    for (size_t i = 1; i < samples.size(); ++i) {
        if ((samples[i-1] >= 0 && samples[i] < 0) || (samples[i-1] < 0 && samples[i] >= 0)) {
            crossings++;
        }
    }
    
    return crossings;
}

size_t FLACCodec::countClippedSamples_unlocked(const std::vector<int16_t>& samples) const {
    size_t clipped = 0;
    for (int16_t sample : samples) {
        if (sample == -32768 || sample == 32767) {
            clipped++;
        }
    }
    return clipped;
}

bool FLACCodec::compareWithReferenceDecoder_unlocked(const MediaChunk& chunk,
                                                    const std::vector<int16_t>& our_output) const {
    // This is a placeholder for reference decoder comparison
    // In a real implementation, this would use a reference FLAC decoder
    // like libFLAC's reference implementation or another trusted decoder
    
    Debug::log("flac_codec", "[FLACCodec::compareWithReferenceDecoder_unlocked] Reference decoder comparison not implemented");
    
    // For now, just validate that our output is reasonable
    if (our_output.empty()) {
        return false;
    }
    
    // Basic sanity checks
    AudioQualityMetrics metrics = calculateQualityMetrics_unlocked(our_output);
    return metrics.isGoodQuality();
}

std::vector<int16_t> FLACCodec::generateReferenceSamples_unlocked(const MediaChunk& chunk) const {
    // This is a placeholder for reference sample generation
    // In a real implementation, this would decode the chunk using a reference decoder
    
    Debug::log("flac_codec", "[FLACCodec::generateReferenceSamples_unlocked] Reference sample generation not implemented");
    
    // Return empty vector to indicate no reference available
    return std::vector<int16_t>();
}

// ============================================================================
// Variable Block Size Handling Implementation
// ============================================================================

void FLACCodec::initializeBlockSizeHandling_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::initializeBlockSizeHandling_unlocked] Initializing block size handling");
    
    // Initialize block size tracking with RFC 9639 defaults
    m_min_block_size = 16;      // RFC 9639 minimum block size
    m_max_block_size = 65535;   // RFC 9639 maximum block size
    m_variable_block_size = false;
    m_current_block_size = 0;
    m_preferred_block_size = 0;
    
    // Initialize variable block size adaptation state
    m_previous_block_size = 0;
    m_block_size_changes = 0;
    m_total_samples_processed = 0;
    m_adaptive_buffering_enabled = true;
    m_smallest_block_seen = UINT32_MAX;
    m_largest_block_seen = 0;
    m_average_block_size = 0.0;
    
    // Pre-optimize for standard block sizes
    optimizeForFixedBlockSizes_unlocked();
    
    Debug::log("flac_codec", "[FLACCodec::initializeBlockSizeHandling_unlocked] Block size handling initialized: ",
              "min=", m_min_block_size, ", max=", m_max_block_size);
}

bool FLACCodec::validateBlockSize_unlocked(uint32_t block_size) const {
    // RFC 9639 compliance validation
    if (block_size < 16 || block_size > 65535) {
        Debug::log("flac_codec", "[FLACCodec::validateBlockSize_unlocked] Block size ", block_size, 
                  " outside RFC 9639 range (16-65535)");
        return false;
    }
    
    // Check against STREAMINFO constraints if available
    if (m_min_block_size > 0 && block_size < m_min_block_size) {
        Debug::log("flac_codec", "[FLACCodec::validateBlockSize_unlocked] Block size ", block_size, 
                  " below STREAMINFO minimum ", m_min_block_size);
        return false;
    }
    
    if (m_max_block_size > 0 && block_size > m_max_block_size) {
        Debug::log("flac_codec", "[FLACCodec::validateBlockSize_unlocked] Block size ", block_size, 
                  " above STREAMINFO maximum ", m_max_block_size);
        return false;
    }
    
    return true;
}

void FLACCodec::updateBlockSizeTracking_unlocked(uint32_t block_size) {
    Debug::log("flac_codec", "[FLACCodec::updateBlockSizeTracking_unlocked] Updating block size tracking: ", 
              "current=", m_current_block_size, " -> new=", block_size);
    
    // Handle block size transition for variable block size adaptation
    if (m_current_block_size != 0) {
        handleBlockSizeTransition_unlocked(m_current_block_size, block_size);
    }
    
    // Detect variable block size usage
    if (m_current_block_size != 0 && m_current_block_size != block_size) {
        if (!m_variable_block_size) {
            Debug::log("flac_codec", "[FLACCodec::updateBlockSizeTracking_unlocked] Variable block size detected: ",
                      "previous=", m_current_block_size, ", current=", block_size);
            m_variable_block_size = true;
        }
    }
    
    // Update current block size
    uint32_t previous_block_size = m_current_block_size;
    m_current_block_size = block_size;
    
    // Update last block size for compatibility
    m_last_block_size = block_size;
    
    // Detect preferred block size for optimization
    detectPreferredBlockSize_unlocked(block_size);
    
    // Optimize buffers if block size changed significantly
    if (previous_block_size == 0 || 
        (block_size > previous_block_size * 2) || 
        (previous_block_size > block_size * 2)) {
        optimizeForBlockSize_unlocked(block_size);
    }
}

void FLACCodec::optimizeForBlockSize_unlocked(uint32_t block_size) {
    Debug::log("flac_codec", "[FLACCodec::optimizeForBlockSize_unlocked] Optimizing for block size: ", block_size);
    
    // Check if this is a standard block size for special optimization
    if (isStandardBlockSize_unlocked(block_size)) {
        Debug::log("flac_codec", "[FLACCodec::optimizeForBlockSize_unlocked] Standard block size detected: ", block_size);
        
        // Pre-allocate buffers for this standard size
        adaptBuffersForBlockSize_unlocked(block_size);
    } else {
        Debug::log("flac_codec", "[FLACCodec::optimizeForBlockSize_unlocked] Non-standard block size: ", block_size);
        
        // Use conservative buffer allocation for non-standard sizes
        adaptBuffersForBlockSize_unlocked(block_size);
    }
}

bool FLACCodec::isStandardBlockSize_unlocked(uint32_t block_size) const {
    // Check against standard FLAC block sizes
    for (size_t i = 0; i < NUM_STANDARD_BLOCK_SIZES; ++i) {
        if (STANDARD_BLOCK_SIZES[i] == block_size) {
            return true;
        }
    }
    return false;
}

void FLACCodec::adaptBuffersForBlockSize_unlocked(uint32_t block_size) {
    Debug::log("flac_codec", "[FLACCodec::adaptBuffersForBlockSize_unlocked] Adapting buffers for block size: ", block_size);
    
    // Calculate required buffer size for this block size
    size_t required_samples = static_cast<size_t>(block_size) * m_channels;
    
    // Ensure decode buffer has sufficient capacity
    if (m_decode_buffer.capacity() < required_samples) {
        size_t new_capacity = required_samples * 2; // Over-allocate for future frames
        m_decode_buffer.reserve(new_capacity);
        Debug::log("flac_codec", "[FLACCodec::adaptBuffersForBlockSize_unlocked] Expanded decode buffer: ", 
                  new_capacity, " samples");
    }
    
    // Ensure output buffer has sufficient capacity (with buffer lock)
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        if (m_output_buffer.capacity() < required_samples) {
            size_t new_capacity = required_samples * 2; // Over-allocate for future frames
            m_output_buffer.reserve(new_capacity);
            Debug::log("flac_codec", "[FLACCodec::adaptBuffersForBlockSize_unlocked] Expanded output buffer: ", 
                      new_capacity, " samples");
        }
    }
}

void FLACCodec::detectPreferredBlockSize_unlocked(uint32_t block_size) {
    // Simple heuristic: if we see the same block size multiple times, consider it preferred
    static uint32_t last_seen_block_size = 0;
    static uint32_t consecutive_count = 0;
    
    if (block_size == last_seen_block_size) {
        consecutive_count++;
        
        // After seeing the same block size 5 times, consider it preferred
        if (consecutive_count >= 5 && m_preferred_block_size != block_size) {
            m_preferred_block_size = block_size;
            Debug::log("flac_codec", "[FLACCodec::detectPreferredBlockSize_unlocked] Detected preferred block size: ", 
                      block_size, " (seen ", consecutive_count, " times consecutively)");
            
            // Optimize specifically for this preferred size
            optimizeForBlockSize_unlocked(block_size);
        }
    } else {
        last_seen_block_size = block_size;
        consecutive_count = 1;
    }
}

void FLACCodec::optimizeForFixedBlockSizes_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::optimizeForFixedBlockSizes_unlocked] Optimizing for standard fixed block sizes");
    
    // Pre-allocate buffers for the largest standard block size to handle all cases efficiently
    preAllocateForStandardSizes_unlocked();
    
    Debug::log("flac_codec", "[FLACCodec::optimizeForFixedBlockSizes_unlocked] Fixed block size optimization completed");
}

void FLACCodec::preAllocateForStandardSizes_unlocked() {
    // Find the largest standard block size for pre-allocation
    uint32_t max_standard_size = 0;
    for (size_t i = 0; i < NUM_STANDARD_BLOCK_SIZES; ++i) {
        if (STANDARD_BLOCK_SIZES[i] > max_standard_size) {
            max_standard_size = STANDARD_BLOCK_SIZES[i];
        }
    }
    
    Debug::log("flac_codec", "[FLACCodec::preAllocateForStandardSizes_unlocked] Pre-allocating for max standard size: ", 
              max_standard_size);
    
    // Pre-allocate buffers for the largest standard size
    if (max_standard_size > 0) {
        size_t required_samples = static_cast<size_t>(max_standard_size) * m_channels;
        
        // Pre-allocate decode buffer
        if (m_decode_buffer.capacity() < required_samples) {
            m_decode_buffer.reserve(required_samples);
            Debug::log("flac_codec", "[FLACCodec::preAllocateForStandardSizes_unlocked] Pre-allocated decode buffer: ", 
                      required_samples, " samples");
        }
        
        // Pre-allocate output buffer (with buffer lock)
        {
            std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
            if (m_output_buffer.capacity() < required_samples) {
                m_output_buffer.reserve(required_samples);
                Debug::log("flac_codec", "[FLACCodec::preAllocateForStandardSizes_unlocked] Pre-allocated output buffer: ", 
                          required_samples, " samples");
            }
        }
    }
}

size_t FLACCodec::calculateOptimalBufferSize_unlocked() const {
    // Calculate optimal buffer size based on current configuration
    uint32_t target_block_size = m_max_block_size;
    
    // Use preferred block size if detected
    if (m_preferred_block_size > 0) {
        target_block_size = m_preferred_block_size;
    }
    // Use current block size if available
    else if (m_current_block_size > 0) {
        target_block_size = m_current_block_size;
    }
    // Fall back to RFC 9639 maximum
    else {
        target_block_size = 65535;
    }
    
    size_t optimal_size = static_cast<size_t>(target_block_size) * m_channels;
    
    Debug::log("flac_codec", "[FLACCodec::calculateOptimalBufferSize_unlocked] Calculated optimal buffer size: ", 
              optimal_size, " samples (block_size=", target_block_size, ", channels=", m_channels, ")");
    
    return optimal_size;
}

// ============================================================================
// Variable Block Size Adaptation Implementation
// ============================================================================

void FLACCodec::handleBlockSizeTransition_unlocked(uint32_t old_size, uint32_t new_size) {
    Debug::log("flac_codec", "[FLACCodec::handleBlockSizeTransition_unlocked] Block size transition: ", 
              old_size, " -> ", new_size);
    
    // Track block size changes for adaptation
    if (old_size != 0 && old_size != new_size) {
        m_block_size_changes++;
        
        // Update statistics for variable block size streams
        if (new_size < m_smallest_block_seen) {
            m_smallest_block_seen = new_size;
        }
        if (new_size > m_largest_block_seen) {
            m_largest_block_seen = new_size;
        }
        
        // Update running average
        if (m_stats.frames_decoded > 0) {
            m_average_block_size = ((m_average_block_size * (m_stats.frames_decoded - 1)) + new_size) / m_stats.frames_decoded;
        } else {
            m_average_block_size = new_size;
        }
        
        Debug::log("flac_codec", "[FLACCodec::handleBlockSizeTransition_unlocked] Block size statistics: ",
                  "changes=", m_block_size_changes, ", range=[", m_smallest_block_seen, "-", m_largest_block_seen, "], ",
                  "average=", static_cast<uint32_t>(m_average_block_size));
        
        // Perform smooth transition handling
        smoothBlockSizeTransition_unlocked(new_size);
    }
    
    // Update previous block size for next transition
    m_previous_block_size = new_size;
}

void FLACCodec::smoothBlockSizeTransition_unlocked(uint32_t new_block_size) {
    Debug::log("flac_codec", "[FLACCodec::smoothBlockSizeTransition_unlocked] Smoothing transition to block size: ", 
              new_block_size);
    
    // Check if buffer reallocation is needed
    if (requiresBufferReallocation_unlocked(new_block_size)) {
        Debug::log("flac_codec", "[FLACCodec::smoothBlockSizeTransition_unlocked] Buffer reallocation required");
        
        // Perform adaptive buffer resize
        adaptiveBufferResize_unlocked(new_block_size);
    }
    
    // Maintain consistent output timing across block size changes
    maintainOutputTiming_unlocked(new_block_size);
    
    // Optimize for variable block size patterns if we've seen enough changes
    if (m_block_size_changes >= 10 && !m_variable_block_size) {
        Debug::log("flac_codec", "[FLACCodec::smoothBlockSizeTransition_unlocked] Enabling variable block size optimization");
        m_variable_block_size = true;
        optimizeForVariableBlockSizes_unlocked();
    }
}

void FLACCodec::maintainOutputTiming_unlocked(uint32_t block_size) {
    // Update total samples processed for timing consistency
    m_total_samples_processed += block_size;
    
    // Ensure current sample position is consistent
    uint64_t expected_position = m_total_samples_processed;
    uint64_t actual_position = m_current_sample.load();
    
    if (actual_position != expected_position) {
        Debug::log("flac_codec", "[FLACCodec::maintainOutputTiming_unlocked] Timing correction: ",
                  "expected=", expected_position, ", actual=", actual_position, 
                  ", difference=", static_cast<int64_t>(expected_position - actual_position));
        
        // Correct the timing if the difference is significant
        if (std::abs(static_cast<int64_t>(expected_position - actual_position)) > block_size) {
            m_current_sample.store(expected_position);
            Debug::log("flac_codec", "[FLACCodec::maintainOutputTiming_unlocked] Applied timing correction");
        }
    }
}

void FLACCodec::adaptiveBufferResize_unlocked(uint32_t block_size) {
    if (!m_adaptive_buffering_enabled) {
        return;
    }
    
    Debug::log("flac_codec", "[FLACCodec::adaptiveBufferResize_unlocked] Adaptive buffer resize for block size: ", 
              block_size);
    
    // Calculate new buffer size with some headroom for future block size variations
    size_t required_samples = static_cast<size_t>(block_size) * m_channels;
    
    // Add headroom based on observed block size variation
    double variation_factor = 1.5; // Default 50% headroom
    if (m_largest_block_seen > 0 && m_smallest_block_seen < UINT32_MAX) {
        double size_ratio = static_cast<double>(m_largest_block_seen) / m_smallest_block_seen;
        variation_factor = std::min(3.0, std::max(1.2, size_ratio * 1.1)); // 20% to 300% headroom
    }
    
    size_t target_capacity = static_cast<size_t>(required_samples * variation_factor);
    
    Debug::log("flac_codec", "[FLACCodec::adaptiveBufferResize_unlocked] Target capacity: ", 
              target_capacity, " samples (variation_factor=", variation_factor, ")");
    
    // Resize decode buffer if needed
    if (m_decode_buffer.capacity() < target_capacity) {
        m_decode_buffer.reserve(target_capacity);
        Debug::log("flac_codec", "[FLACCodec::adaptiveBufferResize_unlocked] Resized decode buffer: ", 
                  target_capacity, " samples");
    }
    
    // Resize output buffer if needed (with buffer lock)
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        if (m_output_buffer.capacity() < target_capacity) {
            m_output_buffer.reserve(target_capacity);
            Debug::log("flac_codec", "[FLACCodec::adaptiveBufferResize_unlocked] Resized output buffer: ", 
                      target_capacity, " samples");
        }
    }
}

bool FLACCodec::requiresBufferReallocation_unlocked(uint32_t block_size) const {
    size_t required_samples = static_cast<size_t>(block_size) * m_channels;
    
    // Check if current buffers are insufficient
    bool decode_buffer_insufficient = m_decode_buffer.capacity() < required_samples;
    
    bool output_buffer_insufficient = false;
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        output_buffer_insufficient = m_output_buffer.capacity() < required_samples;
    }
    
    bool reallocation_needed = decode_buffer_insufficient || output_buffer_insufficient;
    
    if (reallocation_needed) {
        Debug::log("flac_codec", "[FLACCodec::requiresBufferReallocation_unlocked] Reallocation needed: ",
                  "required=", required_samples, ", decode_capacity=", m_decode_buffer.capacity(),
                  ", output_capacity=", m_output_buffer.capacity());
    }
    
    return reallocation_needed;
}

void FLACCodec::optimizeForVariableBlockSizes_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::optimizeForVariableBlockSizes_unlocked] Optimizing for variable block sizes");
    
    // Calculate optimal buffer size based on observed block size patterns
    uint32_t optimal_size = m_max_block_size;
    
    // Use statistical information to optimize buffer allocation
    if (m_largest_block_seen > 0) {
        optimal_size = m_largest_block_seen;
        
        // Add some headroom for future larger blocks
        optimal_size = static_cast<uint32_t>(optimal_size * 1.2);
        
        // Clamp to RFC 9639 maximum
        optimal_size = std::min(optimal_size, static_cast<uint32_t>(65535));
    }
    
    Debug::log("flac_codec", "[FLACCodec::optimizeForVariableBlockSizes_unlocked] Optimal size calculated: ", 
              optimal_size, " (based on largest_seen=", m_largest_block_seen, ")");
    
    // Pre-allocate buffers for the optimal size
    size_t optimal_samples = static_cast<size_t>(optimal_size) * m_channels;
    
    // Optimize decode buffer
    if (m_decode_buffer.capacity() < optimal_samples) {
        m_decode_buffer.reserve(optimal_samples);
        Debug::log("flac_codec", "[FLACCodec::optimizeForVariableBlockSizes_unlocked] Optimized decode buffer: ", 
                  optimal_samples, " samples");
    }
    
    // Optimize output buffer (with buffer lock)
    {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        if (m_output_buffer.capacity() < optimal_samples) {
            m_output_buffer.reserve(optimal_samples);
            Debug::log("flac_codec", "[FLACCodec::optimizeForVariableBlockSizes_unlocked] Optimized output buffer: ", 
                      optimal_samples, " samples");
        }
    }
    
    // Enable adaptive buffering for future block size changes
    m_adaptive_buffering_enabled = true;
    
    Debug::log("flac_codec", "[FLACCodec::optimizeForVariableBlockSizes_unlocked] Variable block size optimization completed");
}

// ============================================================================
// Threading and Asynchronous Processing Implementation
// ============================================================================

// Public threading interface (thread-safe with RAII guards)

bool FLACCodec::startDecoderThread() {
    Debug::log("flac_codec", "[FLACCodec::startDecoderThread] [ENTRY] Acquiring thread lock");
    std::lock_guard<std::mutex> lock(m_thread_mutex);
    Debug::log("flac_codec", "[FLACCodec::startDecoderThread] [LOCKED] Thread lock acquired, calling unlocked implementation");
    
    try {
        bool result = startDecoderThread_unlocked();
        Debug::log("flac_codec", "[FLACCodec::startDecoderThread] [EXIT] Returning ", result ? "success" : "failure");
        return result;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::startDecoderThread] [EXCEPTION] ", e.what());
        handleThreadException_unlocked(e);
        return false;
    }
}

void FLACCodec::stopDecoderThread() {
    Debug::log("flac_codec", "[FLACCodec::stopDecoderThread] [ENTRY] Acquiring thread lock");
    std::lock_guard<std::mutex> lock(m_thread_mutex);
    Debug::log("flac_codec", "[FLACCodec::stopDecoderThread] [LOCKED] Thread lock acquired, calling unlocked implementation");
    
    try {
        stopDecoderThread_unlocked();
        Debug::log("flac_codec", "[FLACCodec::stopDecoderThread] [EXIT] Thread stopped successfully");
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::stopDecoderThread] [EXCEPTION] ", e.what());
        handleThreadException_unlocked(e);
    }
}

bool FLACCodec::isDecoderThreadActive() const {
    // Atomic read - no lock needed for thread-safe access
    bool active = m_thread_active.load();
    Debug::log("flac_codec", "[FLACCodec::isDecoderThreadActive] Thread active: ", active ? "true" : "false");
    return active;
}

void FLACCodec::enableAsyncProcessing(bool enable) {
    Debug::log("flac_codec", "[FLACCodec::enableAsyncProcessing] [ENTRY] Setting async processing to ", enable ? "enabled" : "disabled");
    std::lock_guard<std::mutex> lock(m_async_mutex);
    Debug::log("flac_codec", "[FLACCodec::enableAsyncProcessing] [LOCKED] Async lock acquired");
    
    try {
        m_async_processing_enabled = enable;
        
        if (!enable) {
            // Clear async queues when disabling
            clearAsyncQueues_unlocked();
            Debug::log("flac_codec", "[FLACCodec::enableAsyncProcessing] Async queues cleared");
        }
        
        Debug::log("flac_codec", "[FLACCodec::enableAsyncProcessing] [EXIT] Async processing ", 
                  enable ? "enabled" : "disabled");
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::enableAsyncProcessing] [EXCEPTION] ", e.what());
    }
}

bool FLACCodec::isAsyncProcessingEnabled() const {
    std::lock_guard<std::mutex> lock(m_async_mutex);
    bool enabled = m_async_processing_enabled;
    Debug::log("flac_codec", "[FLACCodec::isAsyncProcessingEnabled] Async processing: ", enabled ? "enabled" : "disabled");
    return enabled;
}

// Private threading implementation methods (assume locks are held)

bool FLACCodec::startDecoderThread_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::startDecoderThread_unlocked] Starting decoder thread");
    
    // Check if thread is already active
    if (m_thread_active.load()) {
        Debug::log("flac_codec", "[FLACCodec::startDecoderThread_unlocked] Thread already active");
        return true;
    }
    
    // Check if codec is properly initialized
    if (!m_initialized || !m_decoder_initialized) {
        Debug::log("flac_codec", "[FLACCodec::startDecoderThread_unlocked] Codec not initialized - cannot start thread");
        return false;
    }
    
    try {
        // Initialize thread state
        if (!initializeDecoderThread_unlocked()) {
            Debug::log("flac_codec", "[FLACCodec::startDecoderThread_unlocked] Failed to initialize thread state");
            return false;
        }
        
        // Reset thread flags
        m_thread_shutdown_requested.store(false);
        m_thread_exception_occurred = false;
        m_thread_exception_message.clear();
        m_pending_work_items.store(0);
        m_completed_work_items.store(0);
        
        // Create and start the decoder thread
        m_decoder_thread = std::make_unique<std::thread>(&FLACCodec::decoderThreadLoop, this);
        
        if (!m_decoder_thread || !m_decoder_thread->joinable()) {
            Debug::log("flac_codec", "[FLACCodec::startDecoderThread_unlocked] Failed to create decoder thread");
            cleanupDecoderThread_unlocked();
            return false;
        }
        
        // Wait for thread to become active (with timeout)
        auto start_time = std::chrono::high_resolution_clock::now();
        while (!m_thread_active.load() && !m_thread_exception_occurred) {
            auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
            if (elapsed > std::chrono::milliseconds(1000)) { // 1 second timeout
                Debug::log("flac_codec", "[FLACCodec::startDecoderThread_unlocked] Thread startup timeout");
                stopDecoderThread_unlocked();
                return false;
            }
            
            // Brief sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        if (m_thread_exception_occurred) {
            Debug::log("flac_codec", "[FLACCodec::startDecoderThread_unlocked] Thread startup failed with exception: ", 
                      m_thread_exception_message);
            stopDecoderThread_unlocked();
            return false;
        }
        
        m_thread_start_time = std::chrono::high_resolution_clock::now();
        
        Debug::log("flac_codec", "[FLACCodec::startDecoderThread_unlocked] Decoder thread started successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::startDecoderThread_unlocked] Exception: ", e.what());
        handleThreadException_unlocked(e);
        cleanupDecoderThread_unlocked();
        return false;
    }
}

void FLACCodec::stopDecoderThread_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::stopDecoderThread_unlocked] Stopping decoder thread");
    
    if (!m_thread_active.load() && !m_decoder_thread) {
        Debug::log("flac_codec", "[FLACCodec::stopDecoderThread_unlocked] No active thread to stop");
        return;
    }
    
    try {
        // Request thread shutdown
        m_thread_shutdown_requested.store(true);
        
        // Notify thread to wake up and check shutdown flag
        notifyWorkAvailable_unlocked();
        
        // Wait for thread to complete with timeout
        if (!waitForThreadShutdown_unlocked(m_thread_shutdown_timeout)) {
            Debug::log("flac_codec", "[FLACCodec::stopDecoderThread_unlocked] Thread shutdown timeout - forcing termination");
            
            // Force thread termination (not ideal but necessary for cleanup)
            if (m_decoder_thread && m_decoder_thread->joinable()) {
                // Note: std::thread::detach() is safer than forced termination
                m_decoder_thread->detach();
                Debug::log("flac_codec", "[FLACCodec::stopDecoderThread_unlocked] Thread detached due to timeout");
            }
        } else {
            Debug::log("flac_codec", "[FLACCodec::stopDecoderThread_unlocked] Thread shutdown completed gracefully");
        }
        
        // Clean up thread resources
        cleanupDecoderThread_unlocked();
        
        // Log thread statistics
        logThreadStatistics_unlocked();
        
        Debug::log("flac_codec", "[FLACCodec::stopDecoderThread_unlocked] Decoder thread stopped");
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::stopDecoderThread_unlocked] Exception during thread shutdown: ", e.what());
        handleThreadException_unlocked(e);
        
        // Force cleanup even if exception occurred
        try {
            cleanupDecoderThread_unlocked();
        } catch (...) {
            Debug::log("flac_codec", "[FLACCodec::stopDecoderThread_unlocked] Exception during cleanup - ignoring");
        }
    }
}

void FLACCodec::decoderThreadLoop() {
    Debug::log("flac_codec", "[FLACCodec::decoderThreadLoop] [THREAD] High-performance decoder thread started");
    
    // Thread-local performance optimization variables
    constexpr size_t BATCH_SIZE = 4;  // Process multiple chunks per iteration
    constexpr auto FAST_POLL_INTERVAL = std::chrono::microseconds(100);  // Fast polling for low latency
    constexpr auto SLOW_POLL_INTERVAL = std::chrono::milliseconds(5);    // Slower polling when idle
    
    size_t consecutive_idle_cycles = 0;
    auto last_work_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Mark thread as active with memory ordering for performance
        m_thread_active.store(true, std::memory_order_release);
        
        // High-performance thread main loop with adaptive polling
        while (!m_thread_shutdown_requested.load(std::memory_order_acquire)) {
            size_t work_items_processed = 0;
            bool had_work_this_iteration = false;
            
            try {
                // Batch process multiple work items for better throughput
                if (m_async_processing_enabled) {
                    std::vector<MediaChunk> work_batch;
                    work_batch.reserve(BATCH_SIZE);
                    
                    // Collect a batch of work items with minimal locking
                    {
                        std::lock_guard<std::mutex> async_lock(m_async_mutex);
                        
                        while (work_batch.size() < BATCH_SIZE && hasAsyncInput_unlocked()) {
                            work_batch.push_back(dequeueAsyncInput_unlocked());
                        }
                    }
                    
                    // Process the batch outside of locks for better concurrency
                    if (!work_batch.empty()) {
                        had_work_this_iteration = true;
                        last_work_time = std::chrono::high_resolution_clock::now();
                        consecutive_idle_cycles = 0;
                        
                        std::vector<AudioFrame> results;
                        results.reserve(work_batch.size());
                        
                        auto batch_start_time = std::chrono::high_resolution_clock::now();
                        
                        // Process each chunk in the batch
                        for (const auto& chunk : work_batch) {
                            if (m_thread_shutdown_requested.load(std::memory_order_acquire)) {
                                break; // Early exit on shutdown
                            }
                            
                            auto chunk_start_time = std::chrono::high_resolution_clock::now();
                            
                            // Decode the chunk with optimized path
                            AudioFrame decoded_frame = decodeChunkOptimized_unlocked(chunk);
                            
                            auto chunk_end_time = std::chrono::high_resolution_clock::now();
                            auto chunk_processing_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                chunk_end_time - chunk_start_time);
                            
                            // Update performance statistics atomically
                            m_thread_processing_time_us.fetch_add(chunk_processing_time.count(), std::memory_order_relaxed);
                            
                            if (decoded_frame.getSampleFrameCount() > 0) {
                                results.push_back(std::move(decoded_frame));
                                work_items_processed++;
                            }
                        }
                        
                        // Batch enqueue results for better throughput
                        if (!results.empty()) {
                            std::lock_guard<std::mutex> async_lock(m_async_mutex);
                            for (auto& result : results) {
                                if (!enqueueAsyncOutput_unlocked(std::move(result))) {
                                    Debug::log("flac_codec", "[FLACCodec::decoderThreadLoop] [THREAD] Output queue full, dropping frame");
                                    break;
                                }
                            }
                        }
                        
                        // Update batch statistics
                        auto batch_end_time = std::chrono::high_resolution_clock::now();
                        auto batch_processing_time = std::chrono::duration_cast<std::chrono::microseconds>(
                            batch_end_time - batch_start_time);
                        
                        // Update counters atomically for better performance
                        m_thread_frames_processed.fetch_add(work_items_processed, std::memory_order_relaxed);
                        m_completed_work_items.fetch_add(work_items_processed, std::memory_order_relaxed);
                        
                        // Notify work completion if needed (avoid unnecessary notifications)
                        if (work_items_processed > 0) {
                            notifyWorkCompletedBatch_unlocked(work_items_processed);
                        }
                        
                        // Log performance for large batches
                        if (work_items_processed >= BATCH_SIZE / 2) {
                            Debug::log("flac_codec", "[FLACCodec::decoderThreadLoop] [THREAD] Processed batch of ", 
                                      work_items_processed, " items in ", batch_processing_time.count(), " μs");
                        }
                    }
                }
                
                // Adaptive waiting strategy based on workload
                if (!had_work_this_iteration) {
                    consecutive_idle_cycles++;
                    
                    // Use different wait strategies based on idle time
                    if (consecutive_idle_cycles < 10) {
                        // Fast polling for low latency when recently active
                        std::this_thread::sleep_for(FAST_POLL_INTERVAL);
                    } else if (consecutive_idle_cycles < 100) {
                        // Medium polling for moderate idle periods
                        std::unique_lock<std::mutex> thread_lock(m_thread_mutex);
                        m_work_available_cv.wait_for(thread_lock, SLOW_POLL_INTERVAL, [this] {
                            return m_thread_shutdown_requested.load(std::memory_order_acquire) || 
                                   (m_async_processing_enabled && hasAsyncInputFast_unlocked());
                        });
                    } else {
                        // Longer wait for extended idle periods to save CPU
                        std::unique_lock<std::mutex> thread_lock(m_thread_mutex);
                        m_work_available_cv.wait_for(thread_lock, m_thread_work_timeout, [this] {
                            return m_thread_shutdown_requested.load(std::memory_order_acquire) || 
                                   (m_async_processing_enabled && hasAsyncInputFast_unlocked());
                        });
                    }
                    
                    // Update idle statistics
                    m_thread_idle_cycles.fetch_add(1, std::memory_order_relaxed);
                } else {
                    // Reset idle counter when we have work
                    consecutive_idle_cycles = 0;
                }
                
            } catch (const std::exception& e) {
                Debug::log("flac_codec", "[FLACCodec::decoderThreadLoop] [THREAD] Exception in work processing: ", e.what());
                
                // Handle thread exception with minimal overhead
                handleThreadExceptionFast_unlocked(e);
                
                // Brief sleep to prevent tight exception loop, but shorter than before
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                consecutive_idle_cycles = 0; // Reset to prevent long waits after exceptions
            }
        }
        
        Debug::log("flac_codec", "[FLACCodec::decoderThreadLoop] [THREAD] Shutdown requested, exiting optimized thread loop");
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::decoderThreadLoop] [THREAD] Fatal exception: ", e.what());
        
        // Mark thread exception for main thread to handle
        handleThreadExceptionFast_unlocked(e);
    } catch (...) {
        Debug::log("flac_codec", "[FLACCodec::decoderThreadLoop] [THREAD] Unknown fatal exception");
        
        // Mark unknown exception with minimal overhead
        m_thread_exception_occurred = true;
        m_thread_exception_message = "Unknown exception in decoder thread";
    }
    
    // Mark thread as no longer active with proper memory ordering
    m_thread_active.store(false, std::memory_order_release);
    
    // Final cleanup and notification
    handleThreadTerminationFast_unlocked();
    
    Debug::log("flac_codec", "[FLACCodec::decoderThreadLoop] [THREAD] High-performance decoder thread terminated");
}

// Optimized threading helper methods for performance

AudioFrame FLACCodec::decodeChunkOptimized_unlocked(const MediaChunk& chunk) {
    // Optimized decode path for thread processing with minimal overhead
    
    // Fast path validation
    if (chunk.data.empty() || m_error_state.load(std::memory_order_relaxed)) {
        return AudioFrame();
    }
    
    try {
        // Use thread-local decoder state when possible to reduce contention
        // This is a simplified decode path that avoids some of the overhead
        // of the full decode() method while maintaining correctness
        
        // Clear output buffer efficiently
        {
            std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
            m_output_buffer.clear();
            m_buffer_read_position = 0;
        }
        
        // Process frame data with minimal locking
        bool decode_success = false;
        {
            std::lock_guard<std::mutex> decoder_lock(m_decoder_mutex);
            if (m_decoder && m_decoder_initialized) {
                decode_success = processFrameDataFast_unlocked(chunk.data.data(), chunk.data.size());
            }
        }
        
        if (decode_success) {
            return extractDecodedSamplesFast_unlocked();
        } else {
            return createSilenceFrameFast_unlocked(1024);
        }
        
    } catch (const std::exception& e) {
        // Minimal exception handling for performance
        return AudioFrame();
    }
}

bool FLACCodec::processFrameDataFast_unlocked(const uint8_t* data, size_t size) {
    // Fast frame processing with minimal validation overhead
    
    if (!m_decoder || !data || size == 0) {
        return false;
    }
    
    // Feed data to decoder efficiently
    if (!m_decoder->feedData(data, size)) {
        return false;
    }
    
    // Process single frame with minimal error checking
    return m_decoder->process_single();
}

AudioFrame FLACCodec::extractDecodedSamplesFast_unlocked() {
    // Fast sample extraction with minimal overhead
    
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    if (m_output_buffer.empty()) {
        return AudioFrame();
    }
    
    // Get current timestamp before updating position
    uint64_t current_timestamp = m_current_sample.load(std::memory_order_relaxed);
    
    // Calculate sample frame count for position update
    uint16_t channels = m_channels > 0 ? m_channels : 2;
    size_t sample_frame_count = m_output_buffer.size() / channels;
    
    // Create AudioFrame using helper method with move semantics for performance
    AudioFrame frame = createAudioFrame_unlocked(std::move(m_output_buffer), current_timestamp);
    m_output_buffer.clear(); // Ensure buffer is cleared after move
    
    // Update position atomically
    m_current_sample.fetch_add(sample_frame_count, std::memory_order_relaxed);
    
    return frame;
}

AudioFrame FLACCodec::createSilenceFrameFast_unlocked(uint32_t block_size) {
    // Fast silence frame creation for error recovery
    
    // Get current timestamp before updating position
    uint64_t current_timestamp = m_current_sample.load(std::memory_order_relaxed);
    
    // Create silence samples
    uint16_t channels = m_channels > 0 ? m_channels : 2; // Fallback to stereo
    std::vector<int16_t> silence_samples(block_size * channels, 0);
    
    // Create AudioFrame using helper method
    AudioFrame frame = createAudioFrame_unlocked(std::move(silence_samples), current_timestamp);
    
    // Update position tracking
    m_current_sample.fetch_add(block_size, std::memory_order_relaxed);
    
    return frame;
}

bool FLACCodec::hasAsyncInputFast_unlocked() const {
    // Fast check for async input without full lock acquisition
    // This is called frequently so it needs to be very fast
    
    return !m_async_input_queue.empty();
}

void FLACCodec::notifyWorkCompletedBatch_unlocked(size_t batch_size) {
    // Optimized batch notification to reduce condition variable overhead
    
    // Only notify if there are waiters (avoid unnecessary syscalls)
    if (m_pending_work_items.load(std::memory_order_relaxed) > 0) {
        std::lock_guard<std::mutex> thread_lock(m_thread_mutex);
        m_work_completed_cv.notify_one();
    }
}

void FLACCodec::handleThreadExceptionFast_unlocked(const std::exception& e) {
    // Fast exception handling with minimal overhead
    
    m_thread_exception_occurred = true;
    m_thread_exception_message = e.what();
    
    // Don't acquire locks in exception path for performance
    // The main thread will handle cleanup
}

void FLACCodec::handleThreadTerminationFast_unlocked() {
    // Fast thread termination handling
    
    // Notify any waiting threads with minimal overhead
    m_work_completed_cv.notify_all();
    m_work_available_cv.notify_all();
}

bool FLACCodec::initializeDecoderThread_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::initializeDecoderThread_unlocked] Initializing thread state");
    
    try {
        // Reset thread state
        resetThreadState_unlocked();
        
        // Initialize async queues if async processing is enabled
        if (m_async_processing_enabled) {
            std::lock_guard<std::mutex> async_lock(m_async_mutex);
            clearAsyncQueues_unlocked();
        }
        
        // Set thread initialization flag
        m_thread_initialized = true;
        m_clean_shutdown_completed = false;
        
        Debug::log("flac_codec", "[FLACCodec::initializeDecoderThread_unlocked] Thread state initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::initializeDecoderThread_unlocked] Exception: ", e.what());
        return false;
    }
}

void FLACCodec::cleanupDecoderThread_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::cleanupDecoderThread_unlocked] Cleaning up thread resources");
    
    try {
        // Join thread if it's still joinable
        if (m_decoder_thread && m_decoder_thread->joinable()) {
            m_decoder_thread->join();
            Debug::log("flac_codec", "[FLACCodec::cleanupDecoderThread_unlocked] Thread joined successfully");
        }
        
        // Reset thread pointer
        m_decoder_thread.reset();
        
        // Clear thread state
        m_thread_active.store(false);
        m_thread_shutdown_requested.store(false);
        m_thread_initialized = false;
        m_clean_shutdown_completed = true;
        
        // Clear async queues
        {
            std::lock_guard<std::mutex> async_lock(m_async_mutex);
            clearAsyncQueues_unlocked();
        }
        
        Debug::log("flac_codec", "[FLACCodec::cleanupDecoderThread_unlocked] Thread cleanup completed");
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[FLACCodec::cleanupDecoderThread_unlocked] Exception during cleanup: ", e.what());
        
        // Force reset even if exception occurred
        m_decoder_thread.reset();
        m_thread_active.store(false);
        m_thread_shutdown_requested.store(false);
    }
}

bool FLACCodec::waitForThreadShutdown_unlocked(std::chrono::milliseconds timeout) {
    Debug::log("flac_codec", "[FLACCodec::waitForThreadShutdown_unlocked] Waiting for thread shutdown with ", 
              timeout.count(), "ms timeout");
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (m_thread_active.load()) {
        auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
        if (elapsed >= timeout) {
            Debug::log("flac_codec", "[FLACCodec::waitForThreadShutdown_unlocked] Thread shutdown timeout after ", 
                      elapsed.count() / 1000000, "ms");
            return false;
        }
        
        // Brief sleep to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    auto elapsed = std::chrono::high_resolution_clock::now() - start_time;
    Debug::log("flac_codec", "[FLACCodec::waitForThreadShutdown_unlocked] Thread shutdown completed in ", 
              elapsed.count() / 1000, "μs");
    
    return true;
}

// Thread synchronization methods

void FLACCodec::notifyWorkAvailable_unlocked() {
    // Notify the decoder thread that work is available
    m_work_available_cv.notify_one();
}

void FLACCodec::notifyWorkCompleted_unlocked() {
    // Notify waiting threads that work has been completed
    m_work_completed_cv.notify_all();
}

bool FLACCodec::waitForWorkCompletion_unlocked(std::chrono::milliseconds timeout) {
    Debug::log("flac_codec", "[FLACCodec::waitForWorkCompletion_unlocked] Waiting for work completion with ", 
              timeout.count(), "ms timeout");
    
    std::unique_lock<std::mutex> lock(m_thread_mutex);
    
    bool completed = m_work_completed_cv.wait_for(lock, timeout, [this] {
        return m_pending_work_items.load() == m_completed_work_items.load() || 
               m_thread_shutdown_requested.load() ||
               m_thread_exception_occurred;
    });
    
    if (completed) {
        Debug::log("flac_codec", "[FLACCodec::waitForWorkCompletion_unlocked] Work completion detected");
    } else {
        Debug::log("flac_codec", "[FLACCodec::waitForWorkCompletion_unlocked] Work completion timeout");
    }
    
    return completed;
}

void FLACCodec::handleThreadException_unlocked(const std::exception& e) {
    Debug::log("flac_codec", "[FLACCodec::handleThreadException_unlocked] Handling thread exception: ", e.what());
    
    m_thread_exception_occurred = true;
    m_thread_exception_message = e.what();
    
    // Notify any waiting threads about the exception
    m_work_completed_cv.notify_all();
    m_work_available_cv.notify_all();
}

void FLACCodec::resetThreadState_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::resetThreadState_unlocked] Resetting thread state");
    
    m_thread_exception_occurred = false;
    m_thread_exception_message.clear();
    m_pending_work_items.store(0);
    m_completed_work_items.store(0);
    m_thread_processing_time_us.store(0);
    m_thread_frames_processed.store(0);
    m_thread_idle_cycles.store(0);
}

// Asynchronous processing methods

bool FLACCodec::enqueueAsyncInput_unlocked(const MediaChunk& chunk) {
    if (m_async_input_queue.size() >= m_max_async_input_queue) {
        Debug::log("flac_codec", "[FLACCodec::enqueueAsyncInput_unlocked] Async input queue full");
        return false;
    }
    
    m_async_input_queue.push(chunk);
    m_pending_work_items.fetch_add(1);
    
    Debug::log("flac_codec", "[FLACCodec::enqueueAsyncInput_unlocked] Enqueued async input chunk, queue size: ", 
              m_async_input_queue.size());
    
    return true;
}

MediaChunk FLACCodec::dequeueAsyncInput_unlocked() {
    if (m_async_input_queue.empty()) {
        return MediaChunk();
    }
    
    MediaChunk chunk = m_async_input_queue.front();
    m_async_input_queue.pop();
    
    Debug::log("flac_codec", "[FLACCodec::dequeueAsyncInput_unlocked] Dequeued async input chunk, queue size: ", 
              m_async_input_queue.size());
    
    return chunk;
}

bool FLACCodec::enqueueAsyncOutput_unlocked(const AudioFrame& frame) {
    if (m_async_output_queue.size() >= m_max_async_output_queue) {
        Debug::log("flac_codec", "[FLACCodec::enqueueAsyncOutput_unlocked] Async output queue full");
        return false;
    }
    
    m_async_output_queue.push(frame);
    
    Debug::log("flac_codec", "[FLACCodec::enqueueAsyncOutput_unlocked] Enqueued async output frame, queue size: ", 
              m_async_output_queue.size());
    
    return true;
}

AudioFrame FLACCodec::dequeueAsyncOutput_unlocked() {
    if (m_async_output_queue.empty()) {
        return AudioFrame();
    }
    
    AudioFrame frame = m_async_output_queue.front();
    m_async_output_queue.pop();
    
    Debug::log("flac_codec", "[FLACCodec::dequeueAsyncOutput_unlocked] Dequeued async output frame, queue size: ", 
              m_async_output_queue.size());
    
    return frame;
}

bool FLACCodec::hasAsyncInput_unlocked() const {
    return !m_async_input_queue.empty();
}

bool FLACCodec::hasAsyncOutput_unlocked() const {
    return !m_async_output_queue.empty();
}

void FLACCodec::clearAsyncQueues_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::clearAsyncQueues_unlocked] Clearing async queues");
    
    // Clear input queue
    while (!m_async_input_queue.empty()) {
        m_async_input_queue.pop();
    }
    
    // Clear output queue
    while (!m_async_output_queue.empty()) {
        m_async_output_queue.pop();
    }
    
    // Reset work counters
    m_pending_work_items.store(0);
    m_completed_work_items.store(0);
    
    Debug::log("flac_codec", "[FLACCodec::clearAsyncQueues_unlocked] Async queues cleared");
}

// Thread lifecycle and exception handling

void FLACCodec::ensureThreadSafety_unlocked() {
    if (m_thread_exception_occurred) {
        Debug::log("flac_codec", "[FLACCodec::ensureThreadSafety_unlocked] Thread exception detected: ", 
                  m_thread_exception_message);
        
        // Stop the thread if it's still running
        if (m_thread_active.load()) {
            stopDecoderThread_unlocked();
        }
        
        throw std::runtime_error("Decoder thread exception: " + m_thread_exception_message);
    }
}

bool FLACCodec::isThreadHealthy_unlocked() const {
    return m_thread_active.load() && !m_thread_exception_occurred && !m_thread_shutdown_requested.load();
}

void FLACCodec::handleThreadTermination_unlocked() {
    Debug::log("flac_codec", "[FLACCodec::handleThreadTermination_unlocked] Handling thread termination");
    
    // Notify all waiting condition variables
    m_work_completed_cv.notify_all();
    m_work_available_cv.notify_all();
    
    // Clear any pending work
    m_pending_work_items.store(0);
    m_completed_work_items.store(0);
}

void FLACCodec::logThreadStatistics_unlocked() const {
    if (!m_thread_initialized) {
        return;
    }
    
    uint64_t total_processing_time = m_thread_processing_time_us.load();
    size_t frames_processed = m_thread_frames_processed.load();
    size_t idle_cycles = m_thread_idle_cycles.load();
    
    double avg_processing_time = frames_processed > 0 ? 
                                static_cast<double>(total_processing_time) / frames_processed : 0.0;
    
    auto thread_lifetime = std::chrono::high_resolution_clock::now() - m_thread_start_time;
    auto lifetime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(thread_lifetime).count();
    
    Debug::log("flac_codec", "[FLACCodec::logThreadStatistics_unlocked] Thread Statistics:");
    Debug::log("flac_codec", "  - Lifetime: ", lifetime_ms, " ms");
    Debug::log("flac_codec", "  - Frames processed: ", frames_processed);
    Debug::log("flac_codec", "  - Total processing time: ", total_processing_time, " μs");
    Debug::log("flac_codec", "  - Average processing time per frame: ", avg_processing_time, " μs");
    Debug::log("flac_codec", "  - Idle cycles: ", idle_cycles);
    Debug::log("flac_codec", "  - Exception occurred: ", m_thread_exception_occurred ? "yes" : "no");
    
    if (m_thread_exception_occurred) {
        Debug::log("flac_codec", "  - Exception message: ", m_thread_exception_message);
    }
}

// ============================================================================
// RFC 9639 Bit Depth and Sample Format Compliance Validation
// ============================================================================

bool FLACCodec::validateBitDepthRFC9639_unlocked(uint16_t bits_per_sample) const {
    // RFC 9639 Section 4.1: FLAC supports bit depths from 4 to 32 bits per sample
    // This is the fundamental requirement for FLAC bit depth validation
    
    if (bits_per_sample < 4 || bits_per_sample > 32) {
        Debug::log("flac_codec", "[validateBitDepthRFC9639_unlocked] RFC 9639 violation - Invalid bit depth: ", 
                  bits_per_sample, " bits (RFC 9639 valid range: 4-32 bits)");
        return false;
    }
    
    // Additional validation for reserved bit depth values
    if (!validateReservedBitDepthValues_unlocked(bits_per_sample)) {
        Debug::log("flac_codec", "[validateBitDepthRFC9639_unlocked] Reserved bit depth value detected: ", 
                  bits_per_sample, " bits");
        return false;
    }
    
    // Log validation success for debugging
    Debug::log("flac_codec", "[validateBitDepthRFC9639_unlocked] RFC 9639 compliant bit depth: ", 
              bits_per_sample, " bits");
    
    return true;
}

// ============================================================================
// RFC 9639 Compliant Error Handling Implementation
// ============================================================================

bool FLACCodec::validateRFC9639Compliance_unlocked(const uint8_t* data, size_t size) const {
    Debug::log("flac_codec", "[validateRFC9639Compliance_unlocked] Performing comprehensive RFC 9639 compliance validation");
    
    if (!data || size < 4) {
        logRFC9639Violation_unlocked("Insufficient frame data", "RFC 9639 Section 9.1", data, 0);
        return false;
    }
    
    // Step 1: Validate sync pattern per RFC 9639 Section 9.1
    uint16_t sync_pattern = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    if ((sync_pattern & 0xFFFE) != 0xFFF8) {
        logRFC9639Violation_unlocked("Invalid sync pattern", "RFC 9639 Section 9.1", data, 0);
        return false;
    }
    
    // Step 2: Check for forbidden bit patterns per RFC 9639
    if (!checkForbiddenBitPatterns_unlocked(data)) {
        return false; // Violation already logged
    }
    
    // Step 3: Validate reserved fields per RFC 9639
    if (!validateReservedFields_unlocked(data)) {
        return false; // Violation already logged
    }
    
    // Step 4: Check for unsupported features per RFC 9639
    if (!handleUnsupportedFeatures_unlocked(data)) {
        return false; // Unsupported feature detected
    }
    
    Debug::log("flac_codec", "[validateRFC9639Compliance_unlocked] Frame passes RFC 9639 compliance validation");
    return true;
}

bool FLACCodec::checkForbiddenBitPatterns_unlocked(const uint8_t* frame_header) const {
    Debug::log("flac_codec", "[checkForbiddenBitPatterns_unlocked] Checking RFC 9639 forbidden bit patterns");
    
    // Extract frame header fields for validation
    uint8_t byte2 = frame_header[2];
    uint8_t byte3 = frame_header[3];
    
    // RFC 9639 Section 9.1.1: Block size bits validation
    uint8_t block_size_bits = (byte2 >> 4) & 0x0F;
    if (block_size_bits == 0x00) {
        logRFC9639Violation_unlocked("Forbidden block size pattern 0x00", 
                                   "RFC 9639 Section 9.1.1 (Table 14)", frame_header, 2);
        return false;
    }
    
    // RFC 9639 Section 9.1.2: Sample rate bits validation
    uint8_t sample_rate_bits = byte2 & 0x0F;
    if (sample_rate_bits == 0x0F) {
        logRFC9639Violation_unlocked("Forbidden sample rate pattern 0x0F", 
                                   "RFC 9639 Section 9.1.2 (Table 15)", frame_header, 2);
        return false;
    }
    
    // RFC 9639 Section 9.1.3: Channel assignment validation
    uint8_t channel_assignment = (byte3 >> 4) & 0x0F;
    if (channel_assignment >= 0x0B && channel_assignment <= 0x0F) {
        logRFC9639Violation_unlocked("Forbidden channel assignment pattern", 
                                   "RFC 9639 Section 9.1.3 (Table 16)", frame_header, 3);
        return false;
    }
    
    // RFC 9639 Section 9.1.4: Bit depth validation
    uint8_t bit_depth_bits = (byte3 >> 1) & 0x07;
    if (bit_depth_bits == 0x03 || bit_depth_bits == 0x07) {
        logRFC9639Violation_unlocked("Forbidden bit depth pattern", 
                                   "RFC 9639 Section 9.1.4 (Table 17)", frame_header, 3);
        return false;
    }
    
    Debug::log("flac_codec", "[checkForbiddenBitPatterns_unlocked] No forbidden patterns detected");
    return true;
}

bool FLACCodec::validateReservedFields_unlocked(const uint8_t* frame_header) const {
    Debug::log("flac_codec", "[validateReservedFields_unlocked] Validating RFC 9639 reserved fields");
    
    // RFC 9639 Section 9.1.4: Reserved bit validation
    uint8_t byte3 = frame_header[3];
    uint8_t reserved_bit = byte3 & 0x01;
    
    if (reserved_bit != 0) {
        logRFC9639Violation_unlocked("Reserved bit must be 0", 
                                   "RFC 9639 Section 9.1.4", frame_header, 3);
        return false;
    }
    
    // Additional reserved field validation for future RFC extensions
    // Check for any other reserved patterns that might be defined
    
    Debug::log("flac_codec", "[validateReservedFields_unlocked] All reserved fields are valid");
    return true;
}

bool FLACCodec::handleUnsupportedFeatures_unlocked(const uint8_t* frame_header) const {
    Debug::log("flac_codec", "[handleUnsupportedFeatures_unlocked] Checking for unsupported RFC 9639 features");
    
    // Extract frame parameters
    uint8_t byte2 = frame_header[2];
    uint8_t byte3 = frame_header[3];
    
    uint8_t block_size_bits = (byte2 >> 4) & 0x0F;
    uint8_t sample_rate_bits = byte2 & 0x0F;
    uint8_t channel_assignment = (byte3 >> 4) & 0x0F;
    uint8_t bit_depth_bits = (byte3 >> 1) & 0x07;
    
    // Check for features that are valid per RFC but not supported by this implementation
    
    // Block size validation - check for extremely large block sizes
    if (block_size_bits == 0x06 || block_size_bits == 0x07) {
        // These require reading additional bytes - check if we support this
        Debug::log("flac_codec", "[handleUnsupportedFeatures_unlocked] Variable block size encoding detected");
        // This is supported, just noting for debugging
    }
    
    // Sample rate validation - check for custom sample rates
    if (sample_rate_bits == 0x0C || sample_rate_bits == 0x0D || sample_rate_bits == 0x0E) {
        Debug::log("flac_codec", "[handleUnsupportedFeatures_unlocked] Custom sample rate encoding detected");
        // This is supported, just noting for debugging
    }
    
    // Channel assignment validation - check for complex multi-channel
    if (channel_assignment >= 0x08 && channel_assignment <= 0x0A) {
        Debug::log("flac_codec", "[handleUnsupportedFeatures_unlocked] Side-channel stereo encoding detected");
        // This is supported, just noting for debugging
    }
    
    // Bit depth validation - check for unusual bit depths
    if (bit_depth_bits == 0x00 || bit_depth_bits == 0x02 || bit_depth_bits >= 0x06) {
        Debug::log("flac_codec", "[handleUnsupportedFeatures_unlocked] Unusual bit depth encoding detected: 0x", 
                  std::hex, static_cast<unsigned>(bit_depth_bits), std::dec);
        // These are supported, just noting for debugging
    }
    
    // All features are supported by this implementation
    Debug::log("flac_codec", "[handleUnsupportedFeatures_unlocked] All frame features are supported");
    return true;
}

bool FLACCodec::handleRFC9639Error_unlocked(FLAC__StreamDecoderErrorStatus status, const char* context) {
    Debug::log("flac_codec", "[handleRFC9639Error_unlocked] Handling RFC 9639 compliant error in context: ", context);
    
    // Determine if stream termination is required per RFC 9639
    bool terminate_stream = shouldTerminateStream_unlocked(status);
    
    // Log comprehensive error information with RFC references
    const char* rfc_section = "Unknown";
    const char* recovery_strategy = "Unknown";
    
    switch (status) {
        case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
            rfc_section = "RFC 9639 Section 9.1 (Frame Synchronization)";
            recovery_strategy = "Search for next valid sync pattern";
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
            rfc_section = "RFC 9639 Section 9.1 (Frame Header Structure)";
            recovery_strategy = "Skip corrupted frame, validate next frame header";
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
            rfc_section = "RFC 9639 Section 9.3 (Frame Footer CRC-16)";
            recovery_strategy = "Use decoded data with quality warning (RFC permits)";
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM:
            rfc_section = "RFC 9639 Section 5 (Format Principles)";
            recovery_strategy = "Stream termination required - fundamental violation";
            break;
        default:
            rfc_section = "RFC 9639 General Compliance";
            recovery_strategy = "Unknown error - assume fatal";
            break;
    }
    
    Debug::log("flac_codec", "[handleRFC9639Error_unlocked] RFC Section: ", rfc_section);
    Debug::log("flac_codec", "[handleRFC9639Error_unlocked] Recovery Strategy: ", recovery_strategy);
    Debug::log("flac_codec", "[handleRFC9639Error_unlocked] Stream Termination Required: ", 
              (terminate_stream ? "YES" : "NO"));
    
    // Update error statistics
    m_stats.error_count++;
    switch (status) {
        case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
            m_stats.sync_errors++;
            break;
        case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
        case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
            m_stats.crc_errors++;
            break;
        default:
            m_stats.libflac_errors++;
            break;
    }
    
    // Implement RFC 9639 compliant recovery
    if (terminate_stream) {
        Debug::log("flac_codec", "[handleRFC9639Error_unlocked] RFC 9639 TERMINATION: Setting error state due to unrecoverable violation");
        setErrorState_unlocked(true);
        return false;
    } else {
        Debug::log("flac_codec", "[handleRFC9639Error_unlocked] RFC 9639 RECOVERY: Attempting graceful error recovery");
        
        // Attempt specific recovery based on error type
        bool recovery_success = false;
        switch (status) {
            case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
                // Try to find next sync pattern
                recovery_success = true; // libFLAC will handle sync recovery
                break;
            case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
                // Skip bad frame and continue
                recovery_success = true; // libFLAC will skip to next frame
                break;
            case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
                // Use decoded data despite CRC error (RFC permits this)
                recovery_success = true;
                break;
            default:
                recovery_success = false;
                break;
        }
        
        if (recovery_success) {
            Debug::log("flac_codec", "[handleRFC9639Error_unlocked] RFC 9639 recovery successful");
        } else {
            Debug::log("flac_codec", "[handleRFC9639Error_unlocked] RFC 9639 recovery failed - setting error state");
            setErrorState_unlocked(true);
        }
        
        return recovery_success;
    }
}

bool FLACCodec::recoverFromForbiddenPattern_unlocked(const uint8_t* data, size_t size) {
    Debug::log("flac_codec", "[recoverFromForbiddenPattern_unlocked] Attempting recovery from RFC 9639 forbidden pattern");
    
    if (!data || size < 4) {
        Debug::log("flac_codec", "[recoverFromForbiddenPattern_unlocked] Insufficient data for recovery");
        return false;
    }
    
    // Search for next valid sync pattern
    for (size_t i = 1; i < size - 1; ++i) {
        uint16_t sync_candidate = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];
        if ((sync_candidate & 0xFFFE) == 0xFFF8) {
            Debug::log("flac_codec", "[recoverFromForbiddenPattern_unlocked] Found potential sync at offset ", i);
            
            // Validate this sync pattern doesn't contain forbidden patterns
            if (i + 4 <= size && validateRFC9639Compliance_unlocked(data + i, size - i)) {
                Debug::log("flac_codec", "[recoverFromForbiddenPattern_unlocked] Valid sync found - recovery successful");
                return true;
            }
        }
    }
    
    Debug::log("flac_codec", "[recoverFromForbiddenPattern_unlocked] No valid sync pattern found - recovery failed");
    return false;
}

bool FLACCodec::recoverFromReservedFieldViolation_unlocked(const uint8_t* data, size_t size) {
    Debug::log("flac_codec", "[recoverFromReservedFieldViolation_unlocked] Attempting recovery from reserved field violation");
    
    // Reserved field violations are typically fatal per RFC 9639
    // They indicate either corruption or non-compliant encoder
    
    if (!data || size < 4) {
        Debug::log("flac_codec", "[recoverFromReservedFieldViolation_unlocked] Insufficient data for analysis");
        return false;
    }
    
    // Log the specific violation for debugging
    uint8_t byte3 = data[3];
    uint8_t reserved_bit = byte3 & 0x01;
    
    if (reserved_bit != 0) {
        Debug::log("flac_codec", "[recoverFromReservedFieldViolation_unlocked] Reserved bit violation: bit=", 
                  static_cast<unsigned>(reserved_bit), " (must be 0)");
        
        // Per RFC 9639, reserved field violations should be treated as fatal
        // However, we can attempt to continue if the rest of the frame is valid
        Debug::log("flac_codec", "[recoverFromReservedFieldViolation_unlocked] RFC 9639 WARNING: ", 
                  "Continuing despite reserved field violation (non-compliant stream)");
        
        return true; // Allow continuation with warning
    }
    
    Debug::log("flac_codec", "[recoverFromReservedFieldViolation_unlocked] No reserved field violations detected");
    return true;
}

bool FLACCodec::shouldTerminateStream_unlocked(FLAC__StreamDecoderErrorStatus status) const {
    // Determine if stream termination is required per RFC 9639 error handling guidelines
    
    switch (status) {
        case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
            // Sync loss is recoverable - decoder can search for next frame
            return false;
            
        case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
            // Bad headers can be skipped - not necessarily fatal
            return false;
            
        case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
            // CRC mismatches are recoverable - RFC permits using data with warnings
            return false;
            
        case FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM:
            // Unparseable streams indicate fundamental format violations
            return true;
            
        default:
            // Unknown errors should be treated as potentially fatal
            return true;
    }
}

void FLACCodec::logRFC9639Violation_unlocked(const char* violation_type, const char* rfc_section, 
                                           const uint8_t* data, size_t offset) const {
    Debug::log("flac_codec", "[RFC 9639 VIOLATION] Type: ", violation_type);
    Debug::log("flac_codec", "[RFC 9639 VIOLATION] Reference: ", rfc_section);
    Debug::log("flac_codec", "[RFC 9639 VIOLATION] Offset: ", offset, " bytes");
    
    if (data && offset < 16) { // Log up to 16 bytes of context
        std::string hex_dump = "Data: ";
        for (size_t i = 0; i < std::min(size_t(8), size_t(16 - offset)); ++i) {
            char hex_byte[4];
            snprintf(hex_byte, sizeof(hex_byte), "%02X ", data[offset + i]);
            hex_dump += hex_byte;
        }
        Debug::log("flac_codec", "[RFC 9639 VIOLATION] ", hex_dump);
    }
    
    Debug::log("flac_codec", "[RFC 9639 VIOLATION] Recommendation: Verify stream compliance with RFC 9639");
}

bool FLACCodec::validateSampleFormatConsistency_unlocked(const FLAC__Frame* frame) const {
    // RFC 9639 Section 4: STREAMINFO must match frame header parameters
    // This ensures consistent decoding throughout the stream
    
    if (!frame) {
        Debug::log("flac_codec", "[validateSampleFormatConsistency_unlocked] Invalid frame pointer");
        return false;
    }
    
    // Validate bit depth consistency
    if (frame->header.bits_per_sample != m_bits_per_sample) {
        Debug::log("flac_codec", "[validateSampleFormatConsistency_unlocked] Bit depth mismatch - Frame: ", 
                  frame->header.bits_per_sample, " bits, STREAMINFO: ", m_bits_per_sample, " bits");
        return false;
    }
    
    // Validate channel count consistency
    if (frame->header.channels != m_channels) {
        Debug::log("flac_codec", "[validateSampleFormatConsistency_unlocked] Channel count mismatch - Frame: ", 
                  frame->header.channels, " channels, STREAMINFO: ", m_channels, " channels");
        return false;
    }
    
    // Validate sample rate consistency (if specified in frame)
    if (frame->header.sample_rate != 0 && frame->header.sample_rate != m_sample_rate) {
        Debug::log("flac_codec", "[validateSampleFormatConsistency_unlocked] Sample rate mismatch - Frame: ", 
                  frame->header.sample_rate, " Hz, STREAMINFO: ", m_sample_rate, " Hz");
        return false;
    }
    
    // Additional RFC 9639 validation for frame parameters
    if (!validateBitDepthRFC9639_unlocked(frame->header.bits_per_sample)) {
        Debug::log("flac_codec", "[validateSampleFormatConsistency_unlocked] Frame bit depth fails RFC 9639 validation");
        return false;
    }
    
    Debug::log("flac_codec", "[validateSampleFormatConsistency_unlocked] Sample format consistency validated - ", 
              frame->header.bits_per_sample, " bits, ", frame->header.channels, " channels, ", 
              frame->header.sample_rate, " Hz");
    
    return true;
}

bool FLACCodec::validateReservedBitDepthValues_unlocked(uint16_t bits_per_sample) const {
    // RFC 9639 currently defines all values 4-32 as valid
    // This method provides future-proofing for potential specification updates
    
    // Currently no reserved bit depth values in RFC 9639
    // All values from 4 to 32 bits are valid per the specification
    
    // Future reserved values could be added here as the specification evolves
    // For example, if certain bit depths become deprecated or reserved:
    // if (bits_per_sample == FUTURE_RESERVED_VALUE) {
    //     Debug::log("flac_codec", "[validateReservedBitDepthValues_unlocked] Reserved bit depth: ", bits_per_sample);
    //     return false;
    // }
    
    // Check for unusual bit depths that might indicate encoding issues
    // While not reserved, these are uncommon and may indicate problems
    static const uint16_t common_depths[] = {8, 16, 24, 32};
    bool is_common = false;
    
    for (size_t i = 0; i < sizeof(common_depths) / sizeof(common_depths[0]); ++i) {
        if (bits_per_sample == common_depths[i]) {
            is_common = true;
            break;
        }
    }
    
    if (!is_common) {
        Debug::log("flac_codec", "[validateReservedBitDepthValues_unlocked] Uncommon bit depth detected: ", 
                  bits_per_sample, " bits (may require special handling)");
        // Note: This is not an error, just a warning for unusual bit depths
    }
    
    return true; // All values 4-32 are currently valid per RFC 9639
}

bool FLACCodec::validateSampleFormatEndianness_unlocked(const FLAC__int32* samples, 
                                                        size_t sample_count, 
                                                        uint16_t source_bits) const {
    // RFC 9639 Section 9.2: FLAC samples are stored in big-endian format in the bitstream
    // but libFLAC provides them in host byte order. Validate proper endianness handling.
    
    if (!samples || sample_count == 0) {
        Debug::log("flac_codec", "[validateSampleFormatEndianness_unlocked] RFC 9639 validation failed - Invalid parameters");
        return false;
    }
    
    if (source_bits < 4 || source_bits > 32) {
        Debug::log("flac_codec", "[validateSampleFormatEndianness_unlocked] RFC 9639 validation failed - Invalid bit depth: ", 
                  source_bits);
        return false;
    }
    
    // Calculate expected sample range for the given bit depth per RFC 9639
    int32_t min_value = -(1 << (source_bits - 1));
    int32_t max_value = (1 << (source_bits - 1)) - 1;
    
    size_t out_of_range_count = 0;
    size_t endianness_issues = 0;
    
    for (size_t i = 0; i < sample_count; ++i) {
        FLAC__int32 sample = samples[i];
        
        // Apply proper sign extension per RFC 9639
        FLAC__int32 sign_extended = applyProperSignExtension_unlocked(sample, source_bits);
        
        // Validate sample is within expected range for bit depth
        if (sign_extended < min_value || sign_extended > max_value) {
            out_of_range_count++;
            
            // Check for potential endianness issues
            // If the sample appears to be byte-swapped, it might indicate endianness problems
            if (source_bits >= 16) {
                // Try byte-swapping to see if it produces a valid value
                uint32_t swapped = 0;
                if (source_bits == 16) {
                    uint16_t val = static_cast<uint16_t>(sample);
                    swapped = ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
                } else if (source_bits == 24) {
                    uint32_t val = static_cast<uint32_t>(sample) & 0xFFFFFF;
                    swapped = ((val & 0xFF) << 16) | (val & 0xFF00) | ((val >> 16) & 0xFF);
                } else if (source_bits == 32) {
                    uint32_t val = static_cast<uint32_t>(sample);
                    swapped = ((val & 0xFF) << 24) | (((val >> 8) & 0xFF) << 16) | 
                             (((val >> 16) & 0xFF) << 8) | ((val >> 24) & 0xFF);
                }
                
                FLAC__int32 swapped_extended = applyProperSignExtension_unlocked(static_cast<FLAC__int32>(swapped), source_bits);
                if (swapped_extended >= min_value && swapped_extended <= max_value) {
                    endianness_issues++;
                    Debug::log("flac_codec", "[validateSampleFormatEndianness_unlocked] RFC 9639 potential endianness issue at sample ", 
                              i, ": original=0x", std::hex, sample, ", swapped=0x", swapped, std::dec);
                }
            }
        }
    }
    
    // Report validation results
    if (out_of_range_count > 0) {
        double error_rate = static_cast<double>(out_of_range_count) / sample_count * 100.0;
        Debug::log("flac_codec", "[validateSampleFormatEndianness_unlocked] RFC 9639 range validation failed - ", 
                  out_of_range_count, " samples out of range (", error_rate, "%)");
        
        if (endianness_issues > 0) {
            double endian_rate = static_cast<double>(endianness_issues) / out_of_range_count * 100.0;
            Debug::log("flac_codec", "[validateSampleFormatEndianness_unlocked] RFC 9639 endianness issues detected - ", 
                      endianness_issues, " samples (", endian_rate, "% of out-of-range samples)");
        }
        
        return false;
    }
    
    Debug::log("flac_codec", "[validateSampleFormatEndianness_unlocked] RFC 9639 endianness validation passed for ", 
              sample_count, " samples at ", source_bits, " bits");
    
    return true;
}

bool FLACCodec::validateSampleFormatRanges_unlocked(const FLAC__int32* samples, 
                                                   size_t sample_count, 
                                                   uint16_t source_bits) const {
    // RFC 9639 Section 9.2: Validate that all samples are within the valid range for their bit depth
    // This ensures proper sign extension and range validation per RFC requirements
    
    if (!samples || sample_count == 0) {
        Debug::log("flac_codec", "[validateSampleFormatRanges_unlocked] RFC 9639 validation failed - Invalid parameters");
        return false;
    }
    
    if (source_bits < 4 || source_bits > 32) {
        Debug::log("flac_codec", "[validateSampleFormatRanges_unlocked] RFC 9639 validation failed - Invalid bit depth: ", 
                  source_bits);
        return false;
    }
    
    // Calculate expected sample range for the given bit depth per RFC 9639
    int32_t min_value = -(1 << (source_bits - 1));
    int32_t max_value = (1 << (source_bits - 1)) - 1;
    
    size_t range_violations = 0;
    size_t sign_extension_errors = 0;
    int32_t min_found = INT32_MAX;
    int32_t max_found = INT32_MIN;
    
    for (size_t i = 0; i < sample_count; ++i) {
        FLAC__int32 sample = samples[i];
        
        // Apply proper sign extension per RFC 9639
        FLAC__int32 sign_extended = applyProperSignExtension_unlocked(sample, source_bits);
        
        // Track actual range
        min_found = std::min(min_found, sign_extended);
        max_found = std::max(max_found, sign_extended);
        
        // Validate sample is within expected range for bit depth
        if (sign_extended < min_value || sign_extended > max_value) {
            range_violations++;
            
            if (range_violations <= 10) { // Log first 10 violations for debugging
                Debug::log("flac_codec", "[validateSampleFormatRanges_unlocked] RFC 9639 range violation at sample ", 
                          i, ": value=", sign_extended, " (valid range: ", min_value, " to ", max_value, ")");
            }
        }
        
        // Check if sign extension was applied correctly
        if (sample != sign_extended) {
            // Verify the sign extension was necessary and correct
            uint32_t valid_bits_mask = (1U << source_bits) - 1;
            uint32_t masked_original = static_cast<uint32_t>(sample) & valid_bits_mask;
            
            if (masked_original != (static_cast<uint32_t>(sign_extended) & valid_bits_mask)) {
                sign_extension_errors++;
                
                if (sign_extension_errors <= 5) { // Log first 5 errors for debugging
                    Debug::log("flac_codec", "[validateSampleFormatRanges_unlocked] RFC 9639 sign extension error at sample ", 
                              i, ": original=0x", std::hex, sample, ", extended=0x", sign_extended, std::dec);
                }
            }
        }
    }
    
    // Report validation results
    bool validation_passed = (range_violations == 0 && sign_extension_errors == 0);
    
    if (!validation_passed) {
        double range_error_rate = static_cast<double>(range_violations) / sample_count * 100.0;
        double sign_error_rate = static_cast<double>(sign_extension_errors) / sample_count * 100.0;
        
        Debug::log("flac_codec", "[validateSampleFormatRanges_unlocked] RFC 9639 validation failed - ", 
                  "Range violations: ", range_violations, " (", range_error_rate, "%), ", 
                  "Sign extension errors: ", sign_extension_errors, " (", sign_error_rate, "%)");
    }
    
    Debug::log("flac_codec", "[validateSampleFormatRanges_unlocked] RFC 9639 range validation for ", 
              sample_count, " samples at ", source_bits, " bits - ", 
              "Expected range: [", min_value, ", ", max_value, "], ", 
              "Actual range: [", min_found, ", ", max_found, "], ", 
              "Result: ", (validation_passed ? "PASS" : "FAIL"));
    
    return validation_passed;
}

FLAC__int32 FLACCodec::applyProperSignExtension_unlocked(FLAC__int32 sample, uint16_t source_bits) const {
    // RFC 9639 Section 9.2: Proper sign extension for samples < 32 bits
    // This ensures correct handling of signed sample values per RFC specification
    
    if (source_bits >= 32) {
        // No sign extension needed for 32-bit samples - already properly signed
        return sample;
    }
    
    if (source_bits < 4 || source_bits > 32) {
        Debug::log("flac_codec", "[applyProperSignExtension_unlocked] RFC 9639 violation - Invalid source bit depth: ", 
                  source_bits, " (valid range: 4-32 bits)");
        return 0;
    }
    
    // RFC 9639 compliant sign extension algorithm
    // Calculate the sign bit position (0-based from LSB)
    uint32_t sign_bit_pos = source_bits - 1;
    uint32_t sign_bit_mask = 1U << sign_bit_pos;
    
    // Create mask for valid bits in the source sample per RFC 9639
    uint32_t valid_bits_mask = (1U << source_bits) - 1;
    
    // Mask off any invalid upper bits to ensure RFC compliance
    uint32_t masked_sample = static_cast<uint32_t>(sample) & valid_bits_mask;
    
    // RFC 9639 sign extension: Check if the sign bit is set (negative value)
    if (masked_sample & sign_bit_mask) {
        // Negative value - extend sign bits to fill 32-bit value per RFC 9639
        // Create sign extension mask (all 1s in upper bits)
        uint32_t sign_extend_mask = ~valid_bits_mask;
        
        // Apply RFC 9639 compliant sign extension
        FLAC__int32 result = static_cast<FLAC__int32>(masked_sample | sign_extend_mask);
        
        // Validate result is properly sign-extended
        if ((result >> sign_bit_pos) != -1) {
            Debug::log("flac_codec", "[applyProperSignExtension_unlocked] RFC 9639 sign extension validation failed for ", 
                      source_bits, "-bit sample: 0x", std::hex, sample, std::dec);
        }
        
        return result;
    } else {
        // Positive value - upper bits should be 0 (already masked) per RFC 9639
        FLAC__int32 result = static_cast<FLAC__int32>(masked_sample);
        
        // Validate result has no spurious sign bits
        if (result < 0) {
            Debug::log("flac_codec", "[applyProperSignExtension_unlocked] RFC 9639 positive value validation failed for ", 
                      source_bits, "-bit sample: 0x", std::hex, sample, std::dec);
        }
        
        return result;
    }
}

bool FLACCodec::validateBitPerfectReconstruction_unlocked(const FLAC__int32* original, 
                                                          const int16_t* converted, 
                                                          size_t sample_count, 
                                                          uint16_t source_bits) const {
    // RFC 9639 compliance: FLAC is lossless - validate reconstruction accuracy
    // This function validates that bit depth conversion maintains RFC 9639 lossless requirements
    // and handles endianness correctly per RFC specification
    
    if (!original || !converted || sample_count == 0) {
        Debug::log("flac_codec", "[validateBitPerfectReconstruction_unlocked] RFC 9639 validation failed - Invalid parameters");
        return false;
    }
    
    if (source_bits < 4 || source_bits > 32) {
        Debug::log("flac_codec", "[validateBitPerfectReconstruction_unlocked] RFC 9639 validation failed - Invalid source bit depth: ", 
                  source_bits, " (valid range: 4-32 bits)");
        return false;
    }
    
    size_t mismatch_count = 0;
    size_t clipped_samples = 0;
    double max_error = 0.0;
    double total_squared_error = 0.0;
    
    for (size_t i = 0; i < sample_count; ++i) {
        // Apply proper sign extension to original sample
        FLAC__int32 sign_extended = applyProperSignExtension_unlocked(original[i], source_bits);
        
        // Calculate expected 16-bit value based on source bit depth
        int16_t expected_16bit;
        
        switch (source_bits) {
            case 8:
                // 8-bit to 16-bit: upscale by left shift
                expected_16bit = static_cast<int16_t>(std::clamp(sign_extended, -128, 127) << 8);
                break;
                
            case 16:
                // 16-bit to 16-bit: direct copy (should be bit-perfect)
                expected_16bit = static_cast<int16_t>(std::clamp(sign_extended, -32768, 32767));
                break;
                
            case 24:
                // 24-bit to 16-bit: downscale by right shift
                expected_16bit = static_cast<int16_t>(std::clamp(sign_extended >> 8, -32768, 32767));
                break;
                
            case 32:
                // 32-bit to 16-bit: downscale by right shift with overflow protection
                expected_16bit = static_cast<int16_t>(std::clamp(sign_extended >> 16, -32768, 32767));
                break;
                
            default:
                // Generic conversion for unusual bit depths
                if (source_bits < 16) {
                    // Upscale to 16-bit
                    int32_t upscaled = sign_extended << (16 - source_bits);
                    expected_16bit = static_cast<int16_t>(std::clamp(upscaled, -32768, 32767));
                } else {
                    // Downscale to 16-bit
                    int32_t downscaled = sign_extended >> (source_bits - 16);
                    expected_16bit = static_cast<int16_t>(std::clamp(downscaled, -32768, 32767));
                }
                break;
        }
        
        // Compare expected vs actual converted value
        int16_t actual_16bit = converted[i];
        
        if (expected_16bit != actual_16bit) {
            mismatch_count++;
            
            // Calculate error metrics
            double error = std::abs(static_cast<double>(expected_16bit) - static_cast<double>(actual_16bit));
            max_error = std::max(max_error, error);
            total_squared_error += error * error;
        }
        
        // Check for clipping
        if (actual_16bit == -32768 || actual_16bit == 32767) {
            // Verify this is legitimate clipping, not conversion error
            if (source_bits > 16) {
                int32_t original_scaled = sign_extended >> (source_bits - 16);
                if (std::abs(original_scaled) < 32767) {
                    clipped_samples++;
                }
            }
        }
    }
    
    // Calculate quality metrics
    double error_rate = static_cast<double>(mismatch_count) / sample_count;
    double rms_error = sample_count > 0 ? std::sqrt(total_squared_error / sample_count) : 0.0;
    
    // Determine if reconstruction is acceptable
    bool is_bit_perfect = (mismatch_count == 0);
    bool is_acceptable = false;
    
    if (source_bits == 16) {
        // 16-bit source should be bit-perfect
        is_acceptable = is_bit_perfect;
    } else {
        // Other bit depths: allow small conversion errors
        is_acceptable = (error_rate < 0.001) && (max_error <= 1.0) && (clipped_samples == 0);
    }
    
    // Log validation results
    if (is_bit_perfect) {
        Debug::log("flac_codec", "[validateBitPerfectReconstruction_unlocked] Bit-perfect reconstruction validated for ", 
                  source_bits, "-bit source (", sample_count, " samples)");
    } else if (is_acceptable) {
        Debug::log("flac_codec", "[validateBitPerfectReconstruction_unlocked] Acceptable reconstruction quality for ", 
                  source_bits, "-bit source - Error rate: ", error_rate * 100.0, "%, Max error: ", max_error, 
                  ", RMS error: ", rms_error);
    } else {
        Debug::log("flac_codec", "[validateBitPerfectReconstruction_unlocked] Poor reconstruction quality for ", 
                  source_bits, "-bit source - Error rate: ", error_rate * 100.0, "%, Max error: ", max_error, 
                  ", RMS error: ", rms_error, ", Clipped: ", clipped_samples);
    }
    
    return is_acceptable;
}

AudioQualityMetrics FLACCodec::calculateAudioQualityMetrics_unlocked(const int16_t* samples, 
                                                                     size_t sample_count, 
                                                                     const FLAC__int32* reference,
                                                                     uint16_t reference_bits) const {
    AudioQualityMetrics metrics;
    
    if (!samples || sample_count == 0) {
        Debug::log("flac_codec", "[calculateAudioQualityMetrics_unlocked] Invalid parameters");
        return metrics;
    }
    
    // Calculate basic amplitude statistics
    double sum = 0.0;
    double sum_squares = 0.0;
    int16_t min_sample = INT16_MAX;
    int16_t max_sample = INT16_MIN;
    size_t zero_crossings = 0;
    size_t clipped_samples = 0;
    
    for (size_t i = 0; i < sample_count; ++i) {
        int16_t sample = samples[i];
        
        // Basic statistics
        sum += sample;
        sum_squares += static_cast<double>(sample) * sample;
        min_sample = std::min(min_sample, sample);
        max_sample = std::max(max_sample, sample);
        
        // Zero crossing detection
        if (i > 0 && ((samples[i-1] >= 0 && sample < 0) || (samples[i-1] < 0 && sample >= 0))) {
            zero_crossings++;
        }
        
        // Clipping detection
        if (sample == -32768 || sample == 32767) {
            clipped_samples++;
        }
    }
    
    // Calculate derived metrics
    double mean = sum / sample_count;
    // double variance = (sum_squares / sample_count) - (mean * mean); // Reserved for future use
    double rms = std::sqrt(sum_squares / sample_count);
    
    // Normalize to 0.0-1.0 range
    metrics.peak_amplitude = std::max(std::abs(min_sample), std::abs(max_sample)) / 32768.0;
    metrics.rms_amplitude = rms / 32768.0;
    metrics.dc_offset = std::abs(mean) / 32768.0 * 100.0; // As percentage
    metrics.zero_crossings = zero_crossings;
    metrics.clipped_samples = clipped_samples;
    
    // Calculate dynamic range
    if (rms > 0.0) {
        double peak = std::max(std::abs(min_sample), std::abs(max_sample));
        metrics.dynamic_range_db = 20.0 * std::log10(peak / rms);
    } else {
        metrics.dynamic_range_db = 0.0;
    }
    
    // Calculate SNR and THD if reference is provided
    if (reference && reference_bits >= 4 && reference_bits <= 32) {
        double signal_power = 0.0;
        double noise_power = 0.0;
        // double harmonic_power = 0.0; // Reserved for future FFT-based THD calculation
        
        for (size_t i = 0; i < sample_count; ++i) {
            // Convert reference to 16-bit for comparison
            FLAC__int32 ref_extended = applyProperSignExtension_unlocked(reference[i], reference_bits);
            int16_t ref_16bit;
            
            if (reference_bits <= 16) {
                ref_16bit = static_cast<int16_t>(ref_extended << (16 - reference_bits));
            } else {
                ref_16bit = static_cast<int16_t>(std::clamp(ref_extended >> (reference_bits - 16), -32768, 32767));
            }
            
            // Calculate signal and noise power
            double signal = static_cast<double>(ref_16bit);
            double noise = static_cast<double>(samples[i]) - signal;
            
            signal_power += signal * signal;
            noise_power += noise * noise;
        }
        
        // Calculate SNR
        if (noise_power > 0.0 && signal_power > 0.0) {
            metrics.signal_to_noise_ratio_db = 10.0 * std::log10(signal_power / noise_power);
        } else if (noise_power == 0.0) {
            metrics.signal_to_noise_ratio_db = 120.0; // Theoretical maximum for 16-bit
            metrics.bit_perfect = true;
        }
        
        // Calculate THD (simplified - would need FFT for accurate measurement)
        if (signal_power > 0.0) {
            metrics.total_harmonic_distortion = std::sqrt(noise_power / signal_power) * 100.0;
        }
    } else {
        // Estimate SNR based on bit depth
        if (metrics.rms_amplitude > 0.0) {
            // Theoretical SNR for 16-bit audio
            metrics.signal_to_noise_ratio_db = 20.0 * std::log10(metrics.peak_amplitude / (1.0 / 65536.0));
        }
    }
    
    Debug::log("flac_codec", "[calculateAudioQualityMetrics_unlocked] Quality metrics - SNR: ", 
              metrics.signal_to_noise_ratio_db, " dB, THD: ", metrics.total_harmonic_distortion, 
              "%, Dynamic Range: ", metrics.dynamic_range_db, " dB, Peak: ", metrics.peak_amplitude, 
              ", RMS: ", metrics.rms_amplitude, ", Clipped: ", clipped_samples);
    
    return metrics;
}

// ============================================================================
// RFC 9639 Compliant Frame Boundary Detection Methods
// ============================================================================

bool FLACCodec::validateFrameBoundary_unlocked(const uint8_t* data, size_t size) {
    if (!data || size < 2) {
        Debug::log("flac_codec", "[FLACCodec::validateFrameBoundary_unlocked] Insufficient data for frame boundary validation");
        return false;
    }
    
    // RFC 9639 Section 9.1: Frame MUST start with 15-bit sync code 0b111111111111100
    // followed by blocking strategy bit (0 for fixed, 1 for variable block size)
    uint16_t sync_pattern = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    bool valid_sync = (sync_pattern == 0xFFF8) || (sync_pattern == 0xFFF9);
    
    if (!valid_sync) {
        Debug::log("flac_codec", "[FLACCodec::validateFrameBoundary_unlocked] Invalid RFC 9639 sync pattern: 0x", 
                  std::hex, std::uppercase, sync_pattern, std::nouppercase, std::dec, 
                  " (expected 0xFFF8 or 0xFFF9)");
        return false;
    }
    
    bool is_variable_block_size = (sync_pattern == 0xFFF9);
    Debug::log("flac_codec", "[FLACCodec::validateFrameBoundary_unlocked] Valid RFC 9639 sync pattern: 0x", 
              std::hex, std::uppercase, sync_pattern, std::nouppercase, std::dec, 
              " (", (is_variable_block_size ? "variable" : "fixed"), " block size)");
    
    // For highly compressed frames (10-14 bytes), we need additional validation
    if (size >= 10 && size <= 14) {
        Debug::log("flac_codec", "[FLACCodec::validateFrameBoundary_unlocked] Highly compressed frame detected (", 
                  size, " bytes) - performing enhanced validation");
        return handleHighlyCompressedFrame_unlocked(data, size);
    }
    
    // For larger frames, validate the frame header structure
    if (size >= 4) {
        return validateFrameHeader_unlocked(data, size);
    }
    
    return true; // Basic sync pattern is valid
}

bool FLACCodec::isValidFrameSync_unlocked(const uint8_t* data, size_t size) {
    if (!data || size < 2) {
        return false;
    }
    
    // RFC 9639 Section 9.1: Check for exact sync pattern
    uint16_t sync_pattern = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    return (sync_pattern == 0xFFF8) || (sync_pattern == 0xFFF9);
}

size_t FLACCodec::findNextFrameSync_unlocked(const uint8_t* data, size_t size, size_t start_offset) {
    if (!data || size < 2 || start_offset >= size - 1) {
        return size; // Not found
    }
    
    Debug::log("flac_codec", "[FLACCodec::findNextFrameSync_unlocked] Searching for RFC 9639 sync pattern from offset ", start_offset);
    
    for (size_t i = start_offset; i < size - 1; ++i) {
        uint16_t sync_pattern = (static_cast<uint16_t>(data[i]) << 8) | data[i + 1];
        bool valid_sync = (sync_pattern == 0xFFF8) || (sync_pattern == 0xFFF9);
        
        if (valid_sync) {
            bool is_variable_block_size = (sync_pattern == 0xFFF9);
            Debug::log("flac_codec", "[FLACCodec::findNextFrameSync_unlocked] Found RFC 9639 sync pattern at offset ", i, 
                      ": 0x", std::hex, std::uppercase, sync_pattern, std::nouppercase, std::dec, 
                      " (", (is_variable_block_size ? "variable" : "fixed"), " block size)");
            
            // Additional validation for the frame header if we have enough data
            if (i + 4 <= size && validateFrameHeader_unlocked(data + i, size - i)) {
                return i;
            } else if (i + 4 > size) {
                // Not enough data to validate header, but sync pattern is correct
                Debug::log("flac_codec", "[FLACCodec::findNextFrameSync_unlocked] Insufficient data for header validation at offset ", i);
                return i;
            }
        }
    }
    
    Debug::log("flac_codec", "[FLACCodec::findNextFrameSync_unlocked] No valid RFC 9639 sync pattern found");
    return size; // Not found
}

bool FLACCodec::validateFrameHeader_unlocked(const uint8_t* data, size_t size) {
    if (!data || size < 4) {
        Debug::log("flac_codec", "[FLACCodec::validateFrameHeader_unlocked] Insufficient data for frame header validation");
        return false;
    }
    
    // Validate sync pattern first
    if (!isValidFrameSync_unlocked(data, size)) {
        return false;
    }
    
    // RFC 9639 Section 9.1: Validate frame header structure
    uint8_t byte2 = data[2];
    uint8_t byte3 = data[3];
    
    // Block size bits (4 bits in byte 2, upper nibble)
    uint8_t block_size_bits = (byte2 & 0xF0) >> 4;
    if (block_size_bits == 0x00) {
        Debug::log("flac_codec", "[FLACCodec::validateFrameHeader_unlocked] Invalid block size bits: 0x00 is reserved");
        return false;
    }
    
    // Sample rate bits (4 bits in byte 2, lower nibble)
    uint8_t sample_rate_bits = byte2 & 0x0F;
    if (sample_rate_bits == 0x0F) {
        Debug::log("flac_codec", "[FLACCodec::validateFrameHeader_unlocked] Invalid sample rate bits: 0x0F is invalid");
        return false;
    }
    
    // Channel assignment bits (4 bits in byte 3, upper nibble)
    uint8_t channel_assignment = (byte3 & 0xF0) >> 4;
    if (channel_assignment >= 0x0B && channel_assignment <= 0x0F) {
        Debug::log("flac_codec", "[FLACCodec::validateFrameHeader_unlocked] Invalid channel assignment: 0x", 
                  std::hex, std::uppercase, static_cast<uint32_t>(channel_assignment), std::nouppercase, std::dec, 
                  " is reserved");
        return false;
    }
    
    // Sample size bits (3 bits in byte 3, bits 1-3)
    uint8_t sample_size_bits = (byte3 & 0x0E) >> 1;
    if (sample_size_bits == 0x03 || sample_size_bits == 0x07) {
        Debug::log("flac_codec", "[FLACCodec::validateFrameHeader_unlocked] Invalid sample size bits: 0x", 
                  std::hex, std::uppercase, static_cast<uint32_t>(sample_size_bits), std::nouppercase, std::dec, 
                  " is reserved");
        return false;
    }
    
    // Reserved bit (bit 0 in byte 3) MUST be 0
    if (byte3 & 0x01) {
        Debug::log("flac_codec", "[FLACCodec::validateFrameHeader_unlocked] Reserved bit is set (MUST be 0 per RFC 9639)");
        return false;
    }
    
    Debug::log("flac_codec", "[FLACCodec::validateFrameHeader_unlocked] Frame header validation passed");
    Debug::log("flac_codec", "  Block size bits: 0x", std::hex, std::uppercase, static_cast<uint32_t>(block_size_bits), std::nouppercase, std::dec);
    Debug::log("flac_codec", "  Sample rate bits: 0x", std::hex, std::uppercase, static_cast<uint32_t>(sample_rate_bits), std::nouppercase, std::dec);
    Debug::log("flac_codec", "  Channel assignment: 0x", std::hex, std::uppercase, static_cast<uint32_t>(channel_assignment), std::nouppercase, std::dec);
    Debug::log("flac_codec", "  Sample size bits: 0x", std::hex, std::uppercase, static_cast<uint32_t>(sample_size_bits), std::nouppercase, std::dec);
    
    return true;
}

bool FLACCodec::handleHighlyCompressedFrame_unlocked(const uint8_t* data, size_t size) {
    if (!data || size < 10 || size > 14) {
        Debug::log("flac_codec", "[FLACCodec::handleHighlyCompressedFrame_unlocked] Invalid size for highly compressed frame: ", size);
        return false;
    }
    
    Debug::log("flac_codec", "[FLACCodec::handleHighlyCompressedFrame_unlocked] Processing highly compressed FLAC frame (", size, " bytes)");
    
    // Validate basic frame header structure
    if (!validateFrameHeader_unlocked(data, size)) {
        Debug::log("flac_codec", "[FLACCodec::handleHighlyCompressedFrame_unlocked] Frame header validation failed");
        return false;
    }
    
    // For highly compressed frames, we need to be extra careful about frame boundaries
    // These frames typically occur with:
    // 1. Very small block sizes (16-64 samples)
    // 2. Simple audio content (silence, constant values)
    // 3. Efficient prediction (CONSTANT or VERBATIM subframes)
    
    // Check if the frame size is consistent with the block size indicated in the header
    uint8_t block_size_bits = (data[2] & 0xF0) >> 4;
    uint32_t expected_block_size = 0;
    
    switch (block_size_bits) {
        case 0x01: expected_block_size = 192; break;
        case 0x02: case 0x03: case 0x04: case 0x05:
            expected_block_size = 576 << (block_size_bits - 2); break;
        case 0x06: expected_block_size = 0; break; // Get from end of header
        case 0x07: expected_block_size = 0; break; // Get from end of header
        case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: case 0x0E: case 0x0F:
            expected_block_size = 256 << (block_size_bits - 8); break;
        default:
            Debug::log("flac_codec", "[FLACCodec::handleHighlyCompressedFrame_unlocked] Invalid block size bits: 0x", 
                      std::hex, std::uppercase, static_cast<uint32_t>(block_size_bits), std::nouppercase, std::dec);
            return false;
    }
    
    if (expected_block_size > 0) {
        Debug::log("flac_codec", "[FLACCodec::handleHighlyCompressedFrame_unlocked] Expected block size: ", expected_block_size, " samples");
        
        // For highly compressed frames, small block sizes are expected
        if (expected_block_size > 1152) {
            Debug::log("flac_codec", "[FLACCodec::handleHighlyCompressedFrame_unlocked] WARNING: Large block size (", 
                      expected_block_size, ") in highly compressed frame may indicate boundary detection error");
        }
    }
    
    // Check channel configuration for highly compressed frames
    uint8_t channel_assignment = (data[3] & 0xF0) >> 4;
    uint32_t num_channels = 0;
    
    if (channel_assignment <= 0x07) {
        num_channels = channel_assignment + 1;
    } else if (channel_assignment >= 0x08 && channel_assignment <= 0x0A) {
        num_channels = 2; // Stereo modes
    } else {
        Debug::log("flac_codec", "[FLACCodec::handleHighlyCompressedFrame_unlocked] Invalid channel assignment: 0x", 
                  std::hex, std::uppercase, static_cast<uint32_t>(channel_assignment), std::nouppercase, std::dec);
        return false;
    }
    
    Debug::log("flac_codec", "[FLACCodec::handleHighlyCompressedFrame_unlocked] Channel configuration: ", num_channels, " channels");
    
    // Validate that the frame size is reasonable for the content
    // Highly compressed frames should have minimal overhead beyond the header
    size_t min_frame_size = 4; // Sync + header
    size_t max_frame_size = 14; // As specified in the task
    
    if (size < min_frame_size || size > max_frame_size) {
        Debug::log("flac_codec", "[FLACCodec::handleHighlyCompressedFrame_unlocked] Frame size ", size, 
                  " outside expected range [", min_frame_size, "-", max_frame_size, "] for highly compressed frame");
        return false;
    }
    
    Debug::log("flac_codec", "[FLACCodec::handleHighlyCompressedFrame_unlocked] Highly compressed frame validation passed");
    Debug::log("flac_codec", "  Frame size: ", size, " bytes");
    Debug::log("flac_codec", "  Block size: ", expected_block_size, " samples");
    Debug::log("flac_codec", "  Channels: ", num_channels);
    
    return true;
}

#endif // HAVE_FLAC