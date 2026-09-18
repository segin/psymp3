/*
 * ChunkDemuxerAvi.cpp - the AVI form of RIFF, for ChunkDemuxer
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Written from the AVI RIFF File Reference and the OpenDML AVI File Format
 * Extensions v1.02. An AVI is RIFF, like a WAV, but the audio is not one
 * chunk: the streams are declared in a 'hdrl' list and their data is cut into
 * chunks interleaved through a 'movi' list.
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Demuxer {

namespace {

constexpr uint32_t aviFourcc(char a, char b, char c, char d)
{
    return (static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24)
         | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 16)
         | (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 8)
         | static_cast<uint32_t>(static_cast<uint8_t>(d));
}

// Chunk and list identifiers, in the order the bytes appear in the file, the
// way ChunkDemuxer reads a FOURCC.
constexpr uint32_t kRIFF = aviFourcc('R', 'I', 'F', 'F');
constexpr uint32_t kLIST = aviFourcc('L', 'I', 'S', 'T');
constexpr uint32_t kAVIX = aviFourcc('A', 'V', 'I', 'X');
constexpr uint32_t kHDRL = aviFourcc('h', 'd', 'r', 'l');
constexpr uint32_t kSTRL = aviFourcc('s', 't', 'r', 'l');
constexpr uint32_t kMOVI = aviFourcc('m', 'o', 'v', 'i');
constexpr uint32_t kREC  = aviFourcc('r', 'e', 'c', ' ');
constexpr uint32_t kINFO = aviFourcc('I', 'N', 'F', 'O');
constexpr uint32_t kSTRH = aviFourcc('s', 't', 'r', 'h');
constexpr uint32_t kSTRF = aviFourcc('s', 't', 'r', 'f');
constexpr uint32_t kINDX = aviFourcc('i', 'n', 'd', 'x');
constexpr uint32_t kIDX1 = aviFourcc('i', 'd', 'x', '1');
constexpr uint32_t kAUDS = aviFourcc('a', 'u', 'd', 's');
constexpr uint32_t kINAM = aviFourcc('I', 'N', 'A', 'M');
constexpr uint32_t kIART = aviFourcc('I', 'A', 'R', 'T');
constexpr uint32_t kIPRD = aviFourcc('I', 'P', 'R', 'D');

/// AVISTREAMHEADER dwFlags: this stream is not to be played by default.
constexpr uint32_t kStreamDisabled = 0x00000001;
/// AVISUPERINDEX and AVISTDINDEX bIndexType.
constexpr uint8_t kIndexOfIndexes = 0x00;
constexpr uint8_t kIndexOfChunks = 0x01;

/// AVI numbers its streams in two decimal digits, so there can be no more
/// than a hundred of them.
constexpr uint32_t kMaxStreams = 100;
/// What a damaged index may make PsyMP3 allocate: about a day of audio.
constexpr size_t kMaxIndexEntries = 4 * 1024 * 1024;
/// A WAVEFORMATEX longer than this is damage, not configuration.
constexpr uint32_t kMaxWaveFormatBytes = 4096;

uint16_t readU16LE(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t readU32LE(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint64_t readU64LE(const uint8_t* p)
{
    return static_cast<uint64_t>(readU32LE(p)) | (static_cast<uint64_t>(readU32LE(p + 4)) << 32);
}

/// A FOURCC as ChunkDemuxer reads one: first byte in the high bits.
uint32_t readFourcc(const uint8_t* p)
{
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
         | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

/// The stream a data chunk belongs to, from the two decimal digits its FOURCC
/// starts with -- '01wb' is stream 1 -- or -1 when it names no stream.
int chunkStream(uint32_t id)
{
    const auto tens = static_cast<char>((id >> 24) & 0xFF);
    const auto units = static_cast<char>((id >> 16) & 0xFF);
    if (tens < '0' || tens > '9' || units < '0' || units > '9') {
        return -1;
    }
    return (tens - '0') * 10 + (units - '0');
}

/// True for an index chunk ('ix##'), which carries no audio whatever stream
/// number the rest of its FOURCC reads as.
bool isIndexChunk(uint32_t id)
{
    return ((id >> 24) & 0xFF) == 'i' && ((id >> 16) & 0xFF) == 'x';
}

/// Chunks are padded to an even length (AVI RIFF reference, RIFF File Format).
uint64_t paddedSize(uint32_t size)
{
    return static_cast<uint64_t>(size) + (size & 1);
}

} // namespace

bool ChunkDemuxer::readAviBytes(uint64_t offset, void* into, size_t count) const
{
    if (!m_handler || count == 0) {
        return false;
    }
    if (m_handler->seek(static_cast<off_t>(offset), SEEK_SET) != 0) {
        return false;
    }
    return m_handler->read(into, 1, count) == count;
}

bool ChunkDemuxer::parseAviForm()
{
    // The file is one or more RIFF forms: 'AVI ' first, then any number of
    // 'AVIX' continuations holding nothing but further 'movi' lists (OpenDML
    // 1.02 section 3). Each form's size is read here rather than taken from
    // the caller, whose chunk reader clamps a size over 2 GB -- which an AVI
    // of that size states legitimately.
    m_handler->seek(0, SEEK_END);
    const uint64_t file_size = static_cast<uint64_t>(m_handler->tell());

    // A 'LIST INFO' may come before the stream it describes, and the tags go
    // to the stream, so where it was is remembered and it is read at the end.
    uint64_t info_offset = 0;
    uint64_t info_end = 0;

    uint64_t at = 0;
    bool first = true;
    while (at + 12 <= file_size) {
        uint8_t header[12];
        if (!readAviBytes(at, header, sizeof(header)) || readFourcc(header) != kRIFF) {
            break;
        }
        if (!first && readFourcc(header + 8) != kAVIX) {
            break; // another RIFF form follows, and it is not ours to read
        }
        first = false;

        // A file cut short mid-write states the size it was going to be, so
        // the form ends at the end of the file when it says otherwise.
        uint64_t end = std::min<uint64_t>(file_size, at + 8 + paddedSize(readU32LE(header + 4)));
        if (end < at + 12) {
            end = file_size;
        }

        uint64_t child = at + 12;
        while (child + 8 <= end) {
            uint8_t chunk[12];
            if (!readAviBytes(child, chunk, 8)) {
                break;
            }
            const uint32_t id = readFourcc(chunk);
            const uint32_t size = readU32LE(chunk + 4);
            const uint64_t body = child + 8;
            const uint64_t next = body + paddedSize(size);
            const bool overruns = next > end || next <= child;
            if (id == kLIST && readAviBytes(body, chunk + 8, 4)) {
                const uint32_t type = readFourcc(chunk + 8);
                if (type == kHDRL && !overruns) {
                    parseAviHeaderList(body + 4, next);
                } else if (type == kMOVI) {
                    // The chunks of a list cut short are still audio, as far
                    // as the file goes; what is missing is only its tail.
                    m_avi.segments.push_back(AviSegment{body + 4, overruns ? end : next});
                } else if (type == kINFO && !overruns) {
                    info_offset = body + 4;
                    info_end = next;
                }
            } else if (id == kIDX1 && !overruns) {
                m_avi.idx1_offset = body;
                m_avi.idx1_size = size;
            }
            if (overruns) {
                break; // a size that runs past the form: read no further
            }
            child = next;
        }
        at = end;
    }

    if (info_end > info_offset) {
        parseAviInfoList(info_offset, info_end);
    }

    if (!m_avi.have_audio) {
        Debug::log("chunk", "ChunkDemuxer: AVI has no audio stream this build can decode");
        reportError("container", "AVI has no playable audio stream");
        return false;
    }
    if (m_avi.segments.empty()) {
        reportError("container", "AVI has no movi list");
        return false;
    }

    buildAviIndex();
    m_avi.segment = 0;
    m_avi.offset = m_avi.segments.front().begin;
    m_duration_ms = aviTicksToMs(m_avi.length_ticks);
    m_eof = false;
    Debug::log("chunk", "ChunkDemuxer: AVI stream ", m_avi.stream_number, " is audio, ",
               m_avi.segments.size(), " movi list(s), ", m_avi.index.size(), " indexed chunks, ",
               m_duration_ms, " ms");
    return true;
}

bool ChunkDemuxer::parseAviHeaderList(uint64_t begin, uint64_t end)
{
    // 'avih' states the frame rate and the stream count, and the audio
    // stream's own header carries everything needed here, so only the 'strl'
    // lists are read. They are numbered by their order (AVI RIFF reference).
    uint32_t stream_number = 0;
    uint64_t at = begin;
    while (at + 8 <= end) {
        uint8_t header[12];
        if (!readAviBytes(at, header, 8)) {
            return false;
        }
        const uint32_t id = readFourcc(header);
        const uint32_t size = readU32LE(header + 4);
        const uint64_t body = at + 8;
        const uint64_t next = body + paddedSize(size);
        if (next <= at || next > end) {
            return false;
        }
        if (id == kLIST && readAviBytes(body, header + 8, 4) && readFourcc(header + 8) == kSTRL) {
            if (stream_number < kMaxStreams) {
                parseAviStreamList(body + 4, next, stream_number);
            }
            ++stream_number;
        }
        at = next;
    }
    return true;
}

bool ChunkDemuxer::parseAviStreamList(uint64_t begin, uint64_t end, uint32_t stream_number)
{
    // AVISTREAMHEADER: the type, then dwFlags, dwScale, dwRate, dwStart,
    // dwLength and dwSampleSize at fixed offsets.
    uint32_t type = 0;
    uint32_t flags = 0;
    uint32_t scale = 1;
    uint32_t rate = 0;
    uint32_t length = 0;
    uint32_t sample_size = 0;
    bool have_header = false;
    // Where this stream's 'indx' is, read once the 'strf' has said whether the
    // stream is one this build can play. The reference puts 'indx' last, but
    // nothing stops a writer putting it first.
    uint64_t index_offset = 0;
    uint32_t index_size = 0;

    uint64_t at = begin;
    while (at + 8 <= end) {
        uint8_t header[8];
        if (!readAviBytes(at, header, sizeof(header))) {
            return false;
        }
        const uint32_t id = readFourcc(header);
        const uint32_t size = readU32LE(header + 4);
        const uint64_t body = at + 8;
        const uint64_t next = body + paddedSize(size);
        if (next <= at || next > end) {
            return false;
        }
        if (id == kSTRH && size >= 48) {
            // rcFrame, the last eight bytes of an AVISTREAMHEADER, is for
            // video and is not read, so a header that stops before it is
            // still usable.
            uint8_t strh[48];
            if (!readAviBytes(body, strh, sizeof(strh))) {
                return false;
            }
            type = readFourcc(strh);
            flags = readU32LE(strh + 8);
            scale = readU32LE(strh + 20);
            rate = readU32LE(strh + 24);
            length = readU32LE(strh + 32);
            sample_size = readU32LE(strh + 44);
            have_header = true;
        } else if (id == kSTRF && size >= 16 && have_header && type == kAUDS && !m_avi.have_audio
                   && (flags & kStreamDisabled) == 0 && rate > 0) {
            // The audio 'strf' is a WAVEFORMATEX, the same structure a WAV's
            // 'fmt ' chunk holds, so the chunk parser reads it as one.
            Chunk format;
            format.fourcc = FMT_FOURCC;
            format.size = std::min(size, kMaxWaveFormatBytes);
            format.data_offset = body;
            m_handler->seek(static_cast<off_t>(body), SEEK_SET);
            if (parseWaveFormat(format) && !m_audio_streams.empty()) {
                // The stream keeps the id parseWaveFormat filed it under, which
                // is the id getStreams() reports and everything downstream asks
                // for. The AVI stream number, which is what names the chunks in
                // the 'movi' list, is a different number and is kept apart.
                const AudioStreamData& stream = m_audio_streams.begin()->second;
                m_current_stream_id = stream.stream_id;
                m_avi.have_audio = true;
                m_avi.stream_number = stream_number;
                m_avi.scale = scale > 0 ? scale : 1;
                m_avi.rate = rate;
                m_avi.length_ticks = length;
                m_avi.sample_size = sample_size;
            } else {
                m_audio_streams.clear();
            }
        } else if (id == kINDX) {
            index_offset = body;
            index_size = size;
        }
        at = next;
    }
    if (index_size > 0 && m_avi.have_audio && m_avi.stream_number == stream_number) {
        readAviSuperIndex(index_offset, index_size);
    }
    return true;
}

void ChunkDemuxer::parseAviInfoList(uint64_t begin, uint64_t end)
{
    // A LIST 'INFO' holds the same tags a WAV's does; the three the rest of
    // PsyMP3 shows are taken, and they reach it through the stream's own
    // metadata fields.
    if (m_audio_streams.empty()) {
        return;
    }
    AudioStreamData& stream = m_audio_streams.begin()->second;
    uint64_t at = begin;
    while (at + 8 <= end) {
        uint8_t header[8];
        if (!readAviBytes(at, header, sizeof(header))) {
            return;
        }
        const uint32_t id = readFourcc(header);
        const uint32_t size = readU32LE(header + 4);
        const uint64_t body = at + 8;
        const uint64_t next = body + paddedSize(size);
        if (next <= at || next > end) {
            return;
        }
        if (size > 0 && size <= 4096) {
            std::vector<char> text(size + 1, '\0');
            if (readAviBytes(body, text.data(), size)) {
                const std::string value(text.data());
                if (!value.empty()) {
                    if (id == kINAM) {
                        stream.title = value;
                    } else if (id == kIART) {
                        stream.artist = value;
                    } else if (id == kIPRD) {
                        stream.album = value;
                    }
                }
            }
        }
        at = next;
    }
}

void ChunkDemuxer::readAviSuperIndex(uint64_t offset, uint32_t size)
{
    // AVISUPERINDEX: an index of the 'ix##' chunks that index one stream
    // (OpenDML 1.02). A 'strl' may hold a standard index in that place
    // instead, which bIndexType tells apart.
    if (size < 24) {
        return;
    }
    uint8_t header[24];
    if (!readAviBytes(offset, header, sizeof(header))) {
        return;
    }
    const uint16_t longs_per_entry = readU16LE(header);
    const uint8_t index_type = header[3];
    const uint32_t entries = readU32LE(header + 4);
    if (index_type == kIndexOfChunks) {
        readAviStandardIndex(offset, size);
        return;
    }
    if (index_type != kIndexOfIndexes || longs_per_entry != 4) {
        return;
    }
    const uint64_t room = (size - 24) / 16;
    const uint64_t count = std::min<uint64_t>(entries, room);
    for (uint64_t i = 0; i < count; ++i) {
        uint8_t entry[16];
        if (!readAviBytes(offset + 24 + i * 16, entry, sizeof(entry))) {
            return;
        }
        const uint64_t where = readU64LE(entry);
        const uint32_t bytes = readU32LE(entry + 8);
        if (where != 0 && bytes >= 24) {
            m_avi.super_index.push_back(AviSegment{where, where + bytes});
        }
    }
}

void ChunkDemuxer::readAviStandardIndex(uint64_t offset, uint32_t size)
{
    // AVISTDINDEX: entries of {dwOffset, dwSize} against a 64-bit base. The
    // offset is of the chunk's data, so its header is the eight bytes before
    // that, and the top bit of the size marks a delta frame, which audio does
    // not use.
    if (size < 32) {
        return;
    }
    uint8_t header[24];
    if (!readAviBytes(offset, header, 8)) {
        return;
    }
    // A super index entry gives "absolute file offset" and "size of index
    // chunk at this offset" (OpenDML 1.02): the 'ix##' chunk itself, header
    // and all. Writers differ over whether that size counts the header, so
    // when one is there it is stepped over and its own size field -- which a
    // RIFF chunk header always states as the body alone -- is believed.
    if (isIndexChunk(readFourcc(header))) {
        const uint32_t body = readU32LE(header + 4);
        offset += 8;
        size = body > 0 && body <= size ? body : size - 8;
        if (size < 32) {
            return;
        }
    }
    if (!readAviBytes(offset, header, sizeof(header))) {
        return;
    }
    const uint16_t longs_per_entry = readU16LE(header);
    const uint8_t index_type = header[3];
    const uint32_t entries = readU32LE(header + 4);
    const uint32_t chunk_id = readFourcc(header + 8);
    const uint64_t base = readU64LE(header + 12);
    if (index_type != kIndexOfChunks || longs_per_entry != 2) {
        return;
    }
    if (chunkStream(chunk_id) != static_cast<int>(m_avi.stream_number)) {
        return;
    }
    const uint64_t room = (size - 24) / 8;
    const uint64_t count = std::min<uint64_t>(entries, room);
    for (uint64_t i = 0; i < count && m_avi.index.size() < kMaxIndexEntries; ++i) {
        uint8_t entry[8];
        if (!readAviBytes(offset + 24 + i * 8, entry, sizeof(entry))) {
            return;
        }
        const uint64_t data = base + readU32LE(entry);
        const uint32_t bytes = readU32LE(entry + 4) & 0x7FFFFFFF;
        if (data < 8) {
            continue;
        }
        m_avi.index.push_back(AviIndexEntry{data - 8, bytes, 0});
    }
}

void ChunkDemuxer::readAviOldIndex(uint64_t offset, uint32_t size)
{
    // AVIOLDINDEX: entries of {dwChunkId, dwFlags, dwOffset, dwSize}. The
    // offset "should be specified as an offset, in bytes, from the start of
    // the 'movi' list; however, in some AVI files it is given as an offset
    // from the start of the file" (AVIOLDINDEX), so both readings are tried
    // against the chunk each claims to point at, and the one that lands on it
    // is used for the rest.
    const uint64_t count = std::min<uint64_t>(size / 16, kMaxIndexEntries);
    if (count == 0 || m_avi.segments.empty()) {
        return;
    }
    std::vector<uint8_t> entries(static_cast<size_t>(count) * 16);
    if (!readAviBytes(offset, entries.data(), entries.size())) {
        return;
    }

    // A 'movi' list's FOURCC sits four bytes before its contents, and that is
    // what an offset from the start of the list counts from.
    const uint64_t movi_base = m_avi.segments.front().begin - 4;
    auto pointsAtChunk = [this](uint64_t where, uint32_t id) {
        uint8_t header[4];
        return readAviBytes(where, header, sizeof(header)) && readFourcc(header) == id;
    };
    uint64_t base = movi_base;
    for (uint64_t i = 0; i < count; ++i) {
        const uint8_t* entry = entries.data() + i * 16;
        const uint32_t id = readFourcc(entry);
        if (chunkStream(id) < 0) {
            continue;
        }
        const uint32_t where = readU32LE(entry + 8);
        if (!pointsAtChunk(movi_base + where, id) && pointsAtChunk(where, id)) {
            base = 0;
            Debug::log("chunk", "ChunkDemuxer: AVI idx1 offsets count from the start of the file");
        }
        break;
    }

    for (uint64_t i = 0; i < count; ++i) {
        const uint8_t* entry = entries.data() + i * 16;
        const uint32_t id = readFourcc(entry);
        if (chunkStream(id) != static_cast<int>(m_avi.stream_number) || isIndexChunk(id)) {
            continue;
        }
        m_avi.index.push_back(AviIndexEntry{base + readU32LE(entry + 8), readU32LE(entry + 12), 0});
    }
}

void ChunkDemuxer::buildAviIndex()
{
    for (const AviSegment& where : m_avi.super_index) {
        readAviStandardIndex(where.begin, static_cast<uint32_t>(where.end - where.begin));
    }
    if (m_avi.index.empty() && m_avi.idx1_size > 0) {
        readAviOldIndex(m_avi.idx1_offset, m_avi.idx1_size);
    }
    if (m_avi.index.empty()) {
        Debug::log("chunk", "ChunkDemuxer: AVI has no index for its audio; seeks are refused");
        return;
    }

    // Each entry's time is the stream samples before it: the sizes give that
    // for a stream of fixed-size samples, and the chunk count where they vary.
    uint64_t ticks = 0;
    for (AviIndexEntry& entry : m_avi.index) {
        entry.ticks = ticks;
        ticks += m_avi.sample_size > 0 ? entry.size / m_avi.sample_size : 1;
    }
    if (m_avi.length_ticks == 0) {
        m_avi.length_ticks = ticks;
    }
}

uint64_t ChunkDemuxer::aviTicksToSamples(uint64_t ticks) const
{
    // A stream sample lasts dwScale over dwRate seconds; PsyMP3 counts in
    // output sample frames. Split so the product cannot overflow.
    if (m_avi.rate == 0 || m_audio_streams.empty()) {
        return 0;
    }
    const uint64_t sample_rate = m_audio_streams.begin()->second.sample_rate;
    const uint64_t scaled = ticks * m_avi.scale;
    return (scaled / m_avi.rate) * sample_rate + ((scaled % m_avi.rate) * sample_rate) / m_avi.rate;
}

uint64_t ChunkDemuxer::aviTicksToMs(uint64_t ticks) const
{
    if (m_avi.rate == 0) {
        return 0;
    }
    const uint64_t scaled = ticks * m_avi.scale;
    return (scaled / m_avi.rate) * 1000ULL + ((scaled % m_avi.rate) * 1000ULL) / m_avi.rate;
}

MediaChunk ChunkDemuxer::readAviChunk()
{
    while (m_avi.segment < m_avi.segments.size()) {
        const AviSegment& segment = m_avi.segments[m_avi.segment];
        if (m_avi.offset + 8 > segment.end) {
            ++m_avi.segment;
            if (m_avi.segment < m_avi.segments.size()) {
                m_avi.offset = m_avi.segments[m_avi.segment].begin;
            }
            continue;
        }

        uint8_t header[12];
        if (!readAviBytes(m_avi.offset, header, 8)) {
            break;
        }
        const uint32_t id = readFourcc(header);
        const uint32_t size = readU32LE(header + 4);
        const uint64_t body = m_avi.offset + 8;
        const uint64_t next = body + paddedSize(size);
        if (next <= m_avi.offset || next > segment.end) {
            break; // a chunk that runs past its list: the rest is unreadable
        }

        // A 'rec ' list groups the chunks of one interleaved record, and is
        // read as though it were not there.
        if (id == kLIST) {
            m_avi.offset = readAviBytes(body, header + 8, 4) && readFourcc(header + 8) == kREC
                         ? body + 4 : next;
            continue;
        }
        if (chunkStream(id) != static_cast<int>(m_avi.stream_number) || isIndexChunk(id)) {
            m_avi.offset = next;
            continue;
        }

        MediaChunk chunk;
        chunk.stream_id = m_current_stream_id;
        chunk.timestamp_samples = aviTicksToSamples(m_avi.ticks);
        chunk.is_keyframe = true;
        chunk.file_offset = m_avi.offset;
        if (size > 0) {
            chunk.data.resize(size);
            if (!readAviBytes(body, chunk.data.data(), size)) {
                break;
            }
        }
        m_avi.ticks += m_avi.sample_size > 0 ? size / m_avi.sample_size : 1;
        m_avi.offset = next;
        m_position_ms = aviTicksToMs(m_avi.ticks);
        m_current_sample = aviTicksToSamples(m_avi.ticks);
        if (size == 0) {
            continue; // a chunk with no audio in it says nothing; read on
        }
        return chunk;
    }

    m_eof = true;
    return MediaChunk{};
}

bool ChunkDemuxer::seekAvi(uint64_t timestamp_ms)
{
    if (m_avi.index.empty() || m_avi.rate == 0) {
        // Without an index there is no way to find a chunk boundary: AVI has
        // no sync word to scan for, and walking every chunk header from the
        // start of the file would be a read per chunk.
        Debug::log("chunk", "ChunkDemuxer: AVI seek refused; the file has no audio index");
        return false;
    }

    const uint64_t target_ticks = (timestamp_ms * m_avi.rate) / (1000ULL * m_avi.scale);
    // The last chunk that starts at or before the target.
    size_t low = 0;
    size_t high = m_avi.index.size();
    while (low < high) {
        const size_t mid = low + (high - low) / 2;
        if (m_avi.index[mid].ticks <= target_ticks) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    const AviIndexEntry& entry = m_avi.index[low > 0 ? low - 1 : 0];

    size_t segment = 0;
    while (segment + 1 < m_avi.segments.size() && entry.offset >= m_avi.segments[segment].end) {
        ++segment;
    }
    if (entry.offset < m_avi.segments[segment].begin || entry.offset >= m_avi.segments[segment].end) {
        Debug::log("chunk", "ChunkDemuxer: AVI index points outside the movi list at ", entry.offset);
        return false;
    }
    m_avi.segment = segment;
    m_avi.offset = entry.offset;
    m_avi.ticks = entry.ticks;
    m_position_ms = aviTicksToMs(m_avi.ticks);
    m_current_sample = aviTicksToSamples(m_avi.ticks);
    m_eof = false;
    return true;
}

uint64_t ChunkDemuxer::getGranulePosition(uint32_t stream_id) const
{
    if (!isAviFile() || stream_id != m_current_stream_id) {
        return 0;
    }
    return aviTicksToSamples(m_avi.ticks);
}

} // namespace Demuxer
} // namespace PsyMP3
