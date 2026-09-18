/*
 * test_avi_demuxer.cpp - ChunkDemuxer reads the audio of an AVI
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The files are written here, chunk by chunk after the AVI RIFF File
 * Reference and the OpenDML AVI File Format Extensions v1.02. Their audio is
 * 16-bit stereo PCM whose samples count up, so any audio lost or repeated
 * shows as a break in the count.
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/MemoryIOHandler.h"

using namespace TestFramework;
using namespace PsyMP3;
using namespace PsyMP3::Demuxer;
using PsyMP3::IO::MemoryIOHandler;

namespace {

using Bytes = std::vector<uint8_t>;

constexpr uint32_t kRate = 44100;
constexpr uint16_t kChannels = 2;
constexpr uint32_t kBlockAlign = 4;          // 16-bit stereo
constexpr uint32_t kFramesPerChunk = 4410;   // a tenth of a second
constexpr uint32_t kAudioChunks = 20;        // two seconds

Bytes operator+(Bytes a, const Bytes& b)
{
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

Bytes le16(uint32_t value)
{
    return {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8)};
}

Bytes le32(uint32_t value)
{
    return {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
            static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)};
}

Bytes le64(uint64_t value)
{
    return le32(static_cast<uint32_t>(value)) + le32(static_cast<uint32_t>(value >> 32));
}

Bytes text(const char* s)
{
    return Bytes(s, s + std::strlen(s));
}

/// A chunk, padded to an even length as RIFF requires.
Bytes chunk(const char* id, const Bytes& body)
{
    Bytes out = text(id) + le32(static_cast<uint32_t>(body.size())) + body;
    if (body.size() & 1) {
        out.push_back(0);
    }
    return out;
}

Bytes list(const char* type, const Bytes& body)
{
    return chunk("LIST", text(type) + body);
}

/// AVIMAINHEADER, of which only the stream count and flags are read back.
Bytes avih(uint32_t streams, uint32_t total_frames)
{
    return chunk("avih", le32(100000) + le32(0) + le32(0) + le32(0x10) // AVIF_HASINDEX
                       + le32(total_frames) + le32(0) + le32(streams) + le32(0)
                       + le32(0) + le32(0) + Bytes(16, 0));
}

/// AVISTREAMHEADER.
Bytes strh(const char* type, uint32_t scale, uint32_t rate, uint32_t length,
           uint32_t sample_size, uint32_t flags = 0)
{
    return chunk("strh", text(type) + le32(0) + le32(flags) + le16(0) + le16(0)
                       + le32(0) + le32(scale) + le32(rate) + le32(0) + le32(length)
                       + le32(0) + le32(0) + le32(sample_size) + Bytes(8, 0));
}

/// A WAVEFORMATEX, as an audio stream's 'strf' holds.
Bytes waveFormat(uint16_t tag, uint16_t channels, uint32_t rate, uint16_t bits,
                 uint16_t block_align, const Bytes& extra = {})
{
    Bytes body = le16(tag) + le16(channels) + le32(rate)
               + le32(rate * block_align) + le16(block_align) + le16(bits);
    if (!extra.empty()) {
        body = body + le16(static_cast<uint32_t>(extra.size())) + extra;
    }
    return chunk("strf", body);
}

/// A video stream, so the audio has something to be interleaved with.
Bytes videoStream(uint32_t frames)
{
    const Bytes bitmapinfo = chunk("strf", le32(40) + le32(16) + le32(16) + le16(1) + le16(24)
                                         + le32(0) + le32(16 * 16 * 3) + le32(0) + le32(0)
                                         + le32(0) + le32(0));
    return list("strl", strh("vids", 1, 10, frames, 0) + bitmapinfo);
}

/// kFramesPerChunk frames of counting samples, starting at frame @p first.
Bytes audioChunkData(uint32_t first)
{
    Bytes pcm;
    pcm.reserve(kFramesPerChunk * kBlockAlign);
    for (uint32_t i = 0; i < kFramesPerChunk; ++i) {
        const auto counter = static_cast<uint16_t>(first + i);
        for (int channel = 0; channel < kChannels; ++channel) {
            pcm.push_back(static_cast<uint8_t>(counter & 0xFF));
            pcm.push_back(static_cast<uint8_t>(counter >> 8));
        }
    }
    return pcm;
}

/// The counter value sample @p n holds, as a signed 16-bit sample.
int32_t counterAt(uint64_t n)
{
    return static_cast<int32_t>(static_cast<int16_t>(static_cast<uint16_t>(n)));
}

struct Built {
    Bytes file;
    std::vector<uint64_t> audio_offsets; ///< of each audio chunk's header
};

/// How an idx1's dwOffset field is written. The AVIOLDINDEX reference says it
/// is "an offset, in bytes, from the start of the 'movi' list; however, in
/// some AVI files it is given as an offset from the start of the file".
enum class IndexBase { FromMoviList, FromFileStart };

struct AviOptions {
    bool with_index = true;
    bool with_video = true;
    /// Group each video and audio pair in a 'rec ' list, as files interleaved
    /// for CD-ROM do.
    bool in_rec = false;
    IndexBase base = IndexBase::FromMoviList;
    /// Put an audio chunk after the 'movi' list, where no audio belongs.
    bool trailing_junk = false;
    /// Let the last audio chunk claim more bytes than the 'movi' list holds,
    /// as a file whose writing was cut short does.
    bool overlong_last_chunk = false;
    /// Which AVI stream the audio is. Writers that put the video first, which
    /// is the usual way round, make the audio stream 1 and name its chunks
    /// '01wb'.
    uint32_t audio_stream = 0;
    /// Write the 'LIST INFO' before the 'LIST hdrl' it describes.
    bool info_first = false;
    /// State a 'movi' size that runs past the end of the file, as a file
    /// whose writing was interrupted does.
    bool truncated_movi = false;
};

Built buildAvi(const AviOptions& options = AviOptions{})
{
    const bool with_index = options.with_index;
    const bool with_video = options.with_video;
    const bool in_rec = options.in_rec;

    Built built;
    const bool audio_first = options.audio_stream == 0;
    const std::string audio_id = audio_first ? "00wb" : "01wb";
    const std::string video_id = audio_first ? "01dc" : "00dc";
    const Bytes audio_strl = list("strl", strh("auds", kBlockAlign, kRate * kBlockAlign,
                                               kAudioChunks * kFramesPerChunk, kBlockAlign)
                                        + waveFormat(0x0001, kChannels, kRate, 16, kBlockAlign));
    const Bytes video_strl = with_video ? videoStream(kAudioChunks) : Bytes{};
    // A stream's number is its 'strl' list's place in 'hdrl'.
    const Bytes hdrl = list("hdrl", avih(with_video ? 2 : 1, kAudioChunks)
                                  + (audio_first ? audio_strl + video_strl
                                                 : video_strl + audio_strl));
    const Bytes info = list("INFO", chunk("INAM", text("A counting file"))
                                  + chunk("IART", text("PsyMP3")));

    // The 'movi' list: a video chunk before each audio chunk, so the audio is
    // not contiguous, and an index chunk that must be stepped over.
    Bytes movi;
    std::vector<std::pair<uint32_t, uint32_t>> index; // offset within movi, size
    for (uint32_t i = 0; i < kAudioChunks; ++i) {
        Bytes record;
        if (with_video) {
            record = record + chunk(video_id.c_str(), Bytes(64, static_cast<uint8_t>(i)));
        }
        const uint32_t audio_at = static_cast<uint32_t>(movi.size() + record.size());
        const Bytes data = audioChunkData(i * kFramesPerChunk);
        record = record + chunk(audio_id.c_str(), data);
        if (in_rec) {
            // The offsets an index states are of the chunks themselves, which
            // the 'rec ' header moves along by twelve bytes.
            movi = movi + list("rec ", record);
            index.emplace_back(audio_at + 12, static_cast<uint32_t>(data.size()));
        } else {
            movi = movi + record;
            index.emplace_back(audio_at, static_cast<uint32_t>(data.size()));
        }
    }

    if (options.overlong_last_chunk) {
        // The last chunk's size field, four bytes into its header, is made to
        // reach past the end of the list it is in -- but not past the end of
        // the file, so that only the bound on the list stops the read.
        const size_t size_at = index.back().first + 4;
        const Bytes stated = le32(static_cast<uint32_t>(index.back().second) + 64);
        std::copy(stated.begin(), stated.end(), movi.begin() + size_at);
    }

    // The 'movi' FOURCC, which an offset from the start of the list counts
    // from, sits four bytes before the list's contents.
    const uint64_t movi_contents = 12 + hdrl.size() + info.size() + 12;
    const uint64_t offset_base = options.base == IndexBase::FromMoviList ? 4 : movi_contents;

    Bytes idx1;
    for (uint32_t i = 0; i < index.size(); ++i) {
        idx1 = idx1 + text(audio_id.c_str()) + le32(0x10)
                    + le32(static_cast<uint32_t>(offset_base + index[i].first))
                    + le32(index[i].second);
    }

    // Anything outside the 'movi' list is not audio, whatever its FOURCC says.
    // A file that ends in a half-written chunk is the usual way this happens.
    const Bytes junk = options.trailing_junk ? chunk(audio_id.c_str(), audioChunkData(0xBAD0)) : Bytes{};

    Bytes movi_list = list("movi", movi);
    if (options.truncated_movi) {
        // The list states the size it was going to be; the file stops short.
        const Bytes stated = le32(static_cast<uint32_t>(movi_list.size()) + 4096);
        std::copy(stated.begin(), stated.end(), movi_list.begin() + 4);
    }

    const Bytes body = text("AVI ")
                     + (options.info_first ? info + hdrl : hdrl + info) + movi_list
                     + (with_index && !options.truncated_movi ? chunk("idx1", idx1) : Bytes{}) + junk;
    built.file = text("RIFF") + le32(static_cast<uint32_t>(body.size())) + body;

    for (const auto& entry : index) {
        built.audio_offsets.push_back(movi_contents + entry.first);
    }
    return built;
}

/// An OpenDML file: the audio's first half in the 'AVI ' form and the rest in
/// an 'AVIX' continuation, indexed by a super index and two 'ix00' chunks.
Bytes buildOpenDmlAvi()
{
    const uint32_t half = kAudioChunks / 2;
    // The super index is written before its contents are known, so it is
    // built twice: once to learn its size, once with the real offsets.
    auto superIndex = [](uint64_t first_at, uint32_t first_size, uint64_t second_at,
                         uint32_t second_size) {
        return chunk("indx", le16(4) + Bytes{0x00, 0x00} + le32(2) + text("00wb") + Bytes(12, 0)
                           + le64(first_at) + le32(first_size) + le32(0)
                           + le64(second_at) + le32(second_size) + le32(0));
    };
    auto standardIndex = [](uint64_t base, const std::vector<std::pair<uint64_t, uint32_t>>& chunks) {
        Bytes body = le16(2) + Bytes{0x00, 0x01} + le32(static_cast<uint32_t>(chunks.size()))
                   + text("00wb") + le64(base) + le32(0);
        for (const auto& entry : chunks) {
            body = body + le32(static_cast<uint32_t>(entry.first - base)) + le32(entry.second);
        }
        return chunk("ix00", body);
    };

    for (int pass = 0; pass < 2; ++pass) {
        static uint64_t ix_first_at = 0;
        static uint32_t ix_first_size = 0;
        static uint64_t ix_second_at = 0;
        static uint32_t ix_second_size = 0;

        const Bytes audio_strl =
            list("strl", strh("auds", kBlockAlign, kRate * kBlockAlign,
                              kAudioChunks * kFramesPerChunk, kBlockAlign)
                       + waveFormat(0x0001, kChannels, kRate, 16, kBlockAlign)
                       + superIndex(ix_first_at, ix_first_size, ix_second_at, ix_second_size));
        const Bytes hdrl = list("hdrl", avih(1, kAudioChunks) + audio_strl);

        // The first form: hdrl, then a movi list of the first half.
        std::vector<std::pair<uint64_t, uint32_t>> first_chunks;
        Bytes first_movi;
        const uint64_t first_movi_at = 12 + hdrl.size() + 12; // RIFF + hdrl + LIST movi headers
        for (uint32_t i = 0; i < half; ++i) {
            const Bytes data = audioChunkData(i * kFramesPerChunk);
            first_chunks.emplace_back(first_movi_at + first_movi.size() + 8,
                                      static_cast<uint32_t>(data.size()));
            first_movi = first_movi + chunk("00wb", data);
        }
        const uint64_t first_ix_at = first_movi_at + first_movi.size();
        const Bytes first_ix = standardIndex(first_chunks.front().first, first_chunks);
        const Bytes first_body = text("AVI ") + hdrl + list("movi", first_movi + first_ix);
        const Bytes first_form = text("RIFF") + le32(static_cast<uint32_t>(first_body.size())) + first_body;

        // The continuation: another movi list with the rest.
        std::vector<std::pair<uint64_t, uint32_t>> second_chunks;
        Bytes second_movi;
        const uint64_t second_movi_at = first_form.size() + 12 + 12;
        for (uint32_t i = half; i < kAudioChunks; ++i) {
            const Bytes data = audioChunkData(i * kFramesPerChunk);
            second_chunks.emplace_back(second_movi_at + second_movi.size() + 8,
                                       static_cast<uint32_t>(data.size()));
            second_movi = second_movi + chunk("00wb", data);
        }
        const uint64_t second_ix_at = second_movi_at + second_movi.size();
        const Bytes second_ix = standardIndex(second_chunks.front().first, second_chunks);
        const Bytes second_body = text("AVIX") + list("movi", second_movi + second_ix);
        const Bytes second_form = text("RIFF") + le32(static_cast<uint32_t>(second_body.size())) + second_body;

        if (pass == 0) {
            // A super index entry points at the 'ix00' chunk and gives its
            // size. Writers differ over whether that size counts the chunk's
            // own eight-byte header, so one entry is written each way.
            ix_first_at = first_ix_at;
            ix_first_size = static_cast<uint32_t>(first_ix.size());
            ix_second_at = second_ix_at;
            ix_second_size = static_cast<uint32_t>(second_ix.size()) - 8;
            continue;
        }
        return first_form + second_form;
    }
    return {};
}

/// The first channel of the next @p frames frames.
std::vector<AudioSample> readFirstChannel(DemuxedStream& stream, size_t frames)
{
    std::vector<AudioSample> buffer(frames * 2);
    size_t filled = 0;
    for (int attempt = 0; attempt < 128 && filled < buffer.size() && !stream.eof(); ++attempt) {
        filled += stream.getData((buffer.size() - filled) * sizeof(AudioSample),
                                 buffer.data() + filled) / sizeof(AudioSample);
    }
    std::vector<AudioSample> first;
    for (size_t i = 0; i + 1 < filled; i += 2) {
        first.push_back(buffer[i]);
    }
    return first;
}

class HeadersTest : public TestCase {
public:
    HeadersTest() : TestCase("An AVI's audio stream is read from its hdrl list") {}

protected:
    void runTest() override
    {
        const Built built = buildAvi();
        ChunkDemuxer demuxer(std::make_unique<MemoryIOHandler>(built.file.data(), built.file.size()));
        ASSERT_TRUE(demuxer.parseContainer(), "the file parses");
        ASSERT_TRUE(demuxer.getContainerName() == "RIFF/AVI", "it says it is an AVI");

        const std::vector<StreamInfo> streams = demuxer.getStreams();
        ASSERT_EQUALS(size_t{1}, streams.size(), "only the audio stream is listed");
        ASSERT_TRUE(streams.front().codec_name == "pcm", "PCM by its WAVE format tag");
        ASSERT_EQUALS(kRate, streams.front().sample_rate, "at the rate the WAVEFORMATEX states");
        ASSERT_EQUALS(uint16_t{kChannels}, streams.front().channels, "in stereo");
        ASSERT_EQUALS(uint64_t{2000}, demuxer.getDuration(), "two seconds long");

        // Times come from the stream header: every chunk is a tenth of a
        // second of samples, whatever sits between them in the file.
        for (uint32_t i = 0; i < kAudioChunks; ++i) {
            const MediaChunk chunk = demuxer.readChunk();
            ASSERT_EQUALS(size_t{kFramesPerChunk * kBlockAlign}, chunk.data.size(),
                          "chunk " + std::to_string(i) + " is whole");
            ASSERT_EQUALS(uint64_t{i} * kFramesPerChunk, chunk.timestamp_samples,
                          "and starts where the samples before it end");
        }
        ASSERT_TRUE(demuxer.readChunk().data.empty(), "and then the file ends");
        ASSERT_TRUE(demuxer.isEOF(), "which it says");
    }
};

class PlaybackTest : public TestCase {
public:
    PlaybackTest() : TestCase("An AVI plays its audio in order, and seeks to the sample") {}

protected:
    void runTest() override
    {
        for (bool in_rec : {false, true}) {
            const std::string what = in_rec ? "in 'rec ' lists: " : "plain: ";
            AviOptions options;
            options.in_rec = in_rec;
            const Built built = buildAvi(options);
            DemuxedStream stream(std::make_unique<MemoryIOHandler>(built.file.data(), built.file.size()),
                                 TagLib::String("counter.avi"));
            ASSERT_EQUALS(kRate, stream.getRate(), what + "opened at the file's rate");
            ASSERT_EQUALS(2u, stream.getChannels(), what + "in stereo");

            const std::vector<AudioSample> start = readFirstChannel(stream, 3 * kFramesPerChunk);
            ASSERT_EQUALS(size_t{3 * kFramesPerChunk}, start.size(), what + "audio plays");
            for (size_t i = 0; i < start.size(); ++i) {
                if ((start[i] >> 16) != counterAt(i)) {
                    ASSERT_EQUALS(counterAt(i), start[i] >> 16,
                                  what + "sample " + std::to_string(i) + " is in order");
                }
            }

            // The index says which chunk holds the target, and the stream
            // trims the samples before it.
            for (unsigned long target_ms : {500UL, 1234UL, 1900UL}) {
                stream.seekTo(target_ms);
                const std::vector<AudioSample> after = readFirstChannel(stream, 8);
                ASSERT_EQUALS(size_t{8}, after.size(), what + "audio follows the seek");
                const uint64_t target = target_ms * kRate / 1000;
                ASSERT_EQUALS(counterAt(target), after.front() >> 16,
                              what + "a seek to " + std::to_string(target_ms) + " ms plays sample "
                              + std::to_string(target) + " first");
            }
        }
    }
};

class SeekLandingTest : public TestCase {
public:
    SeekLandingTest() : TestCase("A seek lands on the chunk holding the target, never a later one") {}

protected:
    void runTest() override
    {
        // DemuxedStream drops the audio between the landing and the target, so
        // it can hide a landing that is too late by seeking again. The
        // demuxer's own contract is that the chunk it lands on holds the
        // target sample.
        const Built built = buildAvi();
        ChunkDemuxer demuxer(std::make_unique<MemoryIOHandler>(built.file.data(), built.file.size()));
        ASSERT_TRUE(demuxer.parseContainer(), "the file parses");

        for (unsigned long target_ms : {0UL, 99UL, 100UL, 550UL, 1234UL, 1999UL}) {
            ASSERT_TRUE(demuxer.seekTo(target_ms), "the seek is taken");
            const MediaChunk chunk = demuxer.readChunk();
            ASSERT_FALSE(chunk.data.empty(), "and reads audio");
            const uint64_t target = target_ms * kRate / 1000;
            const uint64_t first = chunk.timestamp_samples;
            const std::string what = "a seek to " + std::to_string(target_ms) + " ms";
            ASSERT_TRUE(first <= target, what + " lands at or before the target");
            ASSERT_TRUE(first + kFramesPerChunk > target, what + " lands on the chunk holding it");
        }
    }
};

class StrayChunkTest : public TestCase {
public:
    StrayChunkTest() : TestCase("Audio outside the movi list is not played") {}

protected:
    void runTest() override
    {
        AviOptions options;
        options.trailing_junk = true;
        const Built built = buildAvi(options);
        ChunkDemuxer demuxer(std::make_unique<MemoryIOHandler>(built.file.data(), built.file.size()));
        ASSERT_TRUE(demuxer.parseContainer(), "the file parses");

        uint32_t chunks = 0;
        while (chunks < kAudioChunks + 4) {
            const MediaChunk chunk = demuxer.readChunk();
            if (chunk.data.empty()) {
                break;
            }
            ++chunks;
        }
        ASSERT_EQUALS(kAudioChunks, chunks, "only the chunks inside the movi list are audio");
        ASSERT_EQUALS(uint64_t{2000}, demuxer.getDuration(), "and the duration is of those alone");
    }
};

class TruncatedChunkTest : public TestCase {
public:
    TruncatedChunkTest() : TestCase("A chunk that reaches past its movi list is not played") {}

protected:
    void runTest() override
    {
        // The bytes after the list are the index, not audio: a chunk whose
        // size runs past the end of the list it is in cannot be read, and
        // reading what happens to follow would play the index as sound.
        AviOptions options;
        options.overlong_last_chunk = true;
        const Built built = buildAvi(options);
        ChunkDemuxer demuxer(std::make_unique<MemoryIOHandler>(built.file.data(), built.file.size()));
        ASSERT_TRUE(demuxer.parseContainer(), "the file parses");

        uint32_t chunks = 0;
        uint64_t samples = 0;
        while (chunks < kAudioChunks + 4) {
            const MediaChunk chunk = demuxer.readChunk();
            if (chunk.data.empty()) {
                break;
            }
            ASSERT_EQUALS(size_t{kFramesPerChunk * kBlockAlign}, chunk.data.size(),
                          "every chunk read is one whole chunk of audio");
            samples += chunk.data.size() / kBlockAlign;
            ++chunks;
        }
        ASSERT_EQUALS(kAudioChunks - 1, chunks, "the chunk that overruns the list is dropped");
        ASSERT_EQUALS(uint64_t{(kAudioChunks - 1) * kFramesPerChunk}, samples,
                      "and nothing outside the list is played as audio");
    }
};

class AudioNotFirstStreamTest : public TestCase {
public:
    AudioNotFirstStreamTest() : TestCase("The audio of an AVI whose first stream is video plays and seeks") {}

protected:
    void runTest() override
    {
        // The usual way round: video is stream 0, so the audio is stream 1 and
        // its chunks are named '01wb'. The number that names those chunks is
        // not the id the rest of PsyMP3 knows the stream by, and confusing the
        // two left every seek reporting that it had landed at the start of the
        // file, which threw away all the audio up to the seek target.
        AviOptions options;
        options.audio_stream = 1;
        const Built built = buildAvi(options);
        DemuxedStream stream(std::make_unique<MemoryIOHandler>(built.file.data(), built.file.size()),
                             TagLib::String("videofirst.avi"));
        ASSERT_EQUALS(kRate, stream.getRate(), "opened at the file's rate");

        const std::vector<AudioSample> start = readFirstChannel(stream, kFramesPerChunk);
        ASSERT_EQUALS(size_t{kFramesPerChunk}, start.size(), "audio plays");
        ASSERT_EQUALS(counterAt(0), start.front() >> 16, "from the first sample");

        for (unsigned long target_ms : {700UL, 1500UL}) {
            stream.seekTo(target_ms);
            const std::vector<AudioSample> after = readFirstChannel(stream, 8);
            ASSERT_EQUALS(size_t{8}, after.size(),
                          "audio follows a seek to " + std::to_string(target_ms) + " ms");
            ASSERT_EQUALS(counterAt(target_ms * kRate / 1000), after.front() >> 16,
                          "and it is the audio the seek asked for");
        }
    }
};

class TruncatedFormTest : public TestCase {
public:
    TruncatedFormTest() : TestCase("An AVI cut short mid-write plays the audio it does have") {}

protected:
    void runTest() override
    {
        // An interrupted recording states the size the 'movi' list was going
        // to be. Refusing the file over that loses every chunk that is there.
        AviOptions options;
        options.truncated_movi = true;
        const Built built = buildAvi(options);
        ChunkDemuxer demuxer(std::make_unique<MemoryIOHandler>(built.file.data(), built.file.size()));
        ASSERT_TRUE(demuxer.parseContainer(), "the file still opens");

        uint32_t chunks = 0;
        while (chunks < kAudioChunks + 4) {
            const MediaChunk chunk = demuxer.readChunk();
            if (chunk.data.empty()) {
                break;
            }
            ASSERT_EQUALS(uint64_t{chunks} * kFramesPerChunk, chunk.timestamp_samples,
                          "chunk " + std::to_string(chunks) + " keeps its place");
            ++chunks;
        }
        ASSERT_EQUALS(kAudioChunks, chunks, "and every chunk written before the cut plays");
    }
};

class TagsBeforeHeadersTest : public TestCase {
public:
    TagsBeforeHeadersTest() : TestCase("A LIST INFO before the headers still tags the stream") {}

protected:
    void runTest() override
    {
        for (bool info_first : {false, true}) {
            AviOptions options;
            options.info_first = info_first;
            const Built built = buildAvi(options);
            ChunkDemuxer demuxer(std::make_unique<MemoryIOHandler>(built.file.data(), built.file.size()));
            ASSERT_TRUE(demuxer.parseContainer(), "the file parses");
            const std::vector<StreamInfo> streams = demuxer.getStreams();
            ASSERT_EQUALS(size_t{1}, streams.size(), "the audio stream is listed");
            const std::string where = info_first ? "before the headers: " : "after the headers: ";
            ASSERT_TRUE(streams.front().title == "A counting file", where + "INAM is the title");
            ASSERT_TRUE(streams.front().artist == "PsyMP3", where + "IART is the artist");
        }
    }
};

class FileRelativeIndexTest : public TestCase {
public:
    FileRelativeIndexTest() : TestCase("An idx1 counting from the start of the file still seeks") {}

protected:
    void runTest() override
    {
        // Some writers state idx1 offsets from the start of the file rather
        // than of the 'movi' list, which the AVIOLDINDEX reference allows for.
        AviOptions options;
        options.base = IndexBase::FromFileStart;
        const Built built = buildAvi(options);
        DemuxedStream stream(std::make_unique<MemoryIOHandler>(built.file.data(), built.file.size()),
                             TagLib::String("absolute.avi"));
        stream.seekTo(700);
        const std::vector<AudioSample> after = readFirstChannel(stream, 8);
        ASSERT_EQUALS(size_t{8}, after.size(), "audio follows the seek");
        ASSERT_EQUALS(counterAt(700ULL * kRate / 1000), after.front() >> 16,
                      "and it is the audio the seek asked for");
    }
};

class OpenDmlTest : public TestCase {
public:
    OpenDmlTest() : TestCase("An OpenDML AVI plays across its AVIX segments") {}

protected:
    void runTest() override
    {
        const Bytes file = buildOpenDmlAvi();
        ASSERT_TRUE(file.size() > 1024, "the fixture was written");
        DemuxedStream stream(std::make_unique<MemoryIOHandler>(file.data(), file.size()),
                             TagLib::String("opendml.avi"));
        ASSERT_EQUALS(kRate, stream.getRate(), "opened");

        // The second movi list starts half way through, so reading past that
        // point crosses from one RIFF form into the next.
        const std::vector<AudioSample> whole = readFirstChannel(stream, kAudioChunks * kFramesPerChunk);
        ASSERT_EQUALS(size_t{kAudioChunks * kFramesPerChunk}, whole.size(), "all of it plays");
        for (size_t i = 0; i < whole.size(); ++i) {
            if ((whole[i] >> 16) != counterAt(i)) {
                ASSERT_EQUALS(counterAt(i), whole[i] >> 16,
                              "sample " + std::to_string(i) + " is in order");
            }
        }

        // The super index and its 'ix00' chunks cover both halves.
        // The landing is checked at the demuxer as well: a sub-index that
        // loses its last entry still plays the right audio, because the
        // stream reads on from an earlier chunk, so only the chunk the seek
        // lands on shows it.
        ChunkDemuxer demuxer(std::make_unique<MemoryIOHandler>(file.data(), file.size()));
        ASSERT_TRUE(demuxer.parseContainer(), "the file parses");
        for (unsigned long target_ms : {1500UL, 1900UL, 1999UL}) {
            ASSERT_TRUE(demuxer.seekTo(target_ms), "the seek is taken");
            const MediaChunk chunk = demuxer.readChunk();
            ASSERT_FALSE(chunk.data.empty(), "and reads audio");
            const uint64_t target = target_ms * kRate / 1000;
            const std::string what = "a seek to " + std::to_string(target_ms) + " ms";
            ASSERT_TRUE(chunk.timestamp_samples <= target, what + " lands at or before the target");
            ASSERT_TRUE(chunk.timestamp_samples + kFramesPerChunk > target,
                        what + " lands on the chunk holding it");
        }

        // 1900 ms is in the last chunk of the second sub-index, which is the
        // entry a size field counted one way rather than the other drops.
        for (unsigned long target_ms : {1500UL, 1900UL}) {
            stream.seekTo(target_ms);
            const std::vector<AudioSample> after = readFirstChannel(stream, 8);
            ASSERT_EQUALS(size_t{8}, after.size(),
                          "audio follows a seek to " + std::to_string(target_ms) + " ms");
            ASSERT_EQUALS(counterAt(target_ms * kRate / 1000), after.front() >> 16,
                          "which lands on the target sample");
        }
    }
};

class VariableSampleSizeTest : public TestCase {
public:
    VariableSampleSizeTest() : TestCase("With dwSampleSize zero, each chunk is one stream sample") {}

protected:
    void runTest() override
    {
        // MP3 and AC-3 are stored this way: the stream header states no
        // sample size, and a chunk lasts dwScale over dwRate seconds.
        const Bytes audio_strl =
            list("strl", strh("auds", 1, 38, 20, 0)
                       + waveFormat(0x0055, kChannels, kRate, 0, 1,
                                    Bytes{1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
        Bytes movi;
        Bytes idx1;
        for (uint32_t i = 0; i < 20; ++i) {
            idx1 = idx1 + text("00wb") + le32(0x10) + le32(static_cast<uint32_t>(movi.size()) + 4)
                        + le32(417);
            movi = movi + chunk("00wb", Bytes(417, static_cast<uint8_t>(i)));
        }
        const Bytes hdrl = list("hdrl", avih(1, 20) + audio_strl);
        const Bytes body = text("AVI ") + hdrl + list("movi", movi) + chunk("idx1", idx1);
        const Bytes file = text("RIFF") + le32(static_cast<uint32_t>(body.size())) + body;

        ChunkDemuxer demuxer(std::make_unique<MemoryIOHandler>(file.data(), file.size()));
        ASSERT_TRUE(demuxer.parseContainer(), "the file parses");
        ASSERT_TRUE(demuxer.getStreams().front().codec_name == "mp3", "MP3 by its format tag");
        ASSERT_EQUALS(uint64_t{526}, demuxer.getDuration(), "twenty chunks at 38 a second");

        for (uint32_t i = 0; i < 3; ++i) {
            const MediaChunk chunk = demuxer.readChunk();
            ASSERT_EQUALS(size_t{417}, chunk.data.size(), "the chunk is whole");
            ASSERT_EQUALS(uint64_t{i} * kRate / 38, chunk.timestamp_samples,
                          "chunk " + std::to_string(i) + " plays a chunk-length in");
        }
    }
};

class NoIndexTest : public TestCase {
public:
    NoIndexTest() : TestCase("Without an index an AVI still plays, and refuses to seek") {}

protected:
    void runTest() override
    {
        AviOptions options;
        options.with_index = false;
        const Built built = buildAvi(options);
        ChunkDemuxer demuxer(std::make_unique<MemoryIOHandler>(built.file.data(), built.file.size()));
        ASSERT_TRUE(demuxer.parseContainer(), "the file parses");
        ASSERT_FALSE(demuxer.seekTo(500), "a seek is refused rather than guessed at");
        const MediaChunk chunk = demuxer.readChunk();
        ASSERT_EQUALS(size_t{kFramesPerChunk * kBlockAlign}, chunk.data.size(), "playback carries on");
        ASSERT_EQUALS(uint64_t{0}, chunk.timestamp_samples, "from the start");
    }
};

} // namespace

int main()
{
    registerAllCodecs();
    registerAllDemuxers();

    TestSuite suite("AVI demuxing");
    suite.addTest(std::make_unique<HeadersTest>());
    suite.addTest(std::make_unique<PlaybackTest>());
    suite.addTest(std::make_unique<AudioNotFirstStreamTest>());
    suite.addTest(std::make_unique<SeekLandingTest>());
    suite.addTest(std::make_unique<TruncatedFormTest>());
    suite.addTest(std::make_unique<TagsBeforeHeadersTest>());
    suite.addTest(std::make_unique<StrayChunkTest>());
    suite.addTest(std::make_unique<TruncatedChunkTest>());
    suite.addTest(std::make_unique<FileRelativeIndexTest>());
    suite.addTest(std::make_unique<OpenDmlTest>());
    suite.addTest(std::make_unique<VariableSampleSizeTest>());
    suite.addTest(std::make_unique<NoIndexTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
