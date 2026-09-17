/*
 * test_ogg_seek_landing.cpp - A seek in Ogg Vorbis plays from the target
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The stream is made with libvorbisenc and paged with libogg, test-only
 * dependencies: PsyMP3 decodes with stb_vorbis.
 */

#include "psymp3.h"
#include "test_framework.h"
#include "io/MemoryIOHandler.h"

#include <cmath>
#include <cstdlib>
#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>

using namespace TestFramework;
using namespace PsyMP3;
using namespace PsyMP3::Demuxer;
using PsyMP3::IO::MemoryIOHandler;

namespace {

constexpr int kRate = 44100;
constexpr int kToneStartMs = 4500;
constexpr int kTotalMs = 6000;

/// 6 s of stereo: silence, then a 1 kHz tone from 4.5 s, as an Ogg Vorbis
/// file with pages of about a second.
std::vector<uint8_t> oggVorbis()
{
    std::vector<uint8_t> file;
    vorbis_info vi;
    vorbis_info_init(&vi);
    if (vorbis_encode_init_vbr(&vi, 2, kRate, 0.4f) != 0) {
        vorbis_info_clear(&vi);
        return file;
    }
    vorbis_comment vc;
    vorbis_comment_init(&vc);
    vorbis_dsp_state vd;
    vorbis_analysis_init(&vd, &vi);
    vorbis_block vb;
    vorbis_block_init(&vd, &vb);
    ogg_stream_state os;
    ogg_stream_init(&os, 1);

    auto take = [&file](const ogg_page& page) {
        file.insert(file.end(), page.header, page.header + page.header_len);
        file.insert(file.end(), page.body, page.body + page.body_len);
    };
    ogg_page page;
    ogg_packet header[3];
    vorbis_analysis_headerout(&vd, &vc, &header[0], &header[1], &header[2]);
    for (ogg_packet& packet : header) {
        ogg_stream_packetin(&os, &packet);
    }
    while (ogg_stream_flush(&os, &page) != 0) {
        take(page);
    }

    const int total = kRate * kTotalMs / 1000;
    const int tone_from = kRate * kToneStartMs / 1000;
    constexpr int kChunk = 1024;
    int packets = 0;
    for (int position = 0;;) {
        if (position < total) {
            const int frames = std::min(kChunk, total - position);
            float** buffer = vorbis_analysis_buffer(&vd, frames);
            for (int i = 0; i < frames; ++i) {
                const int t = position + i;
                const float value = t < tone_from ? 0.0f
                                  : 0.5f * static_cast<float>(std::sin(2.0 * M_PI * 1000.0 * t / kRate));
                buffer[0][i] = value;
                buffer[1][i] = value;
            }
            vorbis_analysis_wrote(&vd, frames);
            position += frames;
        } else {
            vorbis_analysis_wrote(&vd, 0);
        }
        bool eos = false;
        while (vorbis_analysis_blockout(&vd, &vb) == 1) {
            vorbis_analysis(&vb, nullptr);
            vorbis_bitrate_addblock(&vb);
            ogg_packet packet;
            while (vorbis_bitrate_flushpacket(&vd, &packet)) {
                ogg_stream_packetin(&os, &packet);
                // A page every 40 packets, about a second, as encoders
                // commonly write them: the page a seek lands on ends after
                // the target.
                if (++packets % 40 == 0) {
                    while (ogg_stream_flush(&os, &page) != 0) {
                        take(page);
                        eos = eos || ogg_page_eos(&page);
                    }
                }
            }
        }
        if (position >= total && vorbis_analysis_blockout(&vd, &vb) == 0) {
            while (ogg_stream_flush(&os, &page) != 0) {
                take(page);
            }
            break;
        }
        (void)eos;
    }
    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
    return file;
}

/// The first channel of the next @p frames frames.
std::vector<AudioSample> readFirstChannel(DemuxedStream& stream, size_t frames)
{
    std::vector<AudioSample> buffer(frames * 2);
    size_t filled = 0;
    for (int attempt = 0; attempt < 256 && filled < buffer.size() && !stream.eof(); ++attempt) {
        filled += stream.getData((buffer.size() - filled) * sizeof(AudioSample),
                                 buffer.data() + filled) / sizeof(AudioSample);
    }
    std::vector<AudioSample> first;
    for (size_t i = 0; i + 1 < filled; i += 2) {
        first.push_back(buffer[i]);
    }
    return first;
}

/// Index of the first sample louder than a tenth of full scale, or -1.
long firstLoud(const std::vector<AudioSample>& samples)
{
    for (size_t i = 0; i < samples.size(); ++i) {
        if (std::abs(static_cast<double>(samples[i])) > 0.1 * 2147483647.0) {
            return static_cast<long>(i);
        }
    }
    return -1;
}

class VorbisSeekTest : public TestCase {
public:
    VorbisSeekTest() : TestCase("A seek in Ogg Vorbis finds the audio where playing from the start does") {}

protected:
    void runTest() override
    {
        const std::vector<uint8_t> file = oggVorbis();
        ASSERT_TRUE(file.size() > 4096, "the encoder ran");
        DemuxedStream stream(std::make_unique<MemoryIOHandler>(file.data(), file.size()),
                             TagLib::String("tone.ogg"));
        ASSERT_EQUALS(44100u, stream.getRate(), "opened");

        const std::vector<AudioSample> whole = readFirstChannel(stream, 6 * kRate);
        const long onset = firstLoud(whole);
        ASSERT_TRUE(onset > kRate * kToneStartMs / 1000 - 1024, "the tone is found near its start");

        // The Vorbis decoder drops the first packet it is given after a seek.
        // Counted from the page the seek landed on, the audio after it passed
        // for that much earlier, and that much past the target was cut.
        for (long before_ms : {300L, 250L, 431L, 777L}) {
            const auto target_ms = static_cast<unsigned long>(onset * 1000 / kRate - before_ms);
            stream.seekTo(target_ms);
            const std::vector<AudioSample> after = readFirstChannel(stream, kRate);
            const long seek_onset = firstLoud(after);
            ASSERT_TRUE(seek_onset > 0, "the audio after a seek to " + std::to_string(target_ms)
                                            + " ms starts quiet");
            const long found = static_cast<long>(target_ms * kRate / 1000) + seek_onset;
            ASSERT_TRUE(std::labs(found - onset) <= 1,
                        "a seek to " + std::to_string(target_ms) + " ms finds the tone at "
                        + std::to_string(found) + ", not " + std::to_string(onset));
        }
    }
};

} // namespace

int main()
{
    registerAllCodecs();
    registerAllDemuxers();

    TestSuite suite("Ogg seek landing");
    suite.addTest(std::make_unique<VorbisSeekTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
