/*
 * generate_matroska_corpus.cpp - Writes the .mka fixtures the Matroska tests use.
 *
 * Run from the top of the tree:
 *   ./tests/generate_matroska_corpus
 *
 * The files are not committed -- tests/data holds no tracked binaries -- so
 * this is what produces them, and it is the thing to read to know what each
 * one contains.
 *
 * Every fixture carries raw PCM rather than a compressed codec. That is the
 * whole trick: a sine wave can be synthesised here in a few lines, so the
 * corpus needs no encoder, no ffmpeg and no network, and the files are
 * genuinely decodable by PsyMP3 rather than merely parseable. It also means
 * they are byte-identical on every machine that builds them.
 *
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "matroska_ebml_builder.h"

#include <filesystem>
#include <fstream>
#include <cmath>

using namespace MatroskaBuilder;
using namespace PsyMP3::Demuxer::Matroska;

namespace {

constexpr uint32_t kSampleRate = 44100;
constexpr uint16_t kChannels   = 2;
constexpr uint16_t kBitDepth   = 16;
constexpr int      kDurationMs = 1000;
/// One block per 20 ms, which is about what a muxer emits and gives enough
/// clusters for a seek index to be worth having.
constexpr int      kBlockMs    = 20;

/// A sine at @p hz, 16-bit stereo. Little-endian unless @p big_endian.
std::vector<uint8_t> sinePcm(int milliseconds, bool big_endian = false, double hz = 440.0)
{
    const size_t frames = static_cast<size_t>(kSampleRate) * milliseconds / 1000;
    std::vector<uint8_t> out;
    out.reserve(frames * kChannels * 2);
    for (size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        const auto sample = static_cast<int16_t>(std::lround(std::sin(t * hz * 2.0 * M_PI) * 20000.0));
        const auto value = static_cast<uint16_t>(sample);
        for (int channel = 0; channel < kChannels; ++channel) {
            if (big_endian) {
                out.push_back(static_cast<uint8_t>(value >> 8));
                out.push_back(static_cast<uint8_t>(value & 0xFF));
            } else {
                out.push_back(static_cast<uint8_t>(value & 0xFF));
                out.push_back(static_cast<uint8_t>(value >> 8));
            }
        }
    }
    return out;
}

std::vector<uint8_t> trackEntry(uint64_t number, const char* codec_id,
                                bool is_audio = true, bool is_default = false)
{
    std::vector<uint8_t> body = uintEl(Id::TrackNumber, number)
                              + uintEl(Id::TrackUID, 0x1000 + number)
                              + uintEl(Id::TrackType, is_audio ? TrackType::Audio
                                                               : TrackType::Video)
                              + strEl(Id::CodecID, codec_id)
                              + strEl(Id::Language, "und");
    if (is_default) {
        body = body + uintEl(Id::FlagDefault, 1);
    }
    if (is_audio) {
        body = body + element(Id::Audio, floatEl(Id::SamplingFrequency, kSampleRate)
                                       + uintEl(Id::Channels, kChannels)
                                       + uintEl(Id::BitDepth, kBitDepth));
    }
    return element(Id::TrackEntry, body);
}

std::vector<uint8_t> segmentInfo(const char* title)
{
    const double ticks = kDurationMs; // one tick is one millisecond
    return element(Id::Info, uintEl(Id::TimestampScale, 1000000)
                           + floatEl(Id::Duration, ticks)
                           + strEl(Id::MuxingApp, "PsyMP3 fixture generator")
                           + strEl(Id::WritingApp, "PsyMP3 fixture generator")
                           + strEl(Id::Title, title));
}

/// A SimpleBlock holding one frame, unlaced.
std::vector<uint8_t> simpleBlock(uint64_t track, int16_t relative_ticks,
                                 const std::vector<uint8_t>& frame)
{
    const auto raw = static_cast<uint16_t>(relative_ticks);
    std::vector<uint8_t> body{static_cast<uint8_t>(0x80 | track),
                              static_cast<uint8_t>(raw >> 8),
                              static_cast<uint8_t>(raw & 0xFF),
                              0x80}; // keyframe, no lacing
    return element(Id::SimpleBlock, body + frame);
}

/// A SimpleBlock packing several frames with Xiph or EBML lacing.
std::vector<uint8_t> lacedBlock(uint64_t track, int16_t relative_ticks,
                                const std::vector<std::vector<uint8_t>>& frames,
                                Lacing lacing)
{
    const auto raw = static_cast<uint16_t>(relative_ticks);
    std::vector<uint8_t> body{static_cast<uint8_t>(0x80 | track),
                              static_cast<uint8_t>(raw >> 8),
                              static_cast<uint8_t>(raw & 0xFF),
                              static_cast<uint8_t>(0x80 | (static_cast<int>(lacing) << 1))};
    body.push_back(static_cast<uint8_t>(frames.size() - 1));

    if (lacing == Lacing::Xiph) {
        // Every size but the last, each a run of 0xFF plus a terminator.
        for (size_t i = 0; i + 1 < frames.size(); ++i) {
            size_t size = frames[i].size();
            while (size >= 255) {
                body.push_back(0xFF);
                size -= 255;
            }
            body.push_back(static_cast<uint8_t>(size));
        }
    } else if (lacing == Lacing::EBML) {
        // The first size outright, then signed deltas; the last is implicit.
        const std::vector<uint8_t> first = sizeBytes(frames[0].size());
        body.insert(body.end(), first.begin(), first.end());
        for (size_t i = 1; i + 1 < frames.size(); ++i) {
            const auto delta = static_cast<int64_t>(frames[i].size())
                             - static_cast<int64_t>(frames[i - 1].size());
            // One-byte signed VINT, biased by 63. Every frame here is the same
            // size, so the delta is zero and one byte always suffices.
            body.push_back(static_cast<uint8_t>(0x80 | static_cast<uint8_t>(delta + 63)));
        }
    }

    for (const std::vector<uint8_t>& frame : frames) {
        body.insert(body.end(), frame.begin(), frame.end());
    }
    return element(Id::SimpleBlock, body);
}

/// Splits the audio into one cluster per block period.
/// @param lacing        when not None, each cluster holds one laced block of
///                      @p lace_count frames instead of several unlaced ones
std::vector<std::vector<uint8_t>> clusters(const std::vector<uint8_t>& pcm,
                                           uint64_t track,
                                           Lacing lacing = Lacing::None,
                                           size_t lace_count = 4)
{
    const size_t frame_bytes = static_cast<size_t>(kSampleRate) * kBlockMs / 1000
                             * kChannels * kBitDepth / 8;
    std::vector<std::vector<uint8_t>> out;

    size_t offset = 0;
    int timestamp = 0;
    while (offset < pcm.size()) {
        std::vector<std::vector<uint8_t>> frames;
        const size_t want = (lacing == Lacing::None) ? 1 : lace_count;
        for (size_t i = 0; i < want && offset < pcm.size(); ++i) {
            const size_t take = std::min(frame_bytes, pcm.size() - offset);
            frames.emplace_back(pcm.begin() + static_cast<long>(offset),
                                pcm.begin() + static_cast<long>(offset + take));
            offset += take;
        }
        if (frames.empty()) {
            break;
        }

        std::vector<uint8_t> body = uintEl(Id::Timestamp, static_cast<uint64_t>(timestamp));
        if (lacing == Lacing::None || frames.size() == 1) {
            body = body + simpleBlock(track, 0, frames[0]);
        } else {
            body = body + lacedBlock(track, 0, frames, lacing);
        }
        out.push_back(element(Id::Cluster, body));
        timestamp += kBlockMs * static_cast<int>(frames.size());
    }
    return out;
}

/// A Cues element naming one entry per cluster, for @p track.
std::vector<uint8_t> cuesFor(const std::vector<std::vector<uint8_t>>& cluster_bytes,
                             uint64_t track, uint64_t first_cluster_relative)
{
    std::vector<uint8_t> points;
    uint64_t position = first_cluster_relative;
    int timestamp = 0;
    for (const std::vector<uint8_t>& cluster : cluster_bytes) {
        points = points + element(Id::CuePoint,
                                  uintEl(Id::CueTime, static_cast<uint64_t>(timestamp))
                                + element(Id::CueTrackPositions,
                                          uintEl(Id::CueTrack, track)
                                        + uintEl(Id::CueClusterPosition, position)));
        position += cluster.size();
        timestamp += kBlockMs;
    }
    return element(Id::Cues, points);
}


/// A SeekHead naming where each element begins.
///
/// Each entry gives an offset measured from the *end of the SeekHead*, not from
/// the Segment, because that is the part the caller can compute: what sits
/// between the named elements -- the clusters, typically -- is the caller's
/// layout to know. The absolute position is that offset plus the SeekHead's own
/// size, and that size depends on how many bytes the positions need, so it is
/// built to a fixed point rather than measured once.
///
/// Measuring once against placeholders gives narrower VINTs than the real
/// values need and leaves every element a byte or two from where the file says
/// it is. No parser recovers from that, and a small fixture never exposes it,
/// because the widths do not change until the numbers get large.
std::vector<uint8_t> seekHeadFor(const std::vector<std::pair<uint32_t, uint64_t>>& entries)
{
    auto build = [&](uint64_t own_size) {
        std::vector<uint8_t> body;
        for (const auto& entry : entries) {
            body = body + element(Id::Seek,
                                  element(Id::SeekID, idBytes(entry.first))
                                + uintEl(Id::SeekPosition, own_size + entry.second));
        }
        return element(Id::SeekHead, body);
    };

    uint64_t own_size = 0;
    for (int attempt = 0; attempt < 8; ++attempt) {
        const uint64_t measured = build(own_size).size();
        if (measured == own_size) {
            break;
        }
        own_size = measured;
    }
    return build(own_size);
}

std::vector<uint8_t> flatten(const std::vector<std::vector<uint8_t>>& parts)
{
    std::vector<uint8_t> out;
    for (const std::vector<uint8_t>& part : parts) {
        out.insert(out.end(), part.begin(), part.end());
    }
    return out;
}

void write(const std::string& directory, const std::string& name,
           const std::vector<uint8_t>& bytes)
{
    const std::string path = directory + "/" + name;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    std::cout << "  " << name << "  (" << bytes.size() << " bytes)\n";
}

} // namespace

int main(int argc, char** argv)
{
    const std::string directory = argc > 1 ? argv[1] : "tests/data";
    std::filesystem::create_directories(directory);
    std::cout << "Writing Matroska fixtures to " << directory << "/\n";

    const std::vector<uint8_t> pcm = sinePcm(kDurationMs);
    const std::vector<uint8_t> header = ebmlHeader("matroska");

    // The plain case: one PCM track, one cluster per 20 ms, with Cues.
    {
        const std::vector<uint8_t> info = segmentInfo("PCM basic");
        const std::vector<uint8_t> tracks = element(Id::Tracks, trackEntry(1, "A_PCM/INT/LIT"));
        const auto cluster_bytes = clusters(pcm, 1);
        const std::vector<uint8_t> clusters_flat = flatten(cluster_bytes);

        // Cues live after the clusters, so without a SeekHead naming them
        // nothing finds them -- which is how a real file is laid out and why a
        // fixture without one silently exercises the scan fallback instead.
        // Offsets from the end of the SeekHead: Info first, then Tracks, then
        // the clusters, and the Cues after all of them.
        const std::vector<uint8_t> head =
            seekHeadFor({{Id::Info,   0},
                         {Id::Tracks, info.size()},
                         {Id::Cues,   info.size() + tracks.size() + clusters_flat.size()}});
        const std::vector<uint8_t> cues =
            cuesFor(cluster_bytes, 1, head.size() + info.size() + tracks.size());
        write(directory, "mka_pcm_basic.mka",
              header + element(Id::Segment, head + info + tracks + clusters_flat + cues));
    }

    // No Cues at all, so seeking has to fall back to scanning the clusters.
    {
        const std::vector<uint8_t> info = segmentInfo("PCM without cues");
        const std::vector<uint8_t> tracks = element(Id::Tracks, trackEntry(1, "A_PCM/INT/LIT"));
        write(directory, "mka_pcm_no_cues.mka",
              header + element(Id::Segment, info + tracks + flatten(clusters(pcm, 1))));
    }

    // Laced, in both modes that store sizes. No muxer to hand emits these, so
    // without them the lacing paths see no fixture at all.
    for (const auto& mode : {std::make_pair(Lacing::Xiph, "mka_pcm_laced_xiph.mka"),
                             std::make_pair(Lacing::EBML, "mka_pcm_laced_ebml.mka")}) {
        const std::vector<uint8_t> info = segmentInfo("PCM laced");
        const std::vector<uint8_t> tracks = element(Id::Tracks, trackEntry(1, "A_PCM/INT/LIT"));
        write(directory, mode.second,
              header + element(Id::Segment,
                               info + tracks + flatten(clusters(pcm, 1, mode.first))));
    }

    // Big-endian PCM, which decodes to static if the byte order is ignored --
    // the same failure AIFF had.
    {
        const std::vector<uint8_t> info = segmentInfo("PCM big-endian");
        const std::vector<uint8_t> tracks = element(Id::Tracks, trackEntry(1, "A_PCM/INT/BIG"));
        write(directory, "mka_pcm_big_endian.mka",
              header + element(Id::Segment,
                               info + tracks + flatten(clusters(sinePcm(kDurationMs, true), 1))));
    }

    // Tracks written after the clusters, reachable only through SeekHead.
    {
        const std::vector<uint8_t> info = segmentInfo("Tracks after clusters");
        const std::vector<uint8_t> tracks = element(Id::Tracks, trackEntry(1, "A_PCM/INT/LIT"));
        const std::vector<uint8_t> cluster_bytes = flatten(clusters(pcm, 1));

        // SeekHead has to state where Tracks ended up, which depends on its own
        // size, so it is built twice: once to measure, once for real.
        auto seek_head_for = [](uint64_t position) {
            return element(Id::SeekHead,
                           element(Id::Seek, element(Id::SeekID, idBytes(Id::Tracks))
                                           + uintEl(Id::SeekPosition, position)));
        };
        // Self-referential: the position depends on SeekHead's size, which
        // depends on how many bytes the position needs. Measuring once against
        // a placeholder of 0 gives a one-byte payload where the real value
        // needs three, leaving Tracks two bytes from where the file says it is
        // -- which no parser recovers from, and which a small fixture would
        // never expose because the width would not change. Iterate instead.
        uint64_t position = 0;
        for (int attempt = 0; attempt < 8; ++attempt) {
            const uint64_t next =
                seek_head_for(position).size() + info.size() + cluster_bytes.size();
            if (next == position) {
                break;
            }
            position = next;
        }
        write(directory, "mka_pcm_tracks_last.mka",
              header + element(Id::Segment,
                               seek_head_for(position) + info + cluster_bytes + tracks));
    }

    // A Segment of unknown size, as a muxer streaming to a socket writes it.
    {
        const std::vector<uint8_t> info = segmentInfo("Unknown size segment");
        const std::vector<uint8_t> tracks = element(Id::Tracks, trackEntry(1, "A_PCM/INT/LIT"));
        const std::vector<uint8_t> body = info + tracks + flatten(clusters(pcm, 1));

        std::vector<uint8_t> out = header;
        const std::vector<uint8_t> id = idBytes(Id::Segment);
        out.insert(out.end(), id.begin(), id.end());
        // Eight bytes with every value bit set: the unknown-size marker.
        for (uint8_t byte : {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}) {
            out.push_back(byte);
        }
        out.insert(out.end(), body.begin(), body.end());
        write(directory, "mka_pcm_unknown_size.mka", out);
    }

    // Audio PsyMP3 has no decoder for. The file must be skipped rather than
    // opened silent, which is how every container behaves.
    {
        const std::vector<uint8_t> info = segmentInfo("Unsupported codec only");
        const std::vector<uint8_t> tracks = element(Id::Tracks, trackEntry(1, "A_DTS"));
        write(directory, "mka_unsupported_dts.mka",
              header + element(Id::Segment,
                               info + tracks + flatten(clusters(pcm, 1))));
    }

    // A video track, an undecodable audio track and a playable one, which is
    // the shape of a real .mkv rip. The PCM track is the one to select.
    {
        const std::vector<uint8_t> info = segmentInfo("Mixed tracks");
        const std::vector<uint8_t> tracks =
            element(Id::Tracks, trackEntry(1, "V_MPEG4/ISO/AVC", /*is_audio=*/false)
                              + trackEntry(2, "A_DTS")
                              + trackEntry(3, "A_PCM/INT/LIT"));
        write(directory, "mka_multi_track.mka",
              header + element(Id::Segment, info + tracks + flatten(clusters(pcm, 3))));
    }

    // Two playable audio tracks, the second flagged default, which is how a
    // multi-language release says which to play.
    //
    // Both carry audio, and at different pitches: a fixture where only the
    // wanted track has blocks cannot tell a correct selection from a lucky one,
    // since picking the other yields silence either way rather than the wrong
    // tone.
    {
        const std::vector<uint8_t> info = segmentInfo("Default flag");
        const std::vector<uint8_t> tracks =
            element(Id::Tracks, trackEntry(1, "A_PCM/INT/LIT")
                              + trackEntry(2, "A_PCM/INT/LIT", true, /*is_default=*/true));

        const std::vector<uint8_t> low = sinePcm(kDurationMs, false, 220.0);
        const std::vector<uint8_t> high = sinePcm(kDurationMs, false, 880.0);
        const size_t frame_bytes = static_cast<size_t>(kSampleRate) * kBlockMs / 1000
                                 * kChannels * kBitDepth / 8;

        std::vector<uint8_t> cluster_bytes;
        size_t offset = 0;
        int timestamp = 0;
        while (offset < low.size()) {
            const size_t take = std::min(frame_bytes, low.size() - offset);
            const std::vector<uint8_t> a(low.begin() + static_cast<long>(offset),
                                         low.begin() + static_cast<long>(offset + take));
            const std::vector<uint8_t> b(high.begin() + static_cast<long>(offset),
                                         high.begin() + static_cast<long>(offset + take));
            cluster_bytes = cluster_bytes
                          + element(Id::Cluster,
                                    uintEl(Id::Timestamp, static_cast<uint64_t>(timestamp))
                                  + simpleBlock(1, 0, a)
                                  + simpleBlock(2, 0, b));
            offset += take;
            timestamp += kBlockMs;
        }
        write(directory, "mka_default_flag.mka",
              header + element(Id::Segment, info + tracks + cluster_bytes));
    }

    std::cout << "done\n";
    return 0;
}
