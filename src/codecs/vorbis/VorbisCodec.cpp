/*
 * VorbisCodec.cpp - Container-agnostic Vorbis audio codec implementation
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
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

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

#ifdef HAVE_OGGDEMUXER

// Declarations only; the implementation is compiled separately as a C object
// (stb_vorbis_impl.c) because stb_vorbis defines collision-prone globals.
// The define set must match the impl so the declaration surface agrees.
#define STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_NO_PULLDATA_API
#include "../../../third_party/stb/stb_vorbis.c"
#undef STB_VORBIS_HEADER_ONLY

namespace PsyMP3 {
namespace Codec {
namespace Vorbis {

namespace {

/// Ogg page CRC (RFC 3533): poly 0x04C11DB7, unreflected, zero init, no
/// final xor. stb_vorbis ignores the CRC on its normal decode path but
/// validates it while resynchronizing after a seek, so the synthetic pages
/// built below always carry the real checksum.
uint32_t oggCrc32(const uint8_t* data, size_t len)
{
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t r = i << 24;
            for (int j = 0; j < 8; ++j) {
                r = (r & 0x80000000u) ? (r << 1) ^ 0x04C11DB7u : (r << 1);
            }
            t[i] = r;
        }
        return t;
    }();

    uint32_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc = (crc << 8) ^ table[((crc >> 24) ^ data[i]) & 0xFF];
    }
    return crc;
}

/// Arbitrary constant serial number for the synthetic logical stream;
/// stb_vorbis reads and discards it.
constexpr uint32_t kSyntheticSerial = 0x33505350; // "PSP3"

} // namespace

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
    
    // Read a 32-bit little-endian length. Cast each byte to uint32_t before
    // shifting (byte << 24 is signed-overflow UB).
    auto readLE32 = [&](size_t at) -> uint32_t {
        return static_cast<uint32_t>(packet_data[at]) |
               (static_cast<uint32_t>(packet_data[at + 1]) << 8) |
               (static_cast<uint32_t>(packet_data[at + 2]) << 16) |
               (static_cast<uint32_t>(packet_data[at + 3]) << 24);
    };

    // Parse vendor string. Bounds checks use uint64_t so offset + length cannot
    // wrap on a 32-bit target where size_t is 32-bit.
    if (offset + 4 > packet_data.size()) return info;
    uint32_t vendor_length = readLE32(offset);
    offset += 4;

    if (static_cast<uint64_t>(offset) + vendor_length > packet_data.size()) return info;
    info.vendor_string = std::string(reinterpret_cast<const char*>(&packet_data[offset]), vendor_length);
    offset += vendor_length;

    // Parse user comments
    if (offset + 4 > packet_data.size()) return info;
    uint32_t comment_count = readLE32(offset);
    offset += 4;

    for (uint32_t i = 0; i < comment_count && offset + 4 <= packet_data.size(); i++) {
        uint32_t comment_length = readLE32(offset);
        offset += 4;

        if (static_cast<uint64_t>(offset) + comment_length > packet_data.size()) break;
        
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

    // The stb_vorbis instance is created in decode() once all three header
    // packets have arrived; the constructor only sets up member state.

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
    cleanupVorbisStructures_unlocked();

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
        throw BadFormatException(m_last_error);
    }
    
    // Headers pre-extracted by the demuxer (OggDemuxer concatenates the three
    // Vorbis header packets into codec_data). Consuming them HERE rather than
    // waiting for them to arrive as the first three decode() packets is what
    // makes the decoder survive a seek that happens before any audio has been
    // decoded: the headers exist only at the start of the stream, so once the
    // seek moves past them they are never delivered and the codec could never
    // open its decoder -- every subsequent read returned silence, forever.
    if (!m_stream_info.codec_data.empty() && m_header_packets_received < 3) {
        Debug::log("vorbis", "StreamInfo contains codec_data of size: ",
                   m_stream_info.codec_data.size());
        if (initializeFromCodecData_unlocked()) {
            Debug::log("vorbis", "Decoder opened from codec_data; ready before first decode");
        } else {
            // Not fatal: the packets may still arrive the usual way.
            Debug::log("vorbis", "codec_data did not yield a complete header set; "
                                 "falling back to headers arriving as packets");
        }
    }

    m_initialized = true;
    
    Debug::log("vorbis", "VorbisCodec::initialize completed successfully");
    return true;
}

bool VorbisCodec::canDecode(const StreamInfo& stream_info) const
{
    // Thread-safe read-only operation, no lock needed (Requirement 11.6, 6.6)
    
    // Check if StreamInfo contains "vorbis" codec name (case-insensitive)
    std::string codec_lower = stream_info.codec_name;
    std::transform(codec_lower.begin(), codec_lower.end(), codec_lower.begin(), ::tolower);
    if (codec_lower != "vorbis") {
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

            // After all 3 headers, open the stb_vorbis decoder (Requirement 2.3)
            if (m_header_packets_received == 3) {
                if (!openDecoder_unlocked()) {
                    m_error_state.store(true);
                    Debug::log("vorbis", m_last_error);
                    Debug::log("error", "VorbisCodec: ", m_last_error);
                    throw BadFormatException(m_last_error);
                }
                m_decoder_initialized = true;
                Debug::log("vorbis", "stb_vorbis decoder opened successfully");
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
        // Drain anything stb_vorbis can still decode from buffered pages, then
        // hand over whatever is left in the output buffer (Requirement 4.8, 7.5).
        // Vorbis itself holds back the final half-window (it needs the next
        // packet for overlap-add), matching the old libvorbis behavior of never
        // performing end-of-stream tail synthesis.
        drainDecoder_unlocked();

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
    
    // For seeking, flush the pushdata decoder without reopening headers
    // (Requirement 2.7). This arms stb_vorbis's CRC-validated page resync.
    if (m_decoder_initialized && m_stb) {
        stb_vorbis_flush_pushdata(m_stb);
        Debug::log("vorbis", "stb_vorbis flushed for seeking");
    }

    // Drop any re-paged bytes from before the seek (Requirement 7.6, 11.5)
    m_input.clear();

    // Let the resync scanner lock on a packet-less primer page instead of
    // eating the first real post-seek packet: the scan consumes the entire
    // page it validates, and without the primer that would cost one full
    // audio packet on every seek beyond the warm-up frame libvorbis also
    // discarded after vorbis_synthesis_restart().
    if (m_decoder_initialized && m_stb) {
        appendResyncPrimerPage_unlocked();
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

// Split the demuxer's concatenated header blob back into the three Vorbis
// header packets and run them through the normal header path. The blob has no
// length prefixes, but each Vorbis header begins with its type byte followed
// by the "vorbis" signature (0x01/0x03/0x05), which delimits them reliably.
bool VorbisCodec::initializeFromCodecData_unlocked()
{
    const std::vector<uint8_t>& blob = m_stream_info.codec_data;
    static const char kSig[6] = {'v','o','r','b','i','s'};

    std::vector<size_t> starts;
    for (size_t i = 0; i + 7 <= blob.size(); ++i) {
        const uint8_t type = blob[i];
        if ((type == 0x01 || type == 0x03 || type == 0x05) &&
            std::memcmp(&blob[i + 1], kSig, 6) == 0) {
            starts.push_back(i);
        }
    }
    if (starts.size() < 3) {
        return false;
    }
    // Keep the first three in order; anything after them is not a header.
    starts.resize(3);
    if (blob[starts[0]] != 0x01 || blob[starts[1]] != 0x03 || blob[starts[2]] != 0x05) {
        return false;
    }

    for (size_t n = 0; n < 3; ++n) {
        const size_t begin = starts[n];
        const size_t end = (n + 1 < 3) ? starts[n + 1] : blob.size();
        std::vector<uint8_t> packet(blob.begin() + static_cast<std::ptrdiff_t>(begin),
                                    blob.begin() + static_cast<std::ptrdiff_t>(end));
        if (!processHeaderPacket_unlocked(packet)) {
            m_header_packets_received = 0;
            return false;
        }
        ++m_header_packets_received;
    }

    if (!openDecoder_unlocked()) {
        // Leave the error text for the caller's log; the packet path can retry.
        m_header_packets_received = 0;
        return false;
    }
    m_decoder_initialized = true;
    return true;
}

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

    // Parse and validate the fixed-layout identification header ourselves
    // (Requirement 1.2); stb_vorbis re-validates it when the decoder opens.
    VorbisHeaderInfo info = VorbisHeaderInfo::parseFromPacket(packet_data);
    if (!info.isValid()) {
        // Fatal, matching the old libvorbis OV_EBADHEADER path: without a
        // valid identification header the stream can never decode, and
        // silently returning empty frames forever would just hide that.
        m_error_state.store(true);
        m_last_error = "Invalid Vorbis identification header";
        Debug::log("vorbis", m_last_error);
        Debug::log("error", "VorbisCodec: FATAL - ", m_last_error);
        throw BadFormatException(m_last_error);
    }

    m_sample_rate = info.sample_rate;
    m_channels = info.channels;

    // Block sizes are stored as log2 values (Requirement 4.1, 4.2)
    m_block_size_short = 1u << info.blocksize_0;
    m_block_size_long = 1u << info.blocksize_1;

    m_header_data[0] = packet_data;

    Debug::log("vorbis", "Identification header parsed: rate=", m_sample_rate,
               " channels=", m_channels,
               " block_short=", m_block_size_short,
               " block_long=", m_block_size_long);

    return true;
}

bool VorbisCodec::processCommentHeader_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("vorbis", "Processing comment header (\\x03vorbis)");

    // The signature was already checked by processHeaderPacket_unlocked; the
    // comment contents don't affect decoding, so parse only for the log
    // (Requirement 1.3, 14.1 — tag extraction happens demuxer-side).
    VorbisCommentInfo info = VorbisCommentInfo::parseFromPacket(packet_data);

    m_header_data[1] = packet_data;

    Debug::log("vorbis", "Comment header parsed successfully, vendor: ",
               info.vendor_string.empty() ? "unknown" : info.vendor_string);

    return true;
}

bool VorbisCodec::processSetupHeader_unlocked(const std::vector<uint8_t>& packet_data)
{
    Debug::log("vorbis", "Processing setup header (\\x05vorbis)");

    // The setup header's codebooks can only be validated by the decoder
    // itself; buffer it and let openDecoder_unlocked() do the parse
    // (Requirement 1.4).
    m_header_data[2] = packet_data;

    Debug::log("vorbis", "Setup header buffered - decoder ready for initialization");

    return true;
}

bool VorbisCodec::openDecoder_unlocked()
{
    // Rebuild the three header packets as an Ogg page stream and hand it to
    // stb_vorbis_open_pushdata. The identification header must be the sole
    // packet of the first (BOS-flagged) page — stb enforces exactly one
    // 30-byte segment there — and the comment/setup packets follow on
    // ordinary pages.
    m_input.clear();
    m_page_counter = 0;
    // The identification header is a fixed 30-byte structure and stb demands
    // exactly one 30-byte segment on the first page; libvorbis tolerated
    // trailing garbage after the framing bit, so truncate rather than reject.
    appendPacketAsPages_unlocked(m_header_data[0].data(),
                                 std::min<size_t>(m_header_data[0].size(), 30), true);
    appendPacketAsPages_unlocked(m_header_data[1].data(), m_header_data[1].size(), false);
    appendPacketAsPages_unlocked(m_header_data[2].data(), m_header_data[2].size(), false);

    int consumed = 0;
    int stb_error = 0;
    {
        // stb_vorbis rewrites its file-global CRC table on every open with
        // plain stores; two codec instances opening/decoding concurrently is
        // a data race TSan flags. The writes are idempotent, but serializing
        // opens costs nothing and keeps --enable-tsan runs clean.
        static std::mutex s_open_mutex;
        std::lock_guard<std::mutex> open_lock(s_open_mutex);
        m_stb = stb_vorbis_open_pushdata(m_input.data(), static_cast<int>(m_input.size()),
                                         &consumed, &stb_error, nullptr);
    }
    if (!m_stb) {
        // All three headers are complete in the buffer, so need_more_data
        // cannot legitimately happen — treat every failure as a bad stream
        // (the old code's fatal OV_EBADHEADER path).
        m_last_error = "stb_vorbis rejected the Vorbis headers (error " +
                       std::to_string(stb_error) + ")";
        return false;
    }
    m_input.erase(m_input.begin(), m_input.begin() + consumed);

    stb_vorbis_info info = stb_vorbis_get_info(m_stb);
    m_sample_rate = info.sample_rate;
    m_channels = static_cast<uint16_t>(info.channels);

    // The header packets are no longer needed once the decoder holds the
    // parsed setup; free the (potentially large) setup copy.
    for (auto& h : m_header_data) {
        h.clear();
        h.shrink_to_fit();
    }

    return true;
}

void VorbisCodec::appendPacketAsPages_unlocked(const uint8_t* packet, size_t size, bool bos)
{
    // Wrap one raw Vorbis packet in minimal Ogg page framing and append the
    // page bytes to m_input. The demuxer de-paged the stream (one packet per
    // MediaChunk); stb_vorbis's pushdata API consumes pages, so the framing
    // is rebuilt here. Granulepos is always -1 (unknown): timestamps are the
    // demuxer's job, and it keeps stb's end-of-stream trimming and location
    // tracking inert, matching the old libvorbis packet-mode behavior.
    size_t offset = 0;
    bool continued = false;
    bool finished = false;

    while (!finished) {
        // Lacing: a packet of L bytes uses floor(L/255)+1 lacing values, the
        // last one < 255 (0 for exact multiples). A page holds at most 255
        // lacing values; if the packet doesn't complete within them, every
        // value is 255 and the packet continues on the next page, which is
        // flagged 0x01.
        uint8_t lacing[255];
        int n_lacing = 0;
        size_t payload = 0;
        while (n_lacing < 255) {
            size_t left = size - offset - payload;
            uint8_t seg = left >= 255 ? 255 : static_cast<uint8_t>(left);
            lacing[n_lacing++] = seg;
            payload += seg;
            if (seg < 255) {
                finished = true;
                break;
            }
        }

        const size_t header_size = 27 + static_cast<size_t>(n_lacing);
        const size_t page_start = m_input.size();
        m_input.resize(page_start + header_size + payload);
        uint8_t* h = m_input.data() + page_start;

        std::memcpy(h, "OggS", 4);
        h[4] = 0; // stream structure version
        h[5] = static_cast<uint8_t>((continued ? 0x01 : 0x00) | (bos ? 0x02 : 0x00));
        std::memset(h + 6, 0xFF, 8); // granulepos = -1 (no position claimed)
        h[14] = static_cast<uint8_t>(kSyntheticSerial);
        h[15] = static_cast<uint8_t>(kSyntheticSerial >> 8);
        h[16] = static_cast<uint8_t>(kSyntheticSerial >> 16);
        h[17] = static_cast<uint8_t>(kSyntheticSerial >> 24);
        const uint32_t pageno = m_page_counter++;
        h[18] = static_cast<uint8_t>(pageno);
        h[19] = static_cast<uint8_t>(pageno >> 8);
        h[20] = static_cast<uint8_t>(pageno >> 16);
        h[21] = static_cast<uint8_t>(pageno >> 24);
        std::memset(h + 22, 0, 4); // CRC computed below over the zeroed field
        h[26] = static_cast<uint8_t>(n_lacing);
        std::memcpy(h + 27, lacing, static_cast<size_t>(n_lacing));
        if (payload > 0) {
            std::memcpy(h + header_size, packet + offset, payload);
        }

        const uint32_t crc = oggCrc32(h, header_size + payload);
        h[22] = static_cast<uint8_t>(crc);
        h[23] = static_cast<uint8_t>(crc >> 8);
        h[24] = static_cast<uint8_t>(crc >> 16);
        h[25] = static_cast<uint8_t>(crc >> 24);

        offset += payload;
        continued = true; // any further page continues this packet
        bos = false;
    }
}

void VorbisCodec::appendResyncPrimerPage_unlocked()
{
    // A zero-segment Ogg page: valid framing and CRC, but no packets. The
    // post-flush resync scanner CRC-validates it and locks without consuming
    // any audio; the normal parser then just steps past it to the next page.
    const size_t page_start = m_input.size();
    m_input.resize(page_start + 27);
    uint8_t* h = m_input.data() + page_start;

    std::memcpy(h, "OggS", 4);
    h[4] = 0;
    h[5] = 0;
    std::memset(h + 6, 0xFF, 8); // granulepos = -1
    h[14] = static_cast<uint8_t>(kSyntheticSerial);
    h[15] = static_cast<uint8_t>(kSyntheticSerial >> 8);
    h[16] = static_cast<uint8_t>(kSyntheticSerial >> 16);
    h[17] = static_cast<uint8_t>(kSyntheticSerial >> 24);
    const uint32_t pageno = m_page_counter++;
    h[18] = static_cast<uint8_t>(pageno);
    h[19] = static_cast<uint8_t>(pageno >> 8);
    h[20] = static_cast<uint8_t>(pageno >> 16);
    h[21] = static_cast<uint8_t>(pageno >> 24);
    std::memset(h + 22, 0, 4);
    h[26] = 0; // zero lacing values: a page with no packets

    const uint32_t crc = oggCrc32(h, 27);
    h[22] = static_cast<uint8_t>(crc);
    h[23] = static_cast<uint8_t>(crc >> 8);
    h[24] = static_cast<uint8_t>(crc >> 16);
    h[25] = static_cast<uint8_t>(crc >> 24);
}

void VorbisCodec::drainDecoder_unlocked()
{
    // Feed buffered synthetic pages to stb_vorbis until it wants more data.
    // Each audio packet yields at most one frame (≤ 4096 samples/channel);
    // a return of 0 bytes consumed means the whole next packet isn't in the
    // buffer yet, so the remaining bytes wait for the next chunk.
    if (!m_stb) {
        return;
    }

    while (!m_input.empty()) {
        int channels = 0;
        float** outputs = nullptr;
        int samples = 0;
        int used = stb_vorbis_decode_frame_pushdata(
            m_stb, m_input.data(), static_cast<int>(m_input.size()),
            &channels, &outputs, &samples);
        if (used == 0) {
            break;
        }
        m_input.erase(m_input.begin(), m_input.begin() + used);

        if (samples > 0 && outputs != nullptr) {
            // Reuse the existing float→s16 conversion so output semantics
            // (clamp then ×32767 truncation) are unchanged from libvorbis.
            AudioFrame temp;
            convertFloatToPCM_unlocked(outputs, samples, temp);
            m_output_buffer.insert(m_output_buffer.end(),
                                   temp.samples.begin(), temp.samples.end());
            updateBackpressureState_unlocked();
        } else {
            // 0 samples with bytes consumed: warm-up frame after open/seek,
            // a skipped corrupt packet, or resync progress. A real decode
            // error auto-flushes stb into its CRC page scan; our pages all
            // carry valid CRCs, so the scan re-locks on the next packet —
            // the same skip-and-continue contract the libvorbis path had
            // (Requirement 8.3).
            int stb_error = stb_vorbis_get_error(m_stb);
            if (stb_error != 0 && stb_error != VORBIS_need_more_data) {
                handleVorbisError_unlocked(stb_error);
            }
        }
    }

    // A stream whose packets never satisfy stb's whole-packet check would
    // otherwise grow m_input without bound.
    if (m_input.size() > MAX_PENDING_INPUT) {
        Debug::log("vorbis", "Pending input exceeded ", MAX_PENDING_INPUT,
                   " bytes; dropping buffered data");
        m_input.clear();
    }
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
    
    // Re-page the packet for stb_vorbis and decode whatever is now complete
    // in the buffer (Requirement 2.4). Corrupt packets are skipped inside
    // drainDecoder_unlocked and decoding continues (Requirement 8.3).
    appendPacketAsPages_unlocked(packet_data.data(), packet_data.size(), false);
    drainDecoder_unlocked();

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
    // Thread-safe: atomic read, no lock required (Requirement 10.7)
    return m_error_state.load();
}

void VorbisCodec::clearErrorState()
{
    // Thread-safe error state clearing (Requirement 10.7)
    std::lock_guard<std::mutex> lock(m_mutex);
    m_error_state.store(false);
    m_last_error.clear();
    Debug::log("vorbis", "Error state cleared");
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

AudioSample VorbisCodec::floatToSample(float sample)
{
    // stb_vorbis produces float samples nominally in [-1.0, 1.0]; the pipeline
    // carries full-scale S32. Converting straight from float means no
    // intermediate 16-bit rounding, so this is more accurate than the previous
    // float -> int16 step, not merely wider.
    //
    // Clamping is essential: the MDCT and windowing can push samples slightly
    // outside [-1.0, 1.0], and some encoders emit out-of-range values.
    constexpr float kScale = 2147483520.0f; // largest float below INT32_MAX
    if (sample >= 1.0f)  return std::numeric_limits<AudioSample>::max();
    if (sample <= -1.0f) return std::numeric_limits<AudioSample>::min();
    return static_cast<AudioSample>(sample * kScale);
}

void VorbisCodec::interleaveChannels(float** pcm, int samples, int channels,
                                     std::vector<AudioSample>& output)
{
    // Interleave multi-channel float arrays into full-scale S32 PCM output
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
            output.push_back(floatToSample(pcm[ch][i]));
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
    // Handle stb_vorbis error codes (Requirement 2.6, 8.1-8.7)
    //
    // Mid-stream decode errors are all recoverable by design: stb_vorbis
    // auto-flushes into its CRC-validated page scan, and since every
    // synthetic page carries a real CRC it re-locks on the next packet —
    // log and continue (Requirement 8.3). Only feature limits are worth
    // distinguishing; header-parse failures are handled fatally at
    // openDecoder_unlocked() before this is ever reached.
    switch (vorbis_error) {
        case VORBIS_feature_not_supported:
            // Floor-0 streams (pre-2004 encoders) are the one class of valid
            // Vorbis stb_vorbis cannot decode. In practice this error only
            // arises at openDecoder_unlocked() (setup-header parse), which
            // fails the stream fatally there; it cannot occur mid-stream.
            m_last_error = "Vorbis feature not supported by stb_vorbis (floor 0)";
            Debug::log("vorbis", m_last_error);
            Debug::log("error", "VorbisCodec: ", m_last_error);
            break;

        case VORBIS_bad_packet_type:
            // Header-typed packet in the audio phase (e.g. a chained stream's
            // new headers) - skipped, keep using the current setup
            m_last_error = "Unexpected Vorbis packet type - packet skipped";
            Debug::log("vorbis", m_last_error);
            break;

        default:
            // Corrupt packet or transient stream damage - stb has already
            // armed its resync scan; continue with the next packet
            m_last_error = "Vorbis decode error " + std::to_string(vorbis_error) +
                           " - packet skipped, resynchronizing";
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

    // Close the decoder and drop all buffered data (Requirement 2.8)
    if (m_stb) {
        stb_vorbis_close(m_stb);
        m_stb = nullptr;
    }
    m_decoder_initialized = false;
    m_input.clear();
    m_page_counter = 0;
    for (auto& h : m_header_data) {
        h.clear();
    }

    // Reset header count since structures are cleared
    m_header_packets_received = 0;

    Debug::log("vorbis", "Vorbis decoder cleaned up");
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
