/*
 * test_adts_demuxer.cpp - ADTSDemuxer on synthetic ADTS data
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The frames are built here, header field by header field, so every test
 * knows exactly what the demuxer should find: no fixture files and no
 * encoder. The payloads are not real AAC; the demuxer never looks inside.
 */

#include "psymp3.h"
#include "test_framework.h"

using PsyMP3::Demuxer::ADTS::ADTSDemuxer;
using PsyMP3::Demuxer::ADTS::ADTSHeader;
using PsyMP3::IO::MemoryIOHandler;

namespace {

/** @brief Fields for one synthetic frame. */
struct FrameSpec {
    unsigned profile = 1;       ///< 1 = AAC LC (object type 2)
    unsigned sf_index = 4;      ///< 44100 Hz
    unsigned channels = 2;      ///< channel_configuration
    size_t payload = 100;       ///< raw_data_block bytes
    bool crc = false;           ///< add the two CRC bytes
    bool mpeg2 = false;         ///< ID bit
    unsigned blocks = 1;        ///< raw data blocks in the frame
    uint8_t seed = 0;           ///< payload pattern start
};

/**
 * @brief The payload bytes for a frame: a pattern below 0xF0, so no false
 *        sync word can appear in it.
 * @param spec The frame.
 * @return spec.payload bytes.
 */
std::string payloadOf(const FrameSpec& spec)
{
    std::string p(spec.payload, '\0');
    for (size_t i = 0; i < p.size(); ++i) {
        p[i] = static_cast<char>((spec.seed + i) % 0xE0);
    }
    return p;
}

/**
 * @brief Encode one ADTS frame: header (with zeroed CRC if asked) and payload.
 * @param spec The frame.
 * @return Its bytes.
 */
std::string makeFrame(const FrameSpec& spec)
{
    const size_t header = spec.crc ? 9 : 7;
    const size_t len = header + spec.payload;
    std::string f(header, '\0');
    f[0] = static_cast<char>(0xFF);
    f[1] = static_cast<char>(0xF0 | (spec.mpeg2 ? 0x08 : 0) | (spec.crc ? 0 : 1));
    f[2] = static_cast<char>((spec.profile << 6) | (spec.sf_index << 2) | ((spec.channels >> 2) & 1));
    f[3] = static_cast<char>(((spec.channels & 3) << 6) | ((len >> 11) & 3));
    f[4] = static_cast<char>((len >> 3) & 0xFF);
    f[5] = static_cast<char>(((len & 7) << 5) | 0x1F);
    f[6] = static_cast<char>(0xFC | ((spec.blocks - 1) & 3));
    return f + payloadOf(spec);
}

/**
 * @brief A demuxer reading @p data from memory.
 * @param data The bytes.
 * @return The demuxer, not yet parsed.
 */
std::unique_ptr<ADTSDemuxer> demuxerFor(const std::string& data)
{
    return std::make_unique<ADTSDemuxer>(std::make_unique<MemoryIOHandler>(data.data(), data.size()));
}

/** @brief A memory handler that calls itself live, as internet radio is. */
class LiveMemoryIOHandler : public MemoryIOHandler {
public:
    /**
     * @brief Copy @p data into the handler.
     * @param data The bytes.
     * @param mime The Content-Type it claims, if any.
     */
    explicit LiveMemoryIOHandler(const std::string& data, std::string mime = std::string())
        : MemoryIOHandler(data.data(), data.size()), m_mime(std::move(mime)) {}

    /**
     * @brief Report a live stream.
     * @return true.
     */
    bool isLiveStream() const override { return true; }

    /**
     * @brief The Content-Type given at construction.
     * @return It.
     */
    std::string getMimeType() const override { return m_mime; }

private:
    std::string m_mime;
};

/** @brief Header parsing and the AudioSpecificConfig. */
class HeaderTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    HeaderTest() : TestCase("ADTS header fields and AudioSpecificConfig") {}
protected:
    /** @brief Check every field, the rejections, and two well-known configs. */
    void runTest() override {
        FrameSpec spec;
        spec.payload = 300;
        const std::string f = makeFrame(spec);
        ADTSHeader h;
        ASSERT_TRUE(ADTSDemuxer::parseHeader(reinterpret_cast<const uint8_t*>(f.data()), f.size(), h),
                    "a valid header parses");
        ASSERT_EQUALS(1u, h.profile, "profile");
        ASSERT_EQUALS(44100u, h.sample_rate, "sample rate");
        ASSERT_EQUALS(2u, h.channels, "channels");
        ASSERT_EQUALS(307u, h.frame_length, "frame length includes the header");
        ASSERT_EQUALS(7u, h.header_length, "no CRC: 7-byte header");
        ASSERT_EQUALS(1u, h.raw_blocks, "one raw data block");
        ASSERT_FALSE(h.mpeg2, "MPEG-4 ID");

        FrameSpec crc;
        crc.crc = true;
        crc.mpeg2 = true;
        crc.channels = 7;
        crc.sf_index = 3;
        const std::string g = makeFrame(crc);
        ASSERT_TRUE(ADTSDemuxer::parseHeader(reinterpret_cast<const uint8_t*>(g.data()), g.size(), h),
                    "a CRC header parses");
        ASSERT_EQUALS(9u, h.header_length, "CRC: 9-byte header");
        ASSERT_TRUE(h.mpeg2, "MPEG-2 ID");
        ASSERT_EQUALS(8u, h.channels, "channel configuration 7 is 7.1");
        ASSERT_EQUALS(48000u, h.sample_rate, "index 3 is 48 kHz");

        std::string bad = f;
        bad[1] = static_cast<char>(0xFB); // layer 01: MPEG audio, not ADTS
        ASSERT_FALSE(ADTSDemuxer::parseHeader(reinterpret_cast<const uint8_t*>(bad.data()), bad.size(), h),
                     "an MP3 frame header is not ADTS");
        bad = f;
        bad[2] = static_cast<char>((1 << 6) | (13 << 2)); // reserved rate index
        ASSERT_FALSE(ADTSDemuxer::parseHeader(reinterpret_cast<const uint8_t*>(bad.data()), bad.size(), h),
                     "a reserved sample rate index is refused");
        ASSERT_FALSE(ADTSDemuxer::parseHeader(reinterpret_cast<const uint8_t*>(f.data()), 6, h),
                     "six bytes are too few");

        ASSERT_TRUE(ADTSDemuxer::parseHeader(reinterpret_cast<const uint8_t*>(f.data()), f.size(), h),
                    "reparse");
        const std::vector<uint8_t> asc = ADTSDemuxer::makeAudioSpecificConfig(h);
        ASSERT_EQUALS(size_t(2), asc.size(), "two-byte config");
        ASSERT_EQUALS(0x12, int(asc[0]), "AAC LC 44.1 kHz stereo: 0x12");
        ASSERT_EQUALS(0x10, int(asc[1]), "AAC LC 44.1 kHz stereo: 0x10");
    }
};

/** @brief A file: an ID3v2 tag, then frames of varying length. */
class FileTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    FileTest() : TestCase("File: tag skipped, headers stripped, duration, seek, end") {}
protected:
    /** @brief Demux five frames after a tag and check every chunk and the seek. */
    void runTest() override {
        // ID3v2.4, ten bytes of header and a declared body of 20.
        std::string data = std::string("ID3\x04\x00\x00\x00\x00\x00\x14", 10) + std::string(20, 'x');
        std::vector<FrameSpec> specs(5);
        for (size_t i = 0; i < specs.size(); ++i) {
            specs[i].payload = 50 + i * 37;
            specs[i].seed = static_cast<uint8_t>(i * 11);
            specs[i].crc = (i == 2);
            data += makeFrame(specs[i]);
        }
        auto d = demuxerFor(data);
        ASSERT_TRUE(d->parseContainer(), "parses");
        const auto streams = d->getStreams();
        ASSERT_EQUALS(size_t(1), streams.size(), "one stream");
        ASSERT_EQUALS(std::string("aac"), streams[0].codec_name, "codec aac");
        ASSERT_EQUALS(44100u, streams[0].sample_rate, "sample rate");
        ASSERT_EQUALS(uint16_t(2), streams[0].channels, "channels");
        ASSERT_EQUALS(size_t(2), streams[0].codec_data.size(), "AudioSpecificConfig supplied");
        ASSERT_EQUALS(uint64_t(5 * 1024 * 1000 / 44100), d->getDuration(), "duration from five frames");

        for (size_t i = 0; i < specs.size(); ++i) {
            MediaChunk c = d->readChunk();
            const std::string got(c.data.begin(), c.data.end());
            ASSERT_TRUE(got == payloadOf(specs[i]), "chunk is the payload without its header");
            ASSERT_EQUALS(uint64_t(i * 1024), c.timestamp_samples, "chunk timestamp");
        }
        MediaChunk end = d->readChunk();
        ASSERT_TRUE(end.end_of_stream || end.data.empty(), "end after the last frame");
        ASSERT_TRUE(d->isEOF(), "EOF");

        // Frames start at 0, 23.2, 46.4 and 69.7 ms (1024 samples each), so
        // 70 ms is in frame 3, counting from 0; the seek lands a frame early,
        // on frame 2.
        ASSERT_TRUE(d->seekTo(70), "seek");
        ASSERT_FALSE(d->isEOF(), "seek clears EOF");
        MediaChunk c = d->readChunk();
        const std::string got(c.data.begin(), c.data.end());
        ASSERT_TRUE(got == payloadOf(specs[2]), "seek lands one frame before the target's frame");
        ASSERT_EQUALS(uint64_t(2 * 1024), c.timestamp_samples, "landing frame's timestamp");
    }
};

/** @brief Damage between frames. */
class ResyncTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    ResyncTest() : TestCase("Junk between frames is skipped") {}
protected:
    /** @brief Put bytes after the second frame and check the third is still found. */
    void runTest() override {
        FrameSpec a, b, c, e;
        a.seed = 1; b.seed = 2; c.seed = 3; e.seed = 4;
        const std::string data = makeFrame(a) + makeFrame(b) + std::string(13, '\x55') +
                                 makeFrame(c) + makeFrame(e);
        auto d = demuxerFor(data);
        ASSERT_TRUE(d->parseContainer(), "parses");
        (void)d->readChunk();
        (void)d->readChunk();
        MediaChunk third = d->readChunk();
        const std::string got(third.data.begin(), third.data.end());
        ASSERT_TRUE(got == payloadOf(c), "reading resumes at the next frame");
    }
};

/** @brief Internet radio. */
class LiveTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    LiveTest() : TestCase("Live stream: no duration, no seeking, frames until it ends") {}
protected:
    /** @brief Demux a live source and check duration, seeking and every chunk. */
    void runTest() override {
        std::string data;
        std::vector<FrameSpec> specs(4);
        for (size_t i = 0; i < specs.size(); ++i) {
            specs[i].payload = 200 + i;
            specs[i].seed = static_cast<uint8_t>(i);
            specs[i].profile = 1;
            specs[i].sf_index = 3;
            data += makeFrame(specs[i]);
        }
        auto d = std::make_unique<ADTSDemuxer>(std::make_unique<LiveMemoryIOHandler>(data));
        ASSERT_TRUE(d->parseContainer(), "parses");
        ASSERT_EQUALS(uint64_t(0), d->getDuration(), "no duration");
        ASSERT_FALSE(d->seekTo(10), "no seeking");
        for (size_t i = 0; i < specs.size(); ++i) {
            MediaChunk c = d->readChunk();
            const std::string got(c.data.begin(), c.data.end());
            ASSERT_TRUE(got == payloadOf(specs[i]), "live chunk is the payload");
        }
        MediaChunk end = d->readChunk();
        ASSERT_TRUE(end.end_of_stream || end.data.empty(), "ends when the data does");
    }
};

/** @brief A station joined mid-frame, identified by its Content-Type. */
class MimeFallbackTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    MimeFallbackTest() : TestCase("Live stream joined mid-frame: format from Content-Type") {}
protected:
    /**
     * @brief Start the data part-way into a frame: content probing finds
     *        nothing, the MIME type finds ADTS for a live source only, and the
     *        demuxer then syncs on the next whole frame.
     */
    void runTest() override {
        FrameSpec a, b, c;
        a.seed = 7; b.seed = 8; c.seed = 9;
        const std::string data = makeFrame(a).substr(40) + makeFrame(b) + makeFrame(c);

        LiveMemoryIOHandler live(data, "audio/aacp");
        ASSERT_EQUALS(std::string("adts"),
                      PsyMP3::Demuxer::DemuxerFactory::probeFormat(&live, "http://radio.example/stream"),
                      "live stream: format from audio/aacp");

        MemoryIOHandler file(data.data(), data.size());
        ASSERT_EQUALS(std::string(),
                      PsyMP3::Demuxer::DemuxerFactory::probeFormat(&file, "/music/unknown"),
                      "a file gets no MIME fallback");

        auto d = std::make_unique<ADTSDemuxer>(std::make_unique<LiveMemoryIOHandler>(data, "audio/aacp"));
        ASSERT_TRUE(d->parseContainer(), "the demuxer finds the first whole frame");
        MediaChunk first = d->readChunk();
        const std::string got(first.data.begin(), first.data.end());
        ASSERT_TRUE(got == payloadOf(b), "first chunk is the first whole frame");
    }
};

/**
 * @brief An .aac file with an ID3v2 tag in front: format detection looked
 *        past the tag, took ADTS's sync word for MPEG audio, and sent the
 *        file to the MP3 demuxer.
 */
class TaggedFileProbeTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    TaggedFileProbeTest() : TestCase("ADTS after an ID3v2 tag is detected as ADTS, not MP3") {}
protected:
    /** @brief Probe a tagged ADTS file, and a tagged MPEG one for contrast. */
    void runTest() override {
        const std::string tag = std::string("ID3\x04\x00\x00\x00\x00\x00\x14", 10) + std::string(20, 'x');
        FrameSpec a, b;
        b.seed = 3;
        const std::string aac = tag + makeFrame(a) + makeFrame(b);
        MemoryIOHandler aac_file(aac.data(), aac.size());
        ASSERT_EQUALS(std::string("adts"),
                      PsyMP3::Demuxer::DemuxerFactory::probeFormat(&aac_file, "/music/tagged.aac"),
                      "tagged .aac is ADTS");
        MemoryIOHandler aac_noext(aac.data(), aac.size());
        ASSERT_EQUALS(std::string("adts"),
                      PsyMP3::Demuxer::DemuxerFactory::probeFormat(&aac_noext, "/music/tagged"),
                      "by content alone, too");

        // An MPEG-1 Layer III header (128 kbps, 44.1 kHz) after the same tag.
        const std::string mp3 = tag + std::string("\xFF\xFB\x90\x64", 4) + std::string(413, '\0');
        MemoryIOHandler mp3_file(mp3.data(), mp3.size());
        ASSERT_EQUALS(std::string("mp3"),
                      PsyMP3::Demuxer::DemuxerFactory::probeFormat(&mp3_file, "/music/tagged"),
                      "tagged MPEG audio is still MP3");
    }
};

/** @brief Not ADTS at all. */
class RejectTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    RejectTest() : TestCase("Data without two consecutive frames is refused") {}
protected:
    /** @brief A lone header among junk, and plain junk, must not parse. */
    void runTest() override {
        FrameSpec s;
        const std::string lone = std::string(40, '\x10') + makeFrame(s).substr(0, 30) + std::string(400, '\x22');
        ASSERT_FALSE(demuxerFor(lone)->parseContainer(), "a broken frame among junk");
        ASSERT_FALSE(demuxerFor(std::string(2000, '\x33'))->parseContainer(), "no frames at all");
    }
};

} // namespace

/**
 * @brief Run the ADTS demuxer tests; the entry test_aac_suite calls.
 * @return Number of failed tests.
 */
int test_adts_demuxer_main()
{
    TestFramework::TestSuite suite("ADTSDemuxer Tests");
    suite.addTest(std::make_unique<HeaderTest>());
    suite.addTest(std::make_unique<FileTest>());
    suite.addTest(std::make_unique<ResyncTest>());
    suite.addTest(std::make_unique<LiveTest>());
    suite.addTest(std::make_unique<MimeFallbackTest>());
    suite.addTest(std::make_unique<RejectTest>());
    suite.addTest(std::make_unique<TaggedFileProbeTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
