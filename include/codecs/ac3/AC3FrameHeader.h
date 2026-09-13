/*
 * AC3FrameHeader.h - AC-3 syncframe and bit stream information (A/52 §5.3).
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef AC3FRAMEHEADER_H
#define AC3FRAMEHEADER_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

/// The AC-3 sync word, A/52 §5.4.1.1. Every syncframe opens with it.
constexpr uint16_t kSyncWord = 0x0B77;

/// Samples produced per syncframe: six audio blocks of 256, A/52 §6.1.
constexpr unsigned kSamplesPerFrame = 1536;
constexpr unsigned kBlocksPerFrame = 6;
constexpr unsigned kSamplesPerBlock = 256;

/// Which bit stream a syncframe holds, decided by bsid (A/52 §E2.3.1.6).
///
/// The two share a sync word and nothing else past it: AC-3 follows the sync
/// word with crc1 and frmsizecod, E-AC-3 with strmtyp and frmsiz. bsid sits at
/// the same bit offset in both -- 40 -- precisely so a decoder can read it
/// first and branch, and that is the only reason a stream can be identified at
/// all before it is parsed.
enum class Flavour {
    AC3,          ///< bsid 0..8, what this decoder implements
    AC3Alternate, ///< bsid 9..10, the Annex D alternate syntax
    EAC3,         ///< bsid 11..16, Annex E
    Unknown,
};

/// Audio coding mode, A/52 Table 5.8. The value is the channel arrangement,
/// not a count: 1+1 is two independent mono programmes, while 1/0 is one.
enum class AudioCodingMode {
    DualMono   = 0, ///< 1+1: Ch1, Ch2
    Mono       = 1, ///< 1/0: C
    Stereo     = 2, ///< 2/0: L, R
    ThreeZero  = 3, ///< 3/0: L, C, R
    TwoOne     = 4, ///< 2/1: L, R, S
    ThreeOne   = 5, ///< 3/1: L, C, R, S
    TwoTwo     = 6, ///< 2/2: L, R, SL, SR
    ThreeTwo   = 7, ///< 3/2: L, C, R, SL, SR
};

/// syncinfo() and bsi(), A/52 Tables 5.1 and 5.2.
///
/// Only the fields a caller outside the decoder needs are kept: enough to
/// frame the stream, describe it, and decide whether it can be played.
struct AC3FrameHeader {
    // --- syncinfo ---
    uint16_t crc1 = 0;
    uint8_t fscod = 0;        ///< sample rate code, Table 5.6
    uint8_t frmsizecod = 0;   ///< frame size code, Table 5.18

    // --- bsi ---
    uint8_t bsid = 0;         ///< bit stream identification, A/52 §E2.3.1.6
    Flavour flavour = Flavour::Unknown;
    uint8_t bsmod = 0;        ///< bit stream mode (service type), Table 5.7
    AudioCodingMode acmod = AudioCodingMode::Stereo;
    bool lfeon = false;
    /// Dialogue normalisation, as written: 1..31 means -1..-31 dBFS, and 0 is
    /// reserved. Kept raw rather than negated so a caller can tell 0 apart.
    uint8_t dialnorm = 0;

    /// Derived, since every caller wants these rather than the codes.
    uint32_t sample_rate = 0;
    uint32_t bitrate = 0;         ///< nominal, bits per second
    uint16_t frame_size = 0;      ///< bytes in this syncframe, including header
    uint8_t channels = 0;         ///< full-bandwidth channels, excluding LFE
    /// Audio blocks in the frame. Always six for AC-3; E-AC-3 codes 1, 2, 3
    /// or 6 in numblkscod (Table E1.3), so samples per frame vary with it.
    uint8_t blocks = kBlocksPerFrame;
    /// E-AC-3 stream type, §E2.3.1.1: 0 independent, 1 dependent (extends
    /// the independent frame before it), 2 independent converted from AC-3.
    /// Always 0 for AC-3.
    uint8_t strmtyp = 0;
    /// E-AC-3 substream identification, §E2.3.1.2: which program an
    /// independent substream carries, or which program a dependent one
    /// extends. Always 0 for AC-3.
    uint8_t substreamid = 0;

    /// True for every frame except program 1's independent frames: a
    /// dependent substream, or an independent substream of another program.
    /// Such frames add no time to the program being played (§E3.8.1).
    bool isAuxiliarySubstream() const
    {
        return flavour == Flavour::EAC3 && (strmtyp == 1 || substreamid != 0);
    }

    /// Channels a decoder would output, LFE included.
    uint8_t outputChannels() const { return static_cast<uint8_t>(channels + (lfeon ? 1 : 0)); }

    bool isAC3() const { return flavour == Flavour::AC3; }
    bool isEAC3() const { return flavour == Flavour::EAC3; }
    /// True when this decoder can actually decode the frame, as opposed to
    /// merely having recognised it.
    bool isDecodable() const { return flavour == Flavour::AC3 || flavour == Flavour::EAC3; }

    /// What to call this stream in Media Information. Names the flavour even
    /// when it cannot be decoded, since "E-AC-3" tells a listener why a file
    /// did not play and "AC-3" would not.
    const char* displayName() const;
};

/// Parses a syncframe header from @p data.
///
/// Recognising a stream and being able to decode it are separate questions: an
/// E-AC-3 frame parses here and comes back with its flavour, rate and channel
/// count filled in, so it can be named rather than merely refused. Ask
/// isDecodable() before handing anything to the decoder.
///
/// @return true when @p data begins with a header this understands well enough
///         to describe. False for a bad sync word, a reserved sample rate, or
///         a frame size that cannot be resolved.
bool parseAC3FrameHeader(const uint8_t* data, size_t size, AC3FrameHeader& header);

/// The same, reading from a caller's bit reader and leaving it positioned at
/// the first audio block.
///
/// The buffer form above cannot be used to start decoding, because bsi has no
/// fixed length -- half its fields are optional -- so the only way to know
/// where the audio begins is to have consumed it with the same reader that
/// goes on to read the blocks.
bool ac3ParseFrameHeader(AC3BitReader& reader, AC3FrameHeader& header);

/// Frame length in bytes for a sample rate and frame size code, or 0 if either
/// is out of range. Exposed because a demuxer wants to walk frames without
/// decoding their bsi.
uint16_t ac3FrameSize(uint8_t fscod, uint8_t frmsizecod);

/// Sample rate in Hz for @p fscod, or 0 for the reserved code.
uint32_t ac3SampleRate(uint8_t fscod);

/// Full-bandwidth channel count for @p acmod, A/52 Table 5.8. Excludes LFE.
uint8_t ac3ChannelCount(AudioCodingMode acmod);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // AC3FRAMEHEADER_H
