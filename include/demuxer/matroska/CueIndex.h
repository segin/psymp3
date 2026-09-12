/*
 * CueIndex.h - Matroska seek index: Cues, or a scan when a file has none.
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

#ifndef MATROSKACUEINDEX_H
#define MATROSKACUEINDEX_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Demuxer {
namespace Matroska {

/// Where to start reading to reach a given point in time.
struct CueEntry {
    /// Timestamp in ticks, in the Segment's TimestampScale.
    uint64_t time_ticks = 0;
    /// Absolute file offset of the Cluster to start at.
    uint64_t cluster_offset = 0;
};

/// The seek index for one audio track.
///
/// Matroska stores this as a Cues element, which is optional. Plenty of files
/// ship without one -- a muxer streaming to a socket cannot write it, and
/// plenty of audio-only .mka files simply do not bother -- so a scan of the
/// cluster headers stands in for it. Either way the result is the same shape:
/// timestamps in ticks against the cluster to start from, in ascending order.
class CueIndex {
public:
    /// Reads a Cues element that @p cues_offset points at.
    ///
    /// Entries for other tracks are ignored: a .mkv cues its video and its
    /// audio separately, and starting audio playback from a video keyframe's
    /// cluster is at best approximate.
    ///
    /// @param segment_data_offset  what CueClusterPosition is relative to
    /// @return false when the element is not Cues or yields no usable entry
    bool parseCues(EBMLReader& reader, uint64_t cues_offset,
                   uint64_t segment_data_offset, uint64_t track_number);

    /// Walks cluster headers from @p first_cluster to the end of the file,
    /// recording each cluster's timestamp and offset.
    ///
    /// Only the headers: each Cluster's size says where the next one begins, so
    /// this seeks between them rather than reading any block. That makes it
    /// proportional to the number of clusters -- a few thousand for a long
    /// file -- rather than to its size.
    bool buildByScanning(EBMLReader& reader, uint64_t first_cluster, uint64_t file_size);

    /// The entry to start decoding from to reach @p time_ticks: the last one at
    /// or before it, since a seek must land on or before its target and decode
    /// forward. Null when the index is empty.
    const CueEntry* entryFor(uint64_t time_ticks) const;

    bool empty() const { return m_entries.empty(); }
    std::size_t size() const { return m_entries.size(); }
    const std::vector<CueEntry>& entries() const { return m_entries; }

private:
    void sortAndDedupe();

    std::vector<CueEntry> m_entries;
};

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3

#endif // MATROSKACUEINDEX_H
