/*
 * AC3Downmix.h - AC-3 channel arrangements onto speaker layouts, A/52 §7.8
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CODECS_AC3_AC3DOWNMIX_H
#define PSYMP3_CODECS_AC3_AC3DOWNMIX_H

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// The most channels an output layout has: SDL's 7.1.
constexpr unsigned kMaxOutputChannels = 8;

/// Inputs to an AC3OutputMatrix: the full-bandwidth channels in bitstream
/// order (A/52 Table 5.8), then the LFE.
constexpr unsigned kMixInputs = kMaxFullBandwidthChannels + 1;
constexpr unsigned kMixLfeInput = kMaxFullBandwidthChannels;

/// clev and slev, the centre and surround downmix levels of §7.8.2.
struct AC3MixLevels {
    float center = 0.707f;
    float surround = 0.707f;

    bool operator==(const AC3MixLevels& other) const
    {
        return center == other.center && surround == other.surround;
    }
    bool operator!=(const AC3MixLevels& other) const { return !(*this == other); }
};

/// From AC-3's cmixlev and surmixlev, Tables 5.9 and 5.10. A reserved code
/// takes the intermediate level, as §7.8.2 asks.
AC3MixLevels ac3MixLevels(unsigned cmixlev, unsigned surmixlev);

/// From E-AC-3's lorocmixlev and lorosurmixlev. Annex E defines these by
/// Annex D's Tables D2.5 and D2.6, whose reserved surround codes mean 0.841.
AC3MixLevels eac3MixLevels(unsigned lorocmixlev, unsigned lorosurmixlev);

/// Gains from each decoded channel to each output channel.
struct AC3OutputMatrix {
    unsigned outputs = 0;
    float gain[kMaxOutputChannels][kMixInputs] = {};
};

/// How one coded arrangement plays through @p outputs channels of SDL's
/// default layouts (see ac3OutputChannels()).
///
/// A coded channel with a speaker of its own is routed straight to it. The
/// rest follow §7.8.1: a centre with no centre speaker goes to left and
/// right at clev (-3 dB from 1/0), surrounds with no surround speakers go to
/// the fronts at slev, a single surround with a pair to play through goes to
/// both at -3 dB, and one speaker takes the fronts at -3 dB with the
/// surrounds at slev below that. The LFE is kept only where the layout has
/// one, which §7.8 allows. If any output then sums to more than unity gain,
/// every gain is scaled down alike until none does -- the normalisation
/// §7.8.1 describes -- so a full-scale stream cannot clip in the downmix.
///
/// @param outputs  1 to kMaxOutputChannels
AC3OutputMatrix ac3OutputMatrix(AudioCodingMode acmod, bool lfeon, unsigned outputs,
                                const AC3MixLevels& levels);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // PSYMP3_CODECS_AC3_AC3DOWNMIX_H
