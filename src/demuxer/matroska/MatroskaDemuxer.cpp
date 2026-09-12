/*
 * MatroskaDemuxer.cpp - Matroska/WebM container demuxer.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
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

namespace PsyMP3 {
namespace Demuxer {
namespace Matroska {

MatroskaDemuxer::MatroskaDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler)
    : Demuxer(std::move(handler))
    , m_reader(m_handler.get())
{
}

uint64_t MatroskaDemuxer::ticksToMs(int64_t ticks) const
{
    if (ticks <= 0) {
        return 0;
    }
    const uint64_t scale = m_parser.info().timestamp_scale_ns;
    return (static_cast<uint64_t>(ticks) * scale) / 1000000ULL;
}

bool MatroskaDemuxer::parseContainer()
{
    if (m_parsed) {
        return true;
    }

    m_file_size = static_cast<uint64_t>(m_handler->getFileSize());
    m_reader.seek(0);
    try {
        m_parser.parse(m_reader);
    } catch (const std::exception& e) {
        Debug::log("demux", "MatroskaDemuxer: ", e.what());
        return false;
    }

    if (m_parser.tracks().empty()) {
        Debug::log("demux", "MatroskaDemuxer: the file declares no tracks");
        return false;
    }

    // Every audio track is reported, with the one to play first. An
    // undecodable codec is not filtered out here: DemuxedStream takes the
    // first audio stream and AudioCodecFactory refuses a codec_name it does
    // not know, which is how every container skips a file it cannot play.
    // Hiding the track instead would leave Media Information lying about what
    // the file contains.
    const TrackEntry* chosen = m_parser.preferredAudioTrack();
    for (const TrackEntry& track : m_parser.tracks()) {
        if (!track.isAudio()) {
            continue;
        }
        StreamInfo info = m_parser.toStreamInfo(track);
        if (chosen && track.number == chosen->number) {
            m_streams.insert(m_streams.begin(), info);
        } else {
            m_streams.push_back(info);
        }
    }
    if (m_streams.empty()) {
        Debug::log("demux", "MatroskaDemuxer: no audio tracks");
        return false;
    }

    if (!chosen) {
        // Audio exists but nothing here decodes it. Reported rather than
        // refused, so the failure names the codec at the point a codec is
        // asked for instead of here.
        Debug::log("demux", "MatroskaDemuxer: no decodable audio track; first is ",
                   m_parser.tracks().front().codec_id);
    } else {
        m_track_number = chosen->number;
        m_sample_rate = m_streams.front().sample_rate;
    }

    m_duration_ms = m_parser.info().durationMs();

    // Cues are written after the clusters, so SeekHead is the only way to find
    // them; failing that, the cluster headers are walked instead.
    if (m_track_number != 0) {
        const uint64_t cues_at = m_parser.seekPosition(Id::Cues);
        if (!m_index.parseCues(m_reader, cues_at, m_parser.segmentDataOffset(),
                               m_track_number)) {
            m_index.buildByScanning(m_reader, m_parser.firstClusterOffset(), m_file_size);
        }
    }

    const uint64_t tags_at = m_parser.seekPosition(Id::Tags);
    if (tags_at != 0) {
        parseTags(tags_at);
    }

    m_read_offset = m_parser.firstClusterOffset();
    m_cluster_end = 0;
    m_eof = m_read_offset == 0 || m_read_offset >= m_file_size;
    m_parsed = true;
    return true;
}

void MatroskaDemuxer::parseTags(uint64_t tags_offset)
{
    std::map<std::string, std::vector<std::string>> fields;
    try {
        m_reader.seek(tags_offset);
        EBMLElement tags;
        if (!m_reader.readElementHeader(tags) || tags.id != Id::Tags || tags.unknown_size) {
            return;
        }
        const uint64_t end = tags.end();
        while (m_reader.tell() < end) {
            EBMLElement tag;
            if (!m_reader.readElementHeader(tag)) {
                break;
            }
            if (tag.id != Id::Tag || tag.unknown_size) {
                if (tag.unknown_size) {
                    break;
                }
                m_reader.seek(tag.end());
                continue;
            }

            // TargetTypeValue scopes what a name means: a TITLE at 50 is the
            // album's, at 30 the track's. Without it every release would
            // overwrite its own track title with the album name.
            uint64_t target = 50;
            std::vector<std::pair<std::string, std::string>> simple;
            const uint64_t tag_end = tag.end();
            while (m_reader.tell() < tag_end) {
                EBMLElement child;
                if (!m_reader.readElementHeader(child)) {
                    break;
                }
                if (child.id == Id::Targets) {
                    const uint64_t targets_end = child.end();
                    while (m_reader.tell() < targets_end) {
                        EBMLElement field;
                        if (!m_reader.readElementHeader(field)) {
                            break;
                        }
                        if (field.id == 0x68CA) { // TargetTypeValue
                            target = m_reader.readUInt(field);
                        }
                        m_reader.seek(field.end());
                    }
                } else if (child.id == Id::SimpleTag) {
                    std::string name;
                    std::string value;
                    const uint64_t simple_end = child.end();
                    while (m_reader.tell() < simple_end) {
                        EBMLElement field;
                        if (!m_reader.readElementHeader(field)) {
                            break;
                        }
                        if (field.id == Id::TagName) {
                            name = m_reader.readString(field);
                        } else if (field.id == Id::TagString) {
                            value = m_reader.readString(field);
                        }
                        m_reader.seek(field.end());
                    }
                    if (!name.empty() && !value.empty()) {
                        simple.emplace_back(name, value);
                    }
                }
                m_reader.seek(child.end());
            }

            for (auto& pair : simple) {
                std::string name = pair.first;
                // A TITLE scoped to the album is the album name, which is what
                // the rest of PsyMP3 calls ALBUM. Everything else keeps the
                // name Matroska gave it, which follows the same convention
                // Vorbis comments use.
                if (target >= 50 && name == "TITLE") {
                    name = "ALBUM";
                }
                // Appended rather than assigned: a release with several
                // artists writes several ARTIST tags, and PsyMP3 keeps those
                // value-separate all the way to Last.fm.
                fields[name].push_back(pair.second);
            }
            m_reader.seek(tag_end);
        }
    } catch (const std::exception& e) {
        Debug::log("demux", "MatroskaDemuxer: tags unreadable: ", e.what());
        return;
    }

    if (!fields.empty()) {
        m_tag = std::make_unique<PsyMP3::Tag::VorbisCommentTag>(
            "PsyMP3 Matroska Demuxer", fields);
    }
}

std::vector<StreamInfo> MatroskaDemuxer::getStreams() const
{
    std::lock_guard<std::mutex> lock(m_streams_mutex);
    return m_streams;
}

StreamInfo MatroskaDemuxer::getStreamInfo(uint32_t stream_id) const
{
    std::lock_guard<std::mutex> lock(m_streams_mutex);
    for (const StreamInfo& info : m_streams) {
        if (info.stream_id == stream_id) {
            return info;
        }
    }
    return StreamInfo();
}

void MatroskaDemuxer::takeBlock(const EBMLElement& block, int64_t cluster_ticks,
                                uint64_t discard_padding_ns)
{
    std::vector<uint8_t> payload = m_reader.readBinary(block);
    BlockHeader header;
    std::vector<BlockFrame> frames;
    if (!BlockParser::parse(payload.data(), payload.size(), header, frames)) {
        Debug::log("demux", "MatroskaDemuxer: malformed block at ", block.header_offset);
        return;
    }
    if (header.track_number != m_track_number) {
        return;
    }

    const int64_t ticks = cluster_ticks + header.timestamp_offset;
    const uint64_t milliseconds = ticksToMs(ticks);
    for (const BlockFrame& frame : frames) {
        MediaChunk chunk;
        chunk.stream_id = static_cast<uint32_t>(m_track_number);
        chunk.data.assign(frame.data, frame.data + frame.size);
        chunk.timestamp_samples = m_sample_rate > 0
                                ? (milliseconds * m_sample_rate) / 1000
                                : 0;
        chunk.is_keyframe = header.keyframe;
        chunk.file_offset = block.header_offset;
        m_queue.push_back(std::move(chunk));
    }
    (void)discard_padding_ns; // end trimming is the codec's business, not read here
}

bool MatroskaDemuxer::fillQueue()
{
    if (m_track_number == 0) {
        return false;
    }
    while (m_queue.empty() && !m_eof) {
        try {
            // Between clusters: find the next one. Cues, Tags and Attachments
            // legitimately sit among them, so anything else is stepped over.
            if (m_cluster_end == 0) {
                if (m_read_offset >= m_file_size) {
                    m_eof = true;
                    break;
                }
                m_reader.seek(m_read_offset);
                EBMLElement element;
                if (!m_reader.readElementHeader(element)) {
                    m_eof = true;
                    break;
                }
                if (element.id != Id::Cluster) {
                    if (element.unknown_size) {
                        m_eof = true;
                        break;
                    }
                    m_read_offset = element.end();
                    continue;
                }
                // A cluster of unknown size runs to the next one; bounding it
                // by the file lets the walk continue rather than stop.
                m_cluster_end = element.unknown_size ? m_file_size : element.end();
                m_cluster_ticks = 0;
                m_read_offset = element.data_offset;
            }

            if (m_read_offset >= m_cluster_end) {
                m_read_offset = m_cluster_end;
                m_cluster_end = 0;
                continue;
            }

            m_reader.seek(m_read_offset);
            EBMLElement child;
            if (!m_reader.readElementHeader(child)) {
                m_eof = true;
                break;
            }
            // A nested Cluster means the enclosing one had no size; hand the
            // outer loop back the header so it starts the new one.
            if (child.id == Id::Cluster) {
                m_cluster_end = 0;
                m_read_offset = child.header_offset;
                continue;
            }

            const uint64_t next = child.unknown_size ? m_cluster_end : child.end();
            if (child.id == Id::Timestamp) {
                m_cluster_ticks = static_cast<int64_t>(m_reader.readUInt(child));
            } else if (child.id == Id::SimpleBlock) {
                takeBlock(child, m_cluster_ticks, 0);
            } else if (child.id == Id::BlockGroup && !child.unknown_size) {
                const uint64_t group_end = child.end();
                uint64_t cursor = child.data_offset;
                while (cursor < group_end) {
                    m_reader.seek(cursor);
                    EBMLElement inner;
                    if (!m_reader.readElementHeader(inner)) {
                        break;
                    }
                    if (inner.id == Id::Block) {
                        takeBlock(inner, m_cluster_ticks, 0);
                    }
                    cursor = inner.unknown_size ? group_end : inner.end();
                }
            }
            m_read_offset = next;
        } catch (const std::exception& e) {
            Debug::log("demux", "MatroskaDemuxer: ", e.what());
            m_eof = true;
            break;
        }
    }
    return !m_queue.empty();
}

MediaChunk MatroskaDemuxer::readChunk()
{
    if (!fillQueue()) {
        return MediaChunk();
    }
    MediaChunk chunk = std::move(m_queue.front());
    m_queue.pop_front();
    if (m_sample_rate > 0) {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_position_ms = (chunk.timestamp_samples * 1000) / m_sample_rate;
    }
    return chunk;
}

MediaChunk MatroskaDemuxer::readChunk(uint32_t stream_id)
{
    // Only the selected track is ever queued, so a request for another is a
    // request for nothing rather than a reason to go looking.
    if (stream_id != static_cast<uint32_t>(m_track_number)) {
        return MediaChunk();
    }
    return readChunk();
}

bool MatroskaDemuxer::seekTo(uint64_t timestamp_ms)
{
    if (m_track_number == 0) {
        return false;
    }
    const uint64_t scale = m_parser.info().timestamp_scale_ns;
    const uint64_t ticks = scale > 0 ? (timestamp_ms * 1000000ULL) / scale : timestamp_ms;

    const CueEntry* entry = m_index.entryFor(ticks);
    if (!entry) {
        return false;
    }

    m_queue.clear();
    m_read_offset = entry->cluster_offset;
    m_cluster_end = 0;
    m_cluster_ticks = 0;
    m_eof = false;
    {
        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_position_ms = ticksToMs(static_cast<int64_t>(entry->time_ticks));
    }
    return true;
}

bool MatroskaDemuxer::isEOF() const
{
    return m_eof && m_queue.empty();
}

uint64_t MatroskaDemuxer::getDuration() const
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_duration_ms;
}

uint64_t MatroskaDemuxer::getPosition() const
{
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_position_ms;
}

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3
