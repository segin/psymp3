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
        m_frame_prefix = chosen->stripped_frame_prefix;
        // No real frame lasts a second; anything longer would only overflow
        // the lace arithmetic in takeBlock.
        constexpr uint64_t kMaxFrameNs = 1000000000ULL;
        m_default_duration_ns = chosen->default_duration_ns <= kMaxFrameNs
                              ? chosen->default_duration_ns : 0;
    }

    m_duration_ms = m_parser.info().durationMs();

    // Cues are found only through SeekHead. They usually follow the clusters,
    // and although RFC 9559 6.4 lets them come first instead, the Segment walk
    // does not collect them there. Without Cues usable for the track, the
    // cluster headers are walked instead.
    if (m_track_number != 0) {
        // A damaged index costs fast seeking, not playback: with no index, a
        // seek restarts at the first cluster and decodes forward. Cues are only
        // SHOULD-be-present (5.1.5) and the clusters behind them are intact, but
        // both builders throw on a malformed VINT or an integer wider than eight
        // octets, and an exception escaping here fails parseContainer, which
        // makes DemuxedStream refuse a file whose audio reads perfectly. Log it
        // and carry on unindexed, the way parseTags below already does.
        try {
            const uint64_t cues_at = m_parser.seekPosition(Id::Cues);
            if (!m_index.parseCues(m_reader, cues_at, m_parser.segmentDataOffset(),
                                   m_track_number)) {
                m_index.buildByScanning(m_reader, m_parser.firstClusterOffset(), m_file_size);
            }
        } catch (const std::exception& e) {
            Debug::log("demux", "MatroskaDemuxer: index unusable, seeking disabled: ", e.what());
            m_index = CueIndex();
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
                                int64_t discard_padding_ns)
{
    // The track number leads the payload. Check it before reading the rest:
    // most of a .mkv's blocks are video, and a video frame larger than the
    // reader's allocation ceiling used to throw here and end audio playback.
    {
        uint8_t lead[8] = {};
        const size_t want = static_cast<size_t>(std::min<uint64_t>(block.size, sizeof(lead)));
        m_reader.seek(block.data_offset);
        if (want == 0 || m_handler->read(lead, 1, want) != want) {
            return;
        }
        uint64_t track = 0;
        if (EBMLReader::decodeVInt(lead, want, track, /*keep_marker=*/false) == 0
            || track != m_track_number) {
            return;
        }
    }

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

    // DiscardPadding (RFC 9559 5.1.3.5.7) is silence added to the whole Block:
    // at its end when positive, at its start when negative. It is carried in
    // sample frames on the frame it touches, and DemuxedStream drops it.
    uint32_t padding_frames = 0;
    if (discard_padding_ns != 0 && m_sample_rate > 0) {
        // CodecDelay's bound: no real padding comes near 10 s, and with the
        // rate capped by SegmentParser the product below cannot overflow.
        constexpr uint64_t kMaxPaddingNs = 10ULL * 1000000000ULL;
        const uint64_t magnitude = discard_padding_ns < 0
                                 ? static_cast<uint64_t>(-(discard_padding_ns + 1)) + 1
                                 : static_cast<uint64_t>(discard_padding_ns);
        if (magnitude <= kMaxPaddingNs) {
            padding_frames = static_cast<uint32_t>(
                (magnitude * m_sample_rate + 500000000ULL) / 1000000000ULL);
        }
    }

    // A Cluster Timestamp at the edge of the int64 range would overflow the
    // sum, which is undefined; such a block has no usable time anyway.
    const int64_t offset = header.timestamp_offset;
    if ((offset > 0 && cluster_ticks > std::numeric_limits<int64_t>::max() - offset) ||
        (offset < 0 && cluster_ticks < std::numeric_limits<int64_t>::min() - offset)) {
        Debug::log("demux", "MatroskaDemuxer: block time out of range at ", block.header_offset);
        return;
    }
    const int64_t ticks = cluster_ticks + offset;
    const uint64_t milliseconds = ticksToMs(ticks);

    // A frame whose time is negative is decoded but not played (RFC 9559
    // 11.2). Blocks follow on from each other, so what lies before time 0 is
    // the earliest negative block's distance from it, and a later negative
    // block is already inside that span. The difference rides on the chunk as
    // leading padding.
    uint32_t before_zero = 0;
    if (ticks < 0 && m_sample_rate > 0) {
        // Cluster Timestamps are unsigned, so ticks is at least -32768.
        constexpr uint64_t kMaxLeadNs = 10ULL * 1000000000ULL;
        const uint64_t scale = m_parser.info().timestamp_scale_ns;
        const uint64_t span = static_cast<uint64_t>(-ticks);
        const uint64_t ns = scale > kMaxLeadNs / span ? kMaxLeadNs : span * scale;
        const uint64_t needed = (ns * m_sample_rate + 999999999ULL) / 1000000000ULL;
        if (needed > m_frames_before_zero) {
            before_zero = static_cast<uint32_t>(needed - m_frames_before_zero);
            m_frames_before_zero = needed;
        }
    }
    for (size_t i = 0; i < frames.size(); ++i) {
        const BlockFrame& frame = frames[i];
        MediaChunk chunk;
        chunk.stream_id = static_cast<uint32_t>(m_track_number);
        // Header stripping (RFC 9559 5.1.4.1.31.7) removed these bytes from the
        // front of every frame; the decoder needs them back.
        chunk.data.reserve(m_frame_prefix.size() + frame.size);
        chunk.data.assign(m_frame_prefix.begin(), m_frame_prefix.end());
        chunk.data.insert(chunk.data.end(), frame.data, frame.data + frame.size);
        // The block's time is its first frame's (RFC 9559 10.3.5). The others
        // follow on, each a DefaultDuration later when the track states one;
        // otherwise all a lace can say is where it starts.
        const uint64_t frame_ms = milliseconds + (m_default_duration_ns * i) / 1000000ULL;
        chunk.timestamp_samples = m_sample_rate > 0
                                ? (frame_ms * m_sample_rate) / 1000
                                : 0;
        chunk.is_keyframe = header.keyframe;
        chunk.file_offset = block.header_offset;
        if (i == 0) {
            chunk.padding_head_frames = before_zero + (discard_padding_ns < 0 ? padding_frames : 0);
        }
        if (discard_padding_ns > 0 && i + 1 == frames.size()) {
            chunk.padding_tail_frames = padding_frames;
        }
        m_queue.push_back(std::move(chunk));
    }
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
            // A child that claims to run past its cluster is damage. Reading
            // it anyway pulled the walk back inside what had just been read,
            // so a file of such clusters could keep the decoder busy for
            // hours; the rest of the cluster is skipped instead.
            if (!child.unknown_size && child.end() > m_cluster_end) {
                Debug::log("demux", "MatroskaDemuxer: element at ", child.header_offset,
                           " overruns its cluster");
                m_read_offset = m_cluster_end;
                m_cluster_end = 0;
                continue;
            }

            const uint64_t next = child.unknown_size ? m_cluster_end : child.end();
            if (child.id == Id::Timestamp) {
                // A uinteger; beyond int64 it is held at the limit rather than
                // wrapped by the conversion.
                const uint64_t timestamp = m_reader.readUInt(child);
                m_cluster_ticks = timestamp > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
                                ? std::numeric_limits<int64_t>::max()
                                : static_cast<int64_t>(timestamp);
            } else if (child.id == Id::SimpleBlock) {
                takeBlock(child, m_cluster_ticks, 0);
            } else if (child.id == Id::BlockGroup && !child.unknown_size) {
                // DiscardPadding can come after the Block it applies to, so
                // the whole group is read before the Block is queued. A group
                // holds one Block (RFC 9559 5.1.3.5.1); any further one is
                // ignored rather than given the first one's padding.
                const uint64_t group_end = child.end();
                uint64_t cursor = child.data_offset;
                EBMLElement block;
                bool have_block = false;
                int64_t discard_padding_ns = 0;
                while (cursor < group_end) {
                    m_reader.seek(cursor);
                    EBMLElement inner;
                    if (!m_reader.readElementHeader(inner) || inner.unknown_size
                        || inner.end() > group_end) {
                        break; // damage: nothing past here belongs to the group
                    }
                    if (inner.id == Id::Block && !have_block) {
                        block = inner;
                        have_block = true;
                    } else if (inner.id == Id::DiscardPadding) {
                        discard_padding_ns = m_reader.readInt(inner);
                    }
                    cursor = inner.end();
                }
                if (have_block) {
                    takeBlock(block, m_cluster_ticks, discard_padding_ns);
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
        m_granule_samples = chunk.timestamp_samples;
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

    // Where to start reading. A seek has to land at or before the target:
    // DemuxedStream decodes forward from the landing and drops everything
    // before the target, whereas landing after it would skip audio outright.
    // So when the index has nothing at or before the target -- the target is
    // earlier than the first cue, or there is no usable index at all (damaged
    // Cues, or live-muxed clusters of unknown size) -- reading restarts at the
    // first cluster. That is slow for a late target in a long unindexed file,
    // but it is correct, where refusing left the stream to carry on from
    // wherever it was.
    uint64_t start = 0;
    const CueEntry* entry = m_index.entryFor(ticks);
    if (entry && entry->time_ticks <= ticks) {
        start = entry->cluster_offset;
    } else {
        start = m_parser.firstClusterOffset();
    }
    if (start == 0 || start >= m_file_size) {
        return false;
    }

    m_queue.clear();
    m_read_offset = start;
    m_cluster_end = 0;
    m_cluster_ticks = 0;
    m_frames_before_zero = 0;
    m_eof = false;

    // What the landing actually is: the time of the first frame that will be
    // handed out, because DemuxedStream labels frames from a counter anchored
    // on the value reported here. That is neither a Cues entry's CueTime --
    // the time of a seek point (RFC 9559 5.1.5.1.1) whose Block the cluster
    // holds (5.1.5.1.2.2), not necessarily its first -- nor the Cluster's own
    // Timestamp, since a block's time is that Timestamp plus its own signed
    // offset (RFC 9559 11.2), and in mkvmerge files a cluster's first audio
    // block commonly sits behind a video frame, up to about 200 ms in. Reading
    // ahead to that frame settles it; the frame stays queued for readChunk.
    uint64_t landing_samples = 0;
    uint64_t landing_ms = 0;
    if (fillQueue()) {
        landing_samples = m_queue.front().timestamp_samples;
        landing_ms = m_sample_rate > 0 ? (landing_samples * 1000) / m_sample_rate : 0;
    } else {
        // Nothing left to play from there: the stream is at its end, and the
        // target is as good a statement of where as any.
        landing_ms = timestamp_ms;
        landing_samples = m_sample_rate > 0 ? (landing_ms * m_sample_rate) / 1000 : 0;
    }

    std::lock_guard<std::mutex> lock(m_state_mutex);
    m_position_ms = landing_ms;
    m_granule_samples = landing_samples;
    return true;
}

uint64_t MatroskaDemuxer::getGranulePosition(uint32_t stream_id) const
{
    // Only the selected track is ever queued, so its position is the stream's.
    (void)stream_id;
    std::lock_guard<std::mutex> lock(m_state_mutex);
    return m_granule_samples;
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
