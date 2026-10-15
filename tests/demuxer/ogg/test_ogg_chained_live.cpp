/*
 * test_ogg_chained_live.cpp - OggDemuxer across a chained stream's links
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Internet radio that restarts its encoder for each song sends a chained Ogg
 * stream: one logical stream after another. Live, the demuxer must carry on
 * into each new link under the same stream id and say where it began; a file
 * still ends with its first link. Data: tests/ogg_chain_fixture.h.
 */

#include "psymp3.h"
#include "test_framework.h"
#include "ogg_chain_fixture.h"

using PsyMP3::Demuxer::Ogg::OggDemuxer;
using PsyMP3::IO::MemoryIOHandler;

namespace {

/** @brief A memory handler that calls itself live, as internet radio is. */
class ChainLiveIOHandler : public MemoryIOHandler {
public:
    /**
     * @brief Copy @p size bytes from @p data.
     * @param data The bytes.
     * @param size Their count.
     */
    ChainLiveIOHandler(const void* data, size_t size) : MemoryIOHandler(data, size) {}

    /**
     * @brief Report a live stream.
     * @return true.
     */
    bool isLiveStream() const override { return true; }
};

/** @brief What reading every chunk of the primary stream turned up. */
struct ChainRead {
    size_t chunks = 0;                 ///< chunks with data
    size_t new_link_flags = 0;         ///< chunks flagged new_link
    size_t new_link_index = 0;         ///< index of the (last) flagged chunk
    std::string title_before;          ///< getStreamTitle() before the flag
    std::string title_after;           ///< getStreamTitle() after the end
    std::string codec_after;           ///< the stream's codec after the end
};

/**
 * @brief Parse @p demuxer and read its primary stream to the end.
 * @param demuxer A demuxer over the fixture.
 * @param ok Set false if parsing fails.
 * @return The tally.
 */
ChainRead readAllChunks(OggDemuxer& demuxer, bool& ok)
{
    ChainRead r;
    ok = demuxer.parseContainer();
    if (!ok) return r;
    const auto streams = demuxer.getStreams();
    if (streams.empty()) {
        ok = false;
        return r;
    }
    const uint32_t id = streams[0].stream_id;
    for (int guard = 0; guard < 1000 && !demuxer.isEOF(); ++guard) {
        MediaChunk chunk = demuxer.readChunk(id);
        if (chunk.data.empty()) continue;
        if (chunk.new_link) {
            ++r.new_link_flags;
            r.new_link_index = r.chunks;
        } else if (r.new_link_flags == 0) {
            r.title_before = demuxer.getStreamTitle();
        }
        ++r.chunks;
    }
    r.title_after = demuxer.getStreamTitle();
    r.codec_after = demuxer.getStreamInfo(id).codec_name;
    return r;
}

/** @brief Live: both links, one flag where the second starts. */
class ChainedLiveTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    ChainedLiveTest() : TestCase("Live chained Ogg: carries on into the next link") {}
protected:
    /** @brief Read the fixture as a live stream and check both links come out. */
    void runTest() override {
        OggDemuxer demuxer(std::make_unique<ChainLiveIOHandler>(OggChainFixture::kData,
                                                                sizeof(OggChainFixture::kData)));
        bool ok = false;
        const ChainRead r = readAllChunks(demuxer, ok);
        ASSERT_TRUE(ok, "parses");
        ASSERT_EQUALS(uint64_t(0), demuxer.getDuration(), "a live stream has no duration");
        ASSERT_EQUALS(2 * OggChainFixture::kPacketsPerLink, r.chunks,
                      "every audio packet of both links, and no Opus headers");
        ASSERT_EQUALS(size_t(1), r.new_link_flags, "one link change");
        ASSERT_EQUALS(OggChainFixture::kPacketsPerLink, r.new_link_index,
                      "flagged on the second link's first packet");
        ASSERT_EQUALS(std::string(), r.title_before, "no stream title during the first link");
        ASSERT_EQUALS(std::string("Tester - Link Two"), r.title_after, "the second link's comments");
        ASSERT_EQUALS(std::string("opus"), r.codec_after, "the stream is still Opus");
    }
};

/**
 * @brief Where in the fixture the second link starts: the "OggS" before its
 *        OpusHead packet.
 * @return The offset.
 */
size_t secondLinkOffset()
{
    const std::string all(reinterpret_cast<const char*>(OggChainFixture::kData), sizeof(OggChainFixture::kData));
    const size_t head = all.find("OpusHead", all.find("OpusHead") + 8);
    return all.rfind("OggS", head);
}

/**
 * @brief The second link's BOS page alone: its first page, up to its
 *        OpusTags page.
 * @return The page's bytes.
 */
std::string secondLinkBosPage()
{
    const std::string all(reinterpret_cast<const char*>(OggChainFixture::kData), sizeof(OggChainFixture::kData));
    const size_t start = secondLinkOffset();
    return all.substr(start, all.find("OggS", start + 4) - start);
}

/**
 * @brief A BOS page of a stream PsyMP3 does not play: a Skeleton-like
 *        "fishead" packet, as a link's first BOS page often is.
 * @return The page's bytes.
 */
std::string unknownBosPage()
{
    ogg_stream_state os;
    ogg_stream_init(&os, 0x5ce1e70);
    std::vector<unsigned char> body(64, 0);
    std::memcpy(body.data(), "fishead", 8);
    ogg_packet packet{};
    packet.packet = body.data();
    packet.bytes = static_cast<long>(body.size());
    packet.b_o_s = 1;
    ogg_stream_packetin(&os, &packet);
    ogg_page page;
    std::string out;
    while (ogg_stream_flush(&os, &page) != 0) {
        out.append(reinterpret_cast<const char*>(page.header), static_cast<size_t>(page.header_len));
        out.append(reinterpret_cast<const char*>(page.body), static_cast<size_t>(page.body_len));
    }
    ogg_stream_clear(&os);
    return out;
}

/**
 * @brief A live chain the next link of which does not start cleanly: first
 *        a BOS page of a stream PsyMP3 does not play (the station used to
 *        end there); then, apart, a link whose headers are interrupted by
 *        the stream starting over -- a reconnection -- with a fresh BOS (its
 *        pages used to be thrown away as another serial's).
 */
class ChainedLiveBosTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    ChainedLiveBosTest() : TestCase("Live chained Ogg: an unknown first BOS, and a restart mid-headers") {}
protected:
    /** @brief Both cases, each must still reach the second link. */
    void runTest() override {
        const std::string all(reinterpret_cast<const char*>(OggChainFixture::kData), sizeof(OggChainFixture::kData));
        const size_t split = secondLinkOffset();
        const std::string cases[2] = {
            all.substr(0, split) + unknownBosPage() + all.substr(split),
            all.substr(0, split) + secondLinkBosPage() + all.substr(split),
        };
        const char* names[2] = {"after an unknown BOS", "after a restart mid-headers"};
        for (int c = 0; c < 2; ++c) {
            OggDemuxer demuxer(std::make_unique<ChainLiveIOHandler>(cases[c].data(), cases[c].size()));
            bool ok = false;
            const ChainRead r = readAllChunks(demuxer, ok);
            ASSERT_TRUE(ok, std::string("parses ") + names[c]);
            ASSERT_EQUALS(2 * OggChainFixture::kPacketsPerLink, r.chunks,
                          std::string("both links play ") + names[c]);
            ASSERT_EQUALS(std::string("Tester - Link Two"), r.title_after,
                          std::string("the second link's title ") + names[c]);
        }
    }
};

/** @brief A file: unchanged, it ends with its first link. */
class ChainedFileTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    ChainedFileTest() : TestCase("Chained Ogg file: still ends with its first link") {}
protected:
    /** @brief Read the fixture as a file and check only the first link comes out. */
    void runTest() override {
        OggDemuxer demuxer(std::make_unique<MemoryIOHandler>(OggChainFixture::kData,
                                                             sizeof(OggChainFixture::kData)));
        bool ok = false;
        const ChainRead r = readAllChunks(demuxer, ok);
        ASSERT_TRUE(ok, "parses");
        ASSERT_EQUALS(OggChainFixture::kPacketsPerLink, r.chunks, "the first link only");
        ASSERT_EQUALS(size_t(0), r.new_link_flags, "no link change");
        ASSERT_EQUALS(std::string(), r.title_after, "no stream title");
    }
};

} // namespace

/**
 * @brief Run the chained-Ogg tests; the entry test_ogg_suite calls.
 * @return Number of failed tests.
 */
int test_ogg_chained_live_main()
{
    TestFramework::TestSuite suite("Chained Ogg Tests");
    suite.addTest(std::make_unique<ChainedLiveTest>());
    suite.addTest(std::make_unique<ChainedFileTest>());
    suite.addTest(std::make_unique<ChainedLiveBosTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
