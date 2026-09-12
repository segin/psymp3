/*
 * SegmentParser.h - Matroska Segment header parsing: Info and Tracks.
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

#ifndef MATROSKASEGMENTPARSER_H
#define MATROSKASEGMENTPARSER_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Demuxer {
namespace Matroska {

/// Everything the Segment's Info element says about timing.
struct SegmentInfo {
    /// Nanoseconds per timestamp tick. Every timestamp in the file -- cluster
    /// times, cue times, block offsets -- is in these, so nothing can be turned
    /// into milliseconds without it. Matroska's default is 1,000,000, which
    /// makes a tick a millisecond, and essentially every muxer writes exactly
    /// that; the field is still read rather than assumed, because a file that
    /// sets it differently and is read as if it had not is wrong by whatever
    /// ratio it chose.
    uint64_t timestamp_scale_ns = 1000000;
    /// Duration in ticks, as written: a float, which is unusual enough to be
    /// worth stating. Zero when absent.
    double duration_ticks = 0.0;
    std::string muxing_app;
    std::string writing_app;
    std::string title;

    /// Duration in milliseconds, or 0 when the file does not say.
    uint64_t durationMs() const;
};

/// One TrackEntry, as written. Kept close to the file rather than folded
/// straight into StreamInfo so that the Matroska-shaped facts -- lacing
/// permission, track type, the raw CodecID -- survive for the block parser and
/// for diagnostics.
struct TrackEntry {
    uint64_t number = 0;          ///< what a block's track number refers to
    uint64_t uid = 0;
    uint64_t type = 0;            ///< see TrackType
    std::string codec_id;         ///< e.g. "A_OPUS"
    std::string codec_name;       ///< the encoder's own description, if given
    std::string language;         ///< ISO 639-2, or BCP 47 when the file uses it
    std::string name;
    std::vector<uint8_t> codec_private;
    bool lacing_allowed = true;   ///< FlagLacing; blocks may pack several frames
    bool default_track = false;
    bool enabled = true;
    uint64_t default_duration_ns = 0;
    /// Nanoseconds of decoder startup to discard. Opus in Matroska states its
    /// pre-skip here rather than only in OpusHead.
    uint64_t codec_delay_ns = 0;
    uint64_t seek_preroll_ns = 0;

    // Audio sub-element.
    double sampling_frequency = 0.0;
    /// Non-zero when the decoder outputs at a different rate than the stream is
    /// coded at, which is how SBR is signalled: an HE-AAC track is coded at
    /// half the rate it plays at.
    double output_sampling_frequency = 0.0;
    uint16_t channels = 0;
    uint16_t bit_depth = 0;

    bool isAudio() const { return type == TrackType::Audio; }
};

/// Parses the EBML header and a Segment's Info and Tracks.
///
/// Stops at the first Cluster: everything this produces comes from the
/// Segment's header elements, and a file may put hundreds of megabytes of
/// clusters between them and anything else. Where the header elements sit
/// after the clusters -- which SeekHead exists to describe -- is the parser's
/// problem to chase, not the caller's.
class SegmentParser {
public:
    /// Reads from the current position, which must be the start of the file.
    /// Throws when the stream is not Matroska or its header is unusable.
    void parse(EBMLReader& reader);

    const SegmentInfo& info() const { return m_info; }
    const std::vector<TrackEntry>& tracks() const { return m_tracks; }
    /// DocType from the EBML header: "matroska" or "webm".
    const std::string& docType() const { return m_doc_type; }
    /// Offset of the Segment's payload. Cluster and cue positions are all
    /// relative to it, so nothing can be seeked to without it.
    uint64_t segmentDataOffset() const { return m_segment_data_offset; }
    /// Offset of the first Cluster, where parsing stopped.
    uint64_t firstClusterOffset() const { return m_first_cluster_offset; }

    /// Absolute file offset of the element @p id, from SeekHead, or 0 when the
    /// file's SeekHead does not mention it.
    ///
    /// SeekHead is the index of the Segment's own top-level elements. It
    /// matters because Cues are written after the clusters -- often at the very
    /// end of a large file -- and because a muxer is allowed to put Tracks
    /// there too, in which case a parse that stops at the first Cluster finds
    /// no tracks at all without consulting it.
    uint64_t seekPosition(uint32_t id) const;

    /// The first audio track PsyMP3 can decode, or nullptr.
    ///
    /// A .mkv is usually mostly video, and may carry several audio tracks; this
    /// picks the one to play. A track flagged default wins over one that is
    /// not, and otherwise the first in the file wins.
    const TrackEntry* preferredAudioTrack() const;

    /// Fills a StreamInfo for @p track, mapping Matroska's names and units onto
    /// the ones the rest of PsyMP3 uses.
    StreamInfo toStreamInfo(const TrackEntry& track) const;

private:
    void parseEBMLHeader(EBMLReader& reader);
    void parseSeekHead(EBMLReader& reader, const EBMLElement& seek_head);
    void parseSegment(EBMLReader& reader, const EBMLElement& segment);
    void parseInfo(EBMLReader& reader, const EBMLElement& info);
    void parseTracks(EBMLReader& reader, const EBMLElement& tracks);
    TrackEntry parseTrackEntry(EBMLReader& reader, const EBMLElement& entry);
    void parseAudio(EBMLReader& reader, const EBMLElement& audio, TrackEntry& track);

    SegmentInfo m_info;
    std::vector<TrackEntry> m_tracks;
    std::string m_doc_type;
    /// Element ID to absolute file offset, from SeekHead.
    std::map<uint32_t, uint64_t> m_seek_positions;
    uint64_t m_segment_data_offset = 0;
    uint64_t m_first_cluster_offset = 0;
};

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3

#endif // MATROSKASEGMENTPARSER_H
