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
#include "g722_fixture.h"

using PsyMP3::Codec::PCM::G722Decoder;

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// The bitstream is a captured fixture rather than something encoded here:
// PsyMP3 decodes G.722 in-tree and ships no encoder. See g722_fixture.h.
std::vector<uint8_t> encodedG722Sine()
{
    return std::vector<uint8_t>(std::begin(kG722Sine1600), std::end(kG722Sine1600));
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

// The synthesis QMF's sum can overshoot 16 bits even while both sub-band
// signals are within their limits. ACCUMC/ACCUMD limit the output, so such a
// sample sits at full scale; wrapping it turned a loud passage into noise of
// the opposite polarity.
void testQmfOutputIsLimited()
{
    // A run of 0x04 octets drives the lower band to its limit, and one of the
    // QMF's two sums then overshoots for nearly every octet.
    const std::vector<uint8_t> octets(4000, 0x04);
    std::vector<int16_t> pcm(2 * octets.size());
    G722Decoder decoder(G722Decoder::Bitrate::Rate64k, true);
    require(decoder.decode(octets.data(), octets.size(), pcm.data()) == pcm.size(),
            "every octet decodes to two samples");

    const size_t settled = 1000;
    size_t at_full_scale = 0;
    for (size_t i = settled; i < pcm.size(); ++i) {
        at_full_scale += (pcm[i] == INT16_MAX || pcm[i] == INT16_MIN) ? 1 : 0;
    }
    std::cout << "QMF: " << at_full_scale << " of " << pcm.size() - settled
              << " samples at full scale" << std::endl;
    require(at_full_scale * 5 >= (pcm.size() - settled) * 2,
            "an overshooting QMF sum is clipped to full scale, not wrapped");
}

// The decoder carries all of its state from one call to the next, so a stream
// decodes the same whether it arrives whole or in pieces, and reset() returns
// it to the state it was constructed in, which is what a seek relies on.
void testChunkingAndReset()
{
    const size_t octets = sizeof(kG722Sine1600);
    G722Decoder decoder(G722Decoder::Bitrate::Rate64k, true);
    std::vector<int16_t> whole(decoder.maxSamples(octets));
    require(decoder.decode(kG722Sine1600, octets, whole.data()) == whole.size(),
            "every octet decodes to two samples");

    G722Decoder chunked(G722Decoder::Bitrate::Rate64k, true);
    std::vector<int16_t> pieces(whole.size());
    size_t written = 0;
    for (size_t at = 0; at < octets; at += 333) {
        const size_t len = std::min<size_t>(333, octets - at);
        written += chunked.decode(kG722Sine1600 + at, len, pieces.data() + written);
    }
    require(written == whole.size() && pieces == whole,
            "decoding in pieces matches decoding in one call");

    decoder.reset();
    std::vector<int16_t> again(whole.size());
    written = decoder.decode(kG722Sine1600, octets, again.data());
    require(written == whole.size() && again == whole,
            "after reset() the decoder starts over");
}

} // namespace

int main()
{
    try {
        testQmfOutputIsLimited();
        testChunkingAndReset();

        // Each octet decodes to two 16 kHz samples (Rec. G.722 §1.5.4), so the
        // fixture's 800 octets make 1600.
        const size_t expected_samples = 1600;
        const std::vector<uint8_t> encoded = encodedG722Sine();
        const std::string path = writeTempG722(encoded);

        auto handler = std::make_unique<FileIOHandler>(path);
        PsyMP3::Demuxer::Raw::RawAudioDemuxer demuxer(std::move(handler), path);
        require(demuxer.parseContainer(), "RawAudioDemuxer should parse .g722 files");

        StreamInfo stream = demuxer.getStreamInfo(1);
        require(stream.codec_name == "g722", "RawAudioDemuxer should classify .g722 as g722");
        require(stream.sample_rate == 16000, "Raw G.722 should default to 16 kHz PCM output");
        require(stream.duration_samples == expected_samples, "Duration should count decoded samples, two per octet");

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
            for (AudioSample sample : frame.samples) {
                total_energy += std::abs(static_cast<int>(sample));
            }
        }

        require(decoded_samples == expected_samples, "Every G.722 octet should decode to two samples");
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
