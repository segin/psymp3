/*
 * EAC3AhtMantissas.cpp - E-AC-3 Adaptive Hybrid Transform mantissas, A/52 Annex E §E3.4
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

namespace {

constexpr double kPi = 3.14159265358979323846;

/// Read @p bits and sign-extend them as a two's complement value.
int readSigned(AC3BitReader& reader, unsigned bits)
{
    if (bits == 0) {
        return 0;
    }
    const unsigned raw = reader.read(bits);
    const unsigned sign = 1u << (bits - 1);
    return static_cast<int>(raw ^ sign) - static_cast<int>(sign);
}

/// A two's complement codeword of @p bits as a fraction in [-1, 1).
float fraction(int code, unsigned bits)
{
    return std::ldexp(static_cast<float>(code), -static_cast<int>(bits - 1));
}

/// Table E3.6: y = x + a*x + b, with a and b chosen by gain and by x's sign.
float remap(float x, unsigned hebap, unsigned gain_index)
{
    const unsigned row = hebap - 8;
    const unsigned sign = x < 0.0f ? 1u : 0u;
    const float a = kGaqRemapA[row][sign][gain_index] / 32768.0f;
    const float b = kGaqRemapB[row][sign][gain_index] / 32768.0f;
    return x + a * x + b;
}

} // namespace

unsigned eac3GaqSections(unsigned gaqmod, unsigned active_bins)
{
    switch (gaqmod) {
    case 1:
    case 2:
        return active_bins;               // one 1-bit gain per active bin
    case 3:
        return (active_bins + 2) / 3;     // three 3-state gains per 5-bit word
    default:
        return 0;                         // no gains at all
    }
}

float eac3GaqDequantize(AC3BitReader& reader, unsigned hebap, unsigned gain)
{
    const unsigned m = kHebapBits[hebap];

    if (gain == 1) {
        // A single symmetric quantizer, m bits, no tag; the remap turns the
        // two's complement codeword into a mid-tread reconstruction.
        return remap(fraction(readSigned(reader, m), m), hebap, 0);
    }

    // Gk of 2 or 4: a shorter small codeword, gain-attenuated, unless it is
    // the tag -- the full-scale negative codeword -- in which case a large
    // mantissa follows it and is remapped instead (Table E3.5).
    const unsigned gain_index = gain == 2 ? 1u : 2u;
    const unsigned small_bits = gain == 2 ? m - 1 : m - 2;
    const unsigned large_bits = gain == 2 ? m - 1 : m;
    const int small = readSigned(reader, small_bits);
    const int tag = -(1 << (small_bits - 1));
    if (small == tag) {
        const int large = readSigned(reader, large_bits);
        return remap(fraction(large, large_bits), hebap, gain_index);
    }
    return fraction(small, small_bits) / static_cast<float>(gain);
}

void eac3AhtInverseDct(const float x[6], float c[6])
{
    for (unsigned m = 0; m < 6; ++m) {
        double sum = 0.5 * x[0];
        for (unsigned j = 1; j < 6; ++j) {
            sum += x[j] * std::cos(j * (2.0 * m + 1.0) * kPi / 12.0);
        }
        c[m] = static_cast<float>(2.0 * sum);
    }
}

bool eac3AhtReadChannel(AC3BitReader& reader, const uint8_t* hebap,
                        unsigned start, unsigned end, const uint8_t* exponents,
                        EAC3AhtSpectrum& spectrum, const char** reason)
{
    auto fail = [&](const char* why) { if (reason) { *reason = why; } return false; };
    spectrum = EAC3AhtSpectrum();
    if (end > kBlockSamples || start > end) {
        return fail("AHT bin range out of bounds");
    }

    const unsigned gaqmod = reader.read(2);

    // §E3.4.2: which bins are gain-adaptively quantized. Below hebap 8 the
    // bin is vector quantized; from 8 up to endbap it takes a gain; above
    // that it is GAQ-shaped but always at unit gain.
    const unsigned endbap = gaqmod < 2 ? 12u : 17u;
    int gaqbin[256] = {};
    unsigned active = 0;
    for (unsigned bin = start; bin < end; ++bin) {
        if (hebap[bin] > 7 && hebap[bin] < endbap) {
            gaqbin[bin] = 1;
            ++active;
        } else if (hebap[bin] >= endbap) {
            gaqbin[bin] = -1;
        }
    }

    // Gains, in ascending frequency order over the active bins.
    std::vector<uint8_t> gains;
    const unsigned sections = eac3GaqSections(gaqmod, active);
    if (gaqmod == 1 || gaqmod == 2) {
        const uint8_t large = gaqmod == 1 ? 2 : 4;
        for (unsigned n = 0; n < sections; ++n) {
            gains.push_back(reader.readBit() ? large : 1);
        }
    } else if (gaqmod == 3) {
        static constexpr uint8_t kMapped[3] = { 1, 2, 4 };   // Table E3.4
        for (unsigned n = 0; n < sections; ++n) {
            const unsigned word = reader.read(5);
            if (word > 26) {
                return fail("GAQ gain triplet out of range");
            }
            gains.push_back(kMapped[word / 9]);
            gains.push_back(kMapped[(word % 9) / 3]);
            gains.push_back(kMapped[(word % 9) % 3]);
        }
    }

    unsigned next_gain = 0;
    for (unsigned bin = start; bin < end; ++bin) {
        float x[6] = {};
        const unsigned h = hebap[bin];
        if (h > 19) {
            return fail("hebap out of range");
        }
        if (gaqbin[bin] != 0) {
            // Six scalar mantissas, one per DCT coefficient.
            unsigned gain = 1;
            if (gaqbin[bin] == 1 && next_gain < gains.size()) {
                gain = gains[next_gain];
            }
            if (gaqbin[bin] == 1) {
                ++next_gain;
            }
            for (unsigned j = 0; j < 6; ++j) {
                x[j] = eac3GaqDequantize(reader, h, gain);
            }
        } else if (h > 0) {
            // One vector index standing for all six (§E3.4.4.1).
            const unsigned index = reader.read(kHebapBits[h]);
            for (unsigned j = 0; j < 6; ++j) {
                x[j] = kAhtVqTables[h][index][j] / 32768.0f;
            }
        }

        // §E3.4.5: the DCT is inverted before the exponent is applied.
        float c[6];
        eac3AhtInverseDct(x, c);
        const int shift = -static_cast<int>(exponents[bin]);
        for (unsigned m = 0; m < 6; ++m) {
            spectrum.value[m][bin] = std::ldexp(c[m], shift);
        }
    }

    if (reader.overrun()) {
        return fail("AHT mantissas ran past the end of the frame");
    }
    return true;
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
