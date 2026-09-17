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

/// Samples produced per AC-3 syncframe: six audio blocks of 256, A/52 §5.1.
constexpr unsigned kSamplesPerFrame = 1536;
constexpr unsigned kBlocksPerFrame = 6;
constexpr unsigned kSamplesPerBlock = 256;

/// The most bytes parseAC3FrameHeader() can read. AC-3's bsi grows with its
/// optional fields, and in 1+1 mode with every one present the fields read
/// span 153 bits (A/52 Table 5.2). E-AC-3's header parse stops far sooner.
constexpr size_t kMaxHeaderParseBytes = 20;

/// Which bit stream a syncframe holds, decided by bsid: A/52 §E2.3.1.6 has a
/// decoder play 0..8 as AC-3 and 11..16 as E-AC-3, and mute 9, 10 and
/// anything above 16.
///
/// The two share a sync word and nothing else past it: AC-3 follows the sync
/// word with crc1 and frmsizecod, E-AC-3 with strmtyp and frmsiz. bsid sits at
/// the same bit offset in both -- 40 -- precisely so a decoder can read it
/// first and branch (§E2.1), and that is the only reason a stream can be
/// identified at all before it is parsed.
enum class Flavour {
    AC3,          ///< bsid 0..8, Annex D's alternate syntax (bsid 6) among them
    /// bsid 9..10. A/52 defines no syntax for them, and §5.4.2.1 and
    /// §E2.3.1.6 have a decoder mute them. Their headers are parsed with
    /// AC-3's layout, but only to name the stream.
    AC3Alternate,
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

/// How many channels a coded arrangement plays through, in SDL's default
/// layouts (SDL_audio.h): 1 mono, 2 stereo, 3 2.1, 4 quad, 5 4.1, 6 5.1,
/// 7 6.1, 8 7.1.
///
/// The smallest of those with a speaker for every coded channel. A count of
/// channels is not enough on its own: SDL has no centre speaker below six
/// channels, so 3/0 and 3/2 need all six even without an LFE -- a 5-channel
/// 3/2 would put its centre in the subwoofer -- and 1/0 with an LFE needs
/// them too. A single surround plays through a surround pair at -3 dB each,
/// as §7.8.1 lays out, rather than calling for 6.1.
inline unsigned ac3OutputChannels(AudioCodingMode acmod, bool lfeon)
{
    switch (acmod) {
    case AudioCodingMode::DualMono:
    case AudioCodingMode::Stereo:
        return lfeon ? 3 : 2;
    case AudioCodingMode::Mono:
        return lfeon ? 6 : 1;
    case AudioCodingMode::TwoOne:
    case AudioCodingMode::TwoTwo:
        return lfeon ? 5 : 4;
    case AudioCodingMode::ThreeZero:
    case AudioCodingMode::ThreeOne:
    case AudioCodingMode::ThreeTwo:
        return 6;
    }
    return 2;
}

/// syncinfo() and bsi(), A/52 Tables 5.1 and 5.2 (E-AC-3: E1.1 and E1.2).
///
/// Only the fields a caller outside the decoder needs are kept: enough to
/// frame the stream, describe it, and decide whether it can be played.
struct AC3FrameHeader {
    // --- syncinfo ---
    uint16_t crc1 = 0;
    uint8_t fscod = 0;        ///< sample rate code, Table 5.6 (E-AC-3: Table E2.2)
    uint8_t frmsizecod = 0;   ///< frame size code, Table 5.18

    // --- bsi ---
    uint8_t bsid = 0;         ///< bit stream identification, A/52 §E2.3.1.6
    Flavour flavour = Flavour::Unknown;
    uint8_t bsmod = 0;        ///< bit stream mode (service type), Table 5.7
    AudioCodingMode acmod = AudioCodingMode::Stereo;
    bool lfeon = false;
    /// AC-3's downmix levels, Tables 5.9 and 5.10, as written: 0 when the
    /// mode has no centre or no surround to mix. E-AC-3 carries its own in
    /// EAC3AudioFrame.
    uint8_t cmixlev = 0;
    uint8_t surmixlev = 0;
    /// Dialogue normalisation, as written: 1..31 means -1..-31 dBFS, and 0 is
    /// reserved. Kept raw rather than negated so a caller can tell 0 apart.
    uint8_t dialnorm = 0;

    /// Derived, since every caller wants these rather than the codes.
    uint32_t sample_rate = 0;
    uint32_t bitrate = 0;         ///< nominal, bits per second
    uint16_t frame_size = 0;      ///< bytes in this syncframe, including header
    uint8_t channels = 0;         ///< full-bandwidth channels, excluding LFE
    /// Audio blocks in the frame. Always six for AC-3; E-AC-3 codes 1, 2, 3
    /// or 6 in numblkscod (§E2.3.1.5, Table E2.4), so samples per frame vary
    /// with it.
    uint8_t blocks = kBlocksPerFrame;
    /// E-AC-3 stream type, §E2.3.1.1: 0 independent, 1 dependent (extends
    /// the independent frame before it), 2 independent converted from AC-3,
    /// 3 reserved. Always 0 for AC-3.
    uint8_t strmtyp = 0;
    /// E-AC-3 substream identification, §E2.3.1.2. For an independent
    /// substream, the program it carries: 0 is the first, and always
    /// present. For a dependent substream, its place 0..7 among the
    /// dependents that follow its independent substream. The program it
    /// extends is the one that independent substream carries, which this
    /// field does not name. Always 0 for AC-3.
    uint8_t substreamid = 0;

    /// True for every frame except program 1's independent frames: a
    /// dependent substream, an independent substream of another program, or
    /// a frame of the reserved type 3, whose syntax A/52 does not define.
    /// Such frames add no time to the program being played (§E3.8.1).
    bool isAuxiliarySubstream() const
    {
        const bool independent = strmtyp == 0 || strmtyp == 2;
        return flavour == Flavour::EAC3 && (!independent || substreamid != 0);
    }

    /// Channels the decoder outputs for this arrangement, in the layout
    /// ac3OutputChannels() describes -- which is not always the coded count.
    uint8_t outputChannels() const { return static_cast<uint8_t>(ac3OutputChannels(acmod, lfeon)); }

    bool isAC3() const { return flavour == Flavour::AC3; }
    bool isEAC3() const { return flavour == Flavour::EAC3; }
    /// True when this decoder can actually decode the frame, as opposed to
    /// merely having recognised it.
    ///
    /// E-AC-3 at a reduced sample rate (fscod '11': 24, 22.05 or 16 kHz) is
    /// recognised but not decoded. Bit allocation needs the hearing threshold
    /// of Table 7.15, which has columns for 48, 44.1 and 32 kHz only, and A/52
    /// does not say what the reduced rates use. FFmpeg does not decode these
    /// streams either, so there is nothing to check a guess against, and a
    /// wrong guess misreads every mantissa after the first difference.
    bool isDecodable() const
    {
        return flavour == Flavour::AC3 || (flavour == Flavour::EAC3 && fscod != 3);
    }

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
/// is out of range: A/52 Table 5.18, which parseAC3FrameHeader() looks up.
uint16_t ac3FrameSize(uint8_t fscod, uint8_t frmsizecod);

/// True when the syncframe of @p frame_size bytes at @p frame passes its CRC
/// check. crc2 is written so that the check over the whole frame, sync word
/// excluded, comes out zero (A/52 §7.10.1), for AC-3 and E-AC-3 alike.
bool ac3FrameCrcValid(const uint8_t* frame, size_t frame_size);

/// Sample rate in Hz for @p fscod, or 0 for the reserved code.
uint32_t ac3SampleRate(uint8_t fscod);

/// Full-bandwidth channel count for @p acmod, A/52 Table 5.8. Excludes LFE.
uint8_t ac3ChannelCount(AudioCodingMode acmod);

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3

#endif // AC3FRAMEHEADER_H
