/*
 * test_raw_audio_stream_integration.cpp - Main stream path tests for raw telephony audio
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#include <cstdio>
#include <fstream>

#ifdef HAVE_G722
#include "g722_fixture.h"
#endif

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string makeTempPath(const std::string& extension)
{
    return "/tmp/psymp3-raw-stream-" + std::to_string(::getpid()) + extension;
}

void writeBinaryFile(const std::string& path, const std::vector<uint8_t>& bytes)
{
    std::ofstream out(path, std::ios::binary);
    require(out.good(), "Failed to create temporary file: " + path);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close();
}

void testMuLawViaMediaFactory()
{
    const std::vector<uint8_t> encoded = {
        0xFF, 0x7F, 0x00, 0x80, 0xF0, 0x10, 0xAA, 0x55,
        0xFF, 0x7F, 0x00, 0x80, 0xF0, 0x10, 0xAA, 0x55
    };

    const std::string path = makeTempPath(".ulaw");
    writeBinaryFile(path, encoded);

    try {
        auto stream = PsyMP3::Demuxer::MediaFactory::createStream(path);
        require(stream != nullptr, "MediaFactory should open .ulaw through the main stream path");
        require(stream->getRate() == 8000, ".ulaw should use 8 kHz default rate");
        require(stream->getChannels() == 1, ".ulaw should use mono default");

        std::vector<uint8_t> pcm(64, 0);
        size_t bytes_read = stream->getData(pcm.size(), pcm.data());
        require(bytes_read > 0, ".ulaw stream should yield decoded PCM");
    } catch (...) {
        std::remove(path.c_str());
        throw;
    }

    std::remove(path.c_str());
}

#ifdef HAVE_G722
std::vector<int16_t> makeG722SourcePcm(size_t sample_count)
{
    std::vector<int16_t> pcm(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        double phase = (2.0 * M_PI * 440.0 * static_cast<double>(i)) / 16000.0;
        pcm[i] = static_cast<int16_t>(std::sin(phase) * 12000.0);
    }
    return pcm;
}

// A captured fixture rather than a fresh encode: PsyMP3 decodes G.722
// in-tree and ships no encoder. See g722_fixture.h.
std::vector<uint8_t> encodedG722Sine()
{
    return std::vector<uint8_t>(std::begin(kG722Sine1600), std::end(kG722Sine1600));
}

void testG722ViaMediaFactory()
{
    const std::vector<int16_t> source_pcm = makeG722SourcePcm(1600);
    const std::vector<uint8_t> encoded = encodedG722Sine();
    const std::string path = makeTempPath(".g722");
    writeBinaryFile(path, encoded);

    try {
        auto stream = PsyMP3::Demuxer::MediaFactory::createStream(path);
        require(stream != nullptr, "MediaFactory should open .g722 through the main stream path");
        require(stream->getRate() == 16000, ".g722 should use 16 kHz decoded PCM rate");
        require(stream->getChannels() == 1, ".g722 should use mono default");

        std::vector<uint8_t> pcm(4096, 0);
        size_t bytes_read = stream->getData(pcm.size(), pcm.data());
        require(bytes_read > 0, ".g722 stream should yield decoded PCM");
    } catch (...) {
        std::remove(path.c_str());
        throw;
    }

    std::remove(path.c_str());
}
#endif

} // namespace

int main()
{
    try {
        testMuLawViaMediaFactory();
#ifdef HAVE_G722
        testG722ViaMediaFactory();
#endif
        std::cout << "Raw audio main stream integration tests passed" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Raw audio main stream integration test failed: " << e.what() << std::endl;
        return 1;
    }
}
