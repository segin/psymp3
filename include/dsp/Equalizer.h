/*
 * Equalizer.h - Real-time 7-band biquad equalizer (DSP).
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_DSP_EQUALIZER_H
#define PSYMP3_DSP_EQUALIZER_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace DSP {

// A cascade of RBJ "peaking EQ" biquad filters, one per band, applied to
// interleaved signed-16-bit PCM in place. Designed to run inside the SDL audio
// callback: process() never allocates or locks.
//
// Threading model (mirrors Audio::m_volume):
//   - The UI thread pushes parameters via setBandGain()/setEnabled()/configure()
//     (lock-free atomics). Each such call bumps a dirty counter.
//   - The audio thread owns the filter coefficients and per-channel history and
//     lazily recomputes coefficients inside process() when the dirty counter
//     changes. Nothing but the audio thread touches that working state.
//   - requestReset() (called on seek / track change from another thread) sets a
//     flag that the audio thread honours at the top of the next process().
class Equalizer {
public:
    static constexpr int   kNumBands  = 7;
    static constexpr float kMinGainDb = -12.0f;
    static constexpr float kMaxGainDb = 12.0f;

    Equalizer();

    // Recompute for a new device format. Call before the device is active (from
    // Audio::setup); safe to call again on a format change.
    void configure(int sample_rate, int channels);

    // UI thread. dB is clamped to [kMinGainDb, kMaxGainDb].
    void  setBandGain(int band, float db);
    float getBandGain(int band) const;
    void  setEnabled(bool on);
    bool  isEnabled() const;

    // Audio thread: filter `frame_count` interleaved frames of `channels`
    // samples in place. RT-safe. A no-op when disabled.
    void process(int16_t* samples, size_t frame_count, int channels);

    // Any thread: zero the filter history before the next process() (seek/track
    // change), so filter transients do not bleed across discontinuities.
    void requestReset();

    // Fixed band layout (log-spaced graphic-EQ centres).
    static float       bandFrequency(int band);  // Hz
    static const char* bandLabel(int band);       // e.g. "60", "1k", "15k"

private:
    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    void recompute();  // audio thread only

    static constexpr int kMaxChannels = 8;

    // UI -> audio parameters (lock-free).
    std::atomic<float>    m_gain_db[kNumBands];
    std::atomic<bool>     m_enabled{false};
    std::atomic<uint32_t> m_dirty{1};
    std::atomic<bool>     m_reset_pending{false};
    std::atomic<int>      m_sample_rate{44100};

    // Audio-thread-only working state.
    Biquad   m_coeff[kNumBands];
    uint32_t m_last_dirty = 0;
    double   m_z1[kMaxChannels][kNumBands] = {};
    double   m_z2[kMaxChannels][kNumBands] = {};
};

} // namespace DSP
} // namespace PsyMP3

#endif // PSYMP3_DSP_EQUALIZER_H
