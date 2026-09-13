/*
 * AC3NullDemuxer.cpp - Null/passthrough demuxer for raw AC-3 and E-AC-3 streams
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
namespace AC3 {

using PsyMP3::Codec::AC3::AC3FrameHeader;
using PsyMP3::Codec::AC3::kSyncWord;

namespace {

/// Bytes a header parse needs. AC-3's bsi is variable-length, but everything
/// the demuxer reads -- rate, size, channel mode, bsid, E-AC-3's block count
/// -- sits within the first few bytes.
constexpr size_t kHeaderProbeBytes = 16;

/// How far into the file to look for the first syncframe. A raw stream starts
/// on one; this only covers a file with junk or a tag in front.
constexpr uint64_t kSyncSearchLimit = 1024 * 1024;

/// Read granularity for the frame walk. Syncframes run to a few kilobytes at
/// most, so this covers many of them per read.
constexpr size_t kScanBufferSize = 256 * 1024;

/// A/52 §E2.3.1.1: strmtyp 1 is a dependent substream. It extends the
/// independent frame before it -- extra channels for 7.1 -- and adds no time
/// of its own, so it must not advance the clock.
bool isDependent(const AC3FrameHeader& header)
{
    return header.isEAC3() && header.strmtyp == 1;
}

} // namespace

AC3NullDemuxer::AC3NullDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler)
    : Demuxer(std::move(handler))
{
}

std::string AC3NullDemuxer::getContainerName() const
{
    // Raw elementary streams have no container as such; name the framing.
    return m_eac3 ? "E-AC-3" : "AC-3";
}

bool AC3NullDemuxer::parseContainer()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return parseContainer_unlocked();
}

bool AC3NullDemuxer::readHeaderAt_unlocked(uint64_t offset, AC3FrameHeader& header)
{
    uint8_t probe[kHeaderProbeBytes] = {};
    if (offset + 7 > m_file_size ||
        m_handler->seek(static_cast<off_t>(offset), SEEK_SET) != 0) {
        return false;
    }
    const size_t got = m_handler->read(probe, 1, sizeof(probe));
    return got >= 7 &&
           PsyMP3::Codec::AC3::parseAC3FrameHeader(probe, got, header) &&
           header.frame_size != 0;
}

bool AC3NullDemuxer::parseContainer_unlocked()
{
    if (m_parsed) {
        return true;
    }
    if (!m_handler) {
        Debug::log("ac3demux", "AC3NullDemuxer: No IOHandler");
        return false;
    }

    m_handler->seek(0, SEEK_END);
    m_file_size = static_cast<uint64_t>(m_handler->tell());
    m_handler->seek(0, SEEK_SET);

    // --- find the first syncframe ---
    // A 16-bit sync word turns up by chance in any binary data, so a candidate
    // only counts if its header parses *and* the next frame starts exactly
    // where this one says it ends. Two consecutive frames is a far stronger
    // test than the sync word alone.
    const uint64_t limit = std::min<uint64_t>(m_file_size, kSyncSearchLimit);
    std::vector<uint8_t> window(static_cast<size_t>(limit));
    if (m_handler->seek(0, SEEK_SET) != 0) {
        return false;
    }
    const size_t got = m_handler->read(window.data(), 1, window.size());

    AC3FrameHeader first;
    bool found = false;
    for (size_t i = 0; i + 7 <= got; ++i) {
        if (((window[i] << 8) | window[i + 1]) != kSyncWord) {
            continue;
        }
        AC3FrameHeader candidate;
        if (!PsyMP3::Codec::AC3::parseAC3FrameHeader(&window[i], got - i, candidate) ||
            candidate.frame_size == 0) {
            continue;
        }
        const uint64_t next = static_cast<uint64_t>(i) + candidate.frame_size;
        AC3FrameHeader following;
        // A file holding exactly one frame has nothing to confirm against.
        const bool confirmed = next >= m_file_size || readHeaderAt_unlocked(next, following);
        if (confirmed) {
            m_data_start_offset = i;
            first = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        Debug::log("ac3demux", "AC3NullDemuxer: No syncframe found");
        return false;
    }

    m_eac3 = first.isEAC3();
    m_samples_per_frame = static_cast<unsigned>(first.blocks) * PsyMP3::Codec::AC3::kSamplesPerBlock;
    if (m_samples_per_frame == 0 || first.sample_rate == 0) {
        return false;
    }

    // --- walk the frames ---
    // One linear pass. Every frame states its own length, so this is exact
    // rather than a scan for sync patterns, and it yields both the duration
    // and the offset table a seek uses.
    std::vector<uint8_t> buf(kScanBufferSize);
    uint64_t buf_start = 0;
    size_t buf_len = 0;
    uint64_t offset = m_data_start_offset;

    while (offset + 7 <= m_file_size) {
        if (offset < buf_start || offset + kHeaderProbeBytes > buf_start + buf_len) {
            if (m_handler->seek(static_cast<off_t>(offset), SEEK_SET) != 0) {
                break;
            }
            buf_start = offset;
            buf_len = m_handler->read(buf.data(), 1, buf.size());
            if (buf_len < 7) {
                break;
            }
        }
        const size_t at = static_cast<size_t>(offset - buf_start);
        AC3FrameHeader header;
        if (!PsyMP3::Codec::AC3::parseAC3FrameHeader(&buf[at], buf_len - at, header) ||
            header.frame_size == 0 || offset + header.frame_size > m_file_size) {
            // A short or damaged tail is a truncated frame, not a parse
            // failure; everything up to here is still playable.
            break;
        }
        if (!isDependent(header)) {
            m_frame_offsets.push_back(offset);
            m_total_samples += static_cast<uint64_t>(header.blocks) *
                               PsyMP3::Codec::AC3::kSamplesPerBlock;
        }
        offset += header.frame_size;
    }

    m_frames = m_frame_offsets.size();
    if (m_frames == 0) {
        Debug::log("ac3demux", "AC3NullDemuxer: No complete syncframes");
        return false;
    }

    m_stream_info.stream_id = 1;
    m_stream_info.codec_type = "audio";
    m_stream_info.codec_name = m_eac3 ? "eac3" : "ac3";
    m_stream_info.sample_rate = first.sample_rate;
    m_stream_info.channels = first.outputChannels();
    m_stream_info.duration_samples = m_total_samples;
    m_duration_ms = (m_total_samples * 1000ULL) / first.sample_rate;
    m_stream_info.duration_ms = m_duration_ms;
    if (first.bitrate != 0) {
        m_stream_info.bitrate = first.bitrate;
    } else if (m_duration_ms != 0) {
        // E-AC-3 states a frame length, not a rate, so average it instead.
        m_stream_info.bitrate = static_cast<uint32_t>(
            ((offset - m_data_start_offset) * 8000ULL) / m_duration_ms);
    }

    m_read_offset = m_data_start_offset;
    m_current_sample = 0;
    m_parsed = true;

    Debug::log("ac3demux", "AC3NullDemuxer: ", m_stream_info.codec_name, " ",
               m_stream_info.sample_rate, " Hz ", m_stream_info.channels, " ch, ",
               m_frames, " frames, ", m_duration_ms, " ms");
    return true;
}

std::vector<StreamInfo> AC3NullDemuxer::getStreams() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_parsed) {
        return {};
    }
    return {m_stream_info};
}

StreamInfo AC3NullDemuxer::getStreamInfo(uint32_t stream_id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_parsed || (stream_id != 0 && stream_id != m_stream_info.stream_id)) {
        return StreamInfo();
    }
    return m_stream_info;
}

MediaChunk AC3NullDemuxer::readChunk()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return readChunk_unlocked();
}

MediaChunk AC3NullDemuxer::readChunk(uint32_t stream_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (stream_id != 0 && stream_id != m_stream_info.stream_id) {
        return MediaChunk();
    }
    return readChunk_unlocked();
}

MediaChunk AC3NullDemuxer::readChunk_unlocked()
{
    if (!m_parsed || m_eof_flag.load()) {
        return MediaChunk();
    }

    AC3FrameHeader header;
    if (!readHeaderAt_unlocked(m_read_offset, header) ||
        m_read_offset + header.frame_size > m_file_size) {
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
    chunk.data.resize(header.frame_size);

    if (m_handler->seek(static_cast<off_t>(m_read_offset), SEEK_SET) != 0 ||
        m_handler->read(chunk.data.data(), 1, header.frame_size) != header.frame_size) {
        m_eof_flag.store(true);
        return MediaChunk();
    }

    m_read_offset += header.frame_size;
    if (!isDependent(header)) {
        m_current_sample += static_cast<uint64_t>(header.blocks) *
                            PsyMP3::Codec::AC3::kSamplesPerBlock;
    }
    if (m_stream_info.sample_rate != 0) {
        m_position_ms = (m_current_sample * 1000ULL) / m_stream_info.sample_rate;
    }
    return chunk;
}

bool AC3NullDemuxer::seekTo(uint64_t timestamp_ms)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return seekTo_unlocked(timestamp_ms);
}

bool AC3NullDemuxer::seekTo_unlocked(uint64_t timestamp_ms)
{
    if (!m_parsed || m_stream_info.sample_rate == 0 || m_frame_offsets.empty() ||
        m_samples_per_frame == 0) {
        return false;
    }

    // Every syncframe restarts its own exponents and bit allocation, so any
    // one of them is a valid place to begin. The transform's overlap is the
    // only history, and the codec's reset() drops it; the first half-block
    // after a seek is therefore not the encoder's signal, which at 5 ms is
    // shorter than the pipeline's own seek fade.
    const uint64_t target = (timestamp_ms * m_stream_info.sample_rate) / 1000;
    uint64_t index = target / m_samples_per_frame;
    if (index >= m_frame_offsets.size()) {
        index = m_frame_offsets.size() - 1;
    }

    m_read_offset = m_frame_offsets[static_cast<size_t>(index)];
    m_current_sample = index * m_samples_per_frame;
    m_position_ms = (m_current_sample * 1000ULL) / m_stream_info.sample_rate;
    m_eof_flag.store(false);
    return true;
}

bool AC3NullDemuxer::isEOF() const
{
    return m_eof_flag.load();
}

uint64_t AC3NullDemuxer::getDuration() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_duration_ms;
}

uint64_t AC3NullDemuxer::getPosition() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_position_ms;
}

} // namespace AC3
} // namespace Demuxer
} // namespace PsyMP3
