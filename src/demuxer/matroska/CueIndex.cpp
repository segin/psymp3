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
    // lookup by binary search needs them sorted whatever the file did.
    std::sort(m_entries.begin(), m_entries.end(),
              [](const CueEntry& a, const CueEntry& b) {
                  return a.time_ticks < b.time_ticks;
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
                // A .mkv cues its video and its audio separately. Taking a
                // video entry would start audio decoding from whatever cluster
                // happened to hold a keyframe.
                if (position_seen && track == track_number) {
                    cluster_offset = segment_data_offset + position;
                    have_position = true;
                }
            }
            reader.seek(child.end());
        }

        if (have_time && have_position) {
            m_entries.push_back(CueEntry{time_ticks, cluster_offset});
        }
        reader.seek(point_end);
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

    reader.seek(first_cluster);
    while (reader.tell() < file_size) {
        EBMLElement cluster;
        if (!reader.readElementHeader(cluster)) {
            break;
        }
        if (cluster.id != Id::Cluster) {
            // Cues, Tags and Attachments legitimately sit among or after the
            // clusters; step over anything that is not one.
            if (cluster.unknown_size) {
                break;
            }
            reader.seek(cluster.end());
            continue;
        }

        // A cluster of unknown size gives no way to find the next one without
        // reading its contents, which is the whole cost this scan avoids. Stop
        // rather than fall back to reading the file through.
        if (cluster.unknown_size) {
            break;
        }

        // Timestamp must precede the cluster's blocks, but it is not
        // necessarily the first child: a muxer may put a CRC-32 or Void element
        // ahead of it, and ffmpeg writes exactly that. Reading only the first
        // child finds the CRC and concludes the cluster has no timestamp, which
        // leaves the whole scan empty.
        //
        // Walking stops at the first block regardless, so this still reads a
        // couple of element headers per cluster rather than any block data.
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
            if (child.id == Id::SimpleBlock || child.id == Id::BlockGroup
                || child.unknown_size) {
                break; // reached data: this cluster states no timestamp
            }
            reader.seek(child.end());
        }

        reader.seek(cluster_end);
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
        // Before the first entry: start at the beginning rather than nowhere.
        return &m_entries.front();
    }
    return &*(it - 1);
}

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3
