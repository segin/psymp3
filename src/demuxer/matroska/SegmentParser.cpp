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
/// A CodecID missing from here maps to nothing, so the demuxer can say which
/// codec a file needs rather than open it and fail at the first packet. DTS is
/// the notable absence: nothing in the tree decodes it, and it is common in
/// .mkv. A_MS/ACM is not in the table because the codec it needs is named in
/// its CodecPrivate, which acmCodecName reads.
struct CodecMapping {
    const char* codec_id;
    const char* codec_name;
};

constexpr CodecMapping kCodecMap[] = {
    {"A_OPUS",          "opus"},
    {"A_VORBIS",        "vorbis"},
    {"A_FLAC",          "flac"},
    {"A_ALAC",          "alac"},
    // Layer II has a decoder of its own. minimp3, behind "mp3", decodes
    // Layers I and III.
    {"A_MPEG/L3",       "mp3"},
    {"A_MPEG/L2",       "mp2"},
    {"A_MPEG/L1",       "mp3"},
    // Bare A_AAC carries an AudioSpecificConfig in CodecPrivate, which names
    // the profile. The profile-suffixed IDs are the legacy form and carry
    // none (cellar-codec 3.4.2-3.4.10); toStreamInfo writes one for the LC
    // and HE (SBR) ones. Main, SSR and LTP are left out: the AAC decoder has
    // nothing for them.
    {"A_AAC",           "aac"},
    {"A_AAC/MPEG2/LC",  "aac"},
    {"A_AAC/MPEG2/LC/SBR", "aac"},
    {"A_AAC/MPEG4/LC",  "aac"},
    {"A_AAC/MPEG4/LC/SBR", "aac"},
    {"A_TRUEHD",        "truehd"},
    {"A_MLP",           "mlp"},
    {"A_AC3",           "ac3"},
    {"A_EAC3",          "eac3"},
    {"A_PCM/INT/LIT",   "pcm"},
    {"A_PCM/INT/BIG",   "pcm"},
    {"A_PCM/FLOAT/IEEE","pcm"},
};

/// The AudioSpecificConfig (ISO/IEC 14496-3 1.6.2.1) a legacy AAC CodecID
/// implies: AAC LC at the coded rate, and for the /SBR IDs an explicit SBR
/// extension at the output rate (twice the coded rate when the track does
/// not say). Empty for any other ID, for a rate that is not a whole number
/// of hertz, and for a channel count that needs a program config element.
std::vector<uint8_t> legacyAacConfig(const std::string& codec_id, double coded_hz,
                                     double output_hz, uint64_t channels)
{
    const bool lc = codec_id == "A_AAC/MPEG2/LC" || codec_id == "A_AAC/MPEG4/LC";
    const bool sbr = codec_id == "A_AAC/MPEG2/LC/SBR" || codec_id == "A_AAC/MPEG4/LC/SBR";
    const auto whole = [](double hz) {
        return std::isfinite(hz) && hz >= 1.0 && hz <= 16777215.0 && hz == std::floor(hz);
    };
    if ((!lc && !sbr) || !whole(coded_hz)) {
        return {};
    }
    // Channel configurations 1-6 are those counts; 7 is eight channels (7.1).
    unsigned channel_config = 0;
    if (channels >= 1 && channels <= 6) {
        channel_config = static_cast<unsigned>(channels);
    } else if (channels == 8) {
        channel_config = 7;
    } else {
        return {};
    }
    const uint32_t coded = static_cast<uint32_t>(coded_hz);
    const uint32_t output = whole(output_hz) ? static_cast<uint32_t>(output_hz) : 2 * coded;

    std::vector<uint8_t> out;
    unsigned used = 0;
    const auto put = [&](uint32_t value, unsigned bits) {
        for (unsigned i = bits; i-- > 0;) {
            if (used % 8 == 0) {
                out.push_back(0);
            }
            if ((value >> i) & 1) {
                out.back() |= static_cast<uint8_t>(0x80 >> (used % 8));
            }
            ++used;
        }
    };
    const auto putRate = [&](uint32_t hz) {
        static constexpr uint32_t kRates[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
                                              22050, 16000, 12000, 11025, 8000, 7350};
        for (uint32_t index = 0; index < std::size(kRates); ++index) {
            if (kRates[index] == hz) {
                put(index, 4);
                return;
            }
        }
        put(0xF, 4);   // escape: the rate itself follows
        put(hz, 24);
    };

    put(2, 5);                 // audioObjectType: AAC LC
    putRate(coded);
    put(channel_config, 4);
    put(0, 3);                 // frameLengthFlag, dependsOnCoreCoder, extensionFlag
    if (sbr) {
        put(0x2B7, 11);        // syncExtensionType
        put(5, 5);             // extensionAudioObjectType: SBR
        put(1, 1);             // sbrPresentFlag
        putRate(output);
    }
    return out;
}

/// Whether OpusCodec will start from this CodecPrivate. The checks mirror
/// OpusHeader::parseFromPacket() and isValid() field for field: 19 bytes, the
/// magic, version 1, a non-zero channel count, and mapping family 0, 1 or
/// 255. They are restated here because Opus support is optional and the
/// demuxer is built without it.
bool opusHeadUsable(const std::vector<uint8_t>& cp)
{
    return cp.size() >= 19 &&
           cp[0] == 'O' && cp[1] == 'p' && cp[2] == 'u' && cp[3] == 's' &&
           cp[4] == 'H' && cp[5] == 'e' && cp[6] == 'a' && cp[7] == 'd' &&
           cp[8] == 1 && cp[9] != 0 &&
           (cp[18] == 0 || cp[18] == 1 || cp[18] == 255);
}

/// A_MS/ACM's CodecPrivate is a WAVEFORMATEX, little-endian (cellar-codec
/// 3.4.25): the format tag, channels, rate, byte rate, block align and bit
/// depth, then cbSize and that many extra bytes. For WAVE_FORMAT_EXTENSIBLE
/// the real format is the tag inside the SubFormat GUID, 24 bytes in.
struct AcmFormat {
    bool valid = false;
    uint16_t tag = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    std::vector<uint8_t> extra;   ///< the bytes after cbSize
};

AcmFormat acmFormat(const std::vector<uint8_t>& cp)
{
    AcmFormat format;
    if (cp.size() < 16) {
        return format;
    }
    const auto u16 = [&cp](size_t at) {
        return static_cast<uint16_t>(cp[at] | (cp[at + 1] << 8));
    };
    format.tag = u16(0);
    format.channels = u16(2);
    format.sample_rate = static_cast<uint32_t>(u16(4)) | (static_cast<uint32_t>(u16(6)) << 16);
    format.bits_per_sample = u16(14);
    if (cp.size() >= 18) {
        const size_t size = std::min<size_t>(u16(16), cp.size() - 18);
        format.extra.assign(cp.begin() + 18, cp.begin() + 18 + static_cast<std::ptrdiff_t>(size));
    }
    if (format.tag == 0xFFFE && format.extra.size() >= 22) {
        const uint16_t sub_format = static_cast<uint16_t>(format.extra[6] | (format.extra[7] << 8));
        if (sub_format != 0xFFFE) {
            format.tag = sub_format;
        }
    }
    format.valid = true;
    return format;
}

/// The codec an A_MS/ACM track needs, or "" when its format has no decoder.
std::string acmCodecName(const std::vector<uint8_t>& cp)
{
    const AcmFormat format = acmFormat(cp);
    return format.valid ? waveFormatCodecName(format.tag) : std::string();
}

/// CodecPrivate as the decoder receives it, header stripping undone.
std::vector<uint8_t> codecPrivateOf(const TrackEntry& track)
{
    std::vector<uint8_t> data = track.stripped_private_prefix;
    data.insert(data.end(), track.codec_private.begin(), track.codec_private.end());
    return data;
}

/// An unsigned integer element's value, or @p fallback when the element is
/// empty. An empty element whose schema declares a default takes that default
/// (RFC 8794 6.1); EBMLReader reads it as 0, which is right only when there is
/// none.
uint64_t uintOr(EBMLReader& reader, const EBMLElement& element, uint64_t fallback)
{
    return element.size == 0 ? fallback : reader.readUInt(element);
}

/// The same for a float element.
double floatOr(EBMLReader& reader, const EBMLElement& element, double fallback)
{
    return element.size == 0 ? fallback : reader.readFloat(element);
}

} // namespace

std::string codecNameForId(const std::string& codec_id)
{
    for (const CodecMapping& mapping : kCodecMap) {
        const std::string id(mapping.codec_id);
        if (codec_id == id) {
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
    // Written as !(x > 0) so a NaN, which compares false with everything, is
    // refused here too rather than reaching the cast below.
    if (!(duration_ticks > 0.0)) {
        return 0;
    }
    // Ticks are timestamp_scale_ns nanoseconds each. Going through nanoseconds
    // rather than scaling milliseconds directly keeps a file with an unusual
    // scale from losing the fraction.
    const double nanoseconds = duration_ticks * static_cast<double>(timestamp_scale_ns);
    const double milliseconds = nanoseconds / 1000000.0;
    // Casting an infinity, or anything uint64_t cannot hold, is undefined, and
    // a file claiming that long a duration is not stating a real one.
    if (!(milliseconds < 0x1p64)) {
        return 0;
    }
    return static_cast<uint64_t>(milliseconds);
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
        case Id::Info:
            if (!m_info_seen) {
                parseInfo(reader, element);
                m_info_seen = true;
            }
            break;
        case Id::Tracks:   parseTracks(reader, element);          break;
        case Id::SeekHead: parseSeekHead(reader, element);        break;
        default: break;
        }
        reader.seek(element.end());
    }

    // A muxer may write Info and Tracks after the clusters, as long as a
    // SeekHead before them says where (RFC 9559 6.1), and a walk that stops at
    // the first cluster never reaches them. Following SeekHead is the
    // difference between playing such a file and reporting that it has no
    // tracks -- or, for Info, timing every block by the default
    // TimestampScale and knowing no duration.
    if (!m_info_seen) {
        const uint64_t info_at = seekPosition(Id::Info);
        if (info_at != 0 && info_at > m_segment_data_offset) {
            reader.seek(info_at);
            EBMLElement info;
            if (reader.readElementHeader(info) && info.id == Id::Info && !info.unknown_size) {
                parseInfo(reader, info);
                m_info_seen = true;
            }
        }
    }
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
        case Id::TimestampScale: m_info.timestamp_scale_ns = uintOr(reader, child, 1000000); break;
        case Id::Duration:       m_info.duration_ticks = reader.readFloat(child);    break;
        case Id::MuxingApp:      m_info.muxing_app = reader.readUTF8(child);         break;
        case Id::WritingApp:     m_info.writing_app = reader.readUTF8(child);        break;
        case Id::Title:          m_info.title = reader.readUTF8(child);              break;
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
            TrackEntry track = parseTrackEntry(reader, child);
            track.ordinal = static_cast<uint32_t>(m_tracks.size() + 1);
            m_tracks.push_back(std::move(track));
        }
        reader.seek(child.end());
    }
}

TrackEntry SegmentParser::parseTrackEntry(EBMLReader& reader, const EBMLElement& entry)
{
    TrackEntry track;
    std::string bcp47;
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
        case Id::CodecName:       track.codec_name = reader.readUTF8(child); break;
        case Id::CodecPrivate:    track.codec_private = reader.readBinary(child); break;
        case Id::TrackName:       track.name = reader.readUTF8(child); break;
        case Id::Language:        track.language = reader.readString(child); break;
        case Id::DefaultDuration: track.default_duration_ns = reader.readUInt(child); break;
        case Id::TrackTimestampScale: {
            // The bound keeps a block offset times the scale well inside
            // int64; nothing real comes near it.
            const double scale = floatOr(reader, child, 1.0);
            if (std::isfinite(scale) && scale > 0.0 && scale <= 1000000.0) {
                track.timestamp_scale = scale;
            }
            break;
        }
        case Id::CodecDelay:      track.codec_delay_ns = reader.readUInt(child); break;
        case Id::SeekPreRoll:     track.seek_preroll_ns = reader.readUInt(child); break;
        case Id::FlagLacing:      track.lacing_allowed = uintOr(reader, child, 1) != 0; break;
        case Id::FlagDefault:     track.default_track = uintOr(reader, child, 1) != 0; break;
        case Id::FlagEnabled:     track.enabled = uintOr(reader, child, 1) != 0; break;
        // Where LanguageBCP47 is present, Language is ignored, whichever comes
        // first, so it is kept apart until the walk is done.
        case Id::LanguageBCP47:   bcp47 = reader.readString(child); break;
        case Id::Audio:           parseAudio(reader, child, track); break;
        case Id::ContentEncodings:
            if (!child.unknown_size) {
                parseContentEncodings(reader, child, track);
            }
            break;
        default: break;
        }
        reader.seek(child.end());
    }
    if (!bcp47.empty()) {
        track.language = bcp47;
    }
    return track;
}

void SegmentParser::parseContentEncodings(EBMLReader& reader, const EBMLElement& encodings,
                                          TrackEntry& track)
{
    struct Encoding {
        uint64_t order = 0;
        uint64_t scope = 1;   // schema default: frame contents
        uint64_t type = 0;    // schema default: compression
        bool compression = false;
        uint64_t algo = 0;    // schema default: zlib
        std::vector<uint8_t> settings;
    };
    std::vector<Encoding> list;

    const uint64_t end = encodings.end();
    while (reader.tell() < end) {
        EBMLElement child;
        if (!reader.readElementHeader(child) || child.unknown_size) {
            track.unsupported_encoding = true;
            return;
        }
        if (child.id == Id::ContentEncoding) {
            Encoding encoding;
            const uint64_t encoding_end = child.end();
            while (reader.tell() < encoding_end) {
                EBMLElement field;
                if (!reader.readElementHeader(field) || field.unknown_size) {
                    track.unsupported_encoding = true;
                    return;
                }
                switch (field.id) {
                case Id::ContentEncodingOrder: encoding.order = reader.readUInt(field); break;
                case Id::ContentEncodingScope: encoding.scope = reader.readUInt(field); break;
                case Id::ContentEncodingType:  encoding.type = reader.readUInt(field);  break;
                case Id::ContentCompression: {
                    encoding.compression = true;
                    const uint64_t compression_end = field.end();
                    while (reader.tell() < compression_end) {
                        EBMLElement setting;
                        if (!reader.readElementHeader(setting) || setting.unknown_size) {
                            track.unsupported_encoding = true;
                            return;
                        }
                        if (setting.id == Id::ContentCompAlgo) {
                            encoding.algo = reader.readUInt(setting);
                        } else if (setting.id == Id::ContentCompSettings) {
                            encoding.settings = reader.readBinary(setting);
                        }
                        reader.seek(setting.end());
                    }
                    break;
                }
                default: break;
                }
                reader.seek(field.end());
            }
            list.push_back(std::move(encoding));
        }
        reader.seek(child.end());
    }

    // Decoding starts at the highest ContentEncodingOrder and works down
    // (5.1.4.1.31.2). Each header-stripping step puts its own bytes in front,
    // so the bytes that end up first belong to the lowest order.
    std::stable_sort(list.begin(), list.end(),
                     [](const Encoding& a, const Encoding& b) { return a.order < b.order; });
    // Stripped headers are a few bytes. A megabyte bounds what a hostile file
    // could make every frame grow by.
    constexpr size_t kMaxPrefix = 1u << 20;
    for (const Encoding& encoding : list) {
        const bool header_stripping = encoding.type == 0 && encoding.compression
                                   && encoding.algo == 3;
        const bool known_scope = encoding.scope != 0 && (encoding.scope & ~uint64_t{3}) == 0;
        if (!header_stripping || !known_scope) {
            track.unsupported_encoding = true;
            continue;
        }
        if (encoding.scope & 1) {
            track.stripped_frame_prefix.insert(track.stripped_frame_prefix.end(),
                                               encoding.settings.begin(), encoding.settings.end());
        }
        if (encoding.scope & 2) {
            track.stripped_private_prefix.insert(track.stripped_private_prefix.end(),
                                                 encoding.settings.begin(), encoding.settings.end());
        }
    }
    if (track.stripped_frame_prefix.size() > kMaxPrefix
        || track.stripped_private_prefix.size() > kMaxPrefix) {
        track.unsupported_encoding = true;
    }
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
            track.sampling_frequency = floatOr(reader, child, 8000.0);
            break;
        case Id::OutputSamplingFrequency:
            track.output_sampling_frequency = reader.readFloat(child);
            break;
        case Id::Channels:
            track.channels = static_cast<uint16_t>(uintOr(reader, child, 1));
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
        const std::string codec = track.codec_id == "A_MS/ACM"
                                ? acmCodecName(codecPrivateOf(track))
                                : codecNameForId(track.codec_id);
        if (codec.empty()) {
            continue; // nothing here decodes it; keep looking
        }
        if (track.unsupported_encoding) {
            continue; // its frames would reach the decoder still encoded
        }
        if (track.codec_id == "A_OPUS" && !opusHeadUsable(codecPrivateOf(track))) {
            continue; // the decoder has no OpusHead to start from
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
    info.stream_id = track.ordinal;
    info.codec_type = "audio";
    info.codec_name = codecNameForId(track.codec_id);
    info.big_endian_samples = codecIsBigEndianPCM(track.codec_id);

    // Two codecs need a codec_tag to reach the right decoder, and Matroska has
    // no tag of its own to carry.
    if (track.codec_id == "A_PCM/FLOAT/IEEE") {
        // Float and integer PCM share BitDepth 32, so the CodecID is the only
        // signal; PCMCodec selects its float path on codec_tag 0x0003
        // (WAVE_FORMAT_IEEE_FLOAT). Without it every float bit pattern is
        // reinterpreted as int32 -- +1.0f reads as +1.065e9 -- and the track
        // plays as full-scale broadband noise rather than audio.
        info.codec_tag = 0x0003;
    } else if (track.codec_id == "A_FLAC") {
        // AudioCodecFactory routes codec_name "flac" with codec_tag 0 to the
        // Ogg FLAC passthrough, which exists to strip the RFC 9639 Section 10.1
        // mapping headers Ogg delivers as packets. Matroska keeps those headers
        // in CodecPrivate and every Block payload is already a bare frame, so
        // this wants the native decoder -- the tag the native FLAC demuxer sets.
        info.codec_tag = 0x43614C66; // 'fLaC'
    }

    // OutputSamplingFrequency is what comes out of the decoder and
    // SamplingFrequency is what the stream is coded at; they differ for SBR,
    // where an HE-AAC track is coded at half the rate it plays at. The rest of
    // PsyMP3 wants the playback rate.
    //
    // Both are floats taken straight from the file, and casting a NaN, an
    // infinity or anything past uint32_t is undefined behaviour. Only a finite
    // rate from 1 Hz to 1,048,575 Hz (FLAC's 20-bit ceiling, beyond any real
    // decoder's output) is believed. Otherwise the rate stays unknown (0): a
    // decoder that reports its own rate still supplies one, and without that
    // Audio::setup refuses the track instead of playing it at a made-up rate.
    const auto usable = [](double hz) {
        return std::isfinite(hz) && hz >= 1.0 && hz <= 1048575.0;
    };
    double rate = usable(track.output_sampling_frequency) ? track.output_sampling_frequency
                : usable(track.sampling_frequency)        ? track.sampling_frequency
                                                          : 0.0;
    // Opus is the exception. The codec mapping (draft-ietf-cellar-codec
    // §3.4.32) makes an A_OPUS track's SamplingFrequency OpusHead's "Input
    // Sample Rate", the rate the encoder was fed, while an Opus decoder always
    // produces 48 kHz. Believing the field played a track muxed from a
    // 44.1 kHz source about 8% slow.
    if (track.codec_id == "A_OPUS") {
        rate = 48000.0;
    }
    info.sample_rate = static_cast<uint32_t>(rate + 0.5);
    info.channels = track.channels;
    info.bits_per_sample = track.bit_depth;
    info.codec_data = codecPrivateOf(track);
    // A_MS/ACM describes itself with the WAVEFORMATEX it carries, the same
    // one a WAV's fmt chunk holds, and the decoder gets what follows cbSize,
    // as ChunkDemuxer hands on.
    if (track.codec_id == "A_MS/ACM") {
        const AcmFormat format = acmFormat(info.codec_data);
        info.codec_name = format.valid ? waveFormatCodecName(format.tag) : std::string();
        info.codec_tag = format.tag;
        info.bits_per_sample = format.bits_per_sample;
        if (format.channels != 0) {
            info.channels = format.channels;
        }
        if (format.sample_rate != 0) {
            info.sample_rate = format.sample_rate;
        }
        info.codec_data = format.extra;
    }
    // An Opus track needs an OpusHead the decoder accepts. Without one,
    // OpusCodec takes the first audio packets for headers and plays nothing,
    // so such a track is given no codec and refused by name instead. That
    // covers mapping families 2 and 3 (ambisonics) too.
    if (track.codec_id == "A_OPUS" && !opusHeadUsable(info.codec_data)) {
        info.codec_name.clear();
    }
    if (info.codec_data.empty()) {
        info.codec_data = legacyAacConfig(track.codec_id, track.sampling_frequency,
                                          track.output_sampling_frequency, track.channels);
    }
    info.duration_ms = m_info.durationMs();

    // CodecDelay is nanoseconds of decoder startup to throw away; PsyMP3 counts
    // that in sample frames, as it does for AAC's priming. It restates what the
    // *decoder* discards (RFC 9559 5.1.4.1.25), so it must be applied once.
    // OpusCodec already seeds its own skip counter from the OpusHead in
    // CodecPrivate, and DemuxedStream::trimEncoderDelay would then cut the same
    // frames again: a stock 48 kHz Opus track states pre_skip 312 in OpusHead
    // and CodecDelay 6,500,000 ns, and 624 frames of real audio went missing.
    //
    // So Opus leaves the trim to the decoder. An Opus track the decoder cannot
    // start is refused above, so there is no case where the container's value
    // would have to stand in.
    //
    // Vorbis needs no header test. A Vorbis decoder's output already starts at
    // the stream's first sample, because the first packet only primes the
    // overlap and yields nothing, and the CodecDelay muxers write for Vorbis
    // (ffmpeg's 2,666,667 ns, 128 frames at 48 kHz) describes that same
    // overlap. Trimming it again cut 128 real frames off every such track.
    const bool decoder_trims_its_own_delay = track.codec_id == "A_OPUS" || track.codec_id == "A_VORBIS";

    // RFC 9559 sets no upper bound on CodecDelay, but no codec primes for
    // anything like 10 s (Opus states 6.5 ms). A larger value comes from a
    // damaged or hostile file. Honouring it would have trimEncoderDelay discard
    // up to 2^32 frames, i.e. the whole track played as silence, and at high
    // rates the product below also wraps. Such a value is ignored. With the
    // rate capped at 1,048,575 Hz above, 10 s keeps that product under 2^54.
    constexpr uint64_t kMaxCodecDelayNs = 10ULL * 1000000000ULL;
    if (track.codec_delay_ns > 0 && track.codec_delay_ns <= kMaxCodecDelayNs &&
        info.sample_rate > 0 && !decoder_trims_its_own_delay) {
        info.encoder_delay = static_cast<uint32_t>(
            (track.codec_delay_ns * info.sample_rate) / 1000000000ULL);
    }

    // A Duration that is finite but absurd can still wrap this product, so it is
    // left unknown rather than reported as whatever the wrap happens to give.
    if (info.duration_ms > 0 && info.sample_rate > 0 &&
        info.duration_ms <= UINT64_MAX / info.sample_rate) {
        info.duration_samples = (info.duration_ms * info.sample_rate) / 1000;
    }
    return info;
}

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3
