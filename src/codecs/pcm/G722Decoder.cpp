/*
 * G722Decoder.cpp - ITU-T G.722 sub-band ADPCM decoder
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace PCM {

namespace {

// Normative tables from ITU-T G.722.

/// Inverse quantiser, upper band (two bits).
const int kQm2[4] = { -7408, -1616, 7408, 1616 };

/// Inverse quantiser, lower band at four bits (48 kbit/s), and also the
/// adaptation path at every rate.
const int kQm4[16] = {
         0, -20456, -12896, -8968, -6288, -4240, -2584, -1200,
     20456,  12896,   8968,  6288,  4240,  2584,  1200,     0
};

/// Inverse quantiser, lower band at five bits (56 kbit/s).
const int kQm5[32] = {
      -280,   -280, -23352, -17560, -14120, -11664, -9752, -8184,
     -6864,  -5712,  -4696,  -3784,  -2960,  -2208, -1520,  -880,
     23352,  17560,  14120,  11664,   9752,   8184,  6864,  5712,
      4696,   3784,   2960,   2208,   1520,    880,   280,  -280
};

/// Inverse quantiser, lower band at six bits (64 kbit/s).
const int kQm6[64] = {
      -136,   -136,   -136,   -136,
    -24808, -21904, -19008, -16704, -14984, -13512, -12280, -11192,
    -10232,  -9360,  -8576,  -7856,  -7192,  -6576,  -6000,  -5456,
     -4944,  -4464,  -4008,  -3576,  -3168,  -2776,  -2400,  -2032,
     -1688,  -1360,  -1040,   -728,  24808,  21904,  19008,  16704,
     14984,  13512,  12280,  11192,  10232,   9360,   8576,   7856,
      7192,   6576,   6000,   5456,   4944,   4464,   4008,   3576,
      3168,   2776,   2400,   2032,   1688,   1360,   1040,    728,
       432,    136,   -432,   -136
};

/// Logarithmic scale factor step, lower band, indexed by the 4-bit code.
const int kWl[8]    = { -60, -30, 58, 172, 334, 538, 1198, 3042 };
const int kRl42[16] = { 0, 7, 6, 5, 4, 3, 2, 1, 7, 6, 5, 4, 3, 2, 1, 0 };

/// Logarithmic scale factor step, upper band.
const int kWh[3]   = { 0, -214, 798 };
const int kRh2[4]  = { 2, 1, 2, 1 };

/// Scale factor mantissa, shared by both bands.
const int kIlb[32] = {
    2048, 2093, 2139, 2186, 2233, 2282, 2332, 2383,
    2435, 2489, 2543, 2599, 2656, 2714, 2774, 2834,
    2896, 2960, 3025, 3091, 3158, 3228, 3298, 3371,
    3444, 3520, 3597, 3676, 3756, 3838, 3922, 4008
};

/// The QMF's even-numbered coefficients H0, H2, ..., H22 (Table 11). The
/// filter is symmetric, Hi = H(23-i), so the odd-numbered ones H1, H3, ...,
/// H23 are the same values in reverse order.
const int kQmfCoeffs[12] = {
    3, -11, 12, 32, -210, 951, 3876, -805, 362, -156, 53, -11
};

inline int saturate16(int value)
{
    if (value > 32767) {
        return 32767;
    }
    if (value < -32768) {
        return -32768;
    }
    return value;
}

/// LIMIT (Rec. G.722 §6.2.1.6, §6.2.2.5): the sub-band signals RL and RH are
/// held to -16384..16383 on their way to the receive QMF. §5.1 gives every
/// QMF input and output that range, one bit narrower than int16, because
/// Table 9's format carries the most significant magnitude bit at the third
/// bit position. The adaptive predictors never see the limited values: the
/// lower band's runs on RLT (Figure 23), the upper band's on YH before its
/// LIMIT (Figure 30). So the limit changes only what the QMF, or the 8 kHz
/// output, is given.
inline int clipToQmfRange(int value)
{
    if (value > 16383) {
        return 16383;
    }
    if (value < -16384) {
        return -16384;
    }
    return value;
}

/// Narrows a decoder result, already limited to 16 bits, to the output sample.
inline int16_t toPcm(int value)
{
    return static_cast<int16_t>(value);
}

/// SCALEL/SCALEH, Method 2 (Rec. G.722 §6.2.1.3, §6.2.2.3): turn the
/// logarithmic scale factor into the linear one. The two bands differ only in
/// the exponent's bias, 8 for the lower and 10 for the upper, which is the
/// lower band's extra factor of four in equation (3-17) against (3-18). The
/// shift count can go negative, and §6.2 defines ">> n" with a negative n as
/// a left shift.
inline int scaleFactor(int nb, int bias)
{
    const int mantissa = kIlb[(nb >> 6) & 31];
    const int shift = bias - (nb >> 11);
    return (shift < 0 ? (mantissa << -shift) : (mantissa >> shift)) << 2;
}

} // namespace

G722Decoder::G722Decoder(Bitrate rate, bool wideband_out)
    : m_rate(rate)
    , m_wideband(wideband_out)
{
    reset();
}

void G722Decoder::reset()
{
    m_low = Band{};
    m_high = Band{};
    // Both bands start at the smallest scale factor. Band's defaults already
    // say det = 32 for the lower band; the upper band starts at 8.
    m_low.det = 32;
    m_high.det = 8;
    std::fill(std::begin(m_qmf), std::end(m_qmf), 0);
    m_last_rl = 0;
    m_last_rh = 0;
}

void G722Decoder::adapt(Band& band, int dlt)
{
    // PARREC / RECONS (§6.2.1.4): the partially reconstructed signal,
    // DLT + SZ, drives the pole coefficient updates, and the reconstructed
    // one, S + DLT, feeds the pole section (FILTEP). The zero section runs
    // on the quantised differences alone.
    band.d[0] = dlt;
    band.r[0] = saturate16(band.s + dlt);
    band.p[0] = saturate16(band.sz + dlt);

    // UPPOL2: second-order pole coefficient, driven by whether the partially
    // reconstructed signal agrees in sign with its one- and two-step delays.
    for (int i = 0; i < 3; ++i) {
        band.sg[i] = band.p[i] >> 15;
    }
    int wd1 = saturate16(band.a[1] * 4);
    int wd2 = (band.sg[0] == band.sg[1]) ? -wd1 : wd1;
    if (wd2 > 32767) {
        wd2 = 32767;
    }
    int wd3 = ((band.sg[0] == band.sg[2]) ? 128 : -128)
            + (wd2 >> 7)
            + ((band.a[2] * 32512) >> 15);
    if (wd3 > 12288) {
        wd3 = 12288;
    } else if (wd3 < -12288) {
        wd3 = -12288;
    }
    band.ap[2] = wd3;

    // UPPOL1: first-order pole coefficient, bounded so the pair stays stable.
    wd1 = (band.sg[0] == band.sg[1]) ? 192 : -192;
    wd2 = (band.a[1] * 32640) >> 15;
    band.ap[1] = saturate16(wd1 + wd2);
    wd3 = saturate16(15360 - band.ap[2]);
    if (band.ap[1] > wd3) {
        band.ap[1] = wd3;
    } else if (band.ap[1] < -wd3) {
        band.ap[1] = -wd3;
    }

    // UPZERO: the six zero coefficients leak toward zero, each nudged by
    // whether its delayed difference agrees in sign with the current one. A
    // zero difference nudges nothing at all.
    wd1 = (dlt == 0) ? 0 : 128;
    band.sg[0] = dlt >> 15;
    for (int i = 1; i < 7; ++i) {
        band.sg[i] = band.d[i] >> 15;
        wd2 = (band.sg[i] == band.sg[0]) ? wd1 : -wd1;
        wd3 = (band.b[i] * 32640) >> 15;
        band.bp[i] = saturate16(wd2 + wd3);
    }

    // DELAYA: commit the updated coefficients and shift the histories.
    for (int i = 6; i > 0; --i) {
        band.d[i] = band.d[i - 1];
        band.b[i] = band.bp[i];
    }
    for (int i = 2; i > 0; --i) {
        band.r[i] = band.r[i - 1];
        band.p[i] = band.p[i - 1];
        band.a[i] = band.ap[i];
    }

    // FILTEP: two-pole section. Each term is formed at the doubled sample and
    // truncated on its own, which is not the same as summing then shifting.
    wd1 = saturate16(band.r[1] + band.r[1]);
    wd1 = (band.a[1] * wd1) >> 15;
    wd2 = saturate16(band.r[2] + band.r[2]);
    wd2 = (band.a[2] * wd2) >> 15;
    band.sp = saturate16(wd1 + wd2);

    // FILTEZ: six-zero section, likewise truncated per term, and summed from
    // the sixth term down with the Recommendation's saturating "+" at every
    // step (§6.2 notation, §6.2.1.4). Saturating only the finished sum lets an
    // intermediate overflow cancel out where the Recommendation clips it.
    band.sz = 0;
    for (int i = 6; i > 0; --i) {
        wd1 = saturate16(band.d[i] + band.d[i]);
        band.sz = saturate16(band.sz + ((band.b[i] * wd1) >> 15));
    }

    // PREDIC.
    band.s = saturate16(band.sp + band.sz);
}

std::size_t G722Decoder::decode(const uint8_t* data, std::size_t len, int16_t* out)
{
    std::size_t written = 0;

    for (std::size_t n = 0; n < len; ++n) {
        const int code = data[n];

        // Split the octet (§1.4.4): the two upper-band bits are the most
        // significant, the lower band's six-bit ILR the rest -- in every mode.
        // Modes 2 and 3 repack nothing; they only ignore the one or two least
        // significant bits of ILR, which carry auxiliary data (§1.3, Table 2).
        // Reading a right-justified 5- or 4-bit code instead failed every
        // Appendix II sequence for those modes.
        const int ihigh = (code >> 6) & 0x03;
        const int ilr = code & 0x3F;
        int dlow = 0;
        switch (m_rate) {
            case Bitrate::Rate64k:
                dlow = (m_low.det * kQm6[ilr]) >> 15;
                break;
            case Bitrate::Rate56k: // INVQBL mode 2: RIL = ILR >>> 1
                dlow = (m_low.det * kQm5[ilr >> 1]) >> 15;
                break;
            case Bitrate::Rate48k: // INVQBL mode 3: RIL = ILR >>> 2
                dlow = (m_low.det * kQm4[ilr >> 2]) >> 15;
                break;
        }
        // The adaptation path (INVQAL, LOGSCL) always takes ILR's four most
        // significant bits, whatever the mode.
        const int ilow = ilr >> 2;

        // Lower band: reconstruct, adapt the predictor, then the scale factor.
        const int dlowt = (m_low.det * kQm4[ilow]) >> 15;
        const int rlow = clipToQmfRange(m_low.s + dlow);
        adapt(m_low, dlowt);

        int nb = ((m_low.nb * 127) >> 7) + kWl[kRl42[ilow]];
        if (nb < 0) {
            nb = 0;
        } else if (nb > 18432) {
            nb = 18432;
        }
        m_low.nb = nb;
        m_low.det = scaleFactor(nb, 8);

        // Upper band.
        const int dhigh = (m_high.det * kQm2[ihigh]) >> 15;
        const int rhigh = clipToQmfRange(m_high.s + dhigh);
        m_last_rl = rlow;
        m_last_rh = rhigh;
        adapt(m_high, dhigh);

        nb = ((m_high.nb * 127) >> 7) + kWh[kRh2[ihigh]];
        if (nb < 0) {
            nb = 0;
        } else if (nb > 22528) {
            nb = 22528;
        }
        m_high.nb = nb;
        m_high.det = scaleFactor(nb, 10);

        if (!m_wideband) {
            // 8 kHz mode, which is not part of Rec. G.722 (see the header):
            // the upper band is decoded to keep its adaptive state in step,
            // but only the lower band is emitted. rlow is already inside the
            // QMF's range, so doubling it back to full scale lands exactly
            // inside 16 bits.
            out[written++] = toPcm(rlow * 2);
            continue;
        }

        // Synthesis QMF. The two bands enter the delay line as their sum and
        // difference; the two polyphase sums are the consecutive output
        // samples, which is what doubles the rate back to 16 kHz. `odd` is
        // ACCUMC's XOUT1, the differences through H0, H2, ..., H22, and
        // `even` is ACCUMD's XOUT2, the sums through H1, H3, ..., H23;
        // SELECT emits XOUT1 first (§5.2.2).
        for (int i = 0; i < 22; ++i) {
            m_qmf[i] = m_qmf[i + 2];
        }
        m_qmf[22] = rlow + rhigh;
        m_qmf[23] = rlow - rhigh;

        int even = 0;
        int odd = 0;
        for (int i = 0; i < 12; ++i) {
            even += m_qmf[2 * i] * kQmfCoeffs[i];
            odd += m_qmf[2 * i + 1] * kQmfCoeffs[11 - i];
        }
        // ACCUMC/ACCUMD limit XOUT to the Table 9 range (§5.2), so a sum that
        // overshoots is clipped, not wrapped round to the opposite polarity.
        // XOUT is half full scale; shifting by 11 rather than 12 doubles it
        // back and keeps the accumulator's next bit.
        out[written++] = toPcm(saturate16(odd >> 11));
        out[written++] = toPcm(saturate16(even >> 11));
    }

    return written;
}

} // namespace PCM
} // namespace Codec
} // namespace PsyMP3
