/*
 * ADTSDemuxer.cpp - Demuxer for raw AAC in ADTS framing (.aac, AAC radio)
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
namespace ADTS {

namespace {

/// ADTS header bytes without and with the CRC.
constexpr size_t kHeaderBytes = 7;
constexpr size_t kHeaderBytesWithCRC = 9;

/// Samples a raw_data_block decodes to, at the ADTS sample rate. (SBR doubles
/// the output rate, but the codec accounts for that.)
constexpr uint64_t kSamplesPerBlock = 1024;

/// How far to look for the first frame: past junk, or a tag the skip missed.
constexpr uint64_t kSyncSearchLimit = 64 * 1024;

/// Sampling frequencies by sampling_frequency_index (ISO/IEC 14496-3
/// Table 1.18); 13 and 14 are reserved, 15 means an explicit rate, which
/// ADTS cannot carry.
constexpr unsigned kSampleRates[13] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000,
    22050, 16000, 12000, 11025, 8000, 7350,
};

/// Output channels for channel_configuration 1-7 (Table 1.19).
constexpr unsigned kChannels[8] = {0, 1, 2, 3, 4, 5, 6, 8};

} // namespace

/**
 * @brief Take ownership of the source.
 * @param handler The ADTS data.
 */
ADTSDemuxer::ADTSDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler)
    : Demuxer(std::move(handler))
{
}

/**
 * @brief Check sync, layer, rate and length, and decode the header fields.
 * @param data Candidate frame start.
 * @param size Bytes available there.
 * @param header Receives the fields.
 * @return true for a plausible header.
 */
bool ADTSDemuxer::parseHeader(const uint8_t* data, size_t size, ADTSHeader& header)
{
    if (!data || size < kHeaderBytes) return false;
    // syncword (12) ID (1) layer (2) protection_absent (1)
    if (data[0] != 0xFF || (data[1] & 0xF0) != 0xF0) return false;
    if ((data[1] & 0x06) != 0) return false; // layer must be 0
    ADTSHeader h;
    h.mpeg2 = (data[1] & 0x08) != 0;
    h.has_crc = (data[1] & 0x01) == 0;
    // profile (2) sampling_frequency_index (4) private (1) channel_configuration (3)
    h.profile = (data[2] >> 6) & 0x03;
    h.sf_index = (data[2] >> 2) & 0x0F;
    if (h.sf_index >= 13) return false;
    h.sample_rate = kSampleRates[h.sf_index];
    h.channel_config = ((data[2] & 0x01) << 2) | ((data[3] >> 6) & 0x03);
    h.channels = kChannels[h.channel_config];
    // original/copy, home, copyright bits (4), frame_length (13)
    h.frame_length = (static_cast<unsigned>(data[3] & 0x03) << 11) |
                     (static_cast<unsigned>(data[4]) << 3) |
                     (static_cast<unsigned>(data[5] >> 5) & 0x07);
    // adts_buffer_fullness (11), number_of_raw_data_blocks_in_frame (2)
    h.raw_blocks = (data[6] & 0x03) + 1u;
    h.header_length = static_cast<unsigned>(h.has_crc ? kHeaderBytesWithCRC : kHeaderBytes);
    if (h.frame_length <= h.header_length) return false;
    header = h;
    return true;
}

/**
 * @brief Build AudioSpecificConfig: audioObjectType (5 bits) = profile + 1,
 *        samplingFrequencyIndex (4), channelConfiguration (4), then
 *        frameLengthFlag, dependsOnCoreCoder and extensionFlag, all 0.
 * @param header A parsed header.
 * @return Two bytes.
 */
std::vector<uint8_t> ADTSDemuxer::makeAudioSpecificConfig(const ADTSHeader& header)
{
    const unsigned object_type = header.profile + 1;
    const unsigned bits = (object_type << 11) | (header.sf_index << 7) | (header.channel_config << 3);
    return {static_cast<uint8_t>(bits >> 8), static_cast<uint8_t>(bits & 0xFF)};
}

/**
 * @brief Lock and parse.
 * @return What parseContainer_unlocked() returns.
 */
bool ADTSDemuxer::parseContainer()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return parseContainer_unlocked();
}

/**
 * @brief Seek to @p offset and parse the header there. For a file, an offset
 *        too close to the end to hold a header fails without reading.
 * @param offset Frame offset.
 * @param header Receives the header.
 * @return true if one parsed.
 */
bool ADTSDemuxer::readHeaderAt_unlocked(uint64_t offset, ADTSHeader& header)
{
    if (!m_live && offset + kHeaderBytes > m_file_size) return false;
    if (m_handler->seek(static_cast<off_t>(offset), SEEK_SET) != 0) return false;
    uint8_t buf[kHeaderBytesWithCRC] = {};
    const size_t got = m_handler->read(buf, 1, sizeof(buf));
    return parseHeader(buf, got, header);
}

/**
 * @brief Scan for a header that is confirmed by the next one. A 12-bit sync
 *        turns up by chance in AAC data and tags, so a lone match is not
 *        trusted.
 * @param from Start of the search.
 * @param found_at Receives the frame's offset.
 * @param found Receives its header.
 * @return true on success.
 */
bool ADTSDemuxer::findFrame_unlocked(uint64_t from, uint64_t& found_at, ADTSHeader& found)
{
    uint64_t limit = kSyncSearchLimit;
    if (!m_live) {
        if (from >= m_file_size) return false;
        limit = std::min<uint64_t>(m_file_size - from, limit);
    }
    std::vector<uint8_t> window(static_cast<size_t>(limit));
    if (m_handler->seek(static_cast<off_t>(from), SEEK_SET) != 0) return false;
    const size_t got = m_handler->read(window.data(), 1, window.size());

    for (size_t i = 0; i + kHeaderBytes <= got; ++i) {
        ADTSHeader candidate;
        if (!parseHeader(&window[i], got - i, candidate)) continue;
        const size_t next = i + candidate.frame_length;
        ADTSHeader following;
        bool confirmed = false;
        if (next + kHeaderBytes <= got) {
            confirmed = parseHeader(&window[next], got - next, following) &&
                        following.sf_index == candidate.sf_index &&
                        following.channel_config == candidate.channel_config;
        } else if (!m_live && from + next >= m_file_size) {
            confirmed = true; // a file that ends with this frame
        }
        if (confirmed) {
            found_at = from + i;
            found = candidate;
            return true;
        }
    }
    return false;
}

/**
 * @brief Skip a leading ID3v2 tag, which .aac files often carry.
 * @return Offset just past it, or 0.
 */
uint64_t ADTSDemuxer::skipID3v2_unlocked()
{
    uint8_t header[10] = {};
    if (m_handler->seek(0, SEEK_SET) != 0 ||
        m_handler->read(header, 1, sizeof(header)) != sizeof(header) ||
        header[0] != 'I' || header[1] != 'D' || header[2] != '3') {
        return 0;
    }
    uint64_t size = 10 + ((static_cast<uint64_t>(header[6] & 0x7F) << 21) |
                          (static_cast<uint64_t>(header[7] & 0x7F) << 14) |
                          (static_cast<uint64_t>(header[8] & 0x7F) << 7) |
                          static_cast<uint64_t>(header[9] & 0x7F));
    if (header[5] & 0x10) size += 10; // footer
    return size;
}

/**
 * @brief Describe the stream from its first confirmed frame; for a file, walk
 *        all frames for the duration and the seek table.
 * @return true if the stream is usable.
 */
bool ADTSDemuxer::parseContainer_unlocked()
{
    if (m_parsed) return true;
    if (!m_handler) {
        Debug::log("adts", "ADTSDemuxer: No IOHandler");
        return false;
    }

    m_live = m_handler->isLiveStream();
    if (!m_live) {
        m_handler->seek(0, SEEK_END);
        m_file_size = static_cast<uint64_t>(m_handler->tell());
        m_handler->seek(0, SEEK_SET);
    }

    ADTSHeader first;
    if (!findFrame_unlocked(skipID3v2_unlocked(), m_data_start_offset, first)) {
        Debug::log("adts", "ADTSDemuxer: No ADTS frame found");
        return false;
    }
    if (first.channel_config == 0) {
        Debug::log("adts", "ADTSDemuxer: channel configuration 0 (program_config_element) is not supported");
        return false;
    }

    if (!m_live) {
        // One pass over the frames: each states its own length.
        uint64_t offset = m_data_start_offset;
        ADTSHeader header;
        while (readHeaderAt_unlocked(offset, header) &&
               offset + header.frame_length <= m_file_size) {
            m_frame_offsets.push_back(offset);
            m_total_samples += kSamplesPerBlock * header.raw_blocks;
            offset += header.frame_length;
        }
        if (m_frame_offsets.empty()) {
            Debug::log("adts", "ADTSDemuxer: No complete frames");
            return false;
        }
    }

    m_stream_info.stream_id = 1;
    m_stream_info.codec_type = "audio";
    m_stream_info.codec_name = "aac";
    m_stream_info.sample_rate = first.sample_rate;
    m_stream_info.channels = static_cast<uint16_t>(first.channels);
    m_stream_info.bits_per_sample = 16;
    m_stream_info.codec_data = makeAudioSpecificConfig(first);
    m_stream_info.duration_samples = m_total_samples;
    m_duration_ms = m_live ? 0 : (m_total_samples * 1000ULL) / first.sample_rate;
    m_stream_info.duration_ms = m_duration_ms;
    if (m_duration_ms != 0) {
        m_stream_info.bitrate = static_cast<uint32_t>(
            ((m_file_size - m_data_start_offset) * 8000ULL) / m_duration_ms);
    }

    m_read_offset = m_data_start_offset;
    m_current_sample = 0;
    m_position_ms = 0;
    m_parsed = true;
    m_eof_flag.store(false);
    Debug::log("adts", "ADTSDemuxer: AAC object type ", first.profile + 1, ", ",
               first.sample_rate, " Hz, ", first.channels, " ch, ",
               m_live ? std::string("live") : std::to_string(m_frame_offsets.size()) + " frames",
               ", ", m_duration_ms, " ms");
    return true;
}

/**
 * @brief The stream, once parsed.
 * @return One StreamInfo, or none.
 */
std::vector<StreamInfo> ADTSDemuxer::getStreams() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_parsed) return {};
    return {m_stream_info};
}

/**
 * @brief The stream's description.
 * @param stream_id 0 or its id.
 * @return It, or an empty StreamInfo.
 */
StreamInfo ADTSDemuxer::getStreamInfo(uint32_t stream_id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_parsed || (stream_id != 0 && stream_id != m_stream_info.stream_id)) {
        return StreamInfo();
    }
    return m_stream_info;
}

/**
 * @brief Lock and read.
 * @return The next chunk.
 */
MediaChunk ADTSDemuxer::readChunk()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return readChunk_unlocked();
}

/**
 * @brief Lock and read, for the stream's id.
 * @param stream_id 0 or its id.
 * @return The next chunk, or an empty one for another id.
 */
MediaChunk ADTSDemuxer::readChunk(uint32_t stream_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (stream_id != 0 && stream_id != m_stream_info.stream_id) return MediaChunk();
    return readChunk_unlocked();
}

/**
 * @brief Read the frame at m_read_offset, resynchronising past damage, and
 *        return its raw_data_block. Frames with several blocks are skipped.
 * @return The chunk; end of stream when no further frame is found.
 */
MediaChunk ADTSDemuxer::readChunk_unlocked()
{
    if (!m_parsed || m_eof_flag.load()) return MediaChunk();

    while (true) {
        ADTSHeader header;
        bool parsed = readHeaderAt_unlocked(m_read_offset, header);
        if (!parsed) {
            uint64_t resume = 0;
            parsed = findFrame_unlocked(m_read_offset + 1, resume, header);
            if (parsed) {
                Debug::log("adts", "ADTSDemuxer: resynchronised from ", m_read_offset, " to ", resume);
                m_read_offset = resume;
            }
        }
        if (!parsed || (!m_live && m_read_offset + header.frame_length > m_file_size)) {
            m_eof_flag.store(true);
            MediaChunk eos;
            eos.stream_id = 1;
            eos.end_of_stream = true;
            return eos;
        }

        const uint64_t frame_offset = m_read_offset;
        m_read_offset += header.frame_length;
        if (header.raw_blocks != 1) {
            if (!m_warned_multi_block) {
                Debug::log("adts", "ADTSDemuxer: skipping frames with ", header.raw_blocks, " raw data blocks");
                m_warned_multi_block = true;
            }
            m_current_sample += kSamplesPerBlock * header.raw_blocks;
            continue;
        }

        MediaChunk chunk;
        chunk.stream_id = 1;
        chunk.file_offset = frame_offset;
        chunk.timestamp_samples = m_current_sample;
        chunk.is_keyframe = true;
        chunk.data.resize(header.frame_length - header.header_length);
        if (m_handler->seek(static_cast<off_t>(frame_offset + header.header_length), SEEK_SET) != 0 ||
            m_handler->read(chunk.data.data(), 1, chunk.data.size()) != chunk.data.size()) {
            m_eof_flag.store(true);
            MediaChunk eos;
            eos.stream_id = 1;
            eos.end_of_stream = true;
            return eos;
        }
        m_current_sample += kSamplesPerBlock;
        if (m_stream_info.sample_rate != 0) {
            m_position_ms = (m_current_sample * 1000ULL) / m_stream_info.sample_rate;
        }
        return chunk;
    }
}

/**
 * @brief Lock and seek.
 * @param timestamp_ms Target.
 * @return What seekTo_unlocked() returns.
 */
bool ADTSDemuxer::seekTo(uint64_t timestamp_ms)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return seekTo_unlocked(timestamp_ms);
}

/**
 * @brief Land one frame before the target's frame, so the decoder's overlap
 *        is rebuilt by the time the target plays.
 * @param timestamp_ms Target.
 * @return false for a live stream or an unparsed one.
 */
bool ADTSDemuxer::seekTo_unlocked(uint64_t timestamp_ms)
{
    if (!m_parsed || m_live || m_frame_offsets.empty() || m_stream_info.sample_rate == 0) {
        return false;
    }
    const uint64_t target = (timestamp_ms * m_stream_info.sample_rate) / 1000;
    uint64_t index = target / kSamplesPerBlock;
    if (index >= m_frame_offsets.size()) index = m_frame_offsets.size() - 1;
    if (index > 0) --index;
    m_read_offset = m_frame_offsets[static_cast<size_t>(index)];
    m_current_sample = index * kSamplesPerBlock;
    m_position_ms = (m_current_sample * 1000ULL) / m_stream_info.sample_rate;
    m_eof_flag.store(false);
    return true;
}

/**
 * @brief Whether the end has been reached.
 * @return true at the end.
 */
bool ADTSDemuxer::isEOF() const
{
    return m_eof_flag.load();
}

/**
 * @brief A file's length.
 * @return Milliseconds, or 0 when live.
 */
uint64_t ADTSDemuxer::getDuration() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_duration_ms;
}

/**
 * @brief The next frame's time.
 * @return Milliseconds.
 */
uint64_t ADTSDemuxer::getPosition() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_position_ms;
}

} // namespace ADTS
} // namespace Demuxer
} // namespace PsyMP3
