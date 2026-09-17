/*
 * AC3FrameDecoder.cpp - AC-3 and E-AC-3 syncframes to PCM
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

namespace {

/// The LFE's line, after the full-bandwidth channels.
constexpr unsigned kLfeLine = kMixLfeInput;

/// How far E-AC-3 output is held behind the last decoded sample. A
/// correction not yet applied has its transient at or past the next sample to
/// decode, and writes no further back than this from it, so anything older
/// is final.
constexpr uint64_t kHoldBack = static_cast<uint64_t>(EAC3TransientCorrection::kMaxWriteReach);

/// Released samples kept on each line. A pending correction's synthesis
/// buffer starts at most 2*TC1 + 2*511 = 1534 samples before its transient,
/// which is at most 512 samples before the oldest sample still held.
constexpr uint64_t kHistory = 1024;

} // namespace

void AC3FrameDecoder::reset()
{
    m_state = AC3FrameState();
    for (auto& transform : m_transforms) {
        transform = AC3TransformState();
    }
    m_pending = Frame();
    m_previous_ecpl = false;
    std::fill(std::begin(m_previous_coupling), std::end(m_previous_coupling), 0.0f);
    for (auto& line : m_line) {
        line.clear();
    }
    m_line_start = 0;
    m_decoded = 0;
    m_released = 0;
    m_next_frame = 0;
    m_corrections.clear();
    m_have_layout = false;
}

void AC3FrameDecoder::finishBlock(Frame& frame, unsigned index, const AC3Block* next)
{
    AC3Block& block = frame.block[index];

    // --- enhanced coupling, §E3.5.5 ---
    // The coupling channel is rebuilt free of time-domain aliasing from this
    // block and both neighbours; a neighbour without enhanced coupling
    // contributes silence.
    float current[kTransformSize] = {};
    if (block.ecplinu) {
        static const float kSilence[kTransformSize] = {};
        ac3WindowedImdct(block.ecpl_coupling, false, current);
        float following[kTransformSize] = {};
        if (next && next->ecplinu) {
            ac3WindowedImdct(next->ecpl_coupling, false, following);
        }
        float zr[kBlockSamples];
        float zi[kBlockSamples];
        eac3EcplAnalyse(m_previous_ecpl ? m_previous_coupling : kSilence, current, following, zr, zi);
        for (unsigned ch = 0; ch < frame.fbw; ++ch) {
            if (block.chincpl[ch]) {
                eac3EcplRegenerate(zr, zi, block.ecpl_bands, block.ecpl[ch], ch,
                                   block.ecpl_angle_interpolation, m_ecpl_random,
                                   block.coefficients[ch]);
                // Zero-bit bins keep their dither as drawn, at the coupling
                // channel's level. Standard coupling scales it by the
                // channel's coordinate (§7.4.3), but Annex E does not say
                // whether enhanced coupling's amplitudes apply to it, and
                // there is no encoder at hand that emits enhanced coupling
                // to compare a decode against.
                for (unsigned bin = 0; bin < kBlockSamples; ++bin) {
                    if (block.ecpl_dithered[ch][bin]) {
                        block.coefficients[ch][bin] = block.ecpl_dither[ch][bin];
                    }
                }
            }
        }
    }
    if (block.ecplinu) {
        std::copy(std::begin(current), std::end(current), std::begin(m_previous_coupling));
    }
    m_previous_ecpl = block.ecplinu;

    // Spectral extension held back by the parser because the channel's low
    // band included enhanced-coupled bins, which are only now finished.
    for (unsigned ch = 0; ch < frame.fbw; ++ch) {
        if (block.spx_deferred[ch]) {
            eac3SpxSynthesise(block.coefficients[ch], block.spx_bands, block.spx[ch],
                              m_state.spx_noise);
        }
    }

    // --- dynamic range control and the inverse transform ---
    // §6.1.9: the gain alters the coefficients, so it scales the block
    // before it is transformed (§7.7.1 gives its value). That is the same as
    // scaling the block's output, and keeps the overlap with neighbouring
    // blocks consistent. In 1+1 mode channel 2 has its own, dynrng2
    // (§7.7.1.2). Every line grows by a block, silent where the frame has no
    // such channel, so they stay aligned with each other.
    float samples[kBlockSamples];
    for (unsigned ch = 0; ch < kMaxFullBandwidthChannels; ++ch) {
        std::vector<float>& line = m_line[ch];
        if (ch >= frame.fbw) {
            line.resize(line.size() + kBlockSamples, 0.0f);
            continue;
        }
        const float gain = (ch == 1) ? block.dynamic_range2 : block.dynamic_range;
        if (gain != 1.0f) {
            for (unsigned bin = 0; bin < kBlockSamples; ++bin) {
                block.coefficients[ch][bin] *= gain;
            }
        }
        ac3InverseTransform(block.coefficients[ch], block.block_switch[ch],
                            m_transforms[ch], samples);
        line.insert(line.end(), samples, samples + kBlockSamples);
    }
    std::vector<float>& lfe = m_line[kLfeLine];
    if (frame.lfeon) {
        // The LFE is never block-switched: A/52 Table 5.3 sends blksw[ch]
        // only for the full-bandwidth channels (§5.4.3.1), and Table E1.4
        // does the same.
        if (block.dynamic_range != 1.0f) {
            for (unsigned bin = 0; bin < kBlockSamples; ++bin) {
                block.coefficients[kLfeSlot][bin] *= block.dynamic_range;
            }
        }
        ac3InverseTransform(block.coefficients[kLfeSlot], false, m_transforms[kLfeSlot], samples);
        lfe.insert(lfe.end(), samples, samples + kBlockSamples);
    } else {
        lfe.resize(lfe.size() + kBlockSamples, 0.0f);
    }
    m_decoded += kBlockSamples;
}

void AC3FrameDecoder::queueCorrections(const Frame& frame, uint64_t frame_start)
{
    if (!frame.params.transproce) {
        return;
    }
    for (unsigned ch = 0; ch < frame.fbw; ++ch) {
        if (frame.params.chintransproc[ch]) {
            Correction correction;
            correction.channel = ch;
            correction.span = eac3TransientCorrection(static_cast<int64_t>(frame_start),
                                                      frame.params.transprocloc[ch],
                                                      frame.params.transproclen[ch]);
            m_corrections.push_back(correction);
        }
    }
}

void AC3FrameDecoder::applyCorrections()
{
    // In the order they were sent, each once its transient has decoded. A
    // correction whose span has already been handed on, or whose buffer
    // reaches back before the history kept -- the first frames after a seek
    // -- is dropped: there is nothing left to correct, or nothing to copy.
    auto it = m_corrections.begin();
    while (it != m_corrections.end()) {
        const EAC3TransientCorrection& span = it->span;
        if (span.transloc > static_cast<int64_t>(m_decoded)) {
            ++it;
            continue;
        }
        if (span.start() >= static_cast<int64_t>(m_released) &&
            span.synthesisStart() >= static_cast<int64_t>(m_line_start)) {
            eac3ApplyTransientCorrection(m_line[it->channel].data(),
                                         static_cast<int64_t>(m_line_start), span);
        }
        it = m_corrections.erase(it);
    }
}

void AC3FrameDecoder::release(uint64_t end, std::vector<float>& pcm)
{
    if (end <= m_released || m_output_channels == 0) {
        return;
    }
    const size_t count = static_cast<size_t>(end - m_released);
    const size_t first = static_cast<size_t>(m_released - m_line_start);
    const size_t base = pcm.size();
    const unsigned outputs = m_output_channels;
    pcm.resize(base + count * outputs, 0.0f);
    for (unsigned out = 0; out < outputs; ++out) {
        for (unsigned in = 0; in < kLines; ++in) {
            const float gain = m_matrix.gain[out][in];
            if (gain == 0.0f) {
                continue;
            }
            const float* src = m_line[in].data() + first;
            float* dst = pcm.data() + base + out;
            for (size_t n = 0; n < count; ++n, dst += outputs) {
                *dst += gain * src[n];
            }
        }
    }
    m_released = end;

    // Trim in bulk rather than shifting the lines every block.
    const uint64_t keep_from = m_released > kHistory ? m_released - kHistory : 0;
    if (keep_from >= m_line_start + 4 * kHistory) {
        const auto drop = static_cast<std::ptrdiff_t>(keep_from - m_line_start);
        for (auto& line : m_line) {
            line.erase(line.begin(), line.begin() + drop);
        }
        m_line_start = keep_from;
    }
}

void AC3FrameDecoder::drain(std::vector<float>& pcm)
{
    if (m_pending.valid) {
        // Nothing follows, so the held frame's last block has no successor.
        finishBlock(m_pending, m_pending.blocks - 1, nullptr);
        m_pending = Frame();
    }
    applyCorrections();
    m_corrections.clear();
    release(m_decoded, pcm);
}

void AC3FrameDecoder::conceal(std::vector<float>& pcm)
{
    // Before the first good frame there is no layout to be silent in, and
    // nothing yet whose timing a gap could upset.
    if (!m_have_layout) {
        return;
    }

    // The damaged frame's own header cannot be trusted, so the silence takes
    // the shape of the frame before it. A/52 §7.10 names muting among the
    // responses to an error, and §E3.2 asks for "an appropriate error
    // concealment signal".
    Frame silent;
    silent.valid = true;
    silent.eac3 = m_layout_eac3;
    silent.blocks = m_pending.valid ? m_pending.blocks : kBlocksPerFrame;
    silent.fbw = ac3ChannelCount(static_cast<AudioCodingMode>(m_layout_acmod));
    silent.acmod = m_layout_acmod;
    silent.lfeon = m_layout_lfeon;
    silent.sample_rate = m_sample_rate;
    silent.levels = m_layout_levels;
    silent.block.reset(new AC3Block[silent.blocks]);

    // A correction still waiting was measured against audio that is now
    // silence.
    m_corrections.clear();
    if (m_pending.valid) {
        finishBlock(m_pending, m_pending.blocks - 1, &silent.block[0]);
        m_pending = Frame();
    }
    // Blocks of zero coefficients: the transforms' overlap fades out over the
    // first, the rest are silent, and the next good frame fades in from them.
    for (unsigned b = 0; b < silent.blocks; ++b) {
        finishBlock(silent, b, nullptr);
    }
    m_next_frame += static_cast<uint64_t>(silent.blocks) * kBlockSamples;
    release(m_decoded, pcm);
}

void AC3FrameDecoder::setOutputChannels(unsigned channels)
{
    m_requested_channels = channels <= kMaxOutputChannels ? channels : 0;
    m_output_channels = m_requested_channels;
    // The next frame rebuilds the matrix for the new layout.
    m_have_layout = false;
}

void AC3FrameDecoder::setLayout(const Frame& frame)
{
    const auto acmod = static_cast<AudioCodingMode>(frame.acmod);
    m_have_layout = true;
    m_layout_eac3 = frame.eac3;
    m_layout_acmod = frame.acmod;
    m_layout_lfeon = frame.lfeon;
    m_layout_levels = frame.levels;
    m_sample_rate = frame.sample_rate;
    // With no layout asked for, the first frame's own fixes it for good.
    if (m_output_channels == 0) {
        m_output_channels = ac3OutputChannels(acmod, frame.lfeon);
    }
    m_matrix = ac3OutputMatrix(acmod, frame.lfeon, m_output_channels, frame.levels);
}

bool AC3FrameDecoder::decode(const uint8_t* data, size_t size, std::vector<float>& pcm)
{
    pcm.clear();

    AC3FrameHeader header;
    if (!parseAC3FrameHeader(data, size, header) || !header.isDecodable()) {
        return false;
    }
    if (header.frame_size == 0 || header.frame_size > size) {
        return false;
    }
    if (header.isAuxiliarySubstream()) {
        // §E3.8.1: a reference decoder plays independent substream 0 and
        // skips everything else. A dependent substream carries channels
        // beyond that program's 5.1 -- rendering them is optional and not
        // done here -- and an independent substream with another id is a
        // different program. Either way this program's state is untouched.
        // The skip comes before the parse, because a reserved-type frame has
        // no syntax to parse it with.
        return true;
    }

    // §E3.2: an E-AC-3 decoder must check the CRC before decoding any block,
    // and replace a frame that fails with concealment. §7.10 leaves checking
    // to the implementation for AC-3, where a damaged frame decodes to noise
    // just the same, so AC-3 is checked too. A frame that fails to parse is
    // concealed in the same way rather than dropped, which would shorten the
    // timeline and splice the frames either side together.
    if (!ac3FrameCrcValid(data, header.frame_size)) {
        Debug::log("ac3", "syncframe failed its CRC check; muted");
        conceal(pcm);
        return true;
    }

    AC3BitReader reader(data, header.frame_size);
    AC3FrameHeader consumed;
    Frame frame;
    frame.eac3 = header.isEAC3();
    if (frame.eac3) {
        const char* why = nullptr;
        if (!eac3ParseFrame(reader, consumed, frame.params, &why)) {
            Debug::log("ac3", "E-AC-3 frame header failed: ", why ? why : "unknown", "; muted");
            conceal(pcm);
            return true;
        }
    } else if (!ac3ParseFrameHeader(reader, consumed)) {
        Debug::log("ac3", "AC-3 frame header failed; muted");
        conceal(pcm);
        return true;
    }

    // Each syncframe starts its own bit allocation and exponent history --
    // that is what makes a frame the unit a decoder can resynchronise on --
    // but the transform's delay line survives, because the overlap that
    // reconstructs the signal spans the frame boundary. The noise generators
    // survive too: restarting them every frame would repeat the same noise
    // 31 times a second.
    const AC3Dither dither = m_state.dither;
    const EAC3SpxNoise spx_noise = m_state.spx_noise;
    m_state = AC3FrameState();
    m_state.dither = dither;
    m_state.spx_noise = spx_noise;

    frame.blocks = header.blocks;
    frame.fbw = header.channels;
    frame.acmod = static_cast<unsigned>(header.acmod);
    frame.lfeon = header.lfeon;
    frame.sample_rate = header.sample_rate;
    frame.block.reset(new AC3Block[frame.blocks]);

    for (unsigned b = 0; b < frame.blocks; ++b) {
        const char* reason = nullptr;
        if (!ac3ParseAudioBlock(reader, header, m_state, frame.block[b], &reason,
                                frame.eac3 ? &frame.params : nullptr)) {
            Debug::log("ac3", "frame failed at block ", b, ": ", reason ? reason : "unknown", "; muted");
            conceal(pcm);
            return true;
        }
    }
    frame.valid = true;
    frame.levels = frame.eac3
        ? eac3MixLevels(frame.params.lorocmixlev, frame.params.lorosurmixlev,
                        frame.params.lfemixlevcode, frame.params.lfemixlevcod)
        : ac3MixLevels(header.cmixlev, header.surmixlev);

    // A new channel arrangement -- mode, LFE, rate, or AC-3 against E-AC-3 --
    // ends what came before. Everything held is released in the old mix, and
    // the transforms' overlap and the enhanced coupling history are dropped:
    // they belong to channels that now mean something else. The output
    // layout itself stays put. New downmix levels alone only change the mix.
    if (!m_have_layout || frame.acmod != m_layout_acmod || frame.lfeon != m_layout_lfeon ||
        frame.eac3 != m_layout_eac3 || frame.sample_rate != m_sample_rate) {
        if (m_have_layout) {
            drain(pcm);
            for (auto& transform : m_transforms) {
                transform = AC3TransformState();
            }
            m_previous_ecpl = false;
        }
        setLayout(frame);
    } else if (frame.levels != m_layout_levels) {
        m_layout_levels = frame.levels;
        m_matrix = ac3OutputMatrix(static_cast<AudioCodingMode>(frame.acmod), frame.lfeon,
                                   m_output_channels, frame.levels);
    }

    const uint64_t frame_start = m_next_frame;
    m_next_frame += static_cast<uint64_t>(frame.blocks) * kBlockSamples;

    if (!frame.eac3) {
        for (unsigned b = 0; b < frame.blocks; ++b) {
            finishBlock(frame, b, nullptr);
        }
        release(m_decoded, pcm);
        return true;
    }

    // E-AC-3: the frame held back last time can now finish its last block,
    // with this frame's first as its next neighbour. This frame's blocks all
    // finish but its last, then its corrections join the queue, and whatever
    // no correction can still reach is released.
    if (m_pending.valid) {
        finishBlock(m_pending, m_pending.blocks - 1, &frame.block[0]);
    }
    for (unsigned b = 0; b + 1 < frame.blocks; ++b) {
        finishBlock(frame, b, &frame.block[b + 1]);
    }
    queueCorrections(frame, frame_start);
    applyCorrections();
    if (m_decoded > kHoldBack) {
        release(m_decoded - kHoldBack, pcm);
    }
    m_pending = std::move(frame);
    return true;
}

void AC3FrameDecoder::flush(std::vector<float>& pcm)
{
    pcm.clear();
    drain(pcm);
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
