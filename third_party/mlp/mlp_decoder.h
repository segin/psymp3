/*
 * mlp_decoder.h - Meridian Lossless Packing / Dolby TrueHD decoder
 * This file is part of PsyMP3.
 *
 * Copyright © 2025 Rainbaby (truehdd)
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * Unlike the rest of PsyMP3, which is ISC, this file is Apache-2.0: it is a
 * derived work of an Apache-2.0 reference implementation and carries that
 * licence forward.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This is a C++ implementation of the MLP/TrueHD bitstream derived from truehdd
 * <https://github.com/truehdd/truehdd> (truehd 0.7.1), Copyright © 2025
 * Rainbaby, used under the Apache License, Version 2.0. The bitstream syntax,
 * the Huffman tables, the dither table and the recorrelation and matrixing
 * arithmetic all follow that work; this is a translation of it into C++ rather
 * than an independent reimplementation.
 *
 * Changes from the original, as Apache-2.0 section 4(b) requires them to be
 * stated: translated from Rust to C++; the parser and decoder state are merged
 * and driven per access unit; the timing, FIFO and lossless-check validation
 * are omitted; and the recorrelator's 24-bit range assertions are not
 * enforced.
 */

#ifndef PSYMP3_THIRD_PARTY_MLP_DECODER_H
#define PSYMP3_THIRD_PARTY_MLP_DECODER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace mlp {

/// Sync word of a Dolby TrueHD (FBA) major sync.
static constexpr uint32_t MAJOR_SYNC_FBA = 0xF8726FBA;
/// Sync word of a Meridian MLP (FBB) major sync, as carried on DVD-Audio.
static constexpr uint32_t MAJOR_SYNC_FBB = 0xF8726FBB;

/// Thrown for any bitstream the decoder cannot make sense of. A caller decoding
/// a stream should treat one access unit failing as a lost frame, not as the end
/// of the stream: the next major sync re-establishes everything.
class Error : public std::runtime_error {
public:
    explicit Error(const std::string& what) : std::runtime_error(what) {}
};

struct DecoderState;

/// Decodes MLP/TrueHD access units to PCM.
///
/// The stream is a chain of access units, each prefixed by a four-byte header
/// giving its length; a major sync appears at intervals and carries the sample
/// rate, the substream layout and the presentation map. Nothing can be decoded
/// before the first major sync has been seen.
class Decoder {
public:
    Decoder();
    ~Decoder();

    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    /// Decodes one access unit, appending interleaved samples to `out`.
    ///
    /// Samples are signed and left-justified in the substream's own width, which
    /// is 24 bits for every rate and layout in use; a caller feeding a wider
    /// pipeline scales them up. `size` must be the access unit's own length, as
    /// taken from its header.
    ///
    /// Throws Error if the access unit cannot be decoded.
    void decodeAccessUnit(const uint8_t* data, size_t size, std::vector<int32_t>& out);

    /// Drops all decoder state, as after a seek. The configuration a major sync
    /// established is dropped with it, so the next access unit decoded must
    /// carry one.
    void reset();

    /// Sampling frequency in Hz, or 0 before the first major sync.
    uint32_t sampleRate() const;

    /// Channels the selected presentation decodes to, or 0 before the first
    /// access unit has been decoded.
    size_t channels() const;

    /// Samples per access unit per channel: 40, 80 or 160 by rate.
    size_t samplesPerAccessUnit() const;

    /// Bits per sample the substream carries. Always 24 in practice.
    unsigned bitsPerSample() const;

    /// True once a major sync has configured the decoder.
    bool configured() const;

    /// True for a Dolby TrueHD (FBA) stream, false for Meridian MLP (FBB).
    bool isTrueHD() const;

    /// Length in bytes of the access unit starting at `data`, or 0 if `size` is
    /// too small to hold its header. Does not validate the access unit.
    static size_t accessUnitLength(const uint8_t* data, size_t size);

    /// True if an access unit header at `data` is followed by a major sync.
    static bool hasMajorSync(const uint8_t* data, size_t size);

private:
    std::unique_ptr<DecoderState> m_state;
};

} // namespace mlp

#endif // PSYMP3_THIRD_PARTY_MLP_DECODER_H
