/*
 * CueIndex.cpp - Matroska seek index: Cues, or a scan when a file has none.
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

void CueIndex::sortAndDedupe()
{
    // Cues are conventionally written in order but nothing requires it, and a
    // lookup by binary search needs them sorted whatever the file did. Where
    // two entries share a time, the earlier cluster is the one to keep:
    // starting at the later one would skip the audio of the first.
    std::sort(m_entries.begin(), m_entries.end(),
              [](const CueEntry& a, const CueEntry& b) {
                  return a.time_ticks != b.time_ticks ? a.time_ticks < b.time_ticks
                                                      : a.cluster_offset < b.cluster_offset;
              });
    m_entries.erase(std::unique(m_entries.begin(), m_entries.end(),
                                [](const CueEntry& a, const CueEntry& b) {
                                    return a.time_ticks == b.time_ticks;
                                }),
                    m_entries.end());
}

bool CueIndex::parseCues(EBMLReader& reader, uint64_t cues_offset,
                         uint64_t segment_data_offset, uint64_t track_number)
{
    m_entries.clear();
    if (cues_offset == 0) {
        return false;
    }

    reader.seek(cues_offset);
    EBMLElement cues;
    if (!reader.readElementHeader(cues) || cues.id != Id::Cues || cues.unknown_size) {
        return false;
    }

    std::vector<CueEntry> others;
    const uint64_t end = cues.end();
    while (reader.tell() < end) {
        EBMLElement point;
        if (!reader.readElementHeader(point)) {
            break;
        }
        if (point.id != Id::CuePoint || point.unknown_size) {
            if (point.unknown_size) {
                break;
            }
            reader.seek(point.end());
            continue;
        }

        uint64_t time_ticks = 0;
        bool have_time = false;
        uint64_t cluster_offset = 0;
        bool have_position = false;
        uint64_t other_offset = 0;
        bool have_other = false;

        const uint64_t point_end = point.end();
        while (reader.tell() < point_end) {
            EBMLElement child;
            if (!reader.readElementHeader(child)) {
                break;
            }
            if (child.id == Id::CueTime) {
                time_ticks = reader.readUInt(child);
                have_time = true;
            } else if (child.id == Id::CueTrackPositions) {
                uint64_t track = 0;
                uint64_t position = 0;
                bool position_seen = false;
                const uint64_t positions_end = child.end();
                while (reader.tell() < positions_end) {
                    EBMLElement field;
                    if (!reader.readElementHeader(field)) {
                        break;
                    }
                    if (field.id == Id::CueTrack) {
                        track = reader.readUInt(field);
                    } else if (field.id == Id::CueClusterPosition) {
                        position = reader.readUInt(field);
                        position_seen = true;
                    }
                    reader.seek(field.end());
                }
                // Each CueTrackPositions names one track's block, and only the
                // selected track's are kept. A file with video usually cues no
                // audio at all, which leaves the index empty and the file to
                // the cluster scan.
                if (position_seen && track == track_number) {
                    cluster_offset = segment_data_offset + position;
                    have_position = true;
                } else if (position_seen && !have_other) {
                    other_offset = segment_data_offset + position;
                    have_other = true;
                }
            }
            reader.seek(child.end());
        }

        if (have_time && have_position) {
            m_entries.push_back(CueEntry{time_ticks, cluster_offset});
        } else if (have_time && have_other) {
            others.push_back(CueEntry{time_ticks, other_offset});
        }
        reader.seek(point_end);
    }

    // A file with video usually cues only the video track. Any cluster is a
    // place the audio can restart from, and the seek works out where in it
    // the audio begins, so those cues serve too, where the alternative is
    // reading every cluster header in the file.
    if (m_entries.empty()) {
        m_entries = std::move(others);
    }
    sortAndDedupe();
    return !m_entries.empty();
}

bool CueIndex::buildByScanning(EBMLReader& reader, uint64_t first_cluster,
                               uint64_t file_size)
{
    m_entries.clear();
    if (first_cluster == 0 || first_cluster >= file_size) {
        return false;
    }

    // A read that fails part way -- a truncated file served over HTTP refuses a
    // seek past its end -- keeps the entries found up to there.
    try {
        reader.seek(first_cluster);
        while (reader.tell() < file_size) {
            EBMLElement cluster;
            if (!reader.readElementHeader(cluster)) {
                break;
            }
            if (cluster.id != Id::Cluster) {
                // Cues, Tags and Attachments legitimately sit among or after
                // the clusters; step over anything that is not one.
                if (cluster.unknown_size) {
                    break;
                }
                reader.seek(cluster.end());
                continue;
            }

            // A cluster of unknown size gives no way to find the next one short
            // of walking the header of every element inside it -- one read per
            // block rather than one per cluster, which is the cost this scan
            // avoids. Stop rather than fall back to that.
            if (cluster.unknown_size) {
                break;
            }

            // Timestamp is not necessarily the first child. RFC 9559 5.1.3.1
            // says it SHOULD be first, or second after a CRC-32, and ffmpeg
            // writes exactly that CRC-32 ahead of it; anything else in front is
            // stepped over too. Reading only the first child finds the CRC and
            // concludes the cluster has no timestamp, which leaves the whole
            // scan empty.
            //
            // The spec allows it after the blocks as well, so blocks are
            // stepped over by their sizes: their headers are read, never their
            // data. A cluster that starts with its Timestamp, as nearly all do,
            // still costs a header or two.
            const uint64_t cluster_end = cluster.end();
            while (reader.tell() < cluster_end) {
                EBMLElement child;
                if (!reader.readElementHeader(child)) {
                    break;
                }
                if (child.id == Id::Timestamp) {
                    m_entries.push_back(CueEntry{reader.readUInt(child),
                                                 cluster.header_offset});
                    break;
                }
                if (child.unknown_size) {
                    break; // nothing past it can be found without its size
                }
                reader.seek(child.end());
            }

            if (cluster_end >= file_size) {
                break; // the last cluster, or one the file was cut short in
            }
            reader.seek(cluster_end);
        }
    } catch (const std::exception&) {
        // Keep what was found.
    }

    sortAndDedupe();
    return !m_entries.empty();
}

const CueEntry* CueIndex::entryFor(uint64_t time_ticks) const
{
    if (m_entries.empty()) {
        return nullptr;
    }
    // The last entry at or before the target: a seek has to land on or before
    // what was asked for and decode forward to it. Landing after would skip
    // audio outright.
    auto it = std::upper_bound(m_entries.begin(), m_entries.end(), time_ticks,
                               [](uint64_t value, const CueEntry& entry) {
                                   return value < entry.time_ticks;
                               });
    if (it == m_entries.begin()) {
        // Before the first entry: answer with that entry rather than nothing.
        // It is the first indexed cluster, which with sparse Cues need not be
        // the file's first, so the caller checks its time.
        return &m_entries.front();
    }
    return &*(it - 1);
}

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3
