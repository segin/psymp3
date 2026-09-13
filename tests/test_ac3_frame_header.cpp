/*
 * test_ac3_frame_header.cpp - AC-3 syncframe parsing and AC-3/E-AC-3 telling apart
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;
using namespace PsyMP3::Codec::AC3;

namespace {

/// Writes bits most significant first, so a test can spell a syncframe header
/// out field by field the way A/52 Tables 5.1 and E1.2 print them.
class BitWriter {
public:
    void put(uint32_t value, unsigned bits)
    {
        while (bits-- > 0) {
            const uint32_t bit = (value >> bits) & 1u;
            if (m_count % 8 == 0) {
                m_data.push_back(0);
            }
            if (bit) {
                m_data.back() |= static_cast<uint8_t>(0x80 >> (m_count % 8));
            }
            ++m_count;
        }
    }
    /// Pads out to @p bytes so a header is long enough to parse.
    std::vector<uint8_t> finish(size_t bytes = 16)
    {
        while (m_data.size() < bytes) {
            m_data.push_back(0);
        }
        return m_data;
    }

private:
    std::vector<uint8_t> m_data;
    size_t m_count = 0;
};

/// An AC-3 header: syncinfo() then bsi() up to dialnorm.
std::vector<uint8_t> ac3Header(uint8_t fscod, uint8_t frmsizecod, uint8_t bsid = 8,
                               uint8_t acmod = 2, bool lfeon = false)
{
    BitWriter w;
    w.put(kSyncWord, 16);
    w.put(0x1234, 16);        // crc1
    w.put(fscod, 2);
    w.put(frmsizecod, 6);
    w.put(bsid, 5);
    w.put(0, 3);              // bsmod
    w.put(acmod, 3);
    if ((acmod & 0x1) && acmod != 0x1) { w.put(0, 2); }  // cmixlev
    if (acmod & 0x4)                   { w.put(0, 2); }  // surmixlev
    if (acmod == 0x2)                  { w.put(0, 2); }  // dsurmod
    w.put(lfeon ? 1 : 0, 1);
    w.put(31, 5);             // dialnorm
    return w.finish();
}

/// An E-AC-3 header, A/52 Table E1.2. A different bsi() entirely.
std::vector<uint8_t> eac3Header(uint8_t fscod, uint16_t frmsiz, uint8_t bsid = 16,
                                uint8_t acmod = 2, bool lfeon = false,
                                uint8_t fscod2 = 0)
{
    BitWriter w;
    w.put(kSyncWord, 16);
    w.put(0, 2);              // strmtyp
    w.put(0, 3);              // substreamid
    w.put(frmsiz, 11);
    w.put(fscod, 2);
    w.put(fscod == 0x3 ? fscod2 : 3, 2);   // fscod2 when fscod is '11', else numblkscod
    w.put(acmod, 3);
    w.put(lfeon ? 1 : 0, 1);
    w.put(bsid, 5);
    w.put(31, 5);             // dialnorm
    return w.finish();
}

class SyncWordTest : public TestCase {
public:
    SyncWordTest() : TestCase("A frame must open with the sync word") {}

protected:
    void runTest() override
    {
        AC3FrameHeader header;
        auto good = ac3Header(0, 0);
        ASSERT_TRUE(parseAC3FrameHeader(good.data(), good.size(), header), "0x0B77 parses");

        auto bad = good;
        bad[0] = 0x0C;
        ASSERT_FALSE(parseAC3FrameHeader(bad.data(), bad.size(), header),
                     "anything else is refused");
        ASSERT_FALSE(parseAC3FrameHeader(good.data(), 4, header),
                     "a buffer too short for the header is refused");
        ASSERT_FALSE(parseAC3FrameHeader(nullptr, 16, header), "as is no buffer at all");
    }
};

class SampleRateTest : public TestCase {
public:
    SampleRateTest() : TestCase("fscod selects the sample rate, and '11' is reserved") {}

protected:
    void runTest() override
    {
        // A/52 Table 5.6.
        struct { uint8_t fscod; uint32_t rate; } cases[] = {{0, 48000}, {1, 44100}, {2, 32000}};
        for (const auto& item : cases) {
            AC3FrameHeader header;
            auto frame = ac3Header(item.fscod, 0);
            ASSERT_TRUE(parseAC3FrameHeader(frame.data(), frame.size(), header), "parses");
            ASSERT_TRUE(header.sample_rate == item.rate, "the rate matches Table 5.6");
        }
        // '11' is reserved in AC-3 -- unlike E-AC-3, where it means fscod2 follows.
        AC3FrameHeader header;
        auto reserved = ac3Header(3, 0);
        ASSERT_FALSE(parseAC3FrameHeader(reserved.data(), reserved.size(), header),
                     "the reserved sample rate code is refused");
    }
};

class FrameSizeTest : public TestCase {
public:
    FrameSizeTest() : TestCase("frmsizecod gives the frame length, which 44.1 kHz alternates") {}

protected:
    void runTest() override
    {
        // A/52 Table 5.18, in bytes rather than the table's 16-bit words.
        ASSERT_TRUE(ac3FrameSize(0, 0) == 128, "32 kbps at 48 kHz is 64 words");
        ASSERT_TRUE(ac3FrameSize(2, 0) == 192, "and 96 words at 32 kHz");
        ASSERT_TRUE(ac3FrameSize(0, 37) == 2560, "640 kbps at 48 kHz is 1280 words");

        // The pair of codes sharing a bit rate spell the same length at 48 and
        // 32 kHz but differ by a word at 44.1, because 1536 samples is 34.83 ms
        // there and a frame cannot always be a whole number of words.
        ASSERT_TRUE(ac3FrameSize(0, 0) == ac3FrameSize(0, 1), "48 kHz: the pair matches");
        ASSERT_TRUE(ac3FrameSize(2, 0) == ac3FrameSize(2, 1), "32 kHz: the pair matches");
        ASSERT_TRUE(ac3FrameSize(1, 1) == ac3FrameSize(1, 0) + 2,
                    "44.1 kHz: the odd code is one word longer");

        ASSERT_TRUE(ac3FrameSize(3, 0) == 0, "a reserved fscod has no frame size");
        ASSERT_TRUE(ac3FrameSize(0, 38) == 0, "nor does a frmsizecod past the table");
    }
};

class ChannelModeTest : public TestCase {
public:
    ChannelModeTest() : TestCase("acmod gives the channel arrangement, and LFE is extra") {}

protected:
    void runTest() override
    {
        // A/52 Table 5.8. 1+1 is two independent mono programmes, so it counts
        // two channels even though 1/0 -- the next code up -- counts one.
        ASSERT_TRUE(ac3ChannelCount(AudioCodingMode::DualMono) == 2, "1+1");
        ASSERT_TRUE(ac3ChannelCount(AudioCodingMode::Mono) == 1, "1/0");
        ASSERT_TRUE(ac3ChannelCount(AudioCodingMode::Stereo) == 2, "2/0");
        ASSERT_TRUE(ac3ChannelCount(AudioCodingMode::ThreeTwo) == 5, "3/2");

        // 3/2 plus LFE is 5.1, and the LFE is never counted among the
        // full-bandwidth channels.
        AC3FrameHeader header;
        auto frame = ac3Header(0, 0, 8, 7, /*lfeon=*/true);
        ASSERT_TRUE(parseAC3FrameHeader(frame.data(), frame.size(), header), "parses");
        ASSERT_TRUE(header.channels == 5, "five full-bandwidth channels");
        ASSERT_TRUE(header.lfeon, "with LFE");
        ASSERT_TRUE(header.outputChannels() == 6, "which is six channels out");
    }
};

class FlavourTest : public TestCase {
public:
    FlavourTest() : TestCase("bsid tells AC-3 and E-AC-3 apart at the same bit offset") {}

protected:
    void runTest() override
    {
        // A/52 §E2.3.1.6: 0..8 AC-3, 9..10 the Annex D alternate syntax,
        // 11..16 E-AC-3. bsid sits at bit 40 in both layouts precisely so it
        // can be read before either is parsed.
        AC3FrameHeader header;
        auto ac3 = ac3Header(0, 0, /*bsid=*/8);
        ASSERT_TRUE(parseAC3FrameHeader(ac3.data(), ac3.size(), header), "AC-3 parses");
        ASSERT_TRUE(header.isAC3(), "and is AC-3");
        ASSERT_FALSE(header.isEAC3(), "not E-AC-3");
        ASSERT_TRUE(header.isDecodable(), "and can be decoded");
        ASSERT_TRUE(std::string(header.displayName()) == "AC-3", "named AC-3");

        auto eac3 = eac3Header(0, 415, /*bsid=*/16);
        ASSERT_TRUE(parseAC3FrameHeader(eac3.data(), eac3.size(), header),
                    "E-AC-3 is recognised rather than refused");
        ASSERT_TRUE(header.isEAC3(), "and is E-AC-3");
        ASSERT_FALSE(header.isAC3(), "not AC-3");
        ASSERT_TRUE(header.isDecodable(),
                    "and can be decoded -- the same decoder handles both");
        ASSERT_TRUE(std::string(header.displayName()) == "E-AC-3",
                    "and is named E-AC-3, so Media Information can tell the two apart");

        // The Annex D alternate syntax is still AC-3 by name.
        auto alternate = ac3Header(0, 0, /*bsid=*/9);
        ASSERT_TRUE(parseAC3FrameHeader(alternate.data(), alternate.size(), header), "parses");
        ASSERT_TRUE(std::string(header.displayName()) == "AC-3", "bsid 9 is still AC-3");
        ASSERT_FALSE(header.isDecodable(), "though this decoder does not implement it");
    }
};

class EAC3FieldsTest : public TestCase {
public:
    EAC3FieldsTest() : TestCase("E-AC-3 fields come from its own layout, not AC-3's") {}

protected:
    void runTest() override
    {
        // frmsiz is one less than the length in 16-bit words, so 415 means 416
        // words, which is 832 bytes.
        AC3FrameHeader header;
        auto frame = eac3Header(0, 415);
        ASSERT_TRUE(parseAC3FrameHeader(frame.data(), frame.size(), header), "parses");
        ASSERT_TRUE(header.frame_size == 832, "frmsiz of 415 is 416 words, 832 bytes");
        ASSERT_TRUE(header.sample_rate == 48000, "fscod still selects the rate");

        // fscod '11' is reserved in AC-3 but in E-AC-3 means the rate comes
        // from fscod2 instead: the half rates, A/52 Table E2.3.
        struct { uint8_t fscod2; uint32_t rate; } reduced[] = {{0, 24000}, {1, 22050}, {2, 16000}};
        for (const auto& item : reduced) {
            AC3FrameHeader half;
            auto low = eac3Header(3, 415, 16, 2, false, item.fscod2);
            ASSERT_TRUE(parseAC3FrameHeader(low.data(), low.size(), half), "parses");
            ASSERT_TRUE(half.sample_rate == item.rate,
                        "fscod2 gives the reduced rate an AC-3 stream cannot reach");
        }

        // E-AC-3 states a frame length rather than a bit rate, so none is
        // reported rather than a guess.
        ASSERT_TRUE(header.bitrate == 0, "no bit rate is invented for E-AC-3");
    }
};

class BitReaderTest : public TestCase {
public:
    BitReaderTest() : TestCase("The bit reader is most significant bit first and cannot overrun") {}

protected:
    void runTest() override
    {
        // A/52 §5.3: every element arrives most significant bit first.
        const uint8_t data[] = {0xB5, 0x3C};
        AC3BitReader reader(data, sizeof(data));
        ASSERT_TRUE(reader.read(4) == 0xB, "the high nibble comes first");
        ASSERT_TRUE(reader.read(4) == 0x5, "then the low");
        ASSERT_TRUE(reader.peek(8) == 0x3C, "peek does not consume");
        ASSERT_TRUE(reader.read(8) == 0x3C, "so the same bits read back");
        ASSERT_FALSE(reader.overrun(), "nothing has overrun yet");

        // Reading past the end yields zeros and says so, rather than reading
        // outside the buffer or throwing: a decoder that has mis-tracked its
        // position is already producing nonsense, and what matters is that the
        // caller can find out.
        ASSERT_TRUE(reader.read(8) == 0, "past the end reads as zero");
        ASSERT_TRUE(reader.overrun(), "and is reported");
        ASSERT_TRUE(reader.remaining() == 0, "with nothing remaining");
    }
};

} // namespace

int main()
{
    TestSuite suite("AC-3 Frame Header Tests");
    suite.addTest(std::make_unique<BitReaderTest>());
    suite.addTest(std::make_unique<SyncWordTest>());
    suite.addTest(std::make_unique<SampleRateTest>());
    suite.addTest(std::make_unique<FrameSizeTest>());
    suite.addTest(std::make_unique<ChannelModeTest>());
    suite.addTest(std::make_unique<FlavourTest>());
    suite.addTest(std::make_unique<EAC3FieldsTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
