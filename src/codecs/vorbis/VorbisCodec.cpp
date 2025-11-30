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
    
    // Initialize libvorbis structures (Requirement 2.1)
    vorbis_info_init(&m_vorbis_info);
    vorbis_comment_init(&m_vorbis_comment);
    
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
    
    // Reserve buffer space for efficiency (minimize allocation overhead)
    // Maximum Vorbis block size is 8192 samples per channel
    constexpr size_t MAX_VORBIS_BLOCK_SIZE = 8192;
    constexpr size_t INITIAL_MAX_CHANNELS = 8;
    m_output_buffer.reserve(MAX_VORBIS_BLOCK_SIZE * INITIAL_MAX_CHANNELS);
    
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
    cleanupVorbisStructures_unlocked();
    
    // Clear output buffer
    m_output_buffer.clear();
    
    Debug::log("vorbis", "VorbisCodec destructor completed");
}

// ========== AudioCodec Interface Implementation ==========

bool VorbisCodec::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    Debug::log("vorbis", "VorbisCodec::initialize called");
    
    // Reset state for fresh initialization
    m_header_packets_received = 0;
    m_decoder_initialized = false;
    m_samples_decoded.store(0);
    m_granule_position.store(0);
    m_error_state.store(false);
    m_last_error.clear();
    m_output_buffer.clear();
    
    m_initialized = true;
    
    Debug::log("vorbis", "VorbisCodec::initialize completed successfully");
    return true;
}

bool VorbisCodec::canDecode(const StreamInfo& stream_info) const
{
    // Thread-safe read-only operation, no lock needed
    return stream_info.codec_name == "vorbis";
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
                if (vorbis_synthesis_init(&m_vorbis_dsp, &m_vorbis_info) != 0) {
                    m_error_state.store(true);
                    m_last_error = "Failed to initialize Vorbis synthesis";
                    Debug::log("vorbis", m_last_error);
                    throw BadFormatException(m_last_error);
                }
                if (vorbis_block_init(&m_vorbis_dsp, &m_vorbis_block) != 0) {
                    vorbis_dsp_clear(&m_vorbis_dsp);
                    m_error_state.store(true);
                    m_last_error = "Failed to initialize Vorbis block";
                    Debug::log("vorbis", m_last_error);
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
    
    // Clear internal buffers (Requirement 7.6)
    m_output_buffer.clear();
    
    // Reset position tracking but keep decoder configuration
    m_samples_decoded.store(0);
    m_granule_position.store(0);
    m_error_state.store(false);
    m_last_error.clear();
    
    Debug::log("vorbis", "VorbisCodec::reset completed");
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
    int synthesis_result = vorbis_synthesis(&m_vorbis_block, &packet);
    
    if (synthesis_result == 0) {
        // Add block to DSP state (Requirement 2.4)
        int blockin_result = vorbis_synthesis_blockin(&m_vorbis_dsp, &m_vorbis_block);
        
        if (blockin_result == 0) {
            // Extract PCM samples (Requirement 2.5)
            synthesizeBlock_unlocked();
        } else {
            Debug::log("vorbis", "vorbis_synthesis_blockin failed: ", blockin_result);
            handleVorbisError_unlocked(blockin_result);
        }
    } else if (synthesis_result != OV_ENOTAUDIO) {
        // OV_ENOTAUDIO means this packet doesn't contain audio (e.g., metadata)
        // Other errors indicate corrupted packets (Requirement 8.3)
        Debug::log("vorbis", "vorbis_synthesis failed: ", synthesis_result);
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
    
    // Prevent buffer from growing too large (memory leak protection) (Requirement 7.2)
    constexpr size_t MAX_BUFFER_SIZE = 48000 * 2 * 2; // 2 seconds of stereo at 48kHz
    if (m_output_buffer.size() > MAX_BUFFER_SIZE) {
        Debug::log("vorbis", "Buffer too large (", m_output_buffer.size(), "), clearing to prevent memory leak");
        m_output_buffer.clear();
    }
    
    // Get available PCM samples using vorbis_synthesis_pcmout (Requirement 2.5)
    while ((samples_available = vorbis_synthesis_pcmout(&m_vorbis_dsp, &pcm_channels)) > 0) {
        int channels = m_vorbis_info.channels;
        
        // Prevent excessive accumulation (Requirement 7.4)
        if (m_output_buffer.size() + (samples_available * channels) > MAX_BUFFER_SIZE) {
            Debug::log("vorbis", "Would exceed buffer limit, processing partial samples");
            samples_available = (MAX_BUFFER_SIZE - m_output_buffer.size()) / channels;
            if (samples_available <= 0) {
                break; // Buffer is full enough
            }
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
    }
    
    return !m_output_buffer.empty();
}

void VorbisCodec::convertFloatToPCM_unlocked(float** pcm, int samples, AudioFrame& frame)
{
    // Convert libvorbis float output to 16-bit PCM (Requirement 1.5, 5.1, 5.2)
    frame.sample_rate = m_sample_rate;
    frame.channels = m_channels;
    frame.samples.clear();
    frame.samples.reserve(samples * m_channels);
    
    // Interleave channels and convert to 16-bit (Requirement 5.5, 5.7)
    for (int i = 0; i < samples; i++) {
        for (int ch = 0; ch < m_channels; ch++) {
            float sample = pcm[ch][i];
            
            // Clamp to [-1.0, 1.0] range and convert to 16-bit
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            
            int16_t sample_16 = static_cast<int16_t>(sample * 32767.0f);
            frame.samples.push_back(sample_16);
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
    switch (vorbis_error) {
        case OV_ENOTVORBIS:
            // Not Vorbis data (Requirement 8.1)
            m_last_error = "Not Vorbis data (OV_ENOTVORBIS)";
            Debug::log("vorbis", m_last_error);
            break;
            
        case OV_EBADHEADER:
            // Corrupted header (Requirement 8.2)
            m_last_error = "Bad Vorbis header (OV_EBADHEADER)";
            m_error_state.store(true);
            Debug::log("vorbis", m_last_error);
            break;
            
        case OV_EFAULT:
            // Internal error - reset decoder state (Requirement 8.5)
            m_last_error = "Internal Vorbis error (OV_EFAULT)";
            Debug::log("vorbis", m_last_error, " - attempting recovery");
            if (m_decoder_initialized) {
                vorbis_synthesis_restart(&m_vorbis_dsp);
            }
            break;
            
        case OV_EINVAL:
            // Invalid data
            m_last_error = "Invalid Vorbis data (OV_EINVAL)";
            Debug::log("vorbis", m_last_error);
            break;
            
        case OV_EREAD:
            // Read error
            m_last_error = "Vorbis read error (OV_EREAD)";
            Debug::log("vorbis", m_last_error);
            break;
            
        case OV_EIMPL:
            // Unimplemented feature
            m_last_error = "Unimplemented Vorbis feature (OV_EIMPL)";
            Debug::log("vorbis", m_last_error);
            break;
            
        case OV_EVERSION:
            // Version mismatch
            m_last_error = "Vorbis version mismatch (OV_EVERSION)";
            Debug::log("vorbis", m_last_error);
            break;
            
        default:
            // Unknown error
            m_last_error = "Unknown Vorbis error: " + std::to_string(vorbis_error);
            Debug::log("vorbis", m_last_error);
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
    
    Debug::log("vorbis", "Decoder state reset completed");
}

void VorbisCodec::cleanupVorbisStructures_unlocked()
{
    Debug::log("vorbis", "cleanupVorbisStructures_unlocked called");
    
    // Clean up in reverse order of initialization (Requirement 2.8)
    if (m_decoder_initialized) {
        vorbis_block_clear(&m_vorbis_block);
        vorbis_dsp_clear(&m_vorbis_dsp);
        m_decoder_initialized = false;
    }
    vorbis_comment_clear(&m_vorbis_comment);
    vorbis_info_clear(&m_vorbis_info);
    
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
