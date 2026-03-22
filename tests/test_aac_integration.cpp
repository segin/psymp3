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
using PsyMP3::IO::MemoryIOHandler;

namespace {

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

} // namespace

int main() {
    TestSuite suite("AAC Integration Tests");
    suite.addTest(std::make_unique<AACESDSParsingTest>());
    suite.addTest(std::make_unique<AACCodecInitializationTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}

#else

int main() {
    return 0;
}

#endif // HAVE_AAC
