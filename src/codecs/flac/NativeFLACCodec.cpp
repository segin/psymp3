/*
 * NativeFLACCodec.cpp - Native FLAC decoder implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
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
#include <chrono>
#include <algorithm>

#ifdef HAVE_NATIVE_FLAC

namespace PsyMP3 {
namespace Codec {
namespace FLAC {

// ============================================================================
// FLACCodec Implementation
// ============================================================================

FLACCodec::FLACCodec(const StreamInfo& stream_info)
    : AudioCodec(stream_info)
    , m_stream_info(stream_info)
    , m_state(DecoderState::UNINITIALIZED)
    , m_current_sample(0)
    , m_initialized(false)
    , m_last_error(FLACError::NONE)
    , m_consecutive_errors(0)
    , m_has_seek_table(false)
    , m_has_streaminfo(false)
    , m_md5_validation_enabled(true)
    , m_stats() {
    
    Debug::log("flac_codec", "[NativeFLACCodec] Creating native FLAC codec for stream: ",
              stream_info.codec_name, ", ", stream_info.sample_rate, "Hz, ",
              stream_info.channels, " channels, ", stream_info.bits_per_sample, " bits");
    
    // Explicitly initialize stats (although default constructor does this)
    m_stats.min_frame_decode_time_us = UINT64_MAX;

    // Pre-allocate buffers for performance (Requirement 65)
    m_input_buffer.reserve(INPUT_BUFFER_SIZE);  // 64KB input buffer
    
    // Allocate per-channel decode buffers
    for (size_t i = 0; i < MAX_CHANNELS; i++) {
        m_decode_buffer[i].reserve(MAX_BLOCK_SIZE);
    }
    
    // Allocate output buffer for interleaved samples
    m_output_buffer.reserve(MAX_BLOCK_SIZE * MAX_CHANNELS);
}

FLACCodec::~FLACCodec() {
    Debug::log("flac_codec", "[NativeFLACCodec] Destroying native FLAC codec");
    
    // Acquire locks in documented order to ensure no operations in progress
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    std::lock_guard<std::mutex> decoder_lock(m_decoder_mutex);
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
    
    // Set state to uninitialized
    m_state = DecoderState::UNINITIALIZED;
    m_initialized = false;
    
    // Component cleanup (unique_ptr handles deletion automatically)
    m_bitstream_reader.reset();
    m_frame_parser.reset();
    m_subframe_decoder.reset();
    m_residual_decoder.reset();
    m_channel_decorrelator.reset();
    m_sample_reconstructor.reset();
    m_crc_validator.reset();
    m_metadata_parser.reset();
    m_md5_validator.reset();
    
    // Clear buffers
    m_input_buffer.clear();
    for (size_t i = 0; i < MAX_CHANNELS; i++) {
        m_decode_buffer[i].clear();
    }
    m_output_buffer.clear();
    
    Debug::log("flac_codec", "[NativeFLACCodec] Codec destroyed successfully");
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

FLACCodecStats FLACCodec::getStats() const {
    // Acquire all locks in documented order to ensure thread safety when reading stats
    // and vector capacities. This prevents race conditions with decode() and other threads.
    // Note: We use scoped locking for thread safety across all members.
    std::lock_guard<std::mutex> state_lock(m_state_mutex);
    std::lock_guard<std::mutex> decoder_lock(m_decoder_mutex);
    std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);

    // Create a copy of current stats
    FLACCodecStats stats = m_stats;

    // Update memory usage estimate (snapshot)
    stats.memory_usage_bytes = m_input_buffer.capacity() * sizeof(uint8_t);
    for (size_t i = 0; i < MAX_CHANNELS; i++) {
        stats.memory_usage_bytes += m_decode_buffer[i].capacity() * sizeof(int32_t);
    }
    stats.memory_usage_bytes += m_output_buffer.capacity() * sizeof(int16_t);

    // Add component memory estimates if available
    // (This is a basic estimate, components allocate their own memory)

    return stats;
}

bool FLACCodec::seek(uint64_t target_sample) {
    Debug::log("flac_codec", "[NativeFLACCodec::seek] [ENTRY] Seeking to sample ", target_sample);
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    Debug::log("flac_codec", "[NativeFLACCodec::seek] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        bool result = seek_unlocked(target_sample);
        Debug::log("flac_codec", "[NativeFLACCodec::seek] [EXIT] Seek ", result ? "successful" : "failed");
        return result;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::seek] [EXCEPTION] ", e.what());
        return false;
    }
}

void FLACCodec::setSeekTable(const std::vector<SeekPoint>& seek_table) {
    Debug::log("flac_codec", "[NativeFLACCodec::setSeekTable] [ENTRY] Setting seek table with ",
              seek_table.size(), " points");
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    Debug::log("flac_codec", "[NativeFLACCodec::setSeekTable] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        setSeekTable_unlocked(seek_table);
        Debug::log("flac_codec", "[NativeFLACCodec::setSeekTable] [EXIT] Seek table set successfully");
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::setSeekTable] [EXCEPTION] ", e.what());
    }
}

// Private implementation methods (assume locks are already held)

bool FLACCodec::initialize_unlocked() {
    Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Initializing native FLAC decoder");
    
    // Streamable subset support (Requirement 19.1, 19.8):
    // Allow initialization even without complete stream info for mid-stream synchronization
    // Frame headers will provide necessary parameters
    
    // Validate stream parameters against RFC 9639 requirements (Requirement 14)
    // Allow 0 values for streamable subset - will be filled from frame headers
    if (m_stream_info.sample_rate > 655350) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Invalid sample rate: ",
                  m_stream_info.sample_rate, " (RFC 9639 max: 655350 Hz)");
        m_state = DecoderState::DECODER_ERROR;
        return false;
    }
    
    if (m_stream_info.channels > 8) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Invalid channel count: ",
                  m_stream_info.channels, " (RFC 9639 max: 8)");
        m_state = DecoderState::DECODER_ERROR;
        return false;
    }
    
    if (m_stream_info.bits_per_sample > 32) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Invalid bit depth: ",
                  m_stream_info.bits_per_sample, " (RFC 9639 max: 32 bits)");
        m_state = DecoderState::DECODER_ERROR;
        return false;
    }
    
    // Log if we're in streamable subset mode (missing stream info)
    if (m_stream_info.sample_rate == 0 || m_stream_info.channels == 0 || m_stream_info.bits_per_sample == 0) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Streamable subset mode: ",
                  "will extract parameters from frame headers (Requirement 19)");
    }
    
    // Initialize decoder components (Requirement 14, subtask 11.4)
    try {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Creating decoder components");
        
        // Create BitstreamReader
        m_bitstream_reader = std::make_unique<BitstreamReader>();
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] BitstreamReader created");
        
        // Create CRCValidator (needed by FrameParser)
        m_crc_validator = std::make_unique<CRCValidator>();
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] CRCValidator created");
        
        // Create FrameParser
        m_frame_parser = std::make_unique<FrameParser>(m_bitstream_reader.get(), m_crc_validator.get());
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] FrameParser created");
        
        // Create ResidualDecoder (needed by SubframeDecoder)
        m_residual_decoder = std::make_unique<ResidualDecoder>(m_bitstream_reader.get());
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] ResidualDecoder created");
        
        // Create SubframeDecoder
        m_subframe_decoder = std::make_unique<SubframeDecoder>(m_bitstream_reader.get(), m_residual_decoder.get());
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] SubframeDecoder created");
        
        // Create ChannelDecorrelator
        m_channel_decorrelator = std::make_unique<ChannelDecorrelator>();
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] ChannelDecorrelator created");
        
        // Create SampleReconstructor
        m_sample_reconstructor = std::make_unique<SampleReconstructor>();
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] SampleReconstructor created");
        
        // Create MetadataParser
        m_metadata_parser = std::make_unique<MetadataParser>(m_bitstream_reader.get());
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] MetadataParser created");
        
        // Create MD5Validator
        m_md5_validator = std::make_unique<MD5Validator>();
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] MD5Validator created");
        
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Component creation failed: ", e.what());
        m_state = DecoderState::DECODER_ERROR;
        return false;
    } catch (...) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Component creation failed: unknown exception");
        m_state = DecoderState::DECODER_ERROR;
        return false;
    }
    
    // Allocate buffers based on stream parameters (Requirement 65, subtask 11.5)
    try {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Allocating buffers");
        
        // Input buffer already reserved in constructor
        m_input_buffer.clear();
        
        // Allocate per-channel decode buffers
        for (size_t i = 0; i < MAX_CHANNELS; i++) {
            m_decode_buffer[i].clear();
            m_decode_buffer[i].reserve(MAX_BLOCK_SIZE);
        }
        
        // Allocate output buffer for interleaved samples
        m_output_buffer.clear();
        m_output_buffer.reserve(MAX_BLOCK_SIZE * m_stream_info.channels);
        
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Buffers allocated successfully");
        
    } catch (const std::bad_alloc& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Buffer allocation failed: ", e.what());
        m_state = DecoderState::DECODER_ERROR;
        return false;
    }
    
    // Reset state and transition to INITIALIZED (Requirement 64.1, 64.2)
    m_current_sample.store(0);
    m_last_error = FLACError::NONE;
    m_consecutive_errors = 0;
    
    if (!transitionState(DecoderState::INITIALIZED)) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Failed to transition to INITIALIZED state");
        return false;
    }
    
    m_initialized = true;
    
    // Initialize MD5 validator if enabled (Requirement 25)
    if (m_md5_validation_enabled && m_has_streaminfo && 
        !MD5Validator::isZeroMD5(m_streaminfo.md5_sum)) {
        Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Initializing MD5 validator");
        if (!m_md5_validator->reset()) {
            Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] MD5 validator reset failed (warning)");
        }
    }
    
    Debug::log("flac_codec", "[NativeFLACCodec::initialize_unlocked] Native FLAC decoder initialized successfully");
    return true;
}

AudioFrame FLACCodec::decode_unlocked(const MediaChunk& chunk) {
    auto start_time = std::chrono::high_resolution_clock::now();
    Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Decoding chunk with ", chunk.data.size(), " bytes");
    
    m_stats.total_bytes_processed += chunk.data.size();

    // Validate decoder state - attempt recovery if in ERROR state
    if (!m_initialized || m_state == DecoderState::DECODER_ERROR) {
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Decoder not initialized or in error state, attempting recovery");
        
        // Attempt automatic recovery from ERROR state (Requirement 11.5)
        if (m_state == DecoderState::DECODER_ERROR) {
            if (resetFromErrorState() && initialize_unlocked()) {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Error recovery successful");
            } else {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Error recovery failed");
                return AudioFrame();
            }
        } else {
            // Not recoverable - not initialized
            Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Cannot decode - not initialized");
            return AudioFrame();
        }
    }
    
    // Check state is INITIALIZED (should be after recovery)
    if (m_state != DecoderState::INITIALIZED) {
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Decoder in unexpected state: ", getStateName(m_state));
        return AudioFrame();
    }
    
    if (chunk.data.empty()) {
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Empty chunk");
        return AudioFrame();
    }
    
    // Implement decode pipeline (Requirement 14, subtask 11.6)
    try {
        // Transition to DECODING state (Requirement 64.3)
        if (!transitionState(DecoderState::DECODING)) {
            Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Failed to transition to DECODING state");
            return AudioFrame();
        }
        
        // Step 1: Clear and feed input data to bitstream reader
        // Each chunk from demuxer is an independent frame, so start fresh
        m_bitstream_reader->clearBuffer();
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Feeding ", chunk.data.size(), " bytes to bitstream reader");
        m_bitstream_reader->feedData(chunk.data.data(), chunk.data.size());
        
        // Step 2: Find and parse frame sync with error recovery (Requirement 11.1)
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Searching for frame sync");
        if (!m_frame_parser->findSync()) {
            Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Frame sync not found, attempting recovery");
            
            // Attempt sync loss recovery
            if (!recoverFromSyncLoss()) {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Sync recovery failed");
                transitionState(DecoderState::INITIALIZED);
                return AudioFrame();
            }
        }
        
        // Step 3: Parse frame header with error recovery (Requirement 11.2)
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Parsing frame header");
        FrameHeader header;
        if (!m_frame_parser->parseFrameHeader(header)) {
            Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Frame header parsing failed, attempting recovery");
            
            // Attempt header error recovery
            if (!recoverFromInvalidHeader()) {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Header recovery failed");
                transitionState(DecoderState::DECODER_ERROR);
                return AudioFrame();
            }
            
            // Try parsing header again after recovery
            if (!m_frame_parser->parseFrameHeader(header)) {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Header parsing failed after recovery");
                transitionState(DecoderState::DECODER_ERROR);
                return AudioFrame();
            }
        }
        
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Frame header parsed: block_size=", header.block_size,
                  ", channels=", header.channels, ", bit_depth=", header.bit_depth,
                  ", sample_rate=", header.sample_rate);
        
        // Streamable subset support (Requirement 19):
        // Handle frames without STREAMINFO references by using frame header values
        // If frame header has 0 for sample_rate or bit_depth, use STREAMINFO if available
        
        // Handle sample rate (Requirement 19.2)
        if (header.sample_rate == 0) {
            if (m_has_streaminfo && m_streaminfo.sample_rate > 0) {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Using STREAMINFO sample rate: ",
                          m_streaminfo.sample_rate, " Hz");
                header.sample_rate = m_streaminfo.sample_rate;
            } else if (m_stream_info.sample_rate > 0) {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Using StreamInfo sample rate: ",
                          m_stream_info.sample_rate, " Hz");
                header.sample_rate = m_stream_info.sample_rate;
            } else {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] No sample rate available (frame=0, STREAMINFO unavailable)");
                m_state = DecoderState::DECODER_ERROR;
                return AudioFrame();
            }
        }
        
        // Handle bit depth (Requirement 19.3)
        if (header.bit_depth == 0) {
            if (m_has_streaminfo && m_streaminfo.bits_per_sample > 0) {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Using STREAMINFO bit depth: ",
                          m_streaminfo.bits_per_sample, " bits");
                header.bit_depth = m_streaminfo.bits_per_sample;
            } else if (m_stream_info.bits_per_sample > 0) {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Using StreamInfo bit depth: ",
                          m_stream_info.bits_per_sample, " bits");
                header.bit_depth = m_stream_info.bits_per_sample;
            } else {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] No bit depth available (frame=0, STREAMINFO unavailable)");
                m_state = DecoderState::DECODER_ERROR;
                return AudioFrame();
            }
        }
        
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Final frame parameters after streamable subset handling: ",
                  "sample_rate=", header.sample_rate, " Hz, bit_depth=", header.bit_depth, " bits");
        
        // Validate streamable subset constraints (informational only - Requirement 19)
        validateStreamableSubset(header);
        
        // Step 4: Decode subframes for each channel
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Decoding ", header.channels, " subframes");
        
        for (uint32_t ch = 0; ch < header.channels; ch++) {
            // Determine if this is a side channel (needs extra bit depth)
            bool is_side_channel = false;
            if (header.channel_assignment == ChannelAssignment::LEFT_SIDE && ch == 1) {
                is_side_channel = true;  // Side channel in left-side mode
            } else if (header.channel_assignment == ChannelAssignment::RIGHT_SIDE && ch == 0) {
                is_side_channel = true;  // Side channel in right-side mode
            } else if (header.channel_assignment == ChannelAssignment::MID_SIDE && ch == 1) {
                is_side_channel = true;  // Side channel in mid-side mode
            }
            
            // Resize decode buffer for this channel
            m_decode_buffer[ch].resize(header.block_size);
            
            // Decode subframe with error recovery (Requirement 11.3)
            Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Decoding subframe ", ch,
                      " (side_channel=", is_side_channel, ")");
            
            if (!m_subframe_decoder->decodeSubframe(m_decode_buffer[ch].data(), 
                                                    header.block_size,
                                                    header.bit_depth,
                                                    is_side_channel)) {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Subframe ", ch, " decoding failed, outputting silence");
                
                // Recover by outputting silence for this channel
                recoverFromSubframeError(m_decode_buffer[ch].data(), header.block_size);
                
                // Continue with other channels rather than failing completely
            }
        }
        
        // Step 5: Apply channel decorrelation
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Applying channel decorrelation");
        
        // Create array of channel pointers
        int32_t* channel_ptrs[MAX_CHANNELS];
        for (uint32_t ch = 0; ch < header.channels; ch++) {
            channel_ptrs[ch] = m_decode_buffer[ch].data();
        }
        
        if (!m_channel_decorrelator->decorrelate(channel_ptrs, header.block_size,
                                                 header.channels, header.channel_assignment)) {
            Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Channel decorrelation failed");
            m_state = DecoderState::DECODER_ERROR;
            return AudioFrame();
        }
        
        // Step 6: Update MD5 checksum with decoded samples (before bit depth conversion)
        // Requirements: 25.1, 25.2, 25.3, 25.4
        if (m_md5_validation_enabled && m_has_streaminfo && 
            !MD5Validator::isZeroMD5(m_streaminfo.md5_sum)) {
            Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Updating MD5 checksum");
            
            if (!m_md5_validator->update(const_cast<const int32_t* const*>(channel_ptrs),
                                        header.block_size,
                                        header.channels,
                                        header.bit_depth)) {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] MD5 update failed (warning)");
            }
        }
        
        // Step 7: Reconstruct samples with bit depth conversion
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Reconstructing samples");
        
        // Resize output buffer for interleaved samples
        m_output_buffer.resize(header.block_size * header.channels);
        
        // Reconstruct and interleave samples
        m_sample_reconstructor->reconstructSamples(m_output_buffer.data(),
                                                  channel_ptrs,
                                                  header.block_size,
                                                  header.channels,
                                                  header.bit_depth);
        
        // Step 8: Validate frame footer CRC with error recovery (Requirement 11.4)
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Parsing frame footer");
        FrameFooter footer;
        if (!m_frame_parser->parseFrameFooter(footer)) {
            Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Frame footer parsing failed");
            transitionState(DecoderState::DECODER_ERROR);
            return AudioFrame();
        }
        
        if (!m_frame_parser->validateFrame(header, footer)) {
            Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Frame CRC validation failed");
            
            // Attempt CRC error recovery
            if (!recoverFromCRCError(FLACError::CRC_MISMATCH)) {
                Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] CRC error recovery decided to discard frame");
                transitionState(DecoderState::INITIALIZED);
                return AudioFrame();
            }
            
            Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Using frame data despite CRC error (RFC allows)");
        }
        
        // Step 9: Return AudioFrame with decoded samples
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Creating AudioFrame with ",
                  header.block_size, " sample frames");
        
        // Update current sample position
        m_current_sample.fetch_add(header.block_size);
        
        // Update stats
        auto end_time = std::chrono::high_resolution_clock::now();
        uint64_t duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

        m_stats.frames_decoded++;
        m_stats.samples_decoded += header.block_size;
        m_stats.conversion_operations++; // Assuming one conversion op per frame
        m_stats.total_decode_time_us += duration_us;
        m_stats.max_frame_decode_time_us = std::max(m_stats.max_frame_decode_time_us, duration_us);
        if (m_stats.min_frame_decode_time_us == UINT64_MAX) {
             m_stats.min_frame_decode_time_us = duration_us;
        } else {
             m_stats.min_frame_decode_time_us = std::min(m_stats.min_frame_decode_time_us, duration_us);
        }

        // Update efficiency metrics
        if (m_stats.frames_decoded > 0) {
             m_stats.average_frame_size = static_cast<double>(m_stats.total_bytes_processed) / m_stats.frames_decoded;
        }
        if (m_stats.total_decode_time_us > 0) {
             m_stats.decode_efficiency = (static_cast<double>(m_stats.samples_decoded) * 1000000.0) / m_stats.total_decode_time_us;
        }

        // Create AudioFrame
        AudioFrame frame;
        frame.sample_rate = header.sample_rate;
        frame.channels = header.channels;
        
        // Copy interleaved samples to frame
        frame.samples = m_output_buffer;
        
        // Transition back to INITIALIZED state after successful decode (Requirement 64.3)
        transitionState(DecoderState::INITIALIZED);
        
        // Reset error counter on successful decode
        m_consecutive_errors = 0;
        m_last_error = FLACError::NONE;
        
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Frame decoded successfully");
        return frame;
        
    } catch (const std::bad_alloc& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Memory allocation exception: ", e.what());
        recoverFromMemoryError();
        return AudioFrame();
    } catch (const FLACException& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] FLAC exception: ", e.what(),
                  " (", e.getErrorName(), ")");
        m_last_error = e.getError();
        m_consecutive_errors++;
        m_stats.error_count++;
        
        if (m_consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
            handleUnrecoverableError();
        } else {
            transitionState(DecoderState::DECODER_ERROR);
        }
        return AudioFrame();
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Decoding exception: ", e.what());
        m_consecutive_errors++;
        m_stats.error_count++;
        
        if (m_consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
            handleUnrecoverableError();
        } else {
            transitionState(DecoderState::DECODER_ERROR);
        }
        return AudioFrame();
    } catch (...) {
        Debug::log("flac_codec", "[NativeFLACCodec::decode_unlocked] Decoding exception: unknown error");
        handleUnrecoverableError();
        return AudioFrame();
    }
}

AudioFrame FLACCodec::flush_unlocked() {
    Debug::log("flac_codec", "[NativeFLACCodec::flush_unlocked] Flushing remaining samples");
    
    // FLAC frames are self-contained and complete, so there are no partial
    // samples to flush. Return empty frame.
    // If we had buffered partial frame data, we would process it here.
    
    Debug::log("flac_codec", "[NativeFLACCodec::flush_unlocked] No samples to flush (FLAC frames are complete)");
    return AudioFrame();
}

void FLACCodec::reset_unlocked() {
    Debug::log("flac_codec", "[NativeFLACCodec::reset_unlocked] Resetting decoder state from: ",
              getStateName(m_state));
    
    // Handle reset from error state (Requirement 64.6)
    if (m_state == DecoderState::DECODER_ERROR) {
        if (!resetFromErrorState()) {
            Debug::log("flac_codec", "[NativeFLACCodec::reset_unlocked] Failed to reset from ERROR state");
            return;
        }
        // resetFromErrorState already handles full reset
        return;
    }
    
    // Clear buffers
    m_input_buffer.clear();
    for (size_t i = 0; i < MAX_CHANNELS; i++) {
        m_decode_buffer[i].clear();
    }
    m_output_buffer.clear();
    
    // Reset sample position (Requirement 64.4)
    m_current_sample.store(0);
    
    // Reset error tracking
    m_last_error = FLACError::NONE;
    m_consecutive_errors = 0;
    
    // Reset decoder components
    if (m_bitstream_reader) {
        m_bitstream_reader->clearBuffer();
        m_bitstream_reader->resetPosition();
    }
    
    // Reset MD5 validator if enabled (Requirement 25)
    if (m_md5_validation_enabled && m_has_streaminfo && 
        !MD5Validator::isZeroMD5(m_streaminfo.md5_sum) && m_md5_validator) {
        Debug::log("flac_codec", "[NativeFLACCodec::reset_unlocked] Resetting MD5 validator");
        if (!m_md5_validator->reset()) {
            Debug::log("flac_codec", "[NativeFLACCodec::reset_unlocked] MD5 validator reset failed (warning)");
        }
    }
    
    // Transition state (Requirement 64.8)
    if (m_initialized) {
        transitionState(DecoderState::INITIALIZED);
    } else {
        transitionState(DecoderState::UNINITIALIZED);
    }
    
    Debug::log("flac_codec", "[NativeFLACCodec::reset_unlocked] Decoder state reset complete, new state: ",
              getStateName(m_state));
}

bool FLACCodec::canDecode_unlocked(const StreamInfo& stream_info) const {
    // Check codec name (case-insensitive)
    std::string codec_lower = stream_info.codec_name;
    std::transform(codec_lower.begin(), codec_lower.end(), codec_lower.begin(), ::tolower);
    
    if (codec_lower != "flac") {
        Debug::log("flac_codec", "[NativeFLACCodec::canDecode_unlocked] Codec name mismatch: ", stream_info.codec_name);
        return false;
    }
    
    // Streamable subset support (Requirement 19.1, 19.8):
    // Allow 0 values for parameters - will be extracted from frame headers
    // This enables mid-stream synchronization without prior metadata
    
    // Validate parameters against RFC 9639 (allow 0 for streamable subset)
    if (stream_info.sample_rate > 655350) {
        Debug::log("flac_codec", "[NativeFLACCodec::canDecode_unlocked] Sample rate out of range: ", stream_info.sample_rate);
        return false;
    }
    
    if (stream_info.channels > 8) {
        Debug::log("flac_codec", "[NativeFLACCodec::canDecode_unlocked] Channel count out of range: ", stream_info.channels);
        return false;
    }
    
    if (stream_info.bits_per_sample > 32) {
        Debug::log("flac_codec", "[NativeFLACCodec::canDecode_unlocked] Bit depth out of range: ", stream_info.bits_per_sample);
        return false;
    }
    
    Debug::log("flac_codec", "[NativeFLACCodec::canDecode_unlocked] Can decode FLAC stream: ",
              stream_info.sample_rate, "Hz, ", stream_info.channels, " channels, ",
              stream_info.bits_per_sample, " bits",
              (stream_info.sample_rate == 0 || stream_info.channels == 0 || stream_info.bits_per_sample == 0) 
                ? " (streamable subset mode)" : "");
    
    return true;
}

bool FLACCodec::seek_unlocked(uint64_t target_sample) {
    Debug::log("flac_codec", "[NativeFLACCodec::seek_unlocked] Seeking to sample ", target_sample);
    
    // Validate decoder state
    if (!m_initialized) {
        Debug::log("flac_codec", "[NativeFLACCodec::seek_unlocked] Decoder not initialized");
        return false;
    }
    
    // Check if target is already current position
    if (m_current_sample.load() == target_sample) {
        Debug::log("flac_codec", "[NativeFLACCodec::seek_unlocked] Already at target position");
        return true;
    }
    
    // Try seeking using seek table first (fast path)
    if (m_has_seek_table && !m_seek_table.empty()) {
        Debug::log("flac_codec", "[NativeFLACCodec::seek_unlocked] Using seek table for fast seeking");
        if (seekUsingTable(target_sample)) {
            Debug::log("flac_codec", "[NativeFLACCodec::seek_unlocked] Seek using table successful");
            return true;
        }
        Debug::log("flac_codec", "[NativeFLACCodec::seek_unlocked] Seek using table failed, falling back to scanning");
    }
    
    // Fall back to frame scanning (slow path)
    Debug::log("flac_codec", "[NativeFLACCodec::seek_unlocked] Using frame scanning for seeking");
    if (seekByScanning(target_sample)) {
        Debug::log("flac_codec", "[NativeFLACCodec::seek_unlocked] Seek by scanning successful");
        return true;
    }
    
    Debug::log("flac_codec", "[NativeFLACCodec::seek_unlocked] Seek failed");
    return false;
}

void FLACCodec::setSeekTable_unlocked(const std::vector<SeekPoint>& seek_table) {
    Debug::log("flac_codec", "[NativeFLACCodec::setSeekTable_unlocked] Setting seek table with ",
              seek_table.size(), " points");
    
    m_seek_table = seek_table;
    m_has_seek_table = !seek_table.empty();
    
    // Log seek table contents for debugging
    if (m_has_seek_table) {
        Debug::log("flac_codec", "[NativeFLACCodec::setSeekTable_unlocked] Seek table points:");
        for (size_t i = 0; i < std::min(seek_table.size(), size_t(10)); i++) {
            const auto& point = seek_table[i];
            if (!point.isPlaceholder()) {
                Debug::log("flac_codec", "[NativeFLACCodec::setSeekTable_unlocked]   Point ", i,
                          ": sample=", point.sample_number,
                          ", offset=", point.byte_offset,
                          ", frame_samples=", point.frame_samples);
            }
        }
        if (seek_table.size() > 10) {
            Debug::log("flac_codec", "[NativeFLACCodec::setSeekTable_unlocked]   ... and ",
                      seek_table.size() - 10, " more points");
        }
    }
}

bool FLACCodec::seekUsingTable(uint64_t target_sample) {
    Debug::log("flac_codec", "[NativeFLACCodec::seekUsingTable] Seeking to sample ", target_sample,
              " using seek table");
    
    // Find nearest seek point at or before target sample
    SeekPoint nearest = findNearestSeekPoint(target_sample);
    
    if (nearest.isPlaceholder()) {
        Debug::log("flac_codec", "[NativeFLACCodec::seekUsingTable] No valid seek point found");
        return false;
    }
    
    Debug::log("flac_codec", "[NativeFLACCodec::seekUsingTable] Found seek point: sample=",
              nearest.sample_number, ", offset=", nearest.byte_offset);
    
    // Reset decoder state for seek (Requirement 43)
    Debug::log("flac_codec", "[NativeFLACCodec::seekUsingTable] Resetting decoder state");
    reset_unlocked();
    
    // Update current sample position to seek point
    m_current_sample.store(nearest.sample_number);
    
    // Note: In a complete implementation, we would:
    // 1. Position the input stream to nearest.byte_offset from first frame
    // 2. Feed data from that position to the bitstream reader
    // 3. If target_sample != nearest.sample_number, decode frames until we reach target
    //
    // However, this requires integration with the demuxer/IO layer to reposition
    // the input stream. For now, we set the sample position and rely on the
    // demuxer to provide data from the correct position.
    
    Debug::log("flac_codec", "[NativeFLACCodec::seekUsingTable] Seek using table completed, ",
              "positioned at sample ", nearest.sample_number);
    
    return true;
}

bool FLACCodec::seekByScanning(uint64_t target_sample) {
    Debug::log("flac_codec", "[NativeFLACCodec::seekByScanning] Seeking to sample ", target_sample,
              " by frame scanning");
    
    // Reset decoder state for seek (Requirement 43)
    Debug::log("flac_codec", "[NativeFLACCodec::seekByScanning] Resetting decoder state");
    reset_unlocked();
    
    // Note: Frame scanning requires:
    // 1. Starting from beginning of stream (or last known position)
    // 2. Parsing frame headers to find sample positions
    // 3. Skipping frames until we reach target sample
    //
    // This is a slow operation that requires full stream access.
    // For now, we just reset to beginning and let normal decoding proceed.
    
    m_current_sample.store(0);
    
    Debug::log("flac_codec", "[NativeFLACCodec::seekByScanning] Seek by scanning completed, ",
              "reset to beginning (full implementation requires demuxer integration)");
    
    return true;
}

SeekPoint FLACCodec::findNearestSeekPoint(uint64_t target_sample) const {
    Debug::log("flac_codec", "[NativeFLACCodec::findNearestSeekPoint] Finding nearest seek point for sample ",
              target_sample);
    
    if (m_seek_table.empty()) {
        Debug::log("flac_codec", "[NativeFLACCodec::findNearestSeekPoint] Seek table is empty");
        return SeekPoint();  // Return placeholder
    }
    
    // Find the seek point with largest sample_number <= target_sample
    // Seek table is sorted in ascending order per RFC 9639
    SeekPoint best_point;
    best_point.sample_number = 0xFFFFFFFFFFFFFFFFULL;  // Placeholder
    
    for (const auto& point : m_seek_table) {
        // Skip placeholders
        if (point.isPlaceholder()) {
            continue;
        }
        
        // If this point is at or before target, and better than current best
        if (point.sample_number <= target_sample) {
            if (best_point.isPlaceholder() || point.sample_number > best_point.sample_number) {
                best_point = point;
            }
        }
    }
    
    if (!best_point.isPlaceholder()) {
        Debug::log("flac_codec", "[NativeFLACCodec::findNearestSeekPoint] Found seek point at sample ",
                  best_point.sample_number, " (offset ", best_point.byte_offset, ")");
    } else {
        Debug::log("flac_codec", "[NativeFLACCodec::findNearestSeekPoint] No suitable seek point found");
    }
    
    return best_point;
}

void FLACCodec::setStreamInfo(const StreamInfoMetadata& streaminfo) {
    Debug::log("flac_codec", "[NativeFLACCodec::setStreamInfo] [ENTRY] Setting STREAMINFO metadata");
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    Debug::log("flac_codec", "[NativeFLACCodec::setStreamInfo] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        setStreamInfo_unlocked(streaminfo);
        Debug::log("flac_codec", "[NativeFLACCodec::setStreamInfo] [EXIT] STREAMINFO set successfully");
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::setStreamInfo] [EXCEPTION] ", e.what());
    }
}

void FLACCodec::setMD5ValidationEnabled(bool enabled) {
    Debug::log("flac_codec", "[NativeFLACCodec::setMD5ValidationEnabled] [ENTRY] Setting MD5 validation to ",
              enabled ? "enabled" : "disabled");
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    Debug::log("flac_codec", "[NativeFLACCodec::setMD5ValidationEnabled] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        setMD5ValidationEnabled_unlocked(enabled);
        Debug::log("flac_codec", "[NativeFLACCodec::setMD5ValidationEnabled] [EXIT] MD5 validation setting updated");
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::setMD5ValidationEnabled] [EXCEPTION] ", e.what());
    }
}

bool FLACCodec::checkMD5Validation() const {
    Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation] [ENTRY] Checking MD5 validation result");
    
    std::lock_guard<std::mutex> lock(m_state_mutex);
    
    Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation] [LOCKED] State lock acquired, calling unlocked implementation");
    
    try {
        bool result = checkMD5Validation_unlocked();
        Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation] [EXIT] MD5 validation result: ",
                  result ? "PASS" : "FAIL");
        return result;
    } catch (const std::exception& e) {
        Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation] [EXCEPTION] ", e.what());
        return false;
    }
}

void FLACCodec::setStreamInfo_unlocked(const StreamInfoMetadata& streaminfo) {
    Debug::log("flac_codec", "[NativeFLACCodec::setStreamInfo_unlocked] Setting STREAMINFO metadata");
    
    m_streaminfo = streaminfo;
    m_has_streaminfo = true;
    
    // Log MD5 checksum for debugging
    Debug::log("flac_codec", "[NativeFLACCodec::setStreamInfo_unlocked] STREAMINFO: ",
              "sample_rate=", streaminfo.sample_rate,
              ", channels=", streaminfo.channels,
              ", bits_per_sample=", streaminfo.bits_per_sample,
              ", total_samples=", streaminfo.total_samples);
    
    // Check if MD5 is zero (validation disabled by encoder)
    if (MD5Validator::isZeroMD5(streaminfo.md5_sum)) {
        Debug::log("flac_codec", "[NativeFLACCodec::setStreamInfo_unlocked] MD5 checksum is zero, validation will be skipped");
    } else {
        Debug::log("flac_codec", "[NativeFLACCodec::setStreamInfo_unlocked] MD5 checksum present, validation enabled");
    }
    
    // Reset MD5 validator if already initialized and validation is enabled
    if (m_initialized && m_md5_validation_enabled && 
        !MD5Validator::isZeroMD5(streaminfo.md5_sum) && m_md5_validator) {
        Debug::log("flac_codec", "[NativeFLACCodec::setStreamInfo_unlocked] Resetting MD5 validator for new stream");
        if (!m_md5_validator->reset()) {
            Debug::log("flac_codec", "[NativeFLACCodec::setStreamInfo_unlocked] MD5 validator reset failed (warning)");
        }
    }
}

void FLACCodec::setMD5ValidationEnabled_unlocked(bool enabled) {
    Debug::log("flac_codec", "[NativeFLACCodec::setMD5ValidationEnabled_unlocked] Setting MD5 validation to ",
              enabled ? "enabled" : "disabled");
    
    m_md5_validation_enabled = enabled;
    
    // If enabling validation and we have streaminfo, reset MD5 validator
    if (enabled && m_has_streaminfo && !MD5Validator::isZeroMD5(m_streaminfo.md5_sum) && m_md5_validator) {
        Debug::log("flac_codec", "[NativeFLACCodec::setMD5ValidationEnabled_unlocked] Resetting MD5 validator");
        if (!m_md5_validator->reset()) {
            Debug::log("flac_codec", "[NativeFLACCodec::setMD5ValidationEnabled_unlocked] MD5 validator reset failed (warning)");
        }
    }
}

bool FLACCodec::checkMD5Validation_unlocked() const {
    Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation_unlocked] Checking MD5 validation result");
    
    // Check if validation was enabled
    if (!m_md5_validation_enabled) {
        Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation_unlocked] MD5 validation was disabled");
        return false;
    }
    
    // Check if we have streaminfo
    if (!m_has_streaminfo) {
        Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation_unlocked] No STREAMINFO metadata available");
        return false;
    }
    
    // Check if MD5 is zero (validation skipped by encoder)
    if (MD5Validator::isZeroMD5(m_streaminfo.md5_sum)) {
        Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation_unlocked] MD5 checksum is zero, validation skipped");
        return true;  // Consider it valid if encoder didn't compute MD5
    }
    
    // Check if validator exists
    if (!m_md5_validator) {
        Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation_unlocked] MD5 validator not initialized");
        return false;
    }
    
    // Finalize MD5 computation
    uint8_t computed_md5[16];
    if (!m_md5_validator->finalize(computed_md5)) {
        Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation_unlocked] MD5 finalization failed");
        return false;
    }
    
    // Compare with expected MD5
    bool matches = m_md5_validator->compare(m_streaminfo.md5_sum);
    
    if (matches) {
        Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation_unlocked] MD5 checksum MATCHES - audio integrity verified");
    } else {
        Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation_unlocked] MD5 checksum MISMATCH - audio integrity error!");
        
        // Log both MD5 values for debugging
        Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation_unlocked] Expected MD5: ",
                  std::hex, std::setfill('0'),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[0]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[1]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[2]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[3]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[4]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[5]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[6]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[7]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[8]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[9]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[10]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[11]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[12]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[13]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[14]),
                  std::setw(2), static_cast<int>(m_streaminfo.md5_sum[15]));
        
        Debug::log("flac_codec", "[NativeFLACCodec::checkMD5Validation_unlocked] Computed MD5: ",
                  std::hex, std::setfill('0'),
                  std::setw(2), static_cast<int>(computed_md5[0]),
                  std::setw(2), static_cast<int>(computed_md5[1]),
                  std::setw(2), static_cast<int>(computed_md5[2]),
                  std::setw(2), static_cast<int>(computed_md5[3]),
                  std::setw(2), static_cast<int>(computed_md5[4]),
                  std::setw(2), static_cast<int>(computed_md5[5]),
                  std::setw(2), static_cast<int>(computed_md5[6]),
                  std::setw(2), static_cast<int>(computed_md5[7]),
                  std::setw(2), static_cast<int>(computed_md5[8]),
                  std::setw(2), static_cast<int>(computed_md5[9]),
                  std::setw(2), static_cast<int>(computed_md5[10]),
                  std::setw(2), static_cast<int>(computed_md5[11]),
                  std::setw(2), static_cast<int>(computed_md5[12]),
                  std::setw(2), static_cast<int>(computed_md5[13]),
                  std::setw(2), static_cast<int>(computed_md5[14]),
                  std::setw(2), static_cast<int>(computed_md5[15]));
    }
    
    return matches;
}

/**
 * Validate streamable subset constraints
 * Per RFC 9639 and Requirement 19
 * 
 * Streamable subset constraints:
 * - Block size ≤ 16384 (Requirement 19.4)
 * - For sample rate ≤ 48kHz: block size ≤ 4608 (Requirement 19.5)
 * - For sample rate ≤ 48kHz: LPC order ≤ 12 (Requirement 19.6) - checked in SubframeDecoder
 * - Rice partition order ≤ 8 (Requirement 19.7) - checked in ResidualDecoder
 * 
 * Note: This validation is informational only. The decoder can handle
 * non-streamable files, but this helps identify streamable subset compliance.
 */
bool FLACCodec::validateStreamableSubset(const FrameHeader& header) const {
    bool is_streamable = true;
    
    // Check block size constraint (Requirement 19.4)
    if (header.block_size > 16384) {
        Debug::log("flac_codec", "[NativeFLACCodec::validateStreamableSubset] Block size ", header.block_size,
                  " exceeds streamable subset maximum of 16384");
        is_streamable = false;
    }
    
    // Check sample rate specific constraints (Requirement 19.5)
    if (header.sample_rate > 0 && header.sample_rate <= 48000) {
        if (header.block_size > 4608) {
            Debug::log("flac_codec", "[NativeFLACCodec::validateStreamableSubset] Block size ", header.block_size,
                      " exceeds streamable subset maximum of 4608 for sample rate ≤ 48kHz");
            is_streamable = false;
        }
    }
    
    // LPC order and Rice partition order are validated in their respective decoders
    // (Requirements 19.6, 19.7)
    
    if (is_streamable) {
        Debug::log("flac_codec", "[NativeFLACCodec::validateStreamableSubset] Frame conforms to streamable subset");
    } else {
        Debug::log("flac_codec", "[NativeFLACCodec::validateStreamableSubset] Frame does not conform to streamable subset (decoder can still handle it)");
    }
    
    return is_streamable;
}

// ============================================================================
// FLACCodecSupport Implementation
// ============================================================================

namespace FLACCodecSupport {

bool isFLACStream(const StreamInfo& stream_info) {
    return stream_info.codec_name == "flac" && 
           stream_info.codec_type == "audio";
}

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

std::string getCodecInfo() {
    return "Native FLAC Codec v1.0 - RFC 9639 compliant, pure C++ implementation without libFLAC dependency";
}

} // namespace FLACCodecSupport

// ============================================================================
// Error Recovery Implementation (Requirement 11)
// ============================================================================

bool FLACCodec::recoverFromSyncLoss() {
    Debug::log("flac_codec", "[NativeFLACCodec::recoverFromSyncLoss] Attempting to recover from sync loss");
    
    m_stats.sync_errors++;

    // Search for next valid frame sync pattern (Requirement 11.1)
    // Try multiple times with limited search window to prevent infinite loops
    const size_t MAX_SEARCH_ATTEMPTS = 100;
    
    for (size_t attempt = 0; attempt < MAX_SEARCH_ATTEMPTS; attempt++) {
        if (m_frame_parser->findSync()) {
            Debug::log("flac_codec", "[NativeFLACCodec::recoverFromSyncLoss] Found sync pattern after ",
                      attempt + 1, " attempts");
            
            // Validate that this is a genuine sync by checking frame header
            FrameHeader test_header;
            if (m_frame_parser->parseFrameHeader(test_header)) {
                Debug::log("flac_codec", "[NativeFLACCodec::recoverFromSyncLoss] Sync recovery successful");
                m_consecutive_errors = 0;  // Reset error counter on successful recovery
                return true;
            }
            
            Debug::log("flac_codec", "[NativeFLACCodec::recoverFromSyncLoss] False sync - header validation failed");
        }
    }
    
    Debug::log("flac_codec", "[NativeFLACCodec::recoverFromSyncLoss] Sync recovery failed after ",
              MAX_SEARCH_ATTEMPTS, " attempts");
    m_last_error = FLACError::INVALID_SYNC;
    return false;
}

bool FLACCodec::recoverFromInvalidHeader() {
    Debug::log("flac_codec", "[NativeFLACCodec::recoverFromInvalidHeader] Attempting to recover from invalid header");
    
    // Skip to next frame by searching for next sync pattern (Requirement 11.2)
    // This is similar to sync loss recovery but specifically for header errors
    
    if (recoverFromSyncLoss()) {
        Debug::log("flac_codec", "[NativeFLACCodec::recoverFromInvalidHeader] Header recovery successful");
        m_consecutive_errors = 0;
        return true;
    }
    
    Debug::log("flac_codec", "[NativeFLACCodec::recoverFromInvalidHeader] Header recovery failed");
    m_last_error = FLACError::INVALID_HEADER;
    m_consecutive_errors++;
    
    // Check if we've exceeded maximum consecutive errors
    if (m_consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
        Debug::log("flac_codec", "[NativeFLACCodec::recoverFromInvalidHeader] Too many consecutive errors (",
                  m_consecutive_errors, "), treating as unrecoverable");
        handleUnrecoverableError();
        return false;
    }
    
    return false;
}

void FLACCodec::recoverFromSubframeError(int32_t* channel_buffer, uint32_t sample_count) {
    Debug::log("flac_codec", "[NativeFLACCodec::recoverFromSubframeError] Outputting silence for ",
              sample_count, " samples");
    
    m_stats.error_count++;

    // Fill channel buffer with silence (zeros) (Requirement 11.3)
    std::memset(channel_buffer, 0, sample_count * sizeof(int32_t));
    
    m_last_error = FLACError::INVALID_SUBFRAME;
    m_consecutive_errors++;
    
    // Log warning but allow decoding to continue with other channels
    Debug::log("flac_codec", "[NativeFLACCodec::recoverFromSubframeError] Subframe error handled, continuing with silence");
}

bool FLACCodec::recoverFromCRCError(FLACError error_type) {
    Debug::log("flac_codec", "[NativeFLACCodec::recoverFromCRCError] Handling CRC error: ",
              (error_type == FLACError::CRC_MISMATCH ? "CRC_MISMATCH" : "UNKNOWN"));
    
    m_stats.crc_errors++;
    m_stats.error_count++;

    // RFC 9639 allows using data even with CRC mismatch (Requirement 11.4)
    // Log error but attempt to use decoded data
    
    m_last_error = error_type;
    m_consecutive_errors++;
    
    // For header CRC failures, we should skip the frame
    // For frame CRC failures, we can try to use the data
    if (error_type == FLACError::CRC_MISMATCH) {
        Debug::log("flac_codec", "[NativeFLACCodec::recoverFromCRCError] Frame CRC mismatch - attempting to use data anyway (RFC allows)");
        
        // Check if we've had too many consecutive CRC errors
        if (m_consecutive_errors >= MAX_CONSECUTIVE_ERRORS / 2) {
            Debug::log("flac_codec", "[NativeFLACCodec::recoverFromCRCError] Too many consecutive CRC errors (",
                      m_consecutive_errors, "), discarding frame");
            return false;
        }
        
        // Use the data despite CRC error
        return true;
    }
    
    // For other CRC-related errors, discard the frame
    Debug::log("flac_codec", "[NativeFLACCodec::recoverFromCRCError] Discarding frame due to CRC error");
    return false;
}

void FLACCodec::recoverFromMemoryError() {
    Debug::log("flac_codec", "[NativeFLACCodec::recoverFromMemoryError] Handling memory allocation failure");
    
    m_stats.memory_errors++;
    m_stats.error_count++;

    // Clean up partially allocated resources (Requirement 11.6)
    
    // Clear all buffers to free memory
    m_input_buffer.clear();
    m_input_buffer.shrink_to_fit();
    
    for (size_t i = 0; i < MAX_CHANNELS; i++) {
        m_decode_buffer[i].clear();
        m_decode_buffer[i].shrink_to_fit();
    }
    
    m_output_buffer.clear();
    m_output_buffer.shrink_to_fit();
    
    m_last_error = FLACError::MEMORY_ALLOCATION;
    m_state = DecoderState::DECODER_ERROR;
    
    Debug::log("flac_codec", "[NativeFLACCodec::recoverFromMemoryError] Memory cleanup complete, decoder in ERROR state");
}

void FLACCodec::handleUnrecoverableError() {
    Debug::log("flac_codec", "[NativeFLACCodec::handleUnrecoverableError] Handling unrecoverable error");
    
    // Note: m_stats.error_count is incremented by caller (exception handler or recover function)

    // Transition to ERROR state (Requirement 11.8)
    m_state = DecoderState::DECODER_ERROR;
    m_last_error = FLACError::UNRECOVERABLE_ERROR;
    
    // Clear all buffers
    m_input_buffer.clear();
    for (size_t i = 0; i < MAX_CHANNELS; i++) {
        m_decode_buffer[i].clear();
    }
    m_output_buffer.clear();
    
    // Reset bitstream reader
    if (m_bitstream_reader) {
        m_bitstream_reader->clearBuffer();
    }
    
    Debug::log("flac_codec", "[NativeFLACCodec::handleUnrecoverableError] Decoder reset to ERROR state, reinitialization required");
}

// ============================================================================
// State Management Implementation (Requirement 64)
// ============================================================================

bool FLACCodec::transitionState(DecoderState new_state) {
    // Validate state transition (Requirement 64.8)
    if (!isValidStateTransition(m_state, new_state)) {
        Debug::log("flac_codec", "[NativeFLACCodec::transitionState] Invalid state transition: ",
                  getStateName(m_state), " -> ", getStateName(new_state));
        return false;
    }
    
    DecoderState old_state = m_state;
    m_state = new_state;
    
    Debug::log("flac_codec", "[NativeFLACCodec::transitionState] State transition: ",
              getStateName(old_state), " -> ", getStateName(new_state));
    
    // Reset error counter on successful transition to non-error state
    if (new_state != DecoderState::DECODER_ERROR) {
        m_consecutive_errors = 0;
        m_last_error = FLACError::NONE;
    }
    
    return true;
}

bool FLACCodec::isValidStateTransition(DecoderState current, DecoderState target) const {
    // Define valid state transitions (Requirement 64.8)
    
    switch (current) {
        case DecoderState::UNINITIALIZED:
            // Can only transition to INITIALIZED or ERROR
            return (target == DecoderState::INITIALIZED || 
                    target == DecoderState::DECODER_ERROR);
        
        case DecoderState::INITIALIZED:
            // Can transition to DECODING, ERROR, or END_OF_STREAM
            return (target == DecoderState::DECODING || 
                    target == DecoderState::DECODER_ERROR ||
                    target == DecoderState::END_OF_STREAM ||
                    target == DecoderState::UNINITIALIZED);  // Allow reset
        
        case DecoderState::DECODING:
            // Can transition to INITIALIZED (after frame), ERROR, or END_OF_STREAM
            return (target == DecoderState::INITIALIZED || 
                    target == DecoderState::DECODER_ERROR ||
                    target == DecoderState::END_OF_STREAM ||
                    target == DecoderState::UNINITIALIZED);  // Allow reset
        
        case DecoderState::DECODER_ERROR:
            // Can only transition to UNINITIALIZED (for reset)
            return (target == DecoderState::UNINITIALIZED);
        
        case DecoderState::END_OF_STREAM:
            // Can transition to INITIALIZED (for seek) or UNINITIALIZED (for reset)
            return (target == DecoderState::INITIALIZED || 
                    target == DecoderState::UNINITIALIZED);
        
        default:
            return false;
    }
}

bool FLACCodec::resetFromErrorState() {
    Debug::log("flac_codec", "[NativeFLACCodec::resetFromErrorState] Attempting to reset from ERROR state");
    
    // Can only reset from ERROR state (Requirement 64.5, 64.6)
    if (m_state != DecoderState::DECODER_ERROR) {
        Debug::log("flac_codec", "[NativeFLACCodec::resetFromErrorState] Not in ERROR state, current state: ",
                  getStateName(m_state));
        return false;
    }
    
    // Clear error tracking
    m_last_error = FLACError::NONE;
    m_consecutive_errors = 0;
    
    // Clear all buffers
    m_input_buffer.clear();
    for (size_t i = 0; i < MAX_CHANNELS; i++) {
        m_decode_buffer[i].clear();
    }
    m_output_buffer.clear();
    
    // Reset bitstream reader
    if (m_bitstream_reader) {
        m_bitstream_reader->clearBuffer();
    }
    
    // Reset current sample position
    m_current_sample = 0;
    
    // Transition to UNINITIALIZED state
    if (!transitionState(DecoderState::UNINITIALIZED)) {
        Debug::log("flac_codec", "[NativeFLACCodec::resetFromErrorState] Failed to transition to UNINITIALIZED");
        return false;
    }
    
    m_initialized = false;
    
    Debug::log("flac_codec", "[NativeFLACCodec::resetFromErrorState] Reset successful, decoder ready for reinitialization");
    return true;
}

const char* FLACCodec::getStateName(DecoderState state) const {
    switch (state) {
        case DecoderState::UNINITIALIZED:
            return "UNINITIALIZED";
        case DecoderState::INITIALIZED:
            return "INITIALIZED";
        case DecoderState::DECODING:
            return "DECODING";
        case DecoderState::DECODER_ERROR:
            return "ERROR";
        case DecoderState::END_OF_STREAM:
            return "END_OF_STREAM";
        default:
            return "UNKNOWN";
    }
}

// ============================================================================
// Test methods for RFC 9639 validation
// ============================================================================

bool FLACCodec::testValidateBitDepthRFC9639(uint16_t bits_per_sample) const {
    return (bits_per_sample >= 4 && bits_per_sample <= 32);
}

bool FLACCodec::testValidateSampleFormatConsistency(const FLAC__Frame* frame) const {
    if (!frame) return false;
    return (frame->header.bits_per_sample == m_stream_info.bits_per_sample &&
            frame->header.channels == m_stream_info.channels &&
            frame->header.sample_rate == m_stream_info.sample_rate);
}

bool FLACCodec::testValidateReservedBitDepthValues(uint16_t bits_per_sample) const {
    return (bits_per_sample >= 4 && bits_per_sample <= 32);
}

FLAC__int32 FLACCodec::testApplyProperSignExtension(FLAC__int32 sample, uint16_t source_bits) const {
    if (source_bits == 32) return sample;
    FLAC__int32 sign_bit = 1 << (source_bits - 1);
    return (sample ^ sign_bit) - sign_bit;
}

bool FLACCodec::testValidateBitPerfectReconstruction(const FLAC__int32* original, 
                                                  const int16_t* converted, 
                                                  size_t sample_count, 
                                                  uint16_t source_bits) const {
    if (!original || !converted || sample_count == 0) return false;
    for (size_t i = 0; i < sample_count; ++i) {
        int16_t expected = 0;
        if (source_bits == 8) expected = static_cast<int16_t>(original[i] << 8);
        else if (source_bits == 16) expected = static_cast<int16_t>(original[i]);
        else if (source_bits == 24) expected = static_cast<int16_t>((original[i] + 128) >> 8);
        else if (source_bits == 32) expected = static_cast<int16_t>((original[i] + 32768) >> 16);
        else if (source_bits >= 4 && source_bits <= 12) expected = static_cast<int16_t>(original[i] << (16 - source_bits));
        else {
            uint32_t shift = (source_bits > 16) ? (source_bits - 16) : 0;
            if (shift > 0) expected = static_cast<int16_t>((original[i] + (1 << (shift - 1))) >> shift);
            else expected = static_cast<int16_t>(original[i] << (16 - source_bits));
        }
        if (converted[i] != expected) return false;
    }
    return true;
}

AudioQualityMetrics FLACCodec::testCalculateAudioQualityMetrics(const int16_t* samples, 
                                                             size_t sample_count, 
                                                             const FLAC__int32* reference,
                                                             uint16_t reference_bits) const {
    AudioQualityMetrics metrics;
    if (sample_count == 0 || !samples) return metrics;
    
    double sum_sq_signal = 0;
    double sum_sq_noise = 0;
    int32_t max_val = 0;
    
    for (size_t i = 0; i < sample_count; ++i) {
        int32_t abs_val = std::abs((int32_t)samples[i]);
        if (abs_val > max_val) max_val = abs_val;
        
        sum_sq_signal += (double)samples[i] * samples[i];
        
        if (samples[i] == 32767 || samples[i] == -32768) {
            metrics.clipped_samples++;
        }
        
        if (i > 0 && ((samples[i-1] < 0 && samples[i] >= 0) || (samples[i-1] >= 0 && samples[i] < 0))) {
            metrics.zero_crossings++;
        }
        
        if (reference) {
            int16_t ref_16;
            if (reference_bits == 8) ref_16 = static_cast<int16_t>(reference[i] << 8);
            else if (reference_bits == 16) ref_16 = static_cast<int16_t>(reference[i]);
            else if (reference_bits == 24) ref_16 = static_cast<int16_t>((reference[i] + 128) >> 8);
            else ref_16 = 0;
            
            double noise = (double)samples[i] - (double)ref_16;
            sum_sq_noise += noise * noise;
        }
    }
    
    metrics.peak_amplitude = (double)max_val / 32768.0;
    metrics.rms_amplitude = std::sqrt(sum_sq_signal / sample_count) / 32768.0;
    
    if (reference && sum_sq_noise > 0) {
        metrics.signal_to_noise_ratio_db = 10.0 * std::log10(sum_sq_signal / sum_sq_noise);
    } else if (reference) {
        metrics.signal_to_noise_ratio_db = 100.0; // Perfect match
    }
    
    return metrics;
}

int16_t FLACCodec::testConvert8BitTo16Bit(FLAC__int32 sample) const {
    return static_cast<int16_t>(sample << 8);
}

int16_t FLACCodec::testConvert24BitTo16Bit(FLAC__int32 sample) const {
    return static_cast<int16_t>((sample + 128) >> 8);
}

int16_t FLACCodec::testConvert32BitTo16Bit(FLAC__int32 sample) const {
    return static_cast<int16_t>((sample + 32768) >> 16);
}

} // namespace FLAC
} // namespace Codec
} // namespace PsyMP3

#endif // HAVE_NATIVE_FLAC
