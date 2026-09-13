/*
 * SegmentParser.cpp - Matroska Segment header parsing: Info and Tracks.
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

namespace {

/// Matroska CodecID to the codec_name CodecRegistry dispatches on.
///
/// Absent from this table means absent from PsyMP3: DTS is the notable one,
/// and it is common in .mkv. They map to nothing on
/// purpose, so the demuxer can say which codec a file needs rather than open it
/// and fail at the first packet.
struct CodecMapping {
    const char* codec_id;
    const char* codec_name;
    bool prefix; ///< match "A_AAC/..." as well as "A_AAC"
};

constexpr CodecMapping kCodecMap[] = {
    {"A_OPUS",          "opus",    false},
    {"A_VORBIS",        "vorbis",  false},
    {"A_FLAC",          "flac",    false},
    {"A_ALAC",          "alac",    false},
    // Layer 3 and layer 2 have decoders; layer 1 does not, and is left out
    // rather than pointed at a decoder that would mis-handle it.
    {"A_MPEG/L3",       "mp3",     false},
    {"A_MPEG/L2",       "mp2",     false},
    // AAC appears bare and with a profile suffix ("A_AAC/MPEG4/LC/SBR"). One
    // decoder covers the family, and the profile is read back out of the
    // AudioSpecificConfig in CodecPrivate anyway.
    {"A_AAC",           "aac",     true},
    {"A_TRUEHD",        "truehd",  false},
    {"A_MLP",           "mlp",     false},
    {"A_AC3",           "ac3",     false},
    {"A_EAC3",          "eac3",    false},
    {"A_PCM/INT/LIT",   "pcm",     false},
    {"A_PCM/INT/BIG",   "pcm",     false},
    {"A_PCM/FLOAT/IEEE","pcm",     false},
};

} // namespace

std::string codecNameForId(const std::string& codec_id)
{
    for (const CodecMapping& mapping : kCodecMap) {
        const std::string id(mapping.codec_id);
        if (codec_id == id) {
            return mapping.codec_name;
        }
        if (mapping.prefix && codec_id.size() > id.size()
            && codec_id.compare(0, id.size(), id) == 0
            && codec_id[id.size()] == '/') {
            return mapping.codec_name;
        }
    }
    return std::string();
}

bool codecIsBigEndianPCM(const std::string& codec_id)
{
    return codec_id == "A_PCM/INT/BIG";
}

uint64_t SegmentInfo::durationMs() const
{
    if (duration_ticks <= 0.0) {
        return 0;
    }
    // Ticks are timestamp_scale_ns nanoseconds each. Going through nanoseconds
    // rather than scaling milliseconds directly keeps a file with an unusual
    // scale from losing the fraction.
    const double nanoseconds = duration_ticks * static_cast<double>(timestamp_scale_ns);
    return static_cast<uint64_t>(nanoseconds / 1000000.0);
}

void SegmentParser::parse(EBMLReader& reader)
{
    parseEBMLHeader(reader);

    EBMLElement element;
    while (reader.readElementHeader(element)) {
        if (element.id == Id::Segment) {
            m_segment_data_offset = element.data_offset;
            parseSegment(reader, element);
            return;
        }
        if (element.unknown_size) {
            throw std::runtime_error("Matroska: unknown-size element before the Segment");
        }
        reader.skip(element);
    }
    throw std::runtime_error("Matroska: no Segment element");
}

void SegmentParser::parseEBMLHeader(EBMLReader& reader)
{
    EBMLElement header;
    if (!reader.readElementHeader(header) || header.id != Id::EBMLHeader) {
        throw std::runtime_error("Matroska: file does not begin with an EBML header");
    }
    if (header.unknown_size) {
        throw std::runtime_error("Matroska: EBML header of unknown size");
    }

    const uint64_t end = header.end();
    while (reader.tell() < end) {
        EBMLElement child;
        if (!reader.readElementHeader(child)) {
            break;
        }
        if (child.id == Id::DocType) {
            m_doc_type = reader.readString(child);
        }
        reader.seek(child.end());
    }
    reader.seek(end);

    // Anything EBML-shaped has a header; only these two carry Matroska. A DVB
    // subtitle stream or a WebM-adjacent format would otherwise be parsed as
    // far as its Tracks before failing on something less obvious.
    if (m_doc_type != "matroska" && m_doc_type != "webm") {
        throw std::runtime_error("Matroska: DocType is '" + m_doc_type
                                 + "', not matroska or webm");
    }
}

void SegmentParser::parseSegment(EBMLReader& reader, const EBMLElement& segment)
{
    // A live-muxed Segment has no size, so the walk is bounded by the file
    // rather than by the element.
    const uint64_t end = segment.unknown_size
                       ? std::numeric_limits<uint64_t>::max()
                       : segment.end();

    while (reader.tell() < end) {
        EBMLElement element;
        if (!reader.readElementHeader(element)) {
            break;
        }

        if (element.id == Id::Cluster) {
            // The header elements are all that is wanted here, and the clusters
            // are where the file's bulk is. Stop at the first one.
            m_first_cluster_offset = element.header_offset;
            break;
        }

        if (element.unknown_size) {
            throw std::runtime_error("Matroska: unknown-size element inside the Segment");
        }

        switch (element.id) {
        case Id::Info:     parseInfo(reader, element);            break;
        case Id::Tracks:   parseTracks(reader, element);          break;
        case Id::SeekHead: parseSeekHead(reader, element);        break;
        default: break;
        }
        reader.seek(element.end());
    }

    // A muxer may write Tracks after the clusters, which a walk that stops at
    // the first one never reaches. SeekHead is the index that says where they
    // went, and following it is the difference between playing such a file and
    // reporting that it has no tracks.
    if (m_tracks.empty()) {
        const uint64_t tracks_at = seekPosition(Id::Tracks);
        if (tracks_at != 0 && tracks_at > m_segment_data_offset) {
            reader.seek(tracks_at);
            EBMLElement tracks;
            if (reader.readElementHeader(tracks) && tracks.id == Id::Tracks
                && !tracks.unknown_size) {
                parseTracks(reader, tracks);
            }
        }
    }
}

void SegmentParser::parseSeekHead(EBMLReader& reader, const EBMLElement& seek_head)
{
    const uint64_t end = seek_head.end();
    while (reader.tell() < end) {
        EBMLElement entry;
        if (!reader.readElementHeader(entry)) {
            break;
        }
        if (entry.id != Id::Seek) {
            reader.seek(entry.end());
            continue;
        }

        uint32_t target_id = 0;
        uint64_t position = 0;
        bool have_position = false;
        const uint64_t entry_end = entry.end();
        while (reader.tell() < entry_end) {
            EBMLElement field;
            if (!reader.readElementHeader(field)) {
                break;
            }
            if (field.id == Id::SeekID) {
                // The ID is stored as its raw encoded bytes, marker included,
                // which is the same form EBMLReader reports -- so they compare
                // directly against the Id:: constants.
                const std::vector<uint8_t> bytes = reader.readBinary(field);
                if (bytes.empty() || bytes.size() > 4) {
                    target_id = 0;
                } else {
                    uint32_t value = 0;
                    for (uint8_t byte : bytes) {
                        value = (value << 8) | byte;
                    }
                    target_id = value;
                }
            } else if (field.id == Id::SeekPosition) {
                position = reader.readUInt(field);
                have_position = true;
            }
            reader.seek(field.end());
        }

        // Positions are relative to the start of the Segment's payload, not to
        // the file, so they are absolute only after that is added.
        if (target_id != 0 && have_position) {
            m_seek_positions[target_id] = m_segment_data_offset + position;
        }
        reader.seek(entry_end);
    }
}

uint64_t SegmentParser::seekPosition(uint32_t id) const
{
    const auto it = m_seek_positions.find(id);
    return it == m_seek_positions.end() ? 0 : it->second;
}

void SegmentParser::parseInfo(EBMLReader& reader, const EBMLElement& info)
{
    const uint64_t end = info.end();
    while (reader.tell() < end) {
        EBMLElement child;
        if (!reader.readElementHeader(child)) {
            break;
        }
        switch (child.id) {
        case Id::TimestampScale: m_info.timestamp_scale_ns = reader.readUInt(child); break;
        case Id::Duration:       m_info.duration_ticks = reader.readFloat(child);    break;
        case Id::MuxingApp:      m_info.muxing_app = reader.readString(child);       break;
        case Id::WritingApp:     m_info.writing_app = reader.readString(child);      break;
        case Id::Title:          m_info.title = reader.readString(child);            break;
        default: break;
        }
        reader.seek(child.end());
    }

    // A scale of zero would make every timestamp in the file zero. Treat it as
    // absent rather than propagating it into a division.
    if (m_info.timestamp_scale_ns == 0) {
        m_info.timestamp_scale_ns = 1000000;
    }
}

void SegmentParser::parseTracks(EBMLReader& reader, const EBMLElement& tracks)
{
    const uint64_t end = tracks.end();
    while (reader.tell() < end) {
        EBMLElement child;
        if (!reader.readElementHeader(child)) {
            break;
        }
        if (child.id == Id::TrackEntry) {
            m_tracks.push_back(parseTrackEntry(reader, child));
        }
        reader.seek(child.end());
    }
}

TrackEntry SegmentParser::parseTrackEntry(EBMLReader& reader, const EBMLElement& entry)
{
    TrackEntry track;
    const uint64_t end = entry.end();
    while (reader.tell() < end) {
        EBMLElement child;
        if (!reader.readElementHeader(child)) {
            break;
        }
        switch (child.id) {
        case Id::TrackNumber:     track.number = reader.readUInt(child); break;
        case Id::TrackUID:        track.uid = reader.readUInt(child); break;
        case Id::TrackType:       track.type = reader.readUInt(child); break;
        case Id::CodecID:         track.codec_id = reader.readString(child); break;
        case Id::CodecName:       track.codec_name = reader.readString(child); break;
        case Id::CodecPrivate:    track.codec_private = reader.readBinary(child); break;
        case Id::TrackName:       track.name = reader.readString(child); break;
        case Id::Language:        track.language = reader.readString(child); break;
        case Id::DefaultDuration: track.default_duration_ns = reader.readUInt(child); break;
        case Id::CodecDelay:      track.codec_delay_ns = reader.readUInt(child); break;
        case Id::SeekPreRoll:     track.seek_preroll_ns = reader.readUInt(child); break;
        case Id::FlagLacing:      track.lacing_allowed = reader.readUInt(child) != 0; break;
        case Id::FlagDefault:     track.default_track = reader.readUInt(child) != 0; break;
        case Id::FlagEnabled:     track.enabled = reader.readUInt(child) != 0; break;
        // BCP 47 supersedes the ISO 639-2 field when both are present.
        case Id::LanguageBCP47:   track.language = reader.readString(child); break;
        case Id::Audio:           parseAudio(reader, child, track); break;
        default: break;
        }
        reader.seek(child.end());
    }
    return track;
}

void SegmentParser::parseAudio(EBMLReader& reader, const EBMLElement& audio,
                               TrackEntry& track)
{
    const uint64_t end = audio.end();
    while (reader.tell() < end) {
        EBMLElement child;
        if (!reader.readElementHeader(child)) {
            break;
        }
        switch (child.id) {
        case Id::SamplingFrequency:
            track.sampling_frequency = reader.readFloat(child);
            break;
        case Id::OutputSamplingFrequency:
            track.output_sampling_frequency = reader.readFloat(child);
            break;
        case Id::Channels:
            track.channels = static_cast<uint16_t>(reader.readUInt(child));
            break;
        case Id::BitDepth:
            track.bit_depth = static_cast<uint16_t>(reader.readUInt(child));
            break;
        default: break;
        }
        reader.seek(child.end());
    }
}

const TrackEntry* SegmentParser::preferredAudioTrack() const
{
    const TrackEntry* first = nullptr;
    for (const TrackEntry& track : m_tracks) {
        if (!track.isAudio() || !track.enabled) {
            continue;
        }
        if (codecNameForId(track.codec_id).empty()) {
            continue; // nothing here decodes it; keep looking
        }
        if (track.default_track) {
            return &track;
        }
        if (!first) {
            first = &track;
        }
    }
    return first;
}

StreamInfo SegmentParser::toStreamInfo(const TrackEntry& track) const
{
    StreamInfo info;
    info.stream_id = static_cast<uint32_t>(track.number);
    info.codec_type = "audio";
    info.codec_name = codecNameForId(track.codec_id);
    info.big_endian_samples = codecIsBigEndianPCM(track.codec_id);

    // OutputSamplingFrequency is what comes out of the decoder and
    // SamplingFrequency is what the stream is coded at; they differ for SBR,
    // where an HE-AAC track is coded at half the rate it plays at. The rest of
    // PsyMP3 wants the playback rate.
    const double rate = track.output_sampling_frequency > 0.0
                      ? track.output_sampling_frequency
                      : track.sampling_frequency;
    info.sample_rate = static_cast<uint32_t>(rate + 0.5);
    info.channels = track.channels;
    info.bits_per_sample = track.bit_depth;
    info.codec_data = track.codec_private;
    info.duration_ms = m_info.durationMs();

    // CodecDelay is nanoseconds of decoder startup to throw away; PsyMP3
    // counts that in sample frames, as it does for AAC's priming. Opus states
    // it here as well as in OpusHead, and the container's value is the one the
    // muxer actually timed the stream against.
    if (track.codec_delay_ns > 0 && info.sample_rate > 0) {
        info.encoder_delay = static_cast<uint32_t>(
            (track.codec_delay_ns * info.sample_rate) / 1000000000ULL);
    }

    if (info.duration_ms > 0 && info.sample_rate > 0) {
        info.duration_samples = (info.duration_ms * info.sample_rate) / 1000;
    }
    return info;
}

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3
