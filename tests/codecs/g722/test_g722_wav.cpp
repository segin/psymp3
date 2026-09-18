/*
 * test_g722_wav.cpp - G.722 in WAVE files
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Rec. G.722 defines no container, so a WAV header can describe the same
 * octets several ways. Each octet codes two 16 kHz samples (§1.4.4), and
 * every one of those descriptions has to come out that way.
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/MemoryIOHandler.h"
#include "g722_fixture.h"

using namespace TestFramework;
using PsyMP3::IO::MemoryIOHandler;

namespace {

constexpr size_t kOctets = sizeof(kG722Sine1600);

void putLE(std::vector<uint8_t>& out, uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; ++i) {
        out.push_back(static_cast<uint8_t>(value >> (8 * i)));
    }
}

struct WavFormat {
    uint16_t tag = 0x028F;
    uint32_t rate = 16000;
    uint32_t bytes_per_second = 8000;
    uint16_t bits = 4;
    bool extensible = false;   // tag 0xFFFE, with `tag` as the SubFormat
};

/// A mono WAV around the fixture's octets.
std::vector<uint8_t> buildWav(const WavFormat& format)
{
    std::vector<uint8_t> fmt;
    putLE(fmt, format.extensible ? 0xFFFE : format.tag, 2);
    putLE(fmt, 1, 2);                        // channels
    putLE(fmt, format.rate, 4);
    putLE(fmt, format.bytes_per_second, 4);
    putLE(fmt, 1, 2);                        // block align
    putLE(fmt, format.bits, 2);
    if (format.extensible) {
        putLE(fmt, 22, 2);                   // cbSize
        putLE(fmt, format.bits, 2);          // wValidBitsPerSample
        putLE(fmt, 0x4, 4);                  // dwChannelMask: front centre
        putLE(fmt, format.tag, 4);           // SubFormat: the tag, then the
        const uint8_t suffix[] = {0x00, 0x00, 0x10, 0x00, 0x80, 0x00,
                                  0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
        fmt.insert(fmt.end(), std::begin(suffix), std::end(suffix));
    }

    std::vector<uint8_t> body{'W', 'A', 'V', 'E', 'f', 'm', 't', ' '};
    putLE(body, static_cast<uint32_t>(fmt.size()), 4);
    body.insert(body.end(), fmt.begin(), fmt.end());
    body.insert(body.end(), {'d', 'a', 't', 'a'});
    putLE(body, static_cast<uint32_t>(kOctets), 4);
    body.insert(body.end(), std::begin(kG722Sine1600), std::end(kG722Sine1600));

    std::vector<uint8_t> file{'R', 'I', 'F', 'F'};
    putLE(file, static_cast<uint32_t>(body.size()), 4);
    file.insert(file.end(), body.begin(), body.end());
    return file;
}

/// Where the octets start in buildWav's output.
uint64_t dataOffset(const WavFormat& format)
{
    return 12 + 8 + (format.extensible ? 40 : 16) + 8;
}

std::unique_ptr<ChunkDemuxer> open(const WavFormat& format)
{
    const std::vector<uint8_t> file = buildWav(format);
    auto demuxer = std::make_unique<ChunkDemuxer>(
        std::make_unique<MemoryIOHandler>(file.data(), file.size()));
    if (!demuxer->parseContainer()) {
        return nullptr;
    }
    return demuxer;
}

/// Checks what every G.722 description must agree on: the codec, the length,
/// and where a seek lands.
void checkG722(const WavFormat& format, const std::string& what)
{
    auto demuxer = open(format);
    ASSERT_TRUE(demuxer != nullptr, what + ": parses");
    const std::vector<StreamInfo> streams = demuxer->getStreams();
    ASSERT_EQUALS(size_t{1}, streams.size(), what + ": one stream");
    ASSERT_EQUALS(std::string("g722"), streams[0].codec_name, what + ": is G.722");
    ASSERT_EQUALS(uint64_t{2 * kOctets}, streams[0].duration_samples,
                  what + ": two samples per octet");
    ASSERT_EQUALS(uint64_t{kOctets / 8}, streams[0].duration_ms,
                  what + ": 8000 octets a second");

    ASSERT_TRUE(demuxer->seekTo(50), what + ": seeks");
    const MediaChunk chunk = demuxer->readChunk();
    ASSERT_TRUE(!chunk.data.empty(), what + ": reads after the seek");
    ASSERT_EQUALS(dataOffset(format) + 400, chunk.file_offset,
                  what + ": 50 ms is 400 octets in");
    ASSERT_EQUALS(uint64_t{800}, chunk.timestamp_samples, what + ": and 800 samples");
}

class TagsTest : public TestCase {
public:
    TagsTest() : TestCase("Both G.722 format tags are recognised") {}

protected:
    void runTest() override
    {
        WavFormat format;
        checkG722(format, "tag 0x028F");
        format.tag = 0x0065;
        checkG722(format, "tag 0x0065");
    }
};

class ExtensibleTest : public TestCase {
public:
    ExtensibleTest() : TestCase("G.722 inside WAVE_FORMAT_EXTENSIBLE is counted as G.722") {}

protected:
    void runTest() override
    {
        WavFormat format;
        format.extensible = true;
        checkG722(format, "extensible, 4 bits");
        // WAVEFORMATEXTENSIBLE wants a whole-byte container size, so a writer
        // may well say 8 here.
        format.bits = 8;
        checkG722(format, "extensible, 8 bits");
    }
};

class HeaderRate8000Test : public TestCase {
public:
    HeaderRate8000Test() : TestCase("A G.722 WAV that declares 8000 Hz still plays at 16 kHz") {}

protected:
    void runTest() override
    {
        WavFormat format;
        format.rate = 8000;
        checkG722(format, "8000 Hz header");

        auto demuxer = open(format);
        ASSERT_TRUE(demuxer != nullptr, "parses");
        const StreamInfo stream = demuxer->getStreams().at(0);
        ASSERT_EQUALS(uint32_t{16000}, stream.sample_rate, "the stream is 16 kHz");
        ASSERT_EQUALS(uint64_t{kOctets / 8}, demuxer->getDuration(), "and so is its duration");

        G722Codec codec(stream);
        ASSERT_TRUE(codec.initialize(), "the codec takes it");
        const MediaChunk chunk = demuxer->readChunk();
        const AudioFrame frame = codec.decode(chunk);
        ASSERT_EQUALS(uint32_t{16000}, frame.sample_rate, "decoded at 16 kHz");
        ASSERT_EQUALS(2 * chunk.data.size(), frame.samples.size(),
                      "both sub-bands, two samples an octet");
    }
};

} // namespace

int test_g722_wav_main()
{
    TestSuite suite("G.722 in WAVE");
    suite.addTest(std::make_unique<TagsTest>());
    suite.addTest(std::make_unique<ExtensibleTest>());
    suite.addTest(std::make_unique<HeaderRate8000Test>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
