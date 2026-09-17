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
        // Audio exists but none of it can be played: no decoder for it, or
        // the track is disabled, encoded in a way this cannot undo, or an
        // Opus track without a usable OpusHead. The streams stay listed for
        // Media Information, and where the first one names no codec the
        // failure is reported when a codec is asked for. But a disabled track
        // can name a codec that exists, and that codec would then wait for
        // chunks that never come; the stream is ended here instead.
        Debug::log("demux", "MatroskaDemuxer: no playable audio track; first is ",
                   m_streams.front().codec_name.empty() ? "undecodable" : "disabled or unusable");
        m_eof = true;
    } else {
        m_track_number = chosen->number;
        m_stream_id = chosen->ordinal;
        m_sample_rate = m_streams.front().sample_rate;
        m_codec_delay_frames = m_streams.front().codec_delay;
        m_frame_prefix = chosen->stripped_frame_prefix;
        m_track_timestamp_scale = chosen->timestamp_scale;
        // No real frame lasts a second; anything longer would only overflow
        // the lace arithmetic in takeBlock.
        constexpr uint64_t kMaxFrameNs = 1000000000ULL;
        m_default_duration_ns = chosen->default_duration_ns <= kMaxFrameNs
                              ? chosen->default_duration_ns : 0;
        // SeekPreRoll's default is 0, but an Opus decoder needs 80 ms to
        // converge whatever the file says (RFC 7845 4.6). A value over 10 s,
        // like CodecDelay's, is damage, and would turn every seek into a
        // decode from far back.
        constexpr uint64_t kMaxPrerollNs = 10ULL * 1000000000ULL;
        constexpr uint64_t kOpusPrerollNs = 80ULL * 1000000ULL;
        m_seek_preroll_ns = chosen->seek_preroll_ns <= kMaxPrerollNs ? chosen->seek_preroll_ns : 0;
        if (chosen->codec_id == "A_OPUS") {
            m_seek_preroll_ns = std::max(m_seek_preroll_ns, kOpusPrerollNs);
        }
    }

    m_duration_ms = m_parser.info().durationMs();
    if (m_track_number != 0) {
        findOrigin();
    }

    // The seek index is built at the first seek (buildIndex), as RFC 9559
    // 23.1 advises: without usable Cues it means reading every cluster header
    // in the file, which over HTTP is a request per cluster before the first
    // sample plays.

    const uint64_t tags_at = m_parser.seekPosition(Id::Tags);
    if (tags_at != 0) {
        parseTags(tags_at, chosen ? chosen->uid : 0);
    }

    m_read_offset = m_parser.firstClusterOffset();
    m_cluster_end = 0;
    m_eof = m_eof || m_read_offset == 0 || m_read_offset >= m_file_size;
    m_parsed = true;
    return true;
}

void MatroskaDemuxer::parseTags(uint64_t tags_offset, uint64_t track_uid)
{
    // Every SimpleTag that applies to the track, with the level it was
    // written at. An empty value is kept: at a lower level it says that an
    // upper level's value does not apply (RFC 9559 24.2).
    struct Record {
        std::string name;
        std::string value;
        uint64_t level;
        bool is_default;
    };
    std::vector<Record> records;
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

            // TargetTypeValue says what a name describes: a TITLE at 50 is the
            // album's, at 30 the track's. The UIDs say which track, chapter,
            // edition or attachment the Tag is about.
            uint64_t target = 50;
            std::vector<uint64_t> track_uids;
            bool elsewhere = false;
            std::vector<Record> simple;
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
                        if (field.id == Id::TargetTypeValue) {
                            // Empty means the schema default, 50 (RFC 8794 6.1).
                            target = field.size == 0 ? 50 : m_reader.readUInt(field);
                        } else if (field.id == Id::TagTrackUID) {
                            track_uids.push_back(m_reader.readUInt(field));
                        } else if (field.id == Id::TagEditionUID || field.id == Id::TagChapterUID
                                   || field.id == Id::TagAttachmentUID) {
                            elsewhere = elsewhere || m_reader.readUInt(field) != 0;
                        }
                        m_reader.seek(field.end());
                    }
                } else if (child.id == Id::SimpleTag) {
                    std::string name;
                    std::string value;
                    bool is_default = true;
                    const uint64_t simple_end = child.end();
                    while (m_reader.tell() < simple_end) {
                        EBMLElement field;
                        if (!m_reader.readElementHeader(field)) {
                            break;
                        }
                        if (field.id == Id::TagName) {
                            name = m_reader.readUTF8(field);
                        } else if (field.id == Id::TagString) {
                            value = m_reader.readUTF8(field);
                        } else if (field.id == Id::TagDefault) {
                            is_default = field.size == 0 || m_reader.readUInt(field) != 0;
                        }
                        m_reader.seek(field.end());
                    }
                    if (!name.empty()) {
                        simple.push_back({name, value, 0, is_default});
                    }
                }
                m_reader.seek(child.end());
            }

            // A Tag about a chapter, an edition or an attachment says nothing
            // about the track as a whole, and one aimed at other tracks says
            // nothing about this one. A TagTrackUID of 0 means every track.
            const bool for_this_track =
                track_uids.empty()
                || std::find(track_uids.begin(), track_uids.end(), 0) != track_uids.end()
                || (track_uid != 0
                    && std::find(track_uids.begin(), track_uids.end(), track_uid) != track_uids.end());
            if (!elsewhere && for_this_track) {
                for (Record& record : simple) {
                    record.level = target;
                    records.push_back(std::move(record));
                }
            }
            m_reader.seek(tag_end);
        }
    } catch (const std::exception& e) {
        Debug::log("demux", "MatroskaDemuxer: tags unreadable: ", e.what());
        return;
    }

    // Levels nest (RFC 9559 24.2): a value applies to the levels below it
    // unless one of them states its own, which replaces it, and an empty
    // value there says it does not apply. So a name takes the values of the
    // lowest level in [lowest, highest] that states it. Several values at
    // that level stay separate -- a release with several artists writes
    // several ARTIST tags -- unless some are marked as the default language,
    // in which case only those count.
    const auto resolve = [&records](const std::string& name, uint64_t lowest, uint64_t highest) {
        uint64_t best = std::numeric_limits<uint64_t>::max();
        bool any_default = false;
        for (const Record& record : records) {
            if (record.name == name && record.level >= lowest && record.level <= highest
                && record.level < best) {
                best = record.level;
            }
        }
        for (const Record& record : records) {
            any_default = any_default || (record.name == name && record.level == best && record.is_default);
        }
        std::vector<std::string> values;
        for (const Record& record : records) {
            if (record.name == name && record.level == best && !record.value.empty()
                && (record.is_default || !any_default)
                && std::find(values.begin(), values.end(), record.value) == values.end()) {
                values.push_back(record.value);
            }
        }
        return values;
    };

    // The track is level 30. Its TITLE comes from that level or the part
    // above it, never from 50, where TITLE is the album's name -- which the
    // rest of PsyMP3 calls ALBUM. The album's ARTIST is its ALBUMARTIST, and
    // passes down to ARTIST where the track names none.
    constexpr uint64_t kTrackLevel = 30;
    constexpr uint64_t kAlbumLevel = 50;
    std::map<std::string, std::vector<std::string>> fields;
    for (const Record& record : records) {
        if (record.name == "TITLE" || fields.count(record.name)) {
            continue;
        }
        std::vector<std::string> values =
            resolve(record.name, kTrackLevel, std::numeric_limits<uint64_t>::max());
        if (!values.empty()) {
            fields[record.name] = std::move(values);
        }
    }
    std::vector<std::string> title = resolve("TITLE", kTrackLevel, kAlbumLevel - 1);
    if (!title.empty()) {
        fields["TITLE"] = std::move(title);
    }
    std::vector<std::string> album = resolve("TITLE", kAlbumLevel, kAlbumLevel);
    if (!album.empty()) {
        fields["ALBUM"] = std::move(album);
    }
    if (!fields.count("ALBUMARTIST")) {
        std::vector<std::string> album_artist = resolve("ARTIST", kAlbumLevel, kAlbumLevel);
        if (!album_artist.empty()) {
            fields["ALBUMARTIST"] = std::move(album_artist);
        }
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
    // A Matroska v1-3 track can scale the block's offset (RFC 9559 11.2).
    const int64_t offset = m_track_timestamp_scale == 1.0
                         ? header.timestamp_offset
                         : std::llround(header.timestamp_offset * m_track_timestamp_scale);
    if ((offset > 0 && cluster_ticks > std::numeric_limits<int64_t>::max() - offset) ||
        (offset < 0 && cluster_ticks < std::numeric_limits<int64_t>::min() - offset)) {
        Debug::log("demux", "MatroskaDemuxer: block time out of range at ", block.header_offset);
        return;
    }
    // Times are handed out relative to the track's first block (findOrigin).
    const int64_t absolute_ticks = cluster_ticks + offset;
    if (absolute_ticks < std::numeric_limits<int64_t>::min() + m_origin_ticks) {
        return;
    }
    const int64_t ticks = absolute_ticks - m_origin_ticks;
    const uint64_t milliseconds = ticksToMs(ticks);

    // A frame whose time is negative is decoded but not played (RFC 9559
    // 11.2). Blocks follow on from each other, so what lies before time 0 is
    // the earliest negative block's distance from it, and a later negative
    // block is already inside that span. The difference rides on the chunk as
    // leading padding.
    uint32_t before_zero = 0;
    if (ticks < 0 && m_sample_rate > 0) {
        constexpr uint64_t kMaxLeadNs = 10ULL * 1000000000ULL;
        const uint64_t scale = m_parser.info().timestamp_scale_ns;
        const uint64_t span = static_cast<uint64_t>(-(ticks + 1)) + 1;
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
        chunk.stream_id = m_stream_id;
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

void MatroskaDemuxer::findOrigin()
{
    // A file whose first block is not at time 0 -- the second part of a split
    // recording, say -- is played from its first block, and DemuxedStream
    // counts positions from there. Cluster, block and cue times are absolute,
    // so the first block of this track is what relates the two. Time before 0
    // is not played (RFC 9559 11.2), so the origin is never below it. Only the
    // first few clusters are looked at: a track that starts later than that
    // is taken to start at 0.
    constexpr int kMaxClusters = 64;
    m_origin_ticks = 0;
    try {
        uint64_t offset = m_parser.firstClusterOffset();
        for (int n = 0; n < kMaxClusters && offset != 0 && offset < m_file_size; ++n) {
            m_reader.seek(offset);
            EBMLElement cluster;
            if (!m_reader.readElementHeader(cluster) || cluster.unknown_size) {
                return;
            }
            offset = cluster.end();
            if (cluster.id != Id::Cluster) {
                continue;
            }
            const uint64_t end = cluster.end();
            const int64_t cluster_ticks = clusterTimestamp(cluster.data_offset, end);
            uint64_t at = cluster.data_offset;
            while (at < end) {
                m_reader.seek(at);
                EBMLElement child;
                if (!m_reader.readElementHeader(child) || child.unknown_size) {
                    break;
                }
                at = child.end();
                EBMLElement block = child;
                if (child.id == Id::BlockGroup) {
                    bool found = false;
                    while (m_reader.tell() < child.end()) {
                        EBMLElement inner;
                        if (!m_reader.readElementHeader(inner) || inner.unknown_size) {
                            break;
                        }
                        if (inner.id == Id::Block) {
                            block = inner;
                            found = true;
                            break;
                        }
                        m_reader.seek(inner.end());
                    }
                    if (!found) {
                        continue;
                    }
                } else if (child.id != Id::SimpleBlock) {
                    continue;
                }
                // The track number and the 16-bit relative time lead the
                // payload; nothing more is read.
                uint8_t lead[10] = {};
                const size_t want = static_cast<size_t>(std::min<uint64_t>(block.size, sizeof(lead)));
                m_reader.seek(block.data_offset);
                if (m_handler->read(lead, 1, want) != want) {
                    return;
                }
                uint64_t track = 0;
                const size_t used = EBMLReader::decodeVInt(lead, want, track, /*keep_marker=*/false);
                if (used == 0 || used + 2 > want || track != m_track_number) {
                    continue;
                }
                const auto relative = static_cast<int16_t>((lead[used] << 8) | lead[used + 1]);
                const int64_t offset_ticks = m_track_timestamp_scale == 1.0
                                           ? relative
                                           : std::llround(relative * m_track_timestamp_scale);
                if (offset_ticks > 0 && cluster_ticks > std::numeric_limits<int64_t>::max() - offset_ticks) {
                    m_origin_ticks = std::numeric_limits<int64_t>::max();
                } else {
                    m_origin_ticks = std::max<int64_t>(0, cluster_ticks + offset_ticks);
                }
                return;
            }
        }
    } catch (const std::exception& e) {
        Debug::log("demux", "MatroskaDemuxer: could not find where the track starts: ", e.what());
        m_origin_ticks = 0;
    }
}

void MatroskaDemuxer::buildIndex()
{
    m_index_built = true;
    // Cues are found through SeekHead. They usually follow the clusters, and
    // although RFC 9559 6.4 lets them come first instead, the Segment walk
    // does not collect them there. Without Cues, the cluster headers are
    // walked instead.
    //
    // A damaged index costs fast seeking, not playback: with no index, a seek
    // restarts at the first cluster and decodes forward. Cues are only
    // SHOULD-be-present (5.1.5) and the clusters behind them are intact, but
    // both builders throw on a malformed VINT or an integer wider than eight
    // octets. That is logged, and the stream carries on unindexed.
    try {
        uint64_t cues_at = m_parser.seekPosition(Id::Cues);
        if (cues_at >= m_file_size) {
            cues_at = 0; // a truncated file, or a SeekHead that is wrong
        }
        if (!m_index.parseCues(m_reader, cues_at, m_parser.segmentDataOffset(),
                               m_track_number)) {
            m_index.buildByScanning(m_reader, m_parser.firstClusterOffset(), m_file_size);
        }
    } catch (const std::exception& e) {
        Debug::log("demux", "MatroskaDemuxer: index unusable, seeks will restart at the first cluster: ",
                   e.what());
        m_index = CueIndex();
    }
}

int64_t MatroskaDemuxer::clusterTimestamp(uint64_t from, uint64_t end)
{
    // RFC 9559 5.1.3.1 has the Timestamp come first, or after a CRC-32, but
    // only as a SHOULD: a cluster may state it after its blocks. The blocks
    // ahead of it still need it, so it is found before any of them is read,
    // by walking the child headers and never their data. A nested Cluster
    // ends a cluster of unknown size.
    m_reader.seek(from);
    while (m_reader.tell() < end) {
        EBMLElement child;
        if (!m_reader.readElementHeader(child) || child.id == Id::Cluster || child.unknown_size) {
            break;
        }
        if (child.id == Id::Timestamp) {
            // A uinteger; beyond int64 it is held at the limit rather than
            // wrapped by the conversion.
            const uint64_t timestamp = m_reader.readUInt(child);
            return timestamp > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
                 ? std::numeric_limits<int64_t>::max()
                 : static_cast<int64_t>(timestamp);
        }
        m_reader.seek(child.end());
    }
    return 0;
}

uint64_t MatroskaDemuxer::findClusterAfter(uint64_t from)
{
    // The Cluster ID is four bytes precisely so a reader can find its way
    // back after damage (RFC 8794 17.1). A chance match is ruled out by reading
    // the header and the first child, which must be the Timestamp, or a
    // CRC-32 and then the Timestamp.
    constexpr uint64_t kWindow = 16ULL * 1024 * 1024;
    constexpr size_t kPiece = 64 * 1024;
    constexpr uint8_t kClusterId[4] = {0x1F, 0x43, 0xB6, 0x75};
    const uint64_t limit = std::min<uint64_t>(m_file_size, from + kWindow);
    std::vector<uint8_t> buffer(kPiece + 3);
    for (uint64_t at = from; at + 4 <= limit; at += kPiece) {
        const size_t want = static_cast<size_t>(std::min<uint64_t>(kPiece + 3, limit - at));
        size_t got = 0;
        try {
            m_reader.seek(at);
            got = m_handler->read(buffer.data(), 1, want);
        } catch (const std::exception&) {
            return 0;
        }
        for (size_t i = 0; i + 4 <= got; ++i) {
            if (std::memcmp(&buffer[i], kClusterId, 4) != 0) {
                continue;
            }
            const uint64_t candidate = at + i;
            try {
                m_reader.seek(candidate);
                EBMLElement cluster;
                EBMLElement child;
                if (!m_reader.readElementHeader(cluster) || cluster.id != Id::Cluster
                    || !m_reader.readElementHeader(child)) {
                    continue;
                }
                if (child.id == Id::CRC32 && !child.unknown_size) {
                    m_reader.seek(child.end());
                    if (!m_reader.readElementHeader(child)) {
                        continue;
                    }
                }
                if (child.id == Id::Timestamp) {
                    return candidate;
                }
            } catch (const std::exception&) {
                // Not a Cluster after all; keep looking.
            }
        }
        if (got < want) {
            break;
        }
    }
    return 0;
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
                m_cluster_ticks = clusterTimestamp(element.data_offset, m_cluster_end);
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
            // An ID EBML forbids is damage, and the size after it cannot be
            // trusted; the rest of the cluster goes with it.
            if (child.invalid_id) {
                Debug::log("demux", "MatroskaDemuxer: invalid element ID at ", child.header_offset);
                m_read_offset = m_cluster_end;
                m_cluster_end = 0;
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
            // Damage need not end the file: playback picks up at the next
            // Cluster, if one follows within reach.
            const uint64_t resume = findClusterAfter(m_read_offset + 1);
            Debug::log("demux", "MatroskaDemuxer: ", e.what(),
                       resume != 0 ? "; resuming at the next cluster" : "; nothing follows");
            if (resume == 0) {
                m_eof = true;
                break;
            }
            m_read_offset = resume;
            m_cluster_end = 0;
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
    if (stream_id != m_stream_id) {
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
    // The target counts from the track's first block; the index is absolute.
    const uint64_t relative = scale > 0 ? (timestamp_ms * 1000000ULL) / scale : timestamp_ms;
    const uint64_t origin = static_cast<uint64_t>(m_origin_ticks);
    const uint64_t ticks = relative > std::numeric_limits<uint64_t>::max() - origin
                         ? std::numeric_limits<uint64_t>::max()
                         : origin + relative;

    // Where to start reading. A seek has to land at or before the target:
    // DemuxedStream decodes forward from the landing and drops everything
    // before the target, whereas landing after it would skip audio outright.
    // So when the index has nothing at or before the target -- the target is
    // earlier than the first cue, or there is no usable index at all (damaged
    // Cues, or live-muxed clusters of unknown size) -- reading restarts at the
    // first cluster. That is slow for a late target in a long unindexed file,
    // but it is correct, where refusing left the stream to carry on from
    // wherever it was.
    //
    // The decoder has to have run for SeekPreRoll before its output is valid
    // (RFC 9559 5.1.4.1.26), so the restart is looked up that much before the
    // target; DemuxedStream drops everything before the target either way.
    if (!m_index_built) {
        buildIndex();
    }
    const uint64_t preroll_ticks = scale > 0 ? m_seek_preroll_ns / scale : 0;
    uint64_t lookup = ticks > preroll_ticks ? ticks - preroll_ticks : 0;
    const uint64_t target_samples = m_sample_rate > 0 ? (timestamp_ms * m_sample_rate) / 1000 : 0;

    // A cluster's first frame of this track can come after its Timestamp, and
    // after a cue's time, since audio is interleaved behind video. When it
    // comes after the target too, the audio from the target on starts in an
    // earlier cluster, so the lookup steps back an entry and tries again.
    for (size_t attempt = 0;; ++attempt) {
        uint64_t start = 0;
        const CueEntry* entry = m_index.entryFor(lookup);
        if (entry && entry->time_ticks <= lookup) {
            start = entry->cluster_offset;
        } else {
            entry = nullptr;
            start = m_parser.firstClusterOffset();
        }
        // A Cues position is only a claim. One that is stale or damaged, and
        // does not lead to a Cluster, would end playback at the seek; reading
        // restarts at the first cluster instead.
        if (start != m_parser.firstClusterOffset()) {
            bool at_cluster = false;
            try {
                EBMLElement element;
                m_reader.seek(start);
                at_cluster = start < m_file_size && m_reader.readElementHeader(element)
                          && element.id == Id::Cluster;
            } catch (const std::exception&) {
                at_cluster = false;
            }
            if (!at_cluster) {
                Debug::log("demux", "MatroskaDemuxer: no Cluster at cued offset ", start);
                entry = nullptr;
                start = m_parser.firstClusterOffset();
            }
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

        // What the landing actually is: the time of the first frame that will
        // be handed out, because DemuxedStream labels frames from a counter
        // anchored on the value reported here. That is neither a Cues entry's
        // CueTime -- the time of a seek point (RFC 9559 5.1.5.1.1) whose Block
        // the cluster holds (5.1.5.1.2.2), not necessarily its first -- nor the
        // Cluster's own Timestamp, since a block's time is that Timestamp plus
        // its own signed offset (RFC 9559 11.2), and in mkvmerge files a
        // cluster's first audio block commonly sits behind a video frame, up to
        // about 200 ms in. Reading ahead to that frame settles it; the frame
        // stays queued for readChunk.
        uint64_t landing_samples = 0;
        uint64_t landing_ms = 0;
        if (fillQueue()) {
            landing_samples = m_queue.front().timestamp_samples;
            landing_ms = m_sample_rate > 0 ? (landing_samples * 1000) / m_sample_rate : 0;
        } else {
            // Nothing left to play from there: the stream is at its end, and
            // the target is as good a statement of where as any.
            landing_ms = timestamp_ms;
            landing_samples = target_samples;
        }

        // The landing plays CodecDelay earlier than its block time says.
        if (landing_samples > target_samples + m_codec_delay_frames && entry && entry->time_ticks > 0
            && attempt < m_index.size()) {
            lookup = entry->time_ticks - 1;
            continue;
        }

        std::lock_guard<std::mutex> lock(m_state_mutex);
        m_position_ms = landing_ms;
        m_granule_samples = landing_samples;
        return true;
    }
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
