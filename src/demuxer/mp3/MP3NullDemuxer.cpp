/*
 * MP3NullDemuxer.cpp - Null/passthrough demuxer for self-containerized MP3 streams
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Demuxer {
namespace MP3 {

// MPEG audio version ID table
static constexpr uint32_t s_sample_rates[4][4] = {
    {11025, 12000, 8000, 0},   // MPEG 2.5
    {0, 0, 0, 0},              // reserved
    {22050, 24000, 16000, 0},  // MPEG 2
    {44100, 48000, 32000, 0},  // MPEG 1
};

// MPEG 1 Layer III bitrate table (kbps)
static constexpr uint32_t s_bitrates_v1_l3[16] = {
    0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0
};

// MPEG 2/2.5 Layer III bitrate table (kbps)
static constexpr uint32_t s_bitrates_v2_l3[16] = {
    0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0
};

MP3NullDemuxer::MP3NullDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler)
    : Demuxer(std::move(handler)) {
}

bool MP3NullDemuxer::parseContainer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return parseContainer_unlocked();
}

std::vector<StreamInfo> MP3NullDemuxer::getStreams() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_parsed) return {};
    return {m_stream_info};
}

StreamInfo MP3NullDemuxer::getStreamInfo(uint32_t /*stream_id*/) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stream_info;
}

MediaChunk MP3NullDemuxer::readChunk() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return readChunk_unlocked();
}

MediaChunk MP3NullDemuxer::readChunk(uint32_t /*stream_id*/) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return readChunk_unlocked();
}

bool MP3NullDemuxer::seekTo(uint64_t timestamp_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return seekTo_unlocked(timestamp_ms);
}

bool MP3NullDemuxer::isEOF() const {
    return m_eof_flag.load();
}

uint64_t MP3NullDemuxer::getDuration() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_duration_ms;
}

uint64_t MP3NullDemuxer::getPosition() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_position_ms;
}

// --- Internal methods (assume lock held) ---

bool MP3NullDemuxer::parseContainer_unlocked() {
    if (m_parsed) return true;

    if (!m_handler) {
        Debug::log("mp3demux", "MP3NullDemuxer: No IOHandler");
        return false;
    }

    // Get file size
    m_handler->seek(0, SEEK_END);
    m_file_size = static_cast<uint64_t>(m_handler->tell());
    m_handler->seek(0, SEEK_SET);
    m_data_end_offset = m_file_size;

    // Skip ID3v2 tag if present
    skipID3v2Tag_unlocked();

    // Check for ID3v1 tag at end of file (128 bytes)
    if (m_file_size > 128) {
        m_handler->seek(static_cast<long>(m_file_size - 128), SEEK_SET);
        uint8_t tag_header[3] = {};
        m_handler->read(tag_header, 1, 3);
        if (tag_header[0] == 'T' && tag_header[1] == 'A' && tag_header[2] == 'G') {
            m_data_end_offset = m_file_size - 128;
        }
        // Restore position to data start
        m_handler->seek(static_cast<long>(m_data_start_offset), SEEK_SET);
    }

    // Find and parse first valid MP3 frame
    if (!parseFirstFrame_unlocked()) {
        Debug::log("mp3demux", "MP3NullDemuxer: Could not find valid MP3 frame");
        return false;
    }

    m_parsed = true;
    m_eof_flag.store(false);
    Debug::log("mp3demux", "MP3NullDemuxer: Parsed MP3 stream - ",
               m_stream_info.sample_rate, " Hz, ", m_stream_info.channels, " ch, ",
               m_duration_ms, " ms duration");
    return true;
}

bool MP3NullDemuxer::skipID3v2Tag_unlocked() {
    m_handler->seek(0, SEEK_SET);
    uint8_t header[10] = {};
    if (m_handler->read(header, 1, 10) != 10) {
        m_handler->seek(0, SEEK_SET);
        m_data_start_offset = 0;
        return false;
    }

    // Check for "ID3" magic
    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3') {
        m_handler->seek(0, SEEK_SET);
        m_data_start_offset = 0;
        return false;
    }

    // Parse synchsafe size (4 bytes, 7 bits each)
    uint32_t tag_size = (static_cast<uint32_t>(header[6] & 0x7F) << 21) |
                        (static_cast<uint32_t>(header[7] & 0x7F) << 14) |
                        (static_cast<uint32_t>(header[8] & 0x7F) << 7) |
                        (static_cast<uint32_t>(header[9] & 0x7F));

    m_data_start_offset = 10 + tag_size;

    // Check for footer flag (adds 10 bytes)
    if (header[5] & 0x10) {
        m_data_start_offset += 10;
    }

    Debug::log("mp3demux", "MP3NullDemuxer: Skipping ID3v2 tag, ", m_data_start_offset, " bytes");
    m_handler->seek(static_cast<long>(m_data_start_offset), SEEK_SET);
    return true;
}

bool MP3NullDemuxer::parseFirstFrame_unlocked() {
    m_handler->seek(static_cast<long>(m_data_start_offset), SEEK_SET);

    // Search for the first valid frame sync within reasonable range
    const uint64_t search_limit = std::min(m_data_start_offset + 65536, m_data_end_offset);
    uint8_t header[4] = {};

    while (static_cast<uint64_t>(m_handler->tell()) < search_limit) {
        if (m_handler->read(header, 1, 1) != 1) return false;

        if (header[0] != 0xFF) continue;
        if (m_handler->read(header + 1, 1, 1) != 1) return false;

        // Check frame sync: 0xFFE0 minimum (11 sync bits)
        if ((header[1] & 0xE0) != 0xE0) continue;

        // Read remaining header bytes
        if (m_handler->read(header + 2, 1, 2) != 2) return false;

        if (!isValidFrameHeader(header)) {
            // Seek back to try next byte after the 0xFF
            m_handler->seek(-3, SEEK_CUR);
            continue;
        }

        // Valid header found - record the data start
        uint64_t frame_offset = static_cast<uint64_t>(m_handler->tell()) - 4;
        m_data_start_offset = frame_offset;

        uint32_t sample_rate = getFrameSampleRate(header);
        uint16_t channels = getFrameChannels(header);
        uint32_t bitrate = getFrameBitrate(header);
        uint32_t frame_size = getFrameSize(header);

        // Set up stream info
        m_stream_info.stream_id = 1;
        m_stream_info.codec_type = "audio";
        m_stream_info.codec_name = "mp3";
        m_stream_info.sample_rate = sample_rate;
        m_stream_info.channels = channels;
        m_stream_info.bits_per_sample = 16;
        m_stream_info.bitrate = bitrate * 1000;

        // Try to parse Xing/LAME VBR header from this frame
        if (frame_size > 4 && frame_size < 4096) {
            std::vector<uint8_t> frame_data(frame_size);
            m_handler->seek(static_cast<long>(frame_offset), SEEK_SET);
            if (m_handler->read(frame_data.data(), 1, frame_size) == frame_size) {
                parseXingHeader_unlocked(frame_data, sample_rate, channels);
            }
            // If Xing found, skip past the Xing frame
            if (m_total_frames > 0) {
                m_data_start_offset = frame_offset + frame_size;
            }
        }

        // Estimate duration if not from Xing
        if (m_total_samples == 0 && bitrate > 0) {
            uint64_t audio_bytes = m_data_end_offset - m_data_start_offset;
            m_duration_ms = (audio_bytes * 8) / (bitrate);
            m_stream_info.duration_ms = m_duration_ms;
            uint64_t samples_per_frame = getFrameSamples(header);
            if (samples_per_frame > 0 && frame_size > 0) {
                m_total_frames = static_cast<uint32_t>(audio_bytes / frame_size);
                m_total_samples = static_cast<uint64_t>(m_total_frames) * samples_per_frame;
                m_stream_info.duration_samples = m_total_samples;
            }
        }

        // Seek to actual data start for first readChunk
        m_handler->seek(static_cast<long>(m_data_start_offset), SEEK_SET);
        return true;
    }

    return false;
}

bool MP3NullDemuxer::parseXingHeader_unlocked(const std::vector<uint8_t>& frame_data,
                                               uint32_t sample_rate, uint16_t channels) {
    // Xing header offset depends on MPEG version and channel mode
    // For MPEG1: mono=21, stereo=36
    // For MPEG2/2.5: mono=13, stereo=21
    size_t xing_offset = 0;
    uint8_t version_id = (frame_data[1] >> 3) & 0x03;
    bool is_mpeg1 = (version_id == 3);

    if (is_mpeg1) {
        xing_offset = (channels == 1) ? 21 : 36;
    } else {
        xing_offset = (channels == 1) ? 13 : 21;
    }

    if (xing_offset + 8 > frame_data.size()) return false;

    // Check for "Xing" or "Info" tag
    bool is_xing = (frame_data[xing_offset] == 'X' && frame_data[xing_offset + 1] == 'i' &&
                    frame_data[xing_offset + 2] == 'n' && frame_data[xing_offset + 3] == 'g');
    bool is_info = (frame_data[xing_offset] == 'I' && frame_data[xing_offset + 1] == 'n' &&
                    frame_data[xing_offset + 2] == 'f' && frame_data[xing_offset + 3] == 'o');

    if (!is_xing && !is_info) return false;

    m_is_vbr = is_xing; // "Info" tag = CBR, "Xing" tag = VBR

    uint32_t flags = (static_cast<uint32_t>(frame_data[xing_offset + 4]) << 24) |
                     (static_cast<uint32_t>(frame_data[xing_offset + 5]) << 16) |
                     (static_cast<uint32_t>(frame_data[xing_offset + 6]) << 8) |
                     (static_cast<uint32_t>(frame_data[xing_offset + 7]));

    size_t pos = xing_offset + 8;

    // Frames field
    if (flags & 0x01) {
        if (pos + 4 > frame_data.size()) return false;
        m_total_frames = (static_cast<uint32_t>(frame_data[pos]) << 24) |
                         (static_cast<uint32_t>(frame_data[pos + 1]) << 16) |
                         (static_cast<uint32_t>(frame_data[pos + 2]) << 8) |
                         (static_cast<uint32_t>(frame_data[pos + 3]));
        pos += 4;

        // Calculate total samples and duration
        uint32_t samples_per_frame = is_mpeg1 ? 1152 : 576;
        m_total_samples = static_cast<uint64_t>(m_total_frames) * samples_per_frame;
        if (sample_rate > 0) {
            m_duration_ms = (m_total_samples * 1000ULL) / sample_rate;
            m_stream_info.duration_ms = m_duration_ms;
            m_stream_info.duration_samples = m_total_samples;
        }
    }

    // Bytes field
    if (flags & 0x02) {
        if (pos + 4 > frame_data.size()) return false;
        m_total_bytes = (static_cast<uint32_t>(frame_data[pos]) << 24) |
                        (static_cast<uint32_t>(frame_data[pos + 1]) << 16) |
                        (static_cast<uint32_t>(frame_data[pos + 2]) << 8) |
                        (static_cast<uint32_t>(frame_data[pos + 3]));
        pos += 4;

        // Update bitrate from actual data if we have frames
        if (m_total_frames > 0 && m_duration_ms > 0) {
            m_stream_info.bitrate = static_cast<uint32_t>((static_cast<uint64_t>(m_total_bytes) * 8 * 1000) / m_duration_ms);
        }
    }

    // TOC field (100 bytes)
    if (flags & 0x04) {
        if (pos + 100 > frame_data.size()) return false;
        m_xing_toc.assign(frame_data.begin() + static_cast<ptrdiff_t>(pos),
                          frame_data.begin() + static_cast<ptrdiff_t>(pos) + 100);
        pos += 100;
    }

    Debug::log("mp3demux", "MP3NullDemuxer: Found ", (is_xing ? "Xing" : "Info"), " header - ",
               m_total_frames, " frames, ", m_total_bytes, " bytes, ",
               m_duration_ms, " ms");
    return true;
}

bool MP3NullDemuxer::findFrameSync_unlocked() {
    uint8_t byte;
    while (static_cast<uint64_t>(m_handler->tell()) < m_data_end_offset) {
        if (m_handler->read(&byte, 1, 1) != 1) return false;
        if (byte != 0xFF) continue;

        uint8_t next;
        if (m_handler->read(&next, 1, 1) != 1) return false;
        if ((next & 0xE0) == 0xE0) {
            m_handler->seek(-2, SEEK_CUR);
            return true;
        }
        m_handler->seek(-1, SEEK_CUR);
    }
    return false;
}

MediaChunk MP3NullDemuxer::readChunk_unlocked() {
    if (!m_parsed || m_eof_flag.load()) {
        return MediaChunk();
    }

    uint64_t frame_offset = static_cast<uint64_t>(m_handler->tell());
    if (frame_offset >= m_data_end_offset) {
        m_eof_flag.store(true);
        MediaChunk eos;
        eos.stream_id = 1;
        eos.end_of_stream = true;
        return eos;
    }

    // Read frame header
    uint8_t header[4] = {};
    if (m_handler->read(header, 1, 4) != 4) {
        m_eof_flag.store(true);
        return MediaChunk();
    }

    // Validate and find sync if needed
    if (!isValidFrameHeader(header)) {
        m_handler->seek(-4, SEEK_CUR);
        if (!findFrameSync_unlocked()) {
            m_eof_flag.store(true);
            return MediaChunk();
        }
        frame_offset = static_cast<uint64_t>(m_handler->tell());
        if (m_handler->read(header, 1, 4) != 4) {
            m_eof_flag.store(true);
            return MediaChunk();
        }
        if (!isValidFrameHeader(header)) {
            m_eof_flag.store(true);
            return MediaChunk();
        }
    }

    uint32_t frame_size = getFrameSize(header);
    if (frame_size == 0 || frame_size > 4096) {
        // Invalid or free-format frame, try to resync
        if (!findFrameSync_unlocked()) {
            m_eof_flag.store(true);
            return MediaChunk();
        }
        return readChunk_unlocked();
    }

    // Read the complete frame (including header)
    std::vector<uint8_t> frame_data(frame_size);
    std::memcpy(frame_data.data(), header, 4);
    size_t remaining = frame_size - 4;
    if (remaining > 0) {
        if (m_handler->read(frame_data.data() + 4, 1, remaining) != remaining) {
            m_eof_flag.store(true);
            return MediaChunk();
        }
    }

    // Build MediaChunk
    MediaChunk chunk;
    chunk.stream_id = 1;
    chunk.data = std::move(frame_data);
    chunk.timestamp_samples = m_current_sample;
    chunk.file_offset = frame_offset;
    chunk.is_keyframe = true;

    // Advance position tracking
    uint32_t samples = getFrameSamples(header);
    m_current_sample += samples;
    if (m_stream_info.sample_rate > 0) {
        m_position_ms = (m_current_sample * 1000ULL) / m_stream_info.sample_rate;
    }

    // Check for EOF
    if (static_cast<uint64_t>(m_handler->tell()) >= m_data_end_offset) {
        m_eof_flag.store(true);
        chunk.end_of_stream = true;
    }

    return chunk;
}

bool MP3NullDemuxer::seekTo_unlocked(uint64_t timestamp_ms) {
    if (!m_parsed || m_stream_info.sample_rate == 0) return false;

    // Clamp to valid range
    if (timestamp_ms >= m_duration_ms) {
        m_eof_flag.store(true);
        return false;
    }

    m_eof_flag.store(false);
    uint64_t audio_data_size = m_data_end_offset - m_data_start_offset;

    // Use Xing TOC for VBR seeking if available
    if (!m_xing_toc.empty() && m_total_bytes > 0 && m_duration_ms > 0) {
        double percent = (static_cast<double>(timestamp_ms) / m_duration_ms) * 100.0;
        if (percent > 99.0) percent = 99.0;
        int index = static_cast<int>(percent);
        if (index < 0) index = 0;
        if (index > 99) index = 99;

        // Interpolate TOC entries
        double fa = m_xing_toc[static_cast<size_t>(index)];
        double fb = (index < 99) ? m_xing_toc[static_cast<size_t>(index + 1)] : 256.0;
        double frac = percent - index;
        double file_percent = fa + frac * (fb - fa);
        uint64_t byte_offset = static_cast<uint64_t>((file_percent / 256.0) * m_total_bytes);

        m_handler->seek(static_cast<long>(m_data_start_offset + byte_offset), SEEK_SET);
    } else {
        // CBR seeking: linear byte offset estimation
        double ratio = static_cast<double>(timestamp_ms) / m_duration_ms;
        uint64_t byte_offset = static_cast<uint64_t>(ratio * audio_data_size);
        m_handler->seek(static_cast<long>(m_data_start_offset + byte_offset), SEEK_SET);
    }

    // Find next valid frame sync
    if (!findFrameSync_unlocked()) {
        m_handler->seek(static_cast<long>(m_data_start_offset), SEEK_SET);
    }

    // Update position tracking
    m_current_sample = (timestamp_ms * m_stream_info.sample_rate) / 1000;
    m_position_ms = timestamp_ms;
    return true;
}

// --- Static frame header helpers ---

bool MP3NullDemuxer::isValidFrameHeader(const uint8_t header[4]) {
    // Frame sync: 11 bits set
    if (header[0] != 0xFF || (header[1] & 0xE0) != 0xE0) return false;

    // MPEG version: must not be reserved (01)
    uint8_t version = (header[1] >> 3) & 0x03;
    if (version == 1) return false;

    // Layer: must not be reserved (00)
    uint8_t layer = (header[1] >> 1) & 0x03;
    if (layer == 0) return false;

    // Bitrate: must not be 0xF (bad)
    uint8_t bitrate_index = (header[2] >> 4) & 0x0F;
    if (bitrate_index == 0x0F) return false;

    // Sample rate: must not be reserved (11)
    uint8_t srate_index = (header[2] >> 2) & 0x03;
    if (srate_index == 3) return false;

    return true;
}

uint32_t MP3NullDemuxer::getFrameSampleRate(const uint8_t header[4]) {
    uint8_t version = (header[1] >> 3) & 0x03;
    uint8_t srate_index = (header[2] >> 2) & 0x03;
    return s_sample_rates[version][srate_index];
}

uint16_t MP3NullDemuxer::getFrameChannels(const uint8_t header[4]) {
    uint8_t mode = (header[3] >> 6) & 0x03;
    return (mode == 3) ? 1 : 2; // 3 = mono, 0/1/2 = stereo variants
}

uint32_t MP3NullDemuxer::getFrameBitrate(const uint8_t header[4]) {
    uint8_t version = (header[1] >> 3) & 0x03;
    uint8_t bitrate_index = (header[2] >> 4) & 0x0F;
    bool is_mpeg1 = (version == 3);

    if (is_mpeg1) {
        return s_bitrates_v1_l3[bitrate_index];
    } else {
        return s_bitrates_v2_l3[bitrate_index];
    }
}

uint32_t MP3NullDemuxer::getFrameSize(const uint8_t header[4]) {
    uint32_t bitrate = getFrameBitrate(header) * 1000;
    uint32_t sample_rate = getFrameSampleRate(header);
    if (bitrate == 0 || sample_rate == 0) return 0;

    uint8_t version = (header[1] >> 3) & 0x03;
    bool is_mpeg1 = (version == 3);
    bool padding = (header[2] >> 1) & 0x01;

    if (is_mpeg1) {
        // MPEG 1 Layer III: frame_size = 144 * bitrate / sample_rate + padding
        return (144 * bitrate / sample_rate) + (padding ? 1 : 0);
    } else {
        // MPEG 2/2.5 Layer III: frame_size = 72 * bitrate / sample_rate + padding
        return (72 * bitrate / sample_rate) + (padding ? 1 : 0);
    }
}

uint32_t MP3NullDemuxer::getFrameSamples(const uint8_t header[4]) {
    uint8_t version = (header[1] >> 3) & 0x03;
    bool is_mpeg1 = (version == 3);
    return is_mpeg1 ? 1152 : 576;
}

} // namespace MP3
} // namespace Demuxer
} // namespace PsyMP3
