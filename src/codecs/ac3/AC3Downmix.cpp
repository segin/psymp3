/*
 * AC3Downmix.cpp - AC-3 channel arrangements onto speaker layouts, A/52 §7.8
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

/// Speaker positions, for coded channels and output channels alike.
enum Speaker : int {
    kNone = -1,
    kLeft,
    kRight,
    kCentre,
    kLfe,
    kLeftSurround,    ///< the surround pair of a 2/2 or 3/2 mode
    kRightSurround,
    kSurround,        ///< the single surround of a 2/1 or 3/1 mode
    kBackLeft,        ///< 7.1's second pair, which no AC-3 mode codes
    kBackRight,
    kSpeakers
};

/// SDL's default layouts (SDL_audio.h), by channel count. The 6-channel
/// layout names its pair BL/BR but allows SL/SR, which is what a 5.1
/// surround pair is; 7.1 lists BL/BR before SL/SR, and a coded pair goes to
/// the sides.
constexpr int kLayouts[kMaxOutputChannels + 1][kMaxOutputChannels] = {
    { kNone, kNone, kNone, kNone, kNone, kNone, kNone, kNone },
    { kCentre, kNone, kNone, kNone, kNone, kNone, kNone, kNone },
    { kLeft, kRight, kNone, kNone, kNone, kNone, kNone, kNone },
    { kLeft, kRight, kLfe, kNone, kNone, kNone, kNone, kNone },
    { kLeft, kRight, kLeftSurround, kRightSurround, kNone, kNone, kNone, kNone },
    { kLeft, kRight, kLfe, kLeftSurround, kRightSurround, kNone, kNone, kNone },
    { kLeft, kRight, kCentre, kLfe, kLeftSurround, kRightSurround, kNone, kNone },
    { kLeft, kRight, kCentre, kLfe, kSurround, kLeftSurround, kRightSurround, kNone },
    { kLeft, kRight, kCentre, kLfe, kBackLeft, kBackRight, kLeftSurround, kRightSurround },
};

/// Each mode's coded channels in bitstream order, Table 5.8. The two
/// programmes of 1+1 take left and right: §7.8.1's "Stereo" dual-mono output.
constexpr int kCoded[8][kMaxFullBandwidthChannels] = {
    { kLeft, kRight, kNone, kNone, kNone },                            // 1+1
    { kCentre, kNone, kNone, kNone, kNone },                           // 1/0
    { kLeft, kRight, kNone, kNone, kNone },                            // 2/0
    { kLeft, kCentre, kRight, kNone, kNone },                          // 3/0
    { kLeft, kRight, kSurround, kNone, kNone },                        // 2/1
    { kLeft, kCentre, kRight, kSurround, kNone },                      // 3/1
    { kLeft, kRight, kLeftSurround, kRightSurround, kNone },           // 2/2
    { kLeft, kCentre, kRight, kLeftSurround, kRightSurround },         // 3/2
};

constexpr float kMinus3dB = 0.70710678f;
constexpr float kPlus3dB = 1.41421356f;

} // namespace

AC3MixLevels ac3MixLevels(unsigned cmixlev, unsigned surmixlev)
{
    static constexpr float kCentreLevels[4] = { 0.707f, 0.595f, 0.500f, 0.595f };
    static constexpr float kSurroundLevels[4] = { 0.707f, 0.500f, 0.0f, 0.500f };
    AC3MixLevels levels;
    levels.center = kCentreLevels[cmixlev & 0x3u];
    levels.surround = kSurroundLevels[surmixlev & 0x3u];
    return levels;
}

AC3MixLevels eac3MixLevels(unsigned lorocmixlev, unsigned lorosurmixlev)
{
    static constexpr float kCentreLevels[8] = {
        1.414f, 1.189f, 1.000f, 0.841f, 0.707f, 0.595f, 0.500f, 0.0f };
    static constexpr float kSurroundLevels[8] = {
        0.841f, 0.841f, 0.841f, 0.841f, 0.707f, 0.595f, 0.500f, 0.0f };
    AC3MixLevels levels;
    levels.center = kCentreLevels[lorocmixlev & 0x7u];
    levels.surround = kSurroundLevels[lorosurmixlev & 0x7u];
    return levels;
}

AC3OutputMatrix ac3OutputMatrix(AudioCodingMode acmod, bool lfeon, unsigned outputs,
                                const AC3MixLevels& levels)
{
    AC3OutputMatrix m;
    m.outputs = std::max(1u, std::min(outputs, kMaxOutputChannels));

    int slot[kSpeakers];
    std::fill(std::begin(slot), std::end(slot), -1);
    for (unsigned out = 0; out < m.outputs; ++out) {
        const int speaker = kLayouts[m.outputs][out];
        if (speaker != kNone) {
            slot[speaker] = static_cast<int>(out);
        }
    }
    auto mix = [&](int speaker, unsigned input, float gain) {
        if (slot[speaker] >= 0) {
            m.gain[slot[speaker]][input] += gain;
        }
    };

    const auto mode = static_cast<unsigned>(acmod) & 0x7u;
    const unsigned fronts = mode == 1 ? 1u : ((mode & 0x1u) ? 3u : 2u);
    const bool one_speaker = m.outputs == 1;

    for (unsigned ch = 0; ch < kMaxFullBandwidthChannels; ++ch) {
        const int speaker = kCoded[mode][ch];
        if (speaker == kNone) {
            continue;
        }
        if (acmod == AudioCodingMode::DualMono) {
            // Two programmes into one speaker are summed at -6 dB each.
            mix(one_speaker ? kCentre : speaker, ch, one_speaker ? 0.5f : 1.0f);
            continue;
        }
        if (one_speaker) {
            // §7.8.1's 1/0 output.
            if (speaker == kCentre) {
                mix(kCentre, ch, fronts == 1 ? 1.0f : levels.center * kPlus3dB);
            } else if (speaker == kLeft || speaker == kRight) {
                mix(kCentre, ch, kMinus3dB);
            } else {
                mix(kCentre, ch, levels.surround * kMinus3dB);
            }
            continue;
        }
        if (slot[speaker] >= 0) {
            mix(speaker, ch, 1.0f);
            continue;
        }
        switch (speaker) {
        case kCentre: {
            const float gain = fronts == 1 ? kMinus3dB : levels.center;
            mix(kLeft, ch, gain);
            mix(kRight, ch, gain);
            break;
        }
        case kSurround:
            if (slot[kLeftSurround] >= 0) {
                mix(kLeftSurround, ch, kMinus3dB);
                mix(kRightSurround, ch, kMinus3dB);
            } else {
                mix(kLeft, ch, levels.surround * kMinus3dB);
                mix(kRight, ch, levels.surround * kMinus3dB);
            }
            break;
        case kLeftSurround:
            mix(kLeft, ch, levels.surround);
            break;
        case kRightSurround:
            mix(kRight, ch, levels.surround);
            break;
        default:
            break;
        }
    }
    if (lfeon) {
        mix(kLfe, kMixLfeInput, 1.0f);
    }

    float peak = 0.0f;
    for (unsigned out = 0; out < m.outputs; ++out) {
        float sum = 0.0f;
        for (unsigned in = 0; in < kMixInputs; ++in) {
            sum += std::abs(m.gain[out][in]);
        }
        peak = std::max(peak, sum);
    }
    if (peak > 1.0f) {
        for (unsigned out = 0; out < m.outputs; ++out) {
            for (unsigned in = 0; in < kMixInputs; ++in) {
                m.gain[out][in] /= peak;
            }
        }
    }
    return m;
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
