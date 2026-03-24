/*
 * test_g722_integration.cpp - Integration tests for raw G.722 decoding
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_G722
#include <spandsp/telephony.h>
#include <spandsp/g722.h>

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<int16_t> makeSinePcm(size_t sample_count)
{
    std::vector<int16_t> pcm(sample_count);
    for (size_t i = 0; i < sample_count; ++i) {
        double phase = (2.0 * M_PI * 440.0 * static_cast<double>(i)) / 16000.0;
        pcm[i] = static_cast<int16_t>(std::sin(phase) * 12000.0);
    }
    return pcm;
}

std::vector<uint8_t> encodeG722(const std::vector<int16_t>& pcm)
{
    g722_encode_state_t* encoder = g722_encode_init(nullptr, 64000, 0);
    require(encoder != nullptr, "Failed to initialize G.722 encoder");

    std::vector<uint8_t> encoded(pcm.size());
    int encoded_bytes = g722_encode(encoder, encoded.data(), pcm.data(), static_cast<int>(pcm.size()));
    g722_encode_free(encoder);

    require(encoded_bytes > 0, "Failed to encode G.722 test payload");
    encoded.resize(static_cast<size_t>(encoded_bytes));
    return encoded;
}

std::string writeTempG722(const std::vector<uint8_t>& encoded)
{
    std::string path = "/tmp/psymp3-g722-" + std::to_string(::getpid()) + ".g722";
    std::ofstream out(path, std::ios::binary);
    require(out.good(), "Failed to create temporary G.722 file");
    out.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
    out.close();
    return path;
}

} // namespace

int main()
{
    try {
        const std::vector<int16_t> source_pcm = makeSinePcm(1600);
        const std::vector<uint8_t> encoded = encodeG722(source_pcm);
        const std::string path = writeTempG722(encoded);

        auto handler = std::make_unique<FileIOHandler>(path);
        PsyMP3::Demuxer::Raw::RawAudioDemuxer demuxer(std::move(handler), path);
        require(demuxer.parseContainer(), "RawAudioDemuxer should parse .g722 files");

        StreamInfo stream = demuxer.getStreamInfo(1);
        require(stream.codec_name == "g722", "RawAudioDemuxer should classify .g722 as g722");
        require(stream.sample_rate == 16000, "Raw G.722 should default to 16 kHz PCM output");
        require(stream.duration_samples == source_pcm.size(), "Duration should use decoded PCM samples");

        require(demuxer.seekTo(50), "Seeking within raw G.722 should succeed");
        MediaChunk seek_chunk = demuxer.readChunk();
        require(seek_chunk.timestamp_samples == 800, "50 ms seek should land on the 800-sample boundary");

        auto handler2 = std::make_unique<FileIOHandler>(path);
        PsyMP3::Demuxer::Raw::RawAudioDemuxer playback_demuxer(std::move(handler2), path);
        require(playback_demuxer.parseContainer(), "Playback demuxer should parse .g722 files");

        G722Codec codec(playback_demuxer.getStreamInfo(1));
        require(codec.canDecode(playback_demuxer.getStreamInfo(1)), "G722Codec should accept raw G.722 streams");
        require(codec.initialize(), "G722Codec should initialize successfully");

        size_t decoded_samples = 0;
        int64_t total_energy = 0;
        while (!playback_demuxer.isEOF()) {
            MediaChunk chunk = playback_demuxer.readChunk();
            if (chunk.data.empty()) {
                break;
            }
            AudioFrame frame = codec.decode(chunk);
            decoded_samples += frame.samples.size();
            for (int16_t sample : frame.samples) {
                total_energy += std::abs(static_cast<int>(sample));
            }
        }

        require(decoded_samples == source_pcm.size(), "Decoded G.722 sample count should match source PCM length");
        require(total_energy > 0, "Decoded G.722 output should contain non-silent audio");

        std::remove(path.c_str());
        std::cout << "G.722 integration tests passed" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "G.722 integration test failed: " << e.what() << std::endl;
        return 1;
    }
}

#else

int main()
{
    std::cout << "G.722 integration tests skipped - HAVE_G722 not defined" << std::endl;
    return 0;
}

#endif
