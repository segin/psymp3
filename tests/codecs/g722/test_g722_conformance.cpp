/*
 * test_g722_conformance.cpp - G722Decoder against ITU-T G.722 Appendix II
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Appendix II's digital test sequences check the ADPCM decoders, in all three
 * modes, word for word (Configuration 2; the QMF is outside their scope). The
 * sequences are ITU's and are not kept here: tests/fetch_g722_testvectors.sh
 * downloads them into tests/data/g722. Without them this test skips (77).
 *
 * The files hold 16-bit little-endian words. An input word carries the octet
 * in its upper byte and the reset request RSS in its lowest bit. The decoder
 * output words are the sub-band signal shifted left by one, and a reset word
 * yields the output 0x0001.
 */

#include "psymp3.h"
#include "test_framework.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace TestFramework;
using PsyMP3::Codec::PCM::G722Decoder;

namespace {

std::string g_dir;

std::string findVectors()
{
    std::vector<std::string> candidates;
    if (const char* srcdir = std::getenv("srcdir")) {
        candidates.push_back(std::string(srcdir) + "/data/g722");
    }
    candidates.push_back("data/g722");
    candidates.push_back("tests/data/g722");
    for (const std::string& dir : candidates) {
        if (std::filesystem::exists(dir + "/bt2r1.cod") && std::filesystem::exists(dir + "/bt3h1.rc0")) {
            return dir;
        }
    }
    return std::string();
}

std::vector<uint16_t> readWords(const std::string& name)
{
    std::ifstream in(g_dir + "/" + name, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<uint16_t> words(bytes.size() / 2);
    for (size_t i = 0; i < words.size(); ++i) {
        words[i] = static_cast<uint16_t>(static_cast<uint8_t>(bytes[2 * i])
                                       | static_cast<uint8_t>(bytes[2 * i + 1]) << 8);
    }
    return words;
}

/// Decodes @p input in @p mode and compares both sub-bands with the expected
/// files. Reports the first mismatch of each band.
void runSequence(const std::string& input, G722Decoder::Bitrate mode,
                 const std::string& low_expected, const std::string& high_expected)
{
    const std::vector<uint16_t> in = readWords(input);
    const std::vector<uint16_t> low = readWords(low_expected);
    const std::vector<uint16_t> high = readWords(high_expected);
    ASSERT_TRUE(!in.empty() && in.size() == low.size() && in.size() == high.size(),
                input + ": the sequence and its expected outputs have the same length");

    G722Decoder decoder(mode, false);
    for (size_t i = 0; i < in.size(); ++i) {
        uint16_t got_low = 1;
        uint16_t got_high = 1;
        if (in[i] & 1) {
            decoder.reset(); // RSS: a reset word decodes nothing
        } else {
            const uint8_t octet = static_cast<uint8_t>(in[i] >> 8);
            int16_t sample = 0;
            decoder.decode(&octet, 1, &sample);
            got_low = static_cast<uint16_t>(decoder.lastLowBand() * 2);
            got_high = static_cast<uint16_t>(decoder.lastHighBand() * 2);
        }
        if (got_low != low[i]) {
            ASSERT_EQUALS(low[i], got_low,
                          input + " vs " + low_expected + ": lower band at word " + std::to_string(i));
        }
        if (got_high != high[i]) {
            ASSERT_EQUALS(high[i], got_high,
                          input + " vs " + high_expected + ": higher band at word " + std::to_string(i));
        }
    }
}

struct Sequence {
    const char* input;
    const char* low[3];   // modes 1, 2, 3
    const char* high;
};

const Sequence kSequences[] = {
    {"bt2r1.cod", {"bt3l1.rc1", "bt3l1.rc2", "bt3l1.rc3"}, "bt3h1.rc0"},
    {"bt2r2.cod", {"bt3l2.rc1", "bt3l2.rc2", "bt3l2.rc3"}, "bt3h2.rc0"},
    {"bt1d3.cod", {"bt3l3.rc1", "bt3l3.rc2", "bt3l3.rc3"}, "bt3h3.rc0"},
};

class ModeTest : public TestCase {
public:
    ModeTest(int mode, G722Decoder::Bitrate rate)
        : TestCase("Appendix II sequences, mode " + std::to_string(mode)), m_mode(mode), m_rate(rate) {}

protected:
    void runTest() override
    {
        for (const Sequence& sequence : kSequences) {
            runSequence(sequence.input, m_rate, sequence.low[m_mode - 1], sequence.high);
        }
    }

private:
    int m_mode;
    G722Decoder::Bitrate m_rate;
};

} // namespace

int test_g722_conformance_main()
{
    g_dir = findVectors();
    if (g_dir.empty()) {
        std::cout << "SKIP: G.722 Appendix II sequences not found; run tests/fetch_g722_testvectors.sh"
                  << std::endl;
        return 77;
    }

    TestSuite suite("G.722 Appendix II Conformance");
    suite.addTest(std::make_unique<ModeTest>(1, G722Decoder::Bitrate::Rate64k));
    suite.addTest(std::make_unique<ModeTest>(2, G722Decoder::Bitrate::Rate56k));
    suite.addTest(std::make_unique<ModeTest>(3, G722Decoder::Bitrate::Rate48k));
    auto results = suite.runAll();
    suite.printResults(results);
    return static_cast<int>(results.size()) - suite.getPassedCount(results);
}
