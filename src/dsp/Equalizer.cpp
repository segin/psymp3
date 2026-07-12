/*
 * Equalizer.cpp - Real-time 7-band biquad equalizer (DSP) implementation.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace DSP {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Log-spaced graphic-EQ centre frequencies (Hz) and their short labels.
constexpr float       kBandFreq[Equalizer::kNumBands]  = {60, 150, 400, 1000, 2400, 6000, 15000};
constexpr const char* kBandLabel[Equalizer::kNumBands] = {"60", "150", "400", "1k", "2.4k", "6k", "15k"};

// Filter Q. Broad enough that adjacent bands overlap into a smooth composite
// response rather than isolated bumps.
constexpr double kQ = 1.0;

inline float clampDb(float db)
{
    if (db < Equalizer::kMinGainDb) return Equalizer::kMinGainDb;
    if (db > Equalizer::kMaxGainDb) return Equalizer::kMaxGainDb;
    return db;
}
} // namespace

Equalizer::Equalizer()
{
    for (int b = 0; b < kNumBands; ++b)
        m_gain_db[b].store(0.0f, std::memory_order_relaxed);
}

float       Equalizer::bandFrequency(int band) { return (band >= 0 && band < kNumBands) ? kBandFreq[band]  : 0.0f; }
const char* Equalizer::bandLabel(int band)     { return (band >= 0 && band < kNumBands) ? kBandLabel[band] : ""; }

void Equalizer::configure(int sample_rate, int /*channels*/)
{
    if (sample_rate > 0)
        m_sample_rate.store(sample_rate, std::memory_order_relaxed);
    m_dirty.fetch_add(1, std::memory_order_release);
    requestReset();
}

void Equalizer::setBandGain(int band, float db)
{
    if (band < 0 || band >= kNumBands) return;
    m_gain_db[band].store(clampDb(db), std::memory_order_relaxed);
    m_dirty.fetch_add(1, std::memory_order_release);
}

float Equalizer::getBandGain(int band) const
{
    if (band < 0 || band >= kNumBands) return 0.0f;
    return m_gain_db[band].load(std::memory_order_relaxed);
}

void Equalizer::setEnabled(bool on)
{
    // process() early-returns while disabled, freezing z1/z2 with whatever was
    // playing at the disable instant; resuming the cascade with that stale
    // state would emit an audible click. Reset on the off->on edge.
    const bool was = m_enabled.exchange(on, std::memory_order_relaxed);
    if (on && !was) {
        requestReset();
    }
}
bool Equalizer::isEnabled() const   { return m_enabled.load(std::memory_order_relaxed); }

void Equalizer::requestReset() { m_reset_pending.store(true, std::memory_order_release); }

void Equalizer::recompute()
{
    int sr = m_sample_rate.load(std::memory_order_relaxed);
    if (sr <= 0) sr = 44100;

    for (int b = 0; b < kNumBands; ++b) {
        Biquad& q = m_coeff[b];
        double f0 = kBandFreq[b];
        double gain_db = m_gain_db[b].load(std::memory_order_relaxed);

        // A band above (or near) Nyquist, or one sitting at unity gain, becomes
        // an identity filter so it neither rings nor wastes cycles. Its history
        // must be zeroed too: TDF-II would otherwise flush the stale z1/z2 into
        // the next two output samples (an audible click after a large boost).
        // recompute() runs on the audio thread, which owns the history — safe.
        if (gain_db == 0.0 || f0 >= 0.45 * sr) {
            q = Biquad{};
            for (int c = 0; c < kMaxChannels; ++c) {
                m_z1[c][b] = 0.0;
                m_z2[c][b] = 0.0;
            }
            continue;
        }

        double A     = std::pow(10.0, gain_db / 40.0);
        double w0    = 2.0 * kPi * f0 / sr;
        double cosw0 = std::cos(w0);
        double alpha = std::sin(w0) / (2.0 * kQ);

        double b0 = 1.0 + alpha * A;
        double b1 = -2.0 * cosw0;
        double b2 = 1.0 - alpha * A;
        double a0 = 1.0 + alpha / A;
        double a1 = -2.0 * cosw0;
        double a2 = 1.0 - alpha / A;

        q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0;
        q.a1 = a1 / a0; q.a2 = a2 / a0;
    }
}

void Equalizer::process(int16_t* samples, size_t frame_count, int channels)
{
    if (!m_enabled.load(std::memory_order_relaxed)) return;
    if (!samples || channels <= 0 || channels > kMaxChannels) return;

    if (m_reset_pending.exchange(false, std::memory_order_acquire)) {
        for (int c = 0; c < kMaxChannels; ++c)
            for (int b = 0; b < kNumBands; ++b) { m_z1[c][b] = 0.0; m_z2[c][b] = 0.0; }
    }

    uint32_t d = m_dirty.load(std::memory_order_acquire);
    if (d != m_last_dirty) { recompute(); m_last_dirty = d; }

    for (size_t f = 0; f < frame_count; ++f) {
        for (int c = 0; c < channels; ++c) {
            double s = static_cast<double>(samples[f * channels + c]);
            // Cascade the bands (transposed direct form II per biquad).
            for (int b = 0; b < kNumBands; ++b) {
                const Biquad& q = m_coeff[b];
                double y = q.b0 * s + m_z1[c][b];
                m_z1[c][b] = q.b1 * s - q.a1 * y + m_z2[c][b];
                m_z2[c][b] = q.b2 * s - q.a2 * y;
                s = y;
            }
            if (s >  32767.0) s =  32767.0;
            if (s < -32768.0) s = -32768.0;
            samples[f * channels + c] = static_cast<int16_t>(std::lround(s));
        }
    }
}

} // namespace DSP
} // namespace PsyMP3
