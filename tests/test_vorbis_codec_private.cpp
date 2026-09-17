/*
 * test_vorbis_codec_private.cpp - VorbisCodec and Matroska's laced headers
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The headers are made with libvorbisenc, a test-only dependency: PsyMP3
 * decodes with stb_vorbis and has no encoder of its own.
 */

#include "psymp3.h"
#include "test_framework.h"
#include <cmath>
#include <cstring>
#include <vorbis/vorbisenc.h>

using namespace TestFramework;

namespace {

/// Headers and audio packets from libvorbisenc, for a comment of @p value.
struct Encoded {
    std::vector<std::vector<uint8_t>> headers;
    std::vector<std::vector<uint8_t>> audio;
};

Encoded encode(const std::string& comment_value)
{
    Encoded out;
    vorbis_info vi;
    vorbis_info_init(&vi);
    if (vorbis_encode_init_vbr(&vi, 1, 44100, 0.3f) != 0) {
        vorbis_info_clear(&vi);
        return out;
    }
    vorbis_comment vc;
    vorbis_comment_init(&vc);
    vorbis_comment_add_tag(&vc, const_cast<char*>("TITLE"), const_cast<char*>(comment_value.c_str()));
    vorbis_dsp_state vd;
    vorbis_analysis_init(&vd, &vi);
    vorbis_block vb;
    vorbis_block_init(&vd, &vb);

    ogg_packet h[3];
    vorbis_analysis_headerout(&vd, &vc, &h[0], &h[1], &h[2]);
    for (const ogg_packet& p : h) {
        out.headers.emplace_back(p.packet, p.packet + p.bytes);
    }

    constexpr int kSamples = 8192;
    float** buffer = vorbis_analysis_buffer(&vd, kSamples);
    for (int i = 0; i < kSamples; ++i) {
        buffer[0][i] = 0.2f * static_cast<float>(std::sin((2.0 * M_PI * 440.0 * i) / 44100.0));
    }
    vorbis_analysis_wrote(&vd, kSamples);
    vorbis_analysis_wrote(&vd, 0);
    while (vorbis_analysis_blockout(&vd, &vb) == 1) {
        vorbis_analysis(&vb, nullptr);
        vorbis_bitrate_addblock(&vb);
        ogg_packet packet;
        while (vorbis_bitrate_flushpacket(&vd, &packet)) {
            out.audio.emplace_back(packet.packet, packet.packet + packet.bytes);
        }
    }
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
    return out;
}

/// Matroska's A_VORBIS CodecPrivate: the packet count less one, the first two
/// lengths Xiph-laced, then the three packets (cellar-codec 3.4.41).
std::vector<uint8_t> matroskaCodecPrivate(const std::vector<std::vector<uint8_t>>& headers)
{
    std::vector<uint8_t> blob{0x02};
    for (size_t i = 0; i < 2; ++i) {
        size_t length = headers[i].size();
        while (length >= 255) {
            blob.push_back(255);
            length -= 255;
        }
        blob.push_back(static_cast<uint8_t>(length));
    }
    for (const std::vector<uint8_t>& header : headers) {
        blob.insert(blob.end(), header.begin(), header.end());
    }
    return blob;
}

size_t decodedFrames(const Encoded& encoded)
{
    StreamInfo info;
    info.codec_type = "audio";
    info.codec_name = "vorbis";
    info.sample_rate = 44100;
    info.channels = 1;
    info.codec_data = matroskaCodecPrivate(encoded.headers);
    PsyMP3::Codec::Vorbis::VorbisCodec codec(info);
    if (!codec.initialize()) {
        return 0;
    }
    size_t frames = 0;
    for (const std::vector<uint8_t>& packet : encoded.audio) {
        MediaChunk chunk;
        chunk.stream_id = 1;
        chunk.data = packet;
        frames += codec.decode(chunk).samples.size();
    }
    return frames;
}

class LacedHeadersTest : public TestCase {
public:
    LacedHeadersTest() : TestCase("Matroska's Vorbis headers are split by their laced lengths") {}

protected:
    void runTest() override
    {
        const Encoded plain = encode("Song");
        ASSERT_EQUALS(size_t{3}, plain.headers.size(), "the encoder made three headers");
        ASSERT_TRUE(decodedFrames(plain) > 4096, "an ordinary track decodes");

        // A comment holding what looks like the start of a setup header. A
        // split that searched for signatures cut the comment header there.
        const Encoded tricky = encode(std::string("A\x05vorbis in a title", 19));
        ASSERT_TRUE(decodedFrames(tricky) > 4096, "the lengths, not the signatures, decide");
    }
};

} // namespace

int main()
{
    TestSuite suite("Vorbis CodecPrivate");
    suite.addTest(std::make_unique<LacedHeadersTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
