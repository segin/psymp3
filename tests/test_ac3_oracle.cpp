/*
 * test_ac3_oracle.cpp - AC-3/E-AC-3 decoding compared with ffmpeg's decode
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The broadband streams made by generate_ac3_corpus.sh reach decoder tools
 * that a tone never does: coupling, the dither of coupled bins, and all four
 * rematrixing bands. ffmpeg's decode of the same stream is the oracle. The
 * comparison is of band energies rather than samples, because dither is
 * random in both decoders. Bugs in these tools move whole bands by 13 to
 * 18 dB, far outside the tolerance.
 *
 * The corpus is not checked in. Without it, this test skips (exit 77).
 */

#include "psymp3.h"
#include "test_framework.h"

#include <complex>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace TestFramework;
using namespace PsyMP3;
using namespace PsyMP3::Demuxer;

namespace {

std::string g_corpus;

/// Where generate_ac3_corpus.sh put its output, relative to the source
/// directory (srcdir) or to the directory the test runs in.
std::string findCorpus()
{
    std::vector<std::string> candidates;
    if (const char* srcdir = std::getenv("srcdir")) {
        candidates.push_back(std::string(srcdir) + "/data/ac3");
    }
    candidates.push_back("data/ac3");
    candidates.push_back("tests/data/ac3");
    for (const std::string& dir : candidates) {
        if (std::filesystem::exists(dir + "/raw/ac3_coupled_noise.ac3")
            && std::filesystem::exists(dir + "/ref/ac3_coupled_noise.wav")) {
            return dir;
        }
    }
    return std::string();
}

/// Interleaved PCM scaled to [-1, 1).
struct Pcm {
    unsigned channels = 0;
    std::vector<double> samples;
    size_t frames() const { return channels ? samples.size() / channels : 0; }
};

/// ffmpeg's reference: a 16-bit PCM WAV, read chunk by chunk.
Pcm readWav(const std::string& path)
{
    Pcm pcm;
    std::ifstream in(path, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto u16 = [&](size_t at) {
        return static_cast<unsigned>(static_cast<uint8_t>(bytes[at]))
             | static_cast<unsigned>(static_cast<uint8_t>(bytes[at + 1])) << 8;
    };
    const auto u32 = [&](size_t at) {
        return static_cast<uint32_t>(u16(at)) | static_cast<uint32_t>(u16(at + 2)) << 16;
    };
    if (bytes.size() < 12) {
        return pcm;
    }
    size_t at = 12;
    unsigned bits = 0;
    while (at + 8 <= bytes.size()) {
        const std::string id(&bytes[at], 4);
        const size_t size = u32(at + 4);
        const size_t body = at + 8;
        if (id == "fmt " && body + 16 <= bytes.size()) {
            pcm.channels = u16(body + 2);
            bits = u16(body + 14);
        } else if (id == "data" && bits == 16) {
            const size_t end = std::min(bytes.size(), body + size);
            for (size_t i = body; i + 1 < end; i += 2) {
                pcm.samples.push_back(static_cast<int16_t>(u16(i)) / 32768.0);
            }
            break;
        }
        at = body + size + (size & 1);
    }
    return pcm;
}

/// PsyMP3's decode, through the same DemuxedStream path playback uses.
Pcm decode(const std::string& path)
{
    Pcm pcm;
    DemuxedStream stream{TagLib::String(path, TagLib::String::UTF8)};
    pcm.channels = stream.getChannels();
    std::vector<AudioSample> buffer(8192);
    for (int idle = 0; idle < 64;) {
        const size_t bytes = stream.getData(buffer.size() * sizeof(AudioSample), buffer.data());
        if (bytes == 0) {
            if (stream.eof()) {
                break;
            }
            ++idle;
            continue;
        }
        idle = 0;
        for (size_t i = 0; i < bytes / sizeof(AudioSample); ++i) {
            pcm.samples.push_back(buffer[i] / 2147483648.0);
        }
    }
    return pcm;
}

void fft(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const std::complex<double> step = std::polar(1.0, -2.0 * M_PI / static_cast<double>(len));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = a[i + k];
                const std::complex<double> v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= step;
            }
        }
    }
}

constexpr size_t kWindow = 2048;
constexpr unsigned kBandHz = 1000;

/// Mean power in 1 kHz bands, one channel, Hann-windowed 2048-sample frames.
std::vector<double> bandPower(const Pcm& pcm, unsigned ch, unsigned rate)
{
    const unsigned bands = rate / 2 / kBandHz;
    std::vector<double> power(bands, 0.0);
    const size_t frames = pcm.frames() / kWindow;
    for (size_t f = 0; f < frames; ++f) {
        std::vector<std::complex<double>> x(kWindow);
        for (size_t i = 0; i < kWindow; ++i) {
            const double hann = 0.5 - 0.5 * std::cos(2.0 * M_PI * static_cast<double>(i) / kWindow);
            x[i] = pcm.samples[(f * kWindow + i) * pcm.channels + ch] * hann;
        }
        fft(x);
        for (size_t bin = 0; bin < kWindow / 2; ++bin) {
            const size_t band = bin * rate / kWindow / kBandHz;
            if (band < bands) {
                power[band] += std::norm(x[bin]);
            }
        }
    }
    return power;
}

/// Every band holding real signal in the reference (within 40 dB of its
/// strongest band) must match it to within @p tolerance_db. Bands below that
/// are the encoder's silence above its bandwidth, where both decoders only
/// produce quantisation noise.
void compareStream(const std::string& name, const std::string& ext, double tolerance_db)
{
    const Pcm ours = decode(g_corpus + "/raw/" + name + "." + ext);
    const Pcm oracle = readWav(g_corpus + "/ref/" + name + ".wav");
    ASSERT_EQUALS(oracle.channels, ours.channels, name + ": channel count");
    ASSERT_TRUE(ours.frames() + 1536 >= oracle.frames() && ours.frames() <= oracle.frames() + 1536,
                name + ": decoded length within a syncframe of the reference");
    const unsigned rate = 48000;
    for (unsigned ch = 0; ch < oracle.channels; ++ch) {
        const std::vector<double> want = bandPower(oracle, ch, rate);
        const std::vector<double> got = bandPower(ours, ch, rate);
        double peak = 0.0;
        for (double p : want) {
            peak = std::max(peak, p);
        }
        for (size_t band = 0; band < want.size(); ++band) {
            if (want[band] < peak * 1e-4) {
                continue;
            }
            const double diff = 10.0 * std::log10(got[band] / want[band]);
            const std::string where = name + " channel " + std::to_string(ch) + ", "
                                    + std::to_string(band) + "-" + std::to_string(band + 1)
                                    + " kHz: " + std::to_string(diff) + " dB from ffmpeg";
            std::cout << "  " << where << std::endl;
            ASSERT_TRUE(std::fabs(diff) <= tolerance_db, where);
        }
    }
}

class CouplingTest : public TestCase {
public:
    CouplingTest() : TestCase("Coupled bands decode at ffmpeg's level (A/52 7.4.3's x8)") {}
protected:
    void runTest() override { compareStream("ac3_coupled_noise", "ac3", 1.0); }
};

class EAC3CouplingTest : public TestCase {
public:
    EAC3CouplingTest() : TestCase("E-AC-3 coupled bands decode at ffmpeg's level") {}
protected:
    void runTest() override { compareStream("eac3_coupled_noise", "eac3", 1.0); }
};

class CoupledDitherTest : public TestCase {
public:
    CoupledDitherTest() : TestCase("Dither in coupled bins follows the channel's coordinate") {}
protected:
    void runTest() override { compareStream("ac3_coupled_quiet", "ac3", 1.0); }
};

class RematrixTest : public TestCase {
public:
    RematrixTest() : TestCase("All four rematrixing bands are undone on the right bins") {}
protected:
    void runTest() override { compareStream("ac3_rematrix_noise", "ac3", 1.0); }
};

} // namespace

int main()
{
    g_corpus = findCorpus();
    if (g_corpus.empty()) {
        std::cout << "SKIP: AC-3 oracle corpus not found; run tests/generate_ac3_corpus.sh" << std::endl;
        return 77;
    }
    registerAllCodecs();
    registerAllDemuxers();

    TestSuite suite("AC-3 Oracle Comparison Tests");
    suite.addTest(std::make_unique<CouplingTest>());
    suite.addTest(std::make_unique<EAC3CouplingTest>());
    suite.addTest(std::make_unique<CoupledDitherTest>());
    suite.addTest(std::make_unique<RematrixTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    // Count every test that did not pass: an exception is not a failure to
    // getFailureCount(), and a decoder that throws must not read as green.
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
