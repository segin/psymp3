/*
 * VorbisCodec.cpp - Container-agnostic Vorbis audio codec implementation
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

#ifdef HAVE_OGGDEMUXER

namespace PsyMP3 {
namespace Codec {
namespace Vorbis {

// ========== VorbisHeaderInfo Implementation ==========

bool VorbisHeaderInfo::isValid() const
{
    // Validate according to Vorbis specification
    if (version != 0) return false;
    if (channels == 0 || channels > 255) return false;
    if (sample_rate == 0) return false;
    // Block sizes must be powers of 2 in range [64, 8192]
    // and blocksize_0 <= blocksize_1
    if (blocksize_0 < 6 || blocksize_0 > 13) return false;  // 2^6=64 to 2^13=8192
    if (blocksize_1 < 6 || blocksize_1 > 13) return false;
    if (blocksize_0 > blocksize_1) return false;
    return true;
}

VorbisHeaderInfo VorbisHeaderInfo::parseFromPacket(const std::vector<uint8_t>& packet_data)
{
    VorbisHeaderInfo info = {};
    
    // Vorbis identification header format:
    // [0]: packet type (0x01)
    // [1-6]: "vorbis"
    // [7-10]: version (little-endian uint32)
    // [11]: channels
    // [12-15]: sample_rate (little-endian uint32)
    // [16-19]: bitrate_maximum (little-endian int32)
    // [20-23]: bitrate_nominal (little-endian int32)
    // [24-27]: bitrate_minimum (little-endian int32)
    // [28]: blocksize_0 (4 bits) | blocksize_1 (4 bits)
    // [29]: framing flag
    
    if (packet_data.size() < 30) {
        return info;
    }
    
    // Verify packet type and "vorbis" signature
    if (packet_data[0] != 0x01) return info;
    if (std::memcmp(&packet_data[1], "vorbis", 6) != 0) return info;
    
    // Parse fields (little-endian)
    info.version = packet_data[7] | (packet_data[8] << 8) | 
                   (packet_data[9] << 16) | (packet_data[10] << 24);
    info.channels = packet_data[11];
    info.sample_rate = packet_data[12] | (packet_data[13] << 8) | 
                       (packet_data[14] << 16) | (packet_data[15] << 24);
    info.bitrate_maximum = packet_data[16] | (packet_data[17] << 8) | 
                           (packet_data[18] << 16) | (packet_data[19] << 24);
    info.bitrate_nominal = packet_data[20] | (packet_data[21] << 8) | 
                           (packet_data[22] << 16) | (packet_data[23] << 24);
    info.bitrate_minimum = packet_data[24] | (packet_data[25] << 8) | 
                           (packet_data[26] << 16) | (packet_data[27] << 24);
    
    // Block sizes are stored as log2 values in a single byte
    info.blocksize_0 = packet_data[28] & 0x0F;
    info.blocksize_1 = (packet_data[28] >> 4) & 0x0F;
    
    return info;
}

// ========== VorbisCommentInfo Implementation ==========

VorbisCommentInfo VorbisCommentInfo::parseFromPacket(const std::vector<uint8_t>& packet_data)
{
    VorbisCommentInfo info;
    
    // Vorbis comment header format:
    // [0]: packet type (0x03)
    // [1-6]: "vorbis"
    // [7-10]: vendor_length (little-endian uint32)
    // [11-...]: vendor_string
    // [...]: user_comment_list_length (little-endian uint32)
    // [...]: user_comments (length-prefixed strings)
    
    if (packet_data.size() < 11) {
        return info;
    }
    
    // Verify packet type and "vorbis" signature
    if (packet_data[0] != 0x03) return info;
    if (std::memcmp(&packet_data[1], "vorbis", 6) != 0) return info;
    
    size_t offset = 7;
    
    // Parse vendor string
    if (offset + 4 > packet_data.size()) return info;
    uint32_t vendor_length = packet_data[offset] | (packet_data[offset + 1] << 8) |
                             (packet_data[offset + 2] << 16) | (packet_data[offset + 3] << 24);
    offset += 4;
    
    if (offset + vendor_length > packet_data.size()) return info;
    info.vendor_string = std::string(reinterpret_cast<const char*>(&packet_data[offset]), vendor_length);
    offset += vendor_length;
    
    // Parse user comments
    if (offset + 4 > packet_data.size()) return info;
    uint32_t comment_count = packet_data[offset] | (packet_data[offset + 1] << 8) |
                             (packet_data[offset + 2] << 16) | (packet_data[offset + 3] << 24);
    offset += 4;
    
    for (uint32_t i = 0; i < comment_count && offset + 4 <= packet_data.size(); i++) {
        uint32_t comment_length = packet_data[offset] | (packet_data[offset + 1] << 8) |
                                  (packet_data[offset + 2] << 16) | (packet_data[offset + 3] << 24);
        offset += 4;
        
        if (offset + comment_length > packet_data.size()) break;
        
        std::string comment(reinterpret_cast<const char*>(&packet_data[offset]), comment_length);
        offset += comment_length;
        
        // Split on '=' to get key=value pair
        size_t eq_pos = comment.find('=');
        if (eq_pos != std::string::npos) {
            info.user_comments.emplace_back(
                comment.substr(0, eq_pos),
                comment.substr(eq_pos + 1)
            );
        }
    }
    
    return info;
}

// ========== Vorbis Stream Class ==========

Vorbis::Vorbis(TagLib::String name) : Stream(name)
{
    Debug::log("vorbis", "Vorbis stream constructor called for: ", name.toCString());
    
    // Create a DemuxedStream to handle the actual decoding
    m_demuxed_stream = std::make_unique<DemuxedStream>(name);
    
    // Copy properties from the demuxed stream
    m_rate = m_demuxed_stream->getRate();
    m_channels = m_demuxed_stream->getChannels();
    m_bitrate = m_demuxed_stream->getBitrate();
    m_length = m_demuxed_stream->getLength();
    m_slength = m_demuxed_stream->getSLength();
    
    Debug::log("vorbis", "Vorbis stream initialized: rate=", m_rate, " channels=", m_channels);
}

Vorbis::~Vorbis()
{
    Debug::log("vorbis", "Vorbis stream destructor called");
    // Unique pointer handles cleanup
}

size_t Vorbis::getData(size_t len, void *buf)
{
    return m_demuxed_stream->getData(len, buf);
}

void Vorbis::seekTo(unsigned long pos)
{
    m_demuxed_stream->seekTo(pos);
}

bool Vorbis::eof()
{
    return m_demuxed_stream->eof();
}

// ========== VorbisCodec Constructor ==========

VorbisCodec::VorbisCodec(const StreamInfo& stream_info) : AudioCodec(stream_info)
{
    Debug::log("vorbis", "VorbisCodec constructor called");
    
    // Note: libvorbis structures are initialized in initialize() method (Requirement 2.1)
    // This follows the pattern where constructor sets up member variables,
    // and initialize() performs the actual codec setup
    
    // Zero-initialize libvorbis structures (will be properly initialized in initialize())
    std::memset(&m_vorbis_info, 0, sizeof(m_vorbis_info));
    std::memset(&m_vorbis_comment, 0, sizeof(m_vorbis_comment));
    std::memset(&m_vorbis_dsp, 0, sizeof(m_vorbis_dsp));
    std::memset(&m_vorbis_block, 0, sizeof(m_vorbis_block));
    
    // Initialize atomic variables
    m_samples_decoded.store(0);
    m_granule_position.store(0);
    m_error_state.store(false);
    
    // Initialize state variables
    m_header_packets_received = 0;
    m_decoder_initialized = false;
    m_sample_rate = 0;
    m_channels = 0;
    m_bits_per_sample = 16;  // Always output 16-bit PCM
    m_block_size_short = 0;
    m_block_size_long = 0;
    m_pcm_channels = nullptr;
    
    Debug::log("vorbis", "VorbisCodec constructor completed");
}

// ========== VorbisCodec Destructor ==========

VorbisCodec::~VorbisCodec()
{
    Debug::log("vorbis", "VorbisCodec destructor called");
    
    // Ensure proper cleanup before codec destruction (Requirement 10.6)
    // Thread safety: Acquire lock to ensure no operations are in progress before destruction
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clean up libvorbis structures in reverse initialization order (Requirement 2.8)
    // Only cleanup if initialize() was called (m_initialized is set)
    if (m_initialized) {
        cleanupVorbisStructures_unlocked();
    }
    
    // Clear output buffer
    m_output_buffer.clear();
    
    Debug::log("vorbis", "VorbisCodec destructor completed");
}

// ========== AudioCodec Interface Implementation ==========

bool VorbisCodec::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("vorbis", "VorbisCodec::initialize called");
    
    // Clean up any existing state first (Requirement 2.8)
    if (m_decoder_initialized) {
        cleanupVorbisStructures_unlocked();
    }
    
    // Initialize libvorbis structures (Requirement 2.1)
    vorbis_info_init(&m_vorbis_info);
    vorbis_comment_init(&m_vorbis_comment);
    
    // Reset state for fresh initialization
    m_header_packets_received = 0;
    m_decoder_initialized = false;
    m_samples_decoded.store(0);
    m_granule_position.store(0);
    m_error_state.store(false);
    m_last_error.clear();
    m_output_buffer.clear();
    m_backpressure_active = false;  // Reset backpressure state (Requirement 7.6)
    
    // Extract Vorbis parameters from StreamInfo (Requirement 11.2, 6.2)
    // These will be overwritten when we process the identification header,
    // but we can use them as hints for buffer allocation
    if (m_stream_info.sample_rate > 0) {
        m_sample_rate = m_stream_info.sample_rate;
        Debug::log("vorbis", "Using StreamInfo sample_rate hint: ", m_sample_rate);
    }
    if (m_stream_info.channels > 0) {
        m_channels = m_stream_info.channels;
        Debug::log("vorbis", "Using StreamInfo channels hint: ", m_channels);
    }
    
    // Set up internal buffers and state variables
    // Reserve buffer space based on expected configuration
    // Maximum Vorbis block size is 8192 samples per channel
    constexpr size_t MAX_VORBIS_BLOCK_SIZE = 8192;
    size_t expected_channels = m_channels > 0 ? m_channels : 2; // Default to stereo
    
    // Handle memory allocation failures (Requirement 8.6)
    try {
        m_output_buffer.reserve(MAX_VORBIS_BLOCK_SIZE * expected_channels);
    } catch (const std::bad_alloc& e) {
        m_last_error = "Memory allocation failed during initialization: " + std::string(e.what());
        Debug::log("vorbis", m_last_error);
        Debug::log("error", "VorbisCodec: ", m_last_error);
        // Clean up partially initialized state
        vorbis_comment_clear(&m_vorbis_comment);
        vorbis_info_clear(&m_vorbis_info);
        throw BadFormatException(m_last_error);
    }
    
    // Process codec_data if available (may contain pre-extracted headers)
    // This is useful when headers are stored in container metadata
    if (!m_stream_info.codec_data.empty()) {
        Debug::log("vorbis", "StreamInfo contains codec_data of size: ", 
                   m_stream_info.codec_data.size());
        // Note: codec_data processing is handled during decode() when headers arrive
        // The demuxer typically sends headers as separate packets
    }
    
    m_initialized = true;
    
    Debug::log("vorbis", "VorbisCodec::initialize completed successfully");
    return true;
}

bool VorbisCodec::canDecode(const StreamInfo& stream_info) const
{
    // Thread-safe read-only operation, no lock needed (Requirement 11.6, 6.6)
    
    // Check if StreamInfo contains "vorbis" codec name
    if (stream_info.codec_name != "vorbis") {
        return false;
    }
    
    // Validate basic Vorbis stream parameters
    // Note: Some parameters may be 0 if not yet known from headers
    // We only reject clearly invalid configurations
    
    // If sample_rate is specified, it must be valid (Vorbis supports 1-200000 Hz)
    if (stream_info.sample_rate > 0) {
        if (stream_info.sample_rate > 200000) {
            Debug::log("vorbis", "canDecode: Invalid sample rate ", stream_info.sample_rate);
            return false;
        }
    }
    
    // If channels is specified, it must be valid (Vorbis supports 1-255 channels)
    if (stream_info.channels > 0) {
        if (stream_info.channels > 255) {
            Debug::log("vorbis", "canDecode: Invalid channel count ", stream_info.channels);
            return false;
        }
    }
    
    // Codec type should be "audio" if specified
    if (!stream_info.codec_type.empty() && stream_info.codec_type != "audio") {
        Debug::log("vorbis", "canDecode: Not an audio stream, type=", stream_info.codec_type);
        return false;
    }
    
    return true;
}

AudioFrame VorbisCodec::decode(const MediaChunk& chunk)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("vorbis", "VorbisCodec::decode called - chunk_size=", chunk.data.size());
    
    AudioFrame frame;
    
    // Handle empty packets and end-of-stream conditions (Requirement 11.3)
    if (chunk.data.empty()) {
        Debug::log("vorbis", "Empty chunk received, returning empty frame");
        return frame;
    }
    
    // Check for error state
    if (m_error_state.load()) {
        Debug::log("vorbis", "Codec in error state, returning empty frame");
        return frame;
    }
    
    // First 3 packets are headers (identification, comment, setup) (Requirement 1.1)
    if (m_header_packets_received < 3) {
        Debug::log("vorbis", "Processing header packet ", m_header_packets_received + 1);
        if (processHeaderPacket_unlocked(chunk.data)) {
            m_header_packets_received++;
            Debug::log("vorbis", "Header packet ", m_header_packets_received, " processed successfully");
            
            // After all 3 headers, initialize synthesis (Requirement 2.3)
            if (m_header_packets_received == 3) {
                // Initialize synthesis - handle memory allocation failures (Requirement 8.6)
                int synth_result = vorbis_synthesis_init(&m_vorbis_dsp, &m_vorbis_info);
                if (synth_result != 0) {
                    m_error_state.store(true);
                    m_last_error = "Failed to initialize Vorbis synthesis (error: " + 
                                   std::to_string(synth_result) + ")";
                    Debug::log("vorbis", m_last_error);
                    Debug::log("error", "VorbisCodec: ", m_last_error);
                    // Memory allocation failure during synthesis init
                    throw BadFormatException(m_last_error);
                }
                
                int block_result = vorbis_block_init(&m_vorbis_dsp, &m_vorbis_block);
                if (block_result != 0) {
                    vorbis_dsp_clear(&m_vorbis_dsp);
                    m_error_state.store(true);
                    m_last_error = "Failed to initialize Vorbis block (error: " + 
                                   std::to_string(block_result) + ")";
                    Debug::log("vorbis", m_last_error);
                    Debug::log("error", "VorbisCodec: ", m_last_error);
                    // Memory allocation failure during block init
                    throw BadFormatException(m_last_error);
                }
                m_decoder_initialized = true;
                Debug::log("vorbis", "Vorbis synthesis initialized successfully");
            }
        } else {
            Debug::log("vorbis", "Header packet processing failed");
        }
        return frame; // Headers don't produce audio
    }
    
    // Process audio packet (Requirement 1.5)
    if (!m_decoder_initialized) {
        Debug::log("vorbis", "Decoder not initialized, skipping packet");
        return frame;
    }
    
    // Decode the audio packet
    frame = decodeAudioPacket_unlocked(chunk.data);
    
    // Update timestamp from chunk
    if (!frame.samples.empty()) {
        frame.timestamp_samples = chunk.timestamp_samples;
        // Calculate timestamp_ms from samples if sample rate is known
        if (m_sample_rate > 0) {
            frame.timestamp_ms = (chunk.timestamp_samples * 1000ULL) / m_sample_rate;
        }
    }
    
    return frame;
}

AudioFrame VorbisCodec::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("vorbis", "VorbisCodec::flush called");
    
    AudioFrame frame;
    
    if (m_decoder_initialized) {
        // Process any remaining samples (Requirement 4.8, 7.5)
        synthesizeBlock_unlocked();
        
        if (!m_output_buffer.empty()) {
            frame.sample_rate = m_sample_rate;
            frame.channels = m_channels;
            frame.samples = std::move(m_output_buffer);
            m_output_buffer.clear();
            Debug::log("vorbis", "Flushed ", frame.samples.size(), " samples");
        }
    }
    
    return frame;
}

void VorbisCodec::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("vorbis", "VorbisCodec::reset called");
    
    // For seeking, we use vorbis_synthesis_restart() to reset synthesis state
    // without reinitializing headers (Requirement 2.7)
    if (m_decoder_initialized) {
        vorbis_synthesis_restart(&m_vorbis_dsp);
        Debug::log("vorbis", "Vorbis synthesis restarted for seeking");
    }
    
    // Clear internal buffers (Requirement 7.6, 11.5)
    m_output_buffer.clear();
    
    // Reset backpressure state (Requirement 7.6)
    m_backpressure_active = false;
    
    // Reset position tracking but keep decoder configuration
    m_samples_decoded.store(0);
    m_granule_position.store(0);
    m_error_state.store(false);
    m_last_error.clear();
    
    Debug::log("vorbis", "VorbisCodec::reset completed - all buffers cleared");
}

// ========== Private Implementation Methods ==========

bool VorbisCodec::processHeaderPacket_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("vorbis", "processHeaderPacket_unlocked called - packet_size=", packet_data.size());
    
    // Validate packet has minimum size for header detection
    if (packet_data.size() < 7) {
        Debug::log("vorbis", "Packet too small for Vorbis header");
        return false;
    }
    
    // Check packet type byte and "vorbis" signature
    uint8_t packet_type = packet_data[0];
    
    // Verify "vorbis" signature
    if (std::memcmp(&packet_data[1], "vorbis", 6) != 0) {
        Debug::log("vorbis", "Invalid Vorbis signature in header packet");
        return false;
    }
    
    // Route to appropriate header processor based on packet type
    switch (packet_type) {
        case 0x01:  // Identification header
            return processIdentificationHeader_unlocked(packet_data);
        case 0x03:  // Comment header
            return processCommentHeader_unlocked(packet_data);
        case 0x05:  // Setup header
            return processSetupHeader_unlocked(packet_data);
        default:
            Debug::log("vorbis", "Unknown Vorbis header packet type: ", static_cast<int>(packet_type));
            return false;
    }
}

bool VorbisCodec::processIdentificationHeader_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("vorbis", "Processing identification header (\\x01vorbis)");
    
    // Create ogg_packet structure for libvorbis (Requirement 2.2)
    ogg_packet packet;
    packet.packet = const_cast<unsigned char*>(packet_data.data());
    packet.bytes = static_cast<long>(packet_data.size());
    packet.b_o_s = 1;  // Beginning of stream
    packet.e_o_s = 0;
    packet.granulepos = -1;
    packet.packetno = 0;
    
    // Process header using libvorbis
    int result = vorbis_synthesis_headerin(&m_vorbis_info, &m_vorbis_comment, &packet);
    
    if (result < 0) {
        handleVorbisError_unlocked(result);
        return false;
    }
    
    // Extract configuration from vorbis_info (Requirement 1.2)
    m_sample_rate = m_vorbis_info.rate;
    m_channels = m_vorbis_info.channels;
    
    // Extract block sizes (Requirement 4.1, 4.2)
    // vorbis_info stores blocksize as log2 values
    m_block_size_short = vorbis_info_blocksize(&m_vorbis_info, 0);
    m_block_size_long = vorbis_info_blocksize(&m_vorbis_info, 1);
    
    Debug::log("vorbis", "Identification header parsed: rate=", m_sample_rate, 
               " channels=", m_channels,
               " block_short=", m_block_size_short,
               " block_long=", m_block_size_long);
    
    return true;
}

bool VorbisCodec::processCommentHeader_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("vorbis", "Processing comment header (\\x03vorbis)");
    
    // Create ogg_packet structure for libvorbis (Requirement 2.2)
    ogg_packet packet;
    packet.packet = const_cast<unsigned char*>(packet_data.data());
    packet.bytes = static_cast<long>(packet_data.size());
    packet.b_o_s = 0;
    packet.e_o_s = 0;
    packet.granulepos = -1;
    packet.packetno = 1;
    
    // Process header using libvorbis (Requirement 1.3)
    int result = vorbis_synthesis_headerin(&m_vorbis_info, &m_vorbis_comment, &packet);
    
    if (result < 0) {
        handleVorbisError_unlocked(result);
        return false;
    }
    
    // Comment header data is now available in m_vorbis_comment for demuxer access (Requirement 14.1)
    Debug::log("vorbis", "Comment header parsed successfully, vendor: ", 
               m_vorbis_comment.vendor ? m_vorbis_comment.vendor : "unknown");
    
    return true;
}

bool VorbisCodec::processSetupHeader_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("vorbis", "Processing setup header (\\x05vorbis)");
    
    // Create ogg_packet structure for libvorbis (Requirement 2.2)
    ogg_packet packet;
    packet.packet = const_cast<unsigned char*>(packet_data.data());
    packet.bytes = static_cast<long>(packet_data.size());
    packet.b_o_s = 0;
    packet.e_o_s = 0;
    packet.granulepos = -1;
    packet.packetno = 2;
    
    // Process header using libvorbis (Requirement 1.4)
    int result = vorbis_synthesis_headerin(&m_vorbis_info, &m_vorbis_comment, &packet);
    
    if (result < 0) {
        handleVorbisError_unlocked(result);
        return false;
    }
    
    Debug::log("vorbis", "Setup header parsed successfully - decoder ready for initialization");
    
    return true;
}

AudioFrame VorbisCodec::decodeAudioPacket_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("vorbis", "decodeAudioPacket_unlocked called - packet_size=", packet_data.size());
    
    AudioFrame frame;
    
    if (packet_data.empty()) {
        Debug::log("vorbis", "Empty packet, returning empty frame");
        return frame;
    }
    
    // Validate packet before decoding (Requirement 1.8)
    if (!validateVorbisPacket_unlocked(packet_data)) {
        Debug::log("vorbis", "Invalid Vorbis packet, skipping");
        // Skip corrupted packet and continue (Requirement 8.3)
        return frame;
    }
    
    // Create ogg_packet structure (Requirement 2.4)
    ogg_packet packet;
    packet.packet = const_cast<unsigned char*>(packet_data.data());
    packet.bytes = static_cast<long>(packet_data.size());
    packet.b_o_s = 0;
    packet.e_o_s = 0;
    packet.granulepos = -1;
    packet.packetno = m_header_packets_received + m_samples_decoded.load();
    
    // Decode the packet using vorbis_synthesis (Requirement 2.4)
    // Skip corrupted packets (vorbis_synthesis returns non-zero) and continue (Requirement 8.3)
    int synthesis_result = vorbis_synthesis(&m_vorbis_block, &packet);
    
    if (synthesis_result == 0) {
        // Add block to DSP state (Requirement 2.4)
        int blockin_result = vorbis_synthesis_blockin(&m_vorbis_dsp, &m_vorbis_block);
        
        if (blockin_result == 0) {
            // Extract PCM samples (Requirement 2.5)
            synthesizeBlock_unlocked();
        } else {
            // Log errors via vorbis_synthesis_blockin failures (Requirement 8.4)
            Debug::log("vorbis", "vorbis_synthesis_blockin failed with error: ", blockin_result);
            Debug::log("error", "VorbisCodec: vorbis_synthesis_blockin failed (", blockin_result, ")");
            
            // Check if this indicates state inconsistency
            if (blockin_result == OV_EFAULT) {
                // Call vorbis_synthesis_restart() on state inconsistency (Requirement 8.7)
                Debug::log("vorbis", "State inconsistency detected, calling vorbis_synthesis_restart()");
                vorbis_synthesis_restart(&m_vorbis_dsp);
                m_output_buffer.clear();
                m_backpressure_active = false;
            }
            
            handleVorbisError_unlocked(blockin_result);
        }
    } else if (synthesis_result == OV_ENOTAUDIO) {
        // OV_ENOTAUDIO means this packet doesn't contain audio (e.g., metadata)
        // This is not an error, just skip silently
        Debug::log("vorbis", "Non-audio packet received, skipping");
    } else {
        // Corrupted packet - skip and continue (Requirement 8.3)
        Debug::log("vorbis", "vorbis_synthesis failed with error: ", synthesis_result, " - skipping corrupted packet");
        Debug::log("error", "VorbisCodec: Corrupted packet skipped (vorbis_synthesis error: ", synthesis_result, ")");
        
        // Check for state inconsistency errors that require reset (Requirement 8.7)
        if (synthesis_result == OV_EFAULT) {
            Debug::log("vorbis", "State inconsistency detected in synthesis, calling vorbis_synthesis_restart()");
            vorbis_synthesis_restart(&m_vorbis_dsp);
            m_output_buffer.clear();
            m_backpressure_active = false;
        }
        
        // Log the error but don't throw - we continue with next packet
        handleVorbisError_unlocked(synthesis_result);
    }
    
    // Return accumulated samples
    if (!m_output_buffer.empty()) {
        frame.sample_rate = m_sample_rate;
        frame.channels = m_channels;
        frame.samples = std::move(m_output_buffer);
        m_output_buffer.clear();
        
        m_samples_decoded.fetch_add(frame.samples.size() / m_channels);
        Debug::log("vorbis", "Decoded ", frame.samples.size(), " samples");
    }
    
    return frame;
}

bool VorbisCodec::synthesizeBlock_unlocked()
{
    float **pcm_channels;
    int samples_available;
    
    // Update backpressure state before processing (Requirement 7.4)
    updateBackpressureState_unlocked();
    
    // If backpressure is active, don't process more samples
    if (m_backpressure_active) {
        Debug::log("vorbis", "Backpressure active, buffer at ", getBufferFillPercent_unlocked(), "% - deferring processing");
        return !m_output_buffer.empty();
    }
    
    // Get available PCM samples using vorbis_synthesis_pcmout (Requirement 2.5)
    while ((samples_available = vorbis_synthesis_pcmout(&m_vorbis_dsp, &pcm_channels)) > 0) {
        int channels = m_vorbis_info.channels;
        
        // Check if we can accept more samples (Requirement 7.2, 7.4)
        if (!canAcceptMoreSamples_unlocked()) {
            Debug::log("vorbis", "Buffer full at ", m_output_buffer.size(), " samples, applying backpressure");
            m_backpressure_active = true;
            break;
        }
        
        // Calculate how many samples we can actually accept
        size_t space_available = MAX_BUFFER_SAMPLES - m_output_buffer.size();
        size_t samples_to_process = static_cast<size_t>(samples_available) * static_cast<size_t>(channels);
        
        if (samples_to_process > space_available) {
            // Partial processing - only take what we can fit
            samples_available = static_cast<int>(space_available / channels);
            if (samples_available <= 0) {
                Debug::log("vorbis", "No space for even one sample, buffer full");
                m_backpressure_active = true;
                break;
            }
            Debug::log("vorbis", "Partial processing: ", samples_available, " samples (space limited)");
        }
        
        // Convert float samples to 16-bit PCM (Requirement 1.5)
        AudioFrame temp_frame;
        convertFloatToPCM_unlocked(pcm_channels, samples_available, temp_frame);
        
        // Append to output buffer
        m_output_buffer.insert(m_output_buffer.end(), 
                               temp_frame.samples.begin(), 
                               temp_frame.samples.end());
        
        // Tell libvorbis we consumed these samples (Requirement 2.5)
        vorbis_synthesis_read(&m_vorbis_dsp, samples_available);
        
        // Update backpressure state after adding samples
        updateBackpressureState_unlocked();
    }
    
    return !m_output_buffer.empty();
}

// ========== Streaming and Buffer Management Methods ==========

bool VorbisCodec::canAcceptMoreSamples_unlocked() const
{
    // Check if buffer has room for more samples (Requirement 7.2, 7.4)
    return m_output_buffer.size() < MAX_BUFFER_SAMPLES;
}

int VorbisCodec::getBufferFillPercent_unlocked() const
{
    if (MAX_BUFFER_SAMPLES == 0) return 0;
    return static_cast<int>((m_output_buffer.size() * 100) / MAX_BUFFER_SAMPLES);
}

void VorbisCodec::updateBackpressureState_unlocked()
{
    // Hysteresis-based backpressure control (Requirement 7.4)
    // - Activate backpressure when buffer exceeds high water mark
    // - Deactivate backpressure when buffer drops below low water mark
    // This prevents rapid on/off cycling
    
    if (m_backpressure_active) {
        // Currently in backpressure mode - check if we can release
        if (m_output_buffer.size() < BUFFER_LOW_WATER_MARK) {
            m_backpressure_active = false;
            Debug::log("vorbis", "Backpressure released, buffer at ", getBufferFillPercent_unlocked(), "%");
        }
    } else {
        // Not in backpressure mode - check if we need to activate
        if (m_output_buffer.size() >= BUFFER_HIGH_WATER_MARK) {
            m_backpressure_active = true;
            Debug::log("vorbis", "Backpressure activated, buffer at ", getBufferFillPercent_unlocked(), "%");
        }
    }
}

size_t VorbisCodec::getBufferSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_output_buffer.size();
}

bool VorbisCodec::isBackpressureActive() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_backpressure_active;
}

std::string VorbisCodec::getLastError() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_last_error;
}

bool VorbisCodec::isInErrorState() const
{
    return m_error_state.load();
}

void VorbisCodec::convertFloatToPCM_unlocked(float** pcm, int samples, AudioFrame& frame)
{
    // Convert libvorbis float output to 16-bit PCM (Requirement 1.5, 5.1, 5.2)
    // libvorbis outputs float samples in the range [-1.0, 1.0]
    // We convert to 16-bit signed PCM in the range [-32768, 32767]
    
    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.samples.clear();
    
    // Early return for edge cases
    if (samples <= 0 || m_channels <= 0 || pcm == nullptr) {
        Debug::log("vorbis", "convertFloatToPCM_unlocked: Invalid parameters - samples=", 
                   samples, " channels=", m_channels);
        return;
    }
    
    // Use the static interleaveChannels helper for the actual conversion
    // This handles proper channel interleaving according to Vorbis conventions
    // (Requirement 5.5, 5.7)
    // Handle memory allocation failures (Requirement 8.6)
    try {
        interleaveChannels(pcm, samples, m_channels, frame.samples);
    } catch (const std::bad_alloc& e) {
        m_last_error = "Memory allocation failed during PCM conversion: " + std::string(e.what());
        Debug::log("vorbis", m_last_error);
        Debug::log("error", "VorbisCodec: ", m_last_error);
        frame.samples.clear();
        throw BadFormatException(m_last_error);
    }
    
    // Verify output consistency (Requirement 5.1, 5.2, 5.3, 5.5)
    // The number of samples should be exactly samples * channels
    const size_t expected_samples = static_cast<size_t>(samples) * static_cast<size_t>(m_channels);
    if (frame.samples.size() != expected_samples) {
        Debug::log("vorbis", "WARNING: Sample count mismatch - expected ", expected_samples,
                   " got ", frame.samples.size());
    }
}

// ========== Static Float to PCM Conversion Helpers ==========

int16_t VorbisCodec::floatToInt16(float sample)
{
    // Convert a single float sample to 16-bit PCM with proper clamping
    // (Requirement 1.5, 5.1, 5.2)
    //
    // libvorbis outputs float samples nominally in [-1.0, 1.0]
    // We convert to 16-bit signed PCM in [-32768, 32767]
    //
    // Clamping is essential because:
    // 1. libvorbis can produce samples slightly outside [-1.0, 1.0] due to
    //    floating-point precision in the MDCT and windowing operations
    // 2. Some encoders may produce slightly out-of-range values
    // 3. Prevents integer overflow during conversion
    
    // Clamp to valid range first
    if (sample > 1.0f) {
        sample = 1.0f;
    } else if (sample < -1.0f) {
        sample = -1.0f;
    }
    
    // Scale to 16-bit range
    // Using 32767.0f ensures positive samples map to [0, 32767]
    // and negative samples map to [-32767, 0]
    // The asymmetry of int16_t (-32768 to 32767) means -1.0f maps to -32767,
    // not -32768, which is standard practice for audio conversion
    int32_t scaled = static_cast<int32_t>(sample * 32767.0f);
    
    // Final safety clamp (handles floating-point edge cases)
    if (scaled > 32767) {
        scaled = 32767;
    } else if (scaled < -32768) {
        scaled = -32768;
    }
    
    return static_cast<int16_t>(scaled);
}

void VorbisCodec::interleaveChannels(float** pcm, int samples, int channels,
                                     std::vector<int16_t>& output)
{
    // Interleave multi-channel float arrays into 16-bit PCM output
    // (Requirement 5.5, 5.7)
    //
    // Vorbis channel ordering (from specification):
    // - 1 channel:  mono
    // - 2 channels: left, right
    // - 3 channels: left, center, right
    // - 4 channels: front left, front right, rear left, rear right
    // - 5 channels: front left, center, front right, rear left, rear right
    // - 6 channels: front left, center, front right, rear left, rear right, LFE
    // - 7 channels: front left, center, front right, side left, side right, rear center, LFE
    // - 8 channels: front left, center, front right, side left, side right, rear left, rear right, LFE
    //
    // The output is interleaved: [ch0_s0, ch1_s0, ..., chN_s0, ch0_s1, ch1_s1, ...]
    
    if (samples <= 0 || channels <= 0 || pcm == nullptr) {
        return;
    }
    
    // Reserve exact space needed
    const size_t total_samples = static_cast<size_t>(samples) * static_cast<size_t>(channels);
    output.clear();
    output.reserve(total_samples);
    
    // Interleave: for each sample position, output all channels in order
    for (int i = 0; i < samples; i++) {
        for (int ch = 0; ch < channels; ch++) {
            output.push_back(floatToInt16(pcm[ch][i]));
        }
    }
}

void VorbisCodec::handleVariableBlockSizes_unlocked(const vorbis_block* block)
{
    // libvorbis handles windowing and overlap-add internally (Requirement 4.3, 4.4)
    // We just track block sizes for buffer management
    if (block) {
        uint32_t current_block_size = block->pcmend;
        
        // Adjust output buffer size if needed
        size_t required_size = current_block_size * m_channels;
        if (m_output_buffer.capacity() < required_size) {
            m_output_buffer.reserve(required_size);
        }
    }
}

bool VorbisCodec::validateVorbisPacket_unlocked(const std::vector<uint8_t>& packet_data)
{
    // Basic packet validation (Requirement 1.8)
    if (packet_data.empty()) {
        return false;
    }
    
    // Audio packets should not start with header signatures
    if (packet_data.size() >= 7) {
        uint8_t packet_type = packet_data[0];
        // Header packets have odd type bytes (1, 3, 5)
        // Audio packets have even type bytes or don't follow this pattern
        if ((packet_type & 0x01) && std::memcmp(&packet_data[1], "vorbis", 6) == 0) {
            Debug::log("vorbis", "Received header packet when expecting audio");
            return false;
        }
    }
    
    return true;
}

void VorbisCodec::handleVorbisError_unlocked(int vorbis_error)
{
    // Handle libvorbis error codes (Requirement 2.6, 8.1-8.7)
    // This method implements comprehensive error handling per Requirements 8.1, 8.2, 8.5, 8.6
    // 
    // Error handling strategy:
    // - Fatal errors (OV_EBADHEADER, OV_EVERSION): Set error state and throw BadFormatException
    // - Recoverable errors (OV_ENOTVORBIS, OV_EINVAL): Log and continue (skip packet)
    // - State errors (OV_EFAULT): Reset decoder state and continue
    // - All errors: Report through PsyMP3's Debug logging system (Requirement 8.8)
    
    switch (vorbis_error) {
        case OV_ENOTVORBIS:
            // Not Vorbis data - reject packet (Requirement 8.1)
            // This error indicates the packet is not valid Vorbis data
            // We log the error but continue - this is a per-packet rejection
            m_last_error = "Not Vorbis data (OV_ENOTVORBIS) - packet rejected";
            Debug::log("vorbis", m_last_error);
            Debug::log("error", "VorbisCodec: ", m_last_error);
            // Note: We don't set m_error_state here because this is a per-packet
            // rejection, not a fatal codec error. The codec can continue with
            // subsequent valid packets (Requirement 8.3).
            break;
            
        case OV_EBADHEADER:
            // Corrupted header - reject initialization (Requirement 8.2)
            // This is a fatal error during header processing
            m_last_error = "Bad Vorbis header (OV_EBADHEADER) - initialization rejected";
            m_error_state.store(true);
            Debug::log("vorbis", m_last_error);
            Debug::log("error", "VorbisCodec: FATAL - ", m_last_error);
            // Throw exception to signal initialization failure
            throw BadFormatException(m_last_error);
            break;
            
        case OV_EFAULT:
            // Internal error - reset decoder state (Requirement 8.5, 8.7)
            // This indicates internal inconsistency in libvorbis state
            m_last_error = "Internal Vorbis error (OV_EFAULT) - resetting decoder state";
            Debug::log("vorbis", m_last_error);
            Debug::log("error", "VorbisCodec: ", m_last_error);
            // Attempt recovery by restarting synthesis (Requirement 8.7)
            // Note: The actual vorbis_synthesis_restart() call is done in the caller
            // (decodeAudioPacket_unlocked) to avoid double-reset
            // Clear output buffer to ensure clean state
            m_output_buffer.clear();
            m_backpressure_active = false;
            break;
            
        case OV_EINVAL:
            // Invalid data - skip packet and continue (Requirement 8.3)
            m_last_error = "Invalid Vorbis data (OV_EINVAL) - packet skipped";
            Debug::log("vorbis", m_last_error);
            Debug::log("error", "VorbisCodec: ", m_last_error);
            break;
            
        case OV_EREAD:
            // Read error - typically I/O related, skip and continue
            m_last_error = "Vorbis read error (OV_EREAD) - packet skipped";
            Debug::log("vorbis", m_last_error);
            Debug::log("error", "VorbisCodec: ", m_last_error);
            break;
            
        case OV_EIMPL:
            // Unimplemented feature - this is a codec limitation
            // Report through Debug logging system (Requirement 8.8)
            m_last_error = "Unimplemented Vorbis feature (OV_EIMPL)";
            Debug::log("vorbis", m_last_error);
            Debug::log("error", "VorbisCodec: ", m_last_error);
            break;
            
        case OV_EVERSION:
            // Version mismatch - incompatible Vorbis version
            // This is a fatal error - the stream cannot be decoded
            m_last_error = "Vorbis version mismatch (OV_EVERSION) - stream incompatible";
            m_error_state.store(true);
            Debug::log("vorbis", m_last_error);
            Debug::log("error", "VorbisCodec: FATAL - ", m_last_error);
            throw BadFormatException(m_last_error);
            break;
            
        default:
            // Unknown error - log and continue (Requirement 8.8)
            m_last_error = "Unknown Vorbis error: " + std::to_string(vorbis_error);
            Debug::log("vorbis", m_last_error);
            Debug::log("error", "VorbisCodec: ", m_last_error);
            break;
    }
}

void VorbisCodec::resetDecoderState_unlocked()
{
    Debug::log("vorbis", "resetDecoderState_unlocked called");
    
    // Clean up existing structures
    cleanupVorbisStructures_unlocked();
    
    // Reinitialize libvorbis structures
    vorbis_info_init(&m_vorbis_info);
    vorbis_comment_init(&m_vorbis_comment);
    
    // Reset state variables
    m_header_packets_received = 0;
    m_decoder_initialized = false;
    m_sample_rate = 0;
    m_channels = 0;
    m_block_size_short = 0;
    m_block_size_long = 0;
    m_samples_decoded.store(0);
    m_granule_position.store(0);
    m_error_state.store(false);
    m_last_error.clear();
    m_output_buffer.clear();
    m_backpressure_active = false;  // Reset backpressure state (Requirement 7.6)
    
    Debug::log("vorbis", "Decoder state reset completed");
}

void VorbisCodec::cleanupVorbisStructures_unlocked()
{
    Debug::log("vorbis", "cleanupVorbisStructures_unlocked called");
    
    // Clean up in reverse order of initialization (Requirement 2.8)
    // vorbis_block_clear and vorbis_dsp_clear must only be called if synthesis was initialized
    if (m_decoder_initialized) {
        vorbis_block_clear(&m_vorbis_block);
        vorbis_dsp_clear(&m_vorbis_dsp);
        m_decoder_initialized = false;
    }
    
    // vorbis_comment_clear and vorbis_info_clear are safe to call on initialized structures
    // They check internal state before freeing
    vorbis_comment_clear(&m_vorbis_comment);
    vorbis_info_clear(&m_vorbis_info);
    
    // Reset header count since structures are cleared
    m_header_packets_received = 0;
    
    Debug::log("vorbis", "Vorbis structures cleaned up");
}

// ========== Vorbis Codec Support Functions ==========

namespace VorbisCodecSupport {

void registerCodec()
{
    Debug::log("vorbis", "Registering Vorbis codec with AudioCodecFactory");
    AudioCodecFactory::registerCodec("vorbis", createCodec);
}

std::unique_ptr<AudioCodec> createCodec(const StreamInfo& stream_info)
{
    if (!isVorbisStream(stream_info)) {
        return nullptr;
    }
    return std::make_unique<VorbisCodec>(stream_info);
}

bool isVorbisStream(const StreamInfo& stream_info)
{
    return stream_info.codec_name == "vorbis";
}

} // namespace VorbisCodecSupport

} // namespace Vorbis
} // namespace Codec
} // namespace PsyMP3

#endif // HAVE_OGGDEMUXER
