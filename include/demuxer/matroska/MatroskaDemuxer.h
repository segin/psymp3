/*
 * MatroskaDemuxer.h - Matroska/WebM container demuxer.
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

#ifndef MATROSKADEMUXER_H
#define MATROSKADEMUXER_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Demuxer {
namespace Matroska {

/// Plays the audio out of a .mka, .mkv or .webm.
///
/// Everything structural is done by the pieces underneath -- EBMLReader for the
/// element grammar, SegmentParser for Info and Tracks, BlockParser for frames
/// and lacing, CueIndex for seeking. What is left here is the walk: stepping
/// through clusters, handing out one frame at a time, and keeping enough state
/// to resume after a seek.
class MatroskaDemuxer : public Demuxer {
public:
    explicit MatroskaDemuxer(std::unique_ptr<PsyMP3::IO::IOHandler> handler);

    /// WebM is Matroska with a restricted DocType, and a listener who
    /// picked a .webm expects to be told so.
    std::string getContainerName() const override {
        return m_parser.docType() == "webm" ? "WebM" : "Matroska";
    }

    bool parseContainer() override;
    std::vector<StreamInfo> getStreams() const override;
    StreamInfo getStreamInfo(uint32_t stream_id) const override;
    MediaChunk readChunk() override;
    MediaChunk readChunk(uint32_t stream_id) override;
    bool seekTo(uint64_t timestamp_ms) override;
    bool isEOF() const override;
    uint64_t getDuration() const override;
    uint64_t getPosition() const override;

private:
    /// Reads forward until at least one frame for the selected track is
    /// queued, or the file ends. Laced blocks yield several at once, which is
    /// why the queue exists rather than a single pending chunk.
    bool fillQueue();
    /// Handles one SimpleBlock or the Block inside a BlockGroup.
    void takeBlock(const EBMLElement& block, int64_t cluster_ticks,
                   uint64_t discard_padding_ns);
    /// Matroska Tags into the Tag framework.
    void parseTags(uint64_t tags_offset);
    /// Ticks in the Segment's TimestampScale to milliseconds.
    uint64_t ticksToMs(int64_t ticks) const;

    EBMLReader m_reader;
    SegmentParser m_parser;
    CueIndex m_index;

    /// The track being played. Zero until parseContainer has chosen one.
    uint64_t m_track_number = 0;
    uint32_t m_sample_rate = 0;
    uint64_t m_file_size = 0;

    /// Where the next element will be read from. Kept explicitly rather than
    /// relying on the handler's position, because seeking and tag parsing both
    /// move it out from under the walk.
    uint64_t m_read_offset = 0;
    /// End of the cluster being walked, or 0 when between clusters.
    uint64_t m_cluster_end = 0;
    int64_t m_cluster_ticks = 0;

    std::deque<MediaChunk> m_queue;
    bool m_eof = false;
};

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3

#endif // MATROSKADEMUXER_H
