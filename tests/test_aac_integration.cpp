/*
 * test_aac_integration.cpp - Integration tests for AAC codec and ISO AAC config parsing
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/MemoryIOHandler.h"

#ifdef HAVE_AAC

using namespace TestFramework;
using PsyMP3::Demuxer::ISO::AudioTrackInfo;
using PsyMP3::Demuxer::ISO::BoxHeader;
using PsyMP3::Demuxer::ISO::BoxParser;
using PsyMP3::Demuxer::ISO::BOX_HDLR;
using PsyMP3::Demuxer::ISO::BOX_MDHD;
using PsyMP3::Demuxer::ISO::BOX_MDIA;
using PsyMP3::Demuxer::ISO::BOX_MINF;
using PsyMP3::Demuxer::ISO::BOX_SMHD;
using PsyMP3::Demuxer::ISO::BOX_STBL;
using PsyMP3::Demuxer::ISO::BOX_STCO;
using PsyMP3::Demuxer::ISO::BOX_STSC;
using PsyMP3::Demuxer::ISO::BOX_STSD;
using PsyMP3::Demuxer::ISO::BOX_STSZ;
using PsyMP3::Demuxer::ISO::BOX_STTS;
using PsyMP3::Demuxer::ISO::BOX_TKHD;
using PsyMP3::Demuxer::ISO::BOX_TRAK;
using PsyMP3::Demuxer::ISO::SampleTableManager;
using PsyMP3::Demuxer::ISO::SampleTableInfo;
using PsyMP3::IO::MemoryIOHandler;

namespace {

void writeUInt16BE(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(value & 0xFF));
}

void writeUInt32BE(std::vector<uint8_t>& data, uint32_t value) {
    data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(value & 0xFF));
}

std::vector<uint8_t> makeBox(uint32_t type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> box;
    box.reserve(8 + payload.size());
    writeUInt32BE(box, static_cast<uint32_t>(8 + payload.size()));
    writeUInt32BE(box, type);
    box.insert(box.end(), payload.begin(), payload.end());
    return box;
}

void appendBox(std::vector<uint8_t>& parent, uint32_t type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> box = makeBox(type, payload);
    parent.insert(parent.end(), box.begin(), box.end());
}

std::vector<uint8_t> createESDSPayloadWithASC(const std::vector<uint8_t>& asc) {
    std::vector<uint8_t> payload = {
        0x00, 0x00, 0x00, 0x00, // version + flags
        0x03, 0x19,             // ES_Descriptor tag + length
        0x00, 0x01,             // ES_ID
        0x00,                   // flags
        0x04, 0x11,             // DecoderConfigDescriptor tag + length
        0x40,                   // objectTypeIndication (AAC)
        0x15,                   // streamType
        0x00, 0x00, 0x00,       // bufferSizeDB
        0x00, 0x00, 0x00, 0x00, // maxBitrate
        0x00, 0x00, 0x00, 0x00, // avgBitrate
        0x05, static_cast<uint8_t>(asc.size())
    };

    payload.insert(payload.end(), asc.begin(), asc.end());
    payload.push_back(0x06);
    payload.push_back(0x01);
    payload.push_back(0x02);
    return payload;
}

std::vector<uint8_t> createAACSampleDescription(const std::vector<uint8_t>& asc) {
    std::vector<uint8_t> stsd_payload;
    writeUInt32BE(stsd_payload, 0); // version + flags
    writeUInt32BE(stsd_payload, 1); // entry count

    std::vector<uint8_t> entry_payload(28, 0);
    entry_payload[6] = 0x00;
    entry_payload[7] = 0x01; // data reference index
    entry_payload[16] = 0x00;
    entry_payload[17] = 0x02; // channel count = 2
    entry_payload[18] = 0x00;
    entry_payload[19] = 0x10; // sample size = 16
    entry_payload[24] = 0xAC;
    entry_payload[25] = 0x44; // 44100 in 16.16 fixed point

    appendBox(entry_payload, FOURCC('e','s','d','s'), createESDSPayloadWithASC(asc));

    std::vector<uint8_t> entry;
    writeUInt32BE(entry, static_cast<uint32_t>(8 + entry_payload.size()));
    writeUInt32BE(entry, FOURCC('m','p','4','a'));
    entry.insert(entry.end(), entry_payload.begin(), entry_payload.end());

    stsd_payload.insert(stsd_payload.end(), entry.begin(), entry.end());
    return makeBox(BOX_STSD, stsd_payload);
}

std::vector<uint8_t> createMediaBoxWithAACSampleTables(const std::vector<uint8_t>& asc) {
    std::vector<uint8_t> mdhd_payload(24, 0);
    mdhd_payload[12] = 0x00;
    mdhd_payload[13] = 0x00;
    mdhd_payload[14] = 0xAC;
    mdhd_payload[15] = 0x44; // timescale = 44100
    mdhd_payload[16] = 0x00;
    mdhd_payload[17] = 0x00;
    mdhd_payload[18] = 0xAC;
    mdhd_payload[19] = 0x44; // duration = 44100

    std::vector<uint8_t> hdlr_payload(24, 0);
    hdlr_payload[8] = 's';
    hdlr_payload[9] = 'o';
    hdlr_payload[10] = 'u';
    hdlr_payload[11] = 'n';

    std::vector<uint8_t> stts_payload;
    writeUInt32BE(stts_payload, 0);
    writeUInt32BE(stts_payload, 1);
    writeUInt32BE(stts_payload, 45);
    writeUInt32BE(stts_payload, 1024);

    std::vector<uint8_t> stsc_payload;
    writeUInt32BE(stsc_payload, 0);
    writeUInt32BE(stsc_payload, 1);
    writeUInt32BE(stsc_payload, 1);
    writeUInt32BE(stsc_payload, 45);
    writeUInt32BE(stsc_payload, 1);

    std::vector<uint8_t> stsz_payload;
    writeUInt32BE(stsz_payload, 0);
    writeUInt32BE(stsz_payload, 0);
    writeUInt32BE(stsz_payload, 45);
    for (int i = 0; i < 45; ++i) {
        writeUInt32BE(stsz_payload, 128);
    }

    std::vector<uint8_t> stco_payload;
    writeUInt32BE(stco_payload, 0);
    writeUInt32BE(stco_payload, 1);
    writeUInt32BE(stco_payload, 128);

    std::vector<uint8_t> stbl_payload;
    std::vector<uint8_t> stsd_box = createAACSampleDescription(asc);
    stbl_payload.insert(stbl_payload.end(), stsd_box.begin(), stsd_box.end());
    appendBox(stbl_payload, BOX_STTS, stts_payload);
    appendBox(stbl_payload, BOX_STSC, stsc_payload);
    appendBox(stbl_payload, BOX_STSZ, stsz_payload);
    appendBox(stbl_payload, BOX_STCO, stco_payload);

    std::vector<uint8_t> minf_payload;
    appendBox(minf_payload, BOX_SMHD, std::vector<uint8_t>(8, 0));
    appendBox(minf_payload, BOX_STBL, stbl_payload);

    std::vector<uint8_t> mdia_payload;
    appendBox(mdia_payload, BOX_MDHD, mdhd_payload);
    appendBox(mdia_payload, BOX_HDLR, hdlr_payload);
    appendBox(mdia_payload, BOX_MINF, minf_payload);
    return makeBox(BOX_MDIA, mdia_payload);
}

class AACESDSParsingTest : public TestCase {
public:
    AACESDSParsingTest() : TestCase("AAC ESDS Parsing Test") {}

protected:
    void runTest() override {
        const std::vector<uint8_t> expected_asc = {0x12, 0x10}; // AAC LC, 44.1kHz, stereo
        const std::vector<uint8_t> payload = createESDSPayloadWithASC(expected_asc);

        auto io = std::make_shared<MemoryIOHandler>(payload.data(), payload.size());
        BoxParser parser(io);
        AudioTrackInfo track = {};

        ASSERT_TRUE(parser.ParseAACConfiguration(0, payload.size(), track, 0),
                    "Parser should extract AAC AudioSpecificConfig from esds");
        ASSERT_EQUALS(expected_asc.size(), track.codecConfig.size(),
                      "Extracted AudioSpecificConfig size should match");
        ASSERT_EQUALS(expected_asc[0], track.codecConfig[0],
                      "First AudioSpecificConfig byte should match");
        ASSERT_EQUALS(expected_asc[1], track.codecConfig[1],
                      "Second AudioSpecificConfig byte should match");
    }
};

class AACCodecInitializationTest : public TestCase {
public:
    AACCodecInitializationTest() : TestCase("AAC Codec Initialization Test") {}

protected:
    void runTest() override {
        registerAllCodecs();

        StreamInfo info(1, "audio", "aac");
        info.codec_data = {0x12, 0x10};

        auto codec = AudioCodecFactory::createCodec(info);
        ASSERT_TRUE(codec != nullptr, "Factory should create AAC codec");
        ASSERT_EQUALS(std::string("aac"), codec->getCodecName(),
                      "Factory should return AAC codec");
        ASSERT_TRUE(codec->initialize(), "AAC codec should initialize from AudioSpecificConfig");

        AudioFrame flush_frame = codec->flush();
        ASSERT_TRUE(flush_frame.samples.empty(), "Flush should not emit samples without input");

        codec->reset();
    }
};

class AACISOMediaParsingTest : public TestCase {
public:
    AACISOMediaParsingTest() : TestCase("AAC ISO Media Parsing Test") {}

protected:
    void runTest() override {
        const std::vector<uint8_t> asc = {0x12, 0x10};
        const std::vector<uint8_t> mdia = createMediaBoxWithAACSampleTables(asc);

        auto io = std::make_shared<MemoryIOHandler>(mdia.data(), mdia.size());
        BoxParser parser(io);
        AudioTrackInfo track = {};
        SampleTableInfo sampleTables;
        bool foundAudio = false;
        BoxHeader mdiaHeader = parser.ReadBoxHeader(0);

        ASSERT_TRUE(parser.ParseMediaBoxWithSampleTables(
                        mdiaHeader.dataOffset,
                        mdiaHeader.size - (mdiaHeader.dataOffset - 0),
                        track,
                        foundAudio,
                        sampleTables,
                        0),
                    "Media box should parse successfully");
        ASSERT_TRUE(foundAudio, "Handler box should identify the track as audio");
        ASSERT_EQUALS(std::string("aac"), track.codecType, "Codec type should be AAC");
        ASSERT_EQUALS(44100u, track.timescale, "mdhd timescale should parse correctly");
        ASSERT_EQUALS(44100ull, track.duration, "mdhd duration should parse correctly");
        ASSERT_EQUALS(45u, static_cast<uint32_t>(sampleTables.sampleSizes.size()),
                      "stsz should populate all sample sizes");
        ASSERT_EQUALS(1u, static_cast<uint32_t>(sampleTables.chunkOffsets.size()),
                      "stco should populate chunk offsets");
        ASSERT_EQUALS(45u, static_cast<uint32_t>(sampleTables.sampleTimes.size()),
                      "stts should expand sample times");
        ASSERT_EQUALS(2u, track.channelCount, "Channel count should come from mp4a sample entry");
        ASSERT_EQUALS(44100u, track.sampleRate, "Sample rate should come from mp4a sample entry");
        ASSERT_EQUALS(asc.size(), track.codecConfig.size(),
                      "AudioSpecificConfig should still be extracted");
    }
};

class AACSampleOffsetCalculationTest : public TestCase {
public:
    AACSampleOffsetCalculationTest() : TestCase("AAC Sample Offset Calculation Test") {}

protected:
    void runTest() override {
        SampleTableInfo tables;
        tables.chunkOffsets = {44};
        tables.sampleToChunkEntries.push_back({0, 3, 1});
        tables.sampleSizes = {469, 356, 267};
        tables.sampleTimes = {0, 1024, 2048};

        SampleTableManager manager;
        ASSERT_TRUE(manager.BuildSampleTables(tables),
                    "Sample tables should build for variable-size AAC samples");

        auto first = manager.GetSampleInfo(0);
        auto second = manager.GetSampleInfo(1);
        auto third = manager.GetSampleInfo(2);

        ASSERT_EQUALS(44ull, first.offset, "First sample should start at chunk offset");
        ASSERT_EQUALS(44ull + 469ull, second.offset,
                      "Second sample should start after the first sample size");
        ASSERT_EQUALS(44ull + 469ull + 356ull, third.offset,
                      "Third sample should start after the preceding variable-size samples");
    }
};

} // namespace

int main() {
    TestSuite suite("AAC Integration Tests");
    suite.addTest(std::make_unique<AACESDSParsingTest>());
    suite.addTest(std::make_unique<AACCodecInitializationTest>());
    suite.addTest(std::make_unique<AACISOMediaParsingTest>());
    suite.addTest(std::make_unique<AACSampleOffsetCalculationTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}

#else

int main() {
    return 0;
}

#endif // HAVE_AAC
