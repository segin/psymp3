/*
 * MLPNullDemuxer.cpp - Null/passthrough demuxer for raw MLP/Dolby TrueHD streams
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

#include "../../../third_party/mlp/mlp_decoder.h"

namespace PsyMP3 {
namespace Demuxer {
namespace MLP {

namespace {

/// Read granularity for the header walk. Access units average a few hundred
/// bytes, so this covers many of them per read.
constexpr size_t kScanBufferSize = 256 * 1024;

/// Shortest gap between indexed major syncs. They recur at least every 128
/// access units -- about 106 ms at 48 kHz -- which is finer than a seek needs
/// and would cost an entry every 106 ms over a feature-length stream.
constexpr uint64_t kSyncIndexIntervalMs = 500;

/// How far into the file to look for the first major sync. A raw stream starts
/// on an access unit boundary; this only covers a file with junk in front.
constexpr uint64_t kMajorSyncSearchLimit = 1024 * 1024;

} // namespace

MLPNullDemuxer::MLPNullDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler)
    : Demuxer(std::move(handler))
{
}

bool MLPNullDemuxer::parseContainer()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return parseContainer_unlocked();
}

bool MLPNullDemuxer::parseContainer_unlocked()
{
    if (m_parsed) {
        return true;
    }
    if (!m_handler) {
        Debug::log("mlpdemux", "MLPNullDemuxer: No IOHandler");
        return false;
    }

    m_handler->seek(0, SEEK_END);
    m_file_size = static_cast<uint64_t>(m_handler->tell());
    m_handler->seek(0, SEEK_SET);

    if (m_file_size < 8) {
        Debug::log("mlpdemux", "MLPNullDemuxer: File too small to hold an access unit");
        return false;
    }

    // The rate and the channel layout have to be known before the walk, because
    // the sync index is spaced by time.
    if (!describeStream_unlocked()) {
        return false;
    }
    if (!scanStream_unlocked()) {
        return false;
    }

    m_total_samples = m_access_units * m_samples_per_au;
    m_duration_ms = m_stream_info.sample_rate
        ? (m_total_samples * 1000ULL) / m_stream_info.sample_rate : 0;

    m_stream_info.duration_samples = m_total_samples;
    m_stream_info.duration_ms = m_duration_ms;
    if (m_duration_ms != 0) {
        m_stream_info.bitrate =
            static_cast<uint32_t>(((m_file_size - m_data_start_offset) * 8000ULL) / m_duration_ms);
    }

    m_read_offset = m_data_start_offset;
    m_current_sample = 0;
    m_parsed = true;

    Debug::log("mlpdemux", "MLPNullDemuxer: ", m_stream_info.codec_name, " ",
               m_stream_info.sample_rate, " Hz ", m_stream_info.channels, " ch, ",
               m_access_units, " access units, ", m_duration_ms, " ms, ",
               m_sync_points.size(), " sync points");
    return true;
}

bool MLPNullDemuxer::describeStream_unlocked()
{
    // A raw stream begins on an access unit boundary whose first access unit
    // carries a major sync. Scan for one only if it does not.
    std::vector<uint8_t> head(16);
    if (m_handler->seek(0, SEEK_SET) != 0 || m_handler->read(head.data(), 1, head.size()) < 8) {
        return false;
    }

    m_data_start_offset = 0;
    if (!mlp::Decoder::hasMajorSync(head.data(), head.size())) {
        uint64_t limit = std::min<uint64_t>(m_file_size, kMajorSyncSearchLimit);
        std::vector<uint8_t> window(static_cast<size_t>(limit));
        if (m_handler->seek(0, SEEK_SET) != 0) {
            return false;
        }
        size_t got = m_handler->read(window.data(), 1, window.size());
        bool found = false;
        for (size_t i = 4; i + 4 <= got; ++i) {
            uint32_t sync = static_cast<uint32_t>(window[i]) << 24 |
                            static_cast<uint32_t>(window[i + 1]) << 16 |
                            static_cast<uint32_t>(window[i + 2]) << 8 |
                            static_cast<uint32_t>(window[i + 3]);
            if (sync == mlp::MAJOR_SYNC_FBA || sync == mlp::MAJOR_SYNC_FBB) {
                // The major sync follows the four-byte access unit header.
                m_data_start_offset = i - 4;
                found = true;
                break;
            }
        }
        if (!found) {
            Debug::log("mlpdemux", "MLPNullDemuxer: No major sync found");
            return false;
        }
    }

    // Decode the first access unit rather than re-implementing the major sync
    // here: the channel count comes from the restart header, not the sync.
    uint8_t header[4] = {};
    if (m_handler->seek(static_cast<off_t>(m_data_start_offset), SEEK_SET) != 0 ||
        m_handler->read(header, 1, 4) != 4) {
        return false;
    }
    size_t au_len = mlp::Decoder::accessUnitLength(header, sizeof(header));
    if (au_len < 8 || m_data_start_offset + au_len > m_file_size) {
        Debug::log("mlpdemux", "MLPNullDemuxer: First access unit has an implausible length");
        return false;
    }

    std::vector<uint8_t> au(au_len);
    if (m_handler->seek(static_cast<off_t>(m_data_start_offset), SEEK_SET) != 0 ||
        m_handler->read(au.data(), 1, au_len) != au_len) {
        return false;
    }

    mlp::Decoder probe;
    std::vector<int32_t> pcm;
    try {
        probe.decodeAccessUnit(au.data(), au.size(), pcm);
    } catch (const std::exception& e) {
        Debug::log("mlpdemux", "MLPNullDemuxer: First access unit did not decode (", e.what(), ")");
        return false;
    }

    m_samples_per_au = probe.samplesPerAccessUnit();
    if (m_samples_per_au == 0 || probe.sampleRate() == 0 || probe.channels() == 0) {
        return false;
    }

    m_stream_info.stream_id = 1;
    m_stream_info.codec_type = "audio";
    m_stream_info.codec_name = probe.isTrueHD() ? "truehd" : "mlp";
    m_stream_info.sample_rate = probe.sampleRate();
    m_stream_info.channels = static_cast<uint16_t>(probe.channels());
    m_stream_info.bits_per_sample = static_cast<uint16_t>(probe.bitsPerSample());
    return true;
}

bool MLPNullDemuxer::scanStream_unlocked()
{
    // One linear pass over the access unit headers. Each one states its own
    // length, so the walk is exact rather than a scan for sync patterns, and it
    // gives both the duration and the seek index.
    std::vector<uint8_t> buf(kScanBufferSize);
    uint64_t offset = m_data_start_offset;
    uint64_t buf_start = 0;
    size_t buf_len = 0;

    const uint64_t sync_interval_samples =
        (static_cast<uint64_t>(m_stream_info.sample_rate) * kSyncIndexIntervalMs) / 1000;
    uint64_t sample = 0;
    bool have_first_sync = false;

    while (offset + 4 <= m_file_size) {
        if (offset < buf_start || offset + 8 > buf_start + buf_len) {
            if (m_handler->seek(static_cast<off_t>(offset), SEEK_SET) != 0) {
                break;
            }
            buf_start = offset;
            buf_len = m_handler->read(buf.data(), 1, buf.size());
            if (buf_len < 4) {
                break;
            }
        }

        const uint8_t* p = buf.data() + (offset - buf_start);
        size_t avail = buf_len - static_cast<size_t>(offset - buf_start);

        size_t au_len = mlp::Decoder::accessUnitLength(p, avail);
        if (au_len < 8 || offset + au_len > m_file_size) {
            // A short tail is a truncated access unit, not a parse failure; the
            // stream up to here is still playable.
            break;
        }

        if (mlp::Decoder::hasMajorSync(p, avail) &&
            (!have_first_sync || sample - m_sync_points.back().sample >= sync_interval_samples)) {
            m_sync_points.push_back({offset, sample});
            have_first_sync = true;
        }

        offset += au_len;
        sample += m_samples_per_au;
        ++m_access_units;
    }

    if (m_access_units == 0) {
        Debug::log("mlpdemux", "MLPNullDemuxer: No access units found");
        return false;
    }
    return true;
}

std::vector<StreamInfo> MLPNullDemuxer::getStreams() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_parsed) {
        return {};
    }
    return {m_stream_info};
}

StreamInfo MLPNullDemuxer::getStreamInfo(uint32_t stream_id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_parsed || (stream_id != 0 && stream_id != m_stream_info.stream_id)) {
        return StreamInfo();
    }
    return m_stream_info;
}

MediaChunk MLPNullDemuxer::readChunk()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return readChunk_unlocked();
}

MediaChunk MLPNullDemuxer::readChunk(uint32_t stream_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (stream_id != 0 && stream_id != m_stream_info.stream_id) {
        return MediaChunk();
    }
    return readChunk_unlocked();
}

MediaChunk MLPNullDemuxer::readChunk_unlocked()
{
    if (!m_parsed || m_eof_flag.load()) {
        return MediaChunk();
    }

    if (m_read_offset + 4 > m_file_size) {
        m_eof_flag.store(true);
        MediaChunk eos;
        eos.stream_id = 1;
        eos.end_of_stream = true;
        return eos;
    }

    uint8_t header[4] = {};
    if (m_handler->seek(static_cast<off_t>(m_read_offset), SEEK_SET) != 0 ||
        m_handler->read(header, 1, 4) != 4) {
        m_eof_flag.store(true);
        return MediaChunk();
    }

    size_t au_len = mlp::Decoder::accessUnitLength(header, sizeof(header));
    if (au_len < 8 || m_read_offset + au_len > m_file_size) {
        m_eof_flag.store(true);
        MediaChunk eos;
        eos.stream_id = 1;
        eos.end_of_stream = true;
        return eos;
    }

    MediaChunk chunk;
    chunk.stream_id = 1;
    chunk.file_offset = m_read_offset;
    chunk.timestamp_samples = m_current_sample;
    chunk.data.resize(au_len);

    if (m_handler->seek(static_cast<off_t>(m_read_offset), SEEK_SET) != 0 ||
        m_handler->read(chunk.data.data(), 1, au_len) != au_len) {
        m_eof_flag.store(true);
        return MediaChunk();
    }

    m_read_offset += au_len;
    m_current_sample += m_samples_per_au;
    if (m_stream_info.sample_rate != 0) {
        m_position_ms = (m_current_sample * 1000ULL) / m_stream_info.sample_rate;
    }
    return chunk;
}

bool MLPNullDemuxer::seekTo(uint64_t timestamp_ms)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return seekTo_unlocked(timestamp_ms);
}

bool MLPNullDemuxer::seekTo_unlocked(uint64_t timestamp_ms)
{
    if (!m_parsed || m_stream_info.sample_rate == 0) {
        return false;
    }

    // The decoder needs a major sync to start from, so a seek lands on the last
    // one at or before the target rather than on the nearest access unit.
    uint64_t target = (timestamp_ms * m_stream_info.sample_rate) / 1000;

    if (m_sync_points.empty()) {
        m_read_offset = m_data_start_offset;
        m_current_sample = 0;
    } else {
        size_t lo = 0;
        size_t hi = m_sync_points.size() - 1;
        while (lo < hi) {
            size_t mid = (lo + hi + 1) / 2;
            if (m_sync_points[mid].sample <= target) {
                lo = mid;
            } else {
                hi = mid - 1;
            }
        }
        m_read_offset = m_sync_points[lo].offset;
        m_current_sample = m_sync_points[lo].sample;
    }

    m_position_ms = (m_current_sample * 1000ULL) / m_stream_info.sample_rate;
    m_eof_flag.store(false);
    return true;
}

bool MLPNullDemuxer::isEOF() const
{
    return m_eof_flag.load();
}

uint64_t MLPNullDemuxer::getDuration() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_duration_ms;
}

uint64_t MLPNullDemuxer::getPosition() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_position_ms;
}

} // namespace MLP
} // namespace Demuxer
} // namespace PsyMP3
