/*
 * AC3FrameHeader.cpp - AC-3 syncframe and bit stream information (A/52 §5.3).
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * Written from ATSC A/52:2012, "Digital Audio Compression Standard". Section
 * and table numbers in the comments refer to that document.
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

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Codec {
namespace AC3 {

namespace {

/// A/52 Table 5.6. Index is fscod; '11' is reserved.
constexpr uint32_t kSampleRates[4] = {48000, 44100, 32000, 0};

/// A/52 Table 5.18, words per syncframe, one row per frmsizecod and one column
/// per fscod in the order 48 kHz, 44.1 kHz, 32 kHz -- fscod order, not the
/// table's printed order, so the lookup needs no mapping.
///
/// frmsizecod pairs up: two codes share a nominal bit rate. At 48 and 32 kHz
/// both spell the same length, but at 44.1 kHz they differ by one word,
/// because 1536 samples is 34.83 ms there and the frame cannot be a whole
/// number of words every time. Computing the length instead of reading it
/// would have to reproduce that alternation exactly; the table already does.
constexpr uint16_t kFrameSizeWords[38][3] = {
    {  64,   69,   96}, {  64,   70,   96},   //  32 kbps
    {  80,   87,  120}, {  80,   88,  120},   //  40 kbps
    {  96,  104,  144}, {  96,  105,  144},   //  48 kbps
    { 112,  121,  168}, { 112,  122,  168},   //  56 kbps
    { 128,  139,  192}, { 128,  140,  192},   //  64 kbps
    { 160,  174,  240}, { 160,  175,  240},   //  80 kbps
    { 192,  208,  288}, { 192,  209,  288},   //  96 kbps
    { 224,  243,  336}, { 224,  244,  336},   // 112 kbps
    { 256,  278,  384}, { 256,  279,  384},   // 128 kbps
    { 320,  348,  480}, { 320,  349,  480},   // 160 kbps
    { 384,  417,  576}, { 384,  418,  576},   // 192 kbps
    { 448,  487,  672}, { 448,  488,  672},   // 224 kbps
    { 512,  557,  768}, { 512,  558,  768},   // 256 kbps
    { 640,  696,  960}, { 640,  697,  960},   // 320 kbps
    { 768,  835, 1152}, { 768,  836, 1152},   // 384 kbps
    { 896,  975, 1344}, { 896,  976, 1344},   // 448 kbps
    {1024, 1114, 1536}, {1024, 1115, 1536},   // 512 kbps
    {1152, 1253, 1728}, {1152, 1254, 1728},   // 576 kbps
    {1280, 1393, 1920}, {1280, 1394, 1920},   // 640 kbps
};

/// Nominal bit rate in kbit/s for each pair of frame size codes, A/52 Table
/// 5.18. Indexed by frmsizecod >> 1.
constexpr uint16_t kBitRates[19] = {
     32,  40,  48,  56,  64,  80,  96, 112, 128, 160,
    192, 224, 256, 320, 384, 448, 512, 576, 640,
};

/// A/52 Table E2.3: the rates E-AC-3 reaches when fscod is '11'. Half the
/// ordinary ones, and the reason an E-AC-3 stream can be 24 kHz at all.
constexpr uint32_t kReducedSampleRates[4] = {24000, 22050, 16000, 0};

/// Full-bandwidth channels per acmod, A/52 Table 5.8. 1+1 is two independent
/// mono programmes and so counts two.
constexpr uint8_t kChannelsPerMode[8] = {2, 1, 2, 3, 3, 4, 4, 5};

} // namespace

uint32_t ac3SampleRate(uint8_t fscod)
{
    return fscod < 4 ? kSampleRates[fscod] : 0;
}

uint8_t ac3ChannelCount(AudioCodingMode acmod)
{
    const auto index = static_cast<unsigned>(acmod);
    return index < 8 ? kChannelsPerMode[index] : 0;
}

uint16_t ac3FrameSize(uint8_t fscod, uint8_t frmsizecod)
{
    if (fscod > 2 || frmsizecod > 37) {
        return 0;
    }
    // The table is in 16-bit words, which is what "1 word = 16 bits" in its
    // caption means; callers want bytes.
    return static_cast<uint16_t>(kFrameSizeWords[frmsizecod][fscod] * 2);
}

const char* AC3FrameHeader::displayName() const
{
    switch (flavour) {
    case Flavour::AC3:          return "AC-3";
    case Flavour::AC3Alternate: return "AC-3";  // Annex D differs in syntax, not in name
    case Flavour::EAC3:         return "E-AC-3";
    case Flavour::Unknown:      break;
    }
    return "AC-3";
}

namespace {

/// A/52 §E2.3.1.6 assigns the ranges: 0..8 is AC-3, 9 and 10 are the Annex D
/// alternate syntax, and 11..16 is E-AC-3, with 16 being Annex E proper.
Flavour flavourForBsid(uint8_t bsid)
{
    if (bsid <= 8) {
        return Flavour::AC3;
    }
    if (bsid <= 10) {
        return Flavour::AC3Alternate;
    }
    if (bsid <= 16) {
        return Flavour::EAC3;
    }
    return Flavour::Unknown;
}

/// AC-3 proper: syncinfo() then bsi(), A/52 Tables 5.1 and 5.2.
bool parseAC3(AC3BitReader& reader, AC3FrameHeader& header)
{
    header.crc1 = static_cast<uint16_t>(reader.read(16));
    header.fscod = static_cast<uint8_t>(reader.read(2));
    header.frmsizecod = static_cast<uint8_t>(reader.read(6));

    header.sample_rate = ac3SampleRate(header.fscod);
    header.frame_size = ac3FrameSize(header.fscod, header.frmsizecod);
    if (header.sample_rate == 0 || header.frame_size == 0) {
        return false; // reserved sample rate, or a frame size code past the table
    }
    header.bitrate = static_cast<uint32_t>(kBitRates[header.frmsizecod >> 1]) * 1000;

    header.bsid = static_cast<uint8_t>(reader.read(5));
    header.flavour = flavourForBsid(header.bsid);
    header.bsmod = static_cast<uint8_t>(reader.read(3));
    header.acmod = static_cast<AudioCodingMode>(reader.read(3));
    header.channels = ac3ChannelCount(header.acmod);

    const auto acmod_bits = static_cast<unsigned>(header.acmod);
    if ((acmod_bits & 0x1) && acmod_bits != 0x1) {
        reader.skip(2); // cmixlev, present with three front channels
    }
    if (acmod_bits & 0x4) {
        reader.skip(2); // surmixlev, present with a surround channel
    }
    if (header.acmod == AudioCodingMode::Stereo) {
        reader.skip(2); // dsurmod, Dolby Surround mode, 2/0 only
    }
    header.lfeon = reader.readBit() != 0;
    header.dialnorm = static_cast<uint8_t>(reader.read(5));

    // The rest of bsi is metadata this decoder does not use, but it has to be
    // stepped over exactly: the first audio block starts wherever bsi ends,
    // and every field past here is optional, so its length depends on the
    // flags rather than being fixed. Guessing leaves the block parser reading
    // from the wrong bit.
    if (reader.readBit()) { reader.skip(8); }   // compre / compr
    if (reader.readBit()) { reader.skip(8); }   // langcode / langcod
    if (reader.readBit()) { reader.skip(7); }   // audprodie: mixlevel 5 + roomtyp 2
    if (header.acmod == AudioCodingMode::DualMono) {
        // 1+1 carries a second programme, so these repeat.
        reader.skip(5);                          // dialnorm2
        if (reader.readBit()) { reader.skip(8); } // compr2e / compr2
        if (reader.readBit()) { reader.skip(8); } // langcod2e / langcod2
        if (reader.readBit()) { reader.skip(7); } // audprodi2e
    }
    reader.skip(2);                              // copyrightb, origbs
    if (reader.readBit()) { reader.skip(14); }   // timecod1e / timecod1
    if (reader.readBit()) { reader.skip(14); }   // timecod2e / timecod2
    if (reader.readBit()) {                      // addbsie
        const unsigned length = reader.read(6) + 1;
        reader.skip(length * 8);                 // addbsi
    }

    return !reader.overrun();
}

/// E-AC-3: a different bsi() entirely, A/52 Table E1.2. Parsed to describe the
/// stream, not to decode it -- Annex E is a separate decoder.
bool parseEAC3(AC3BitReader& reader, AC3FrameHeader& header)
{
    reader.skip(2);                                           // strmtyp
    reader.skip(3);                                           // substreamid
    // frmsiz is one less than the frame length in 16-bit words, so a frame is
    // never zero-length and 2047 means 2048 words.
    const uint32_t frmsiz = reader.read(11);
    header.frame_size = static_cast<uint16_t>((frmsiz + 1) * 2);

    header.fscod = static_cast<uint8_t>(reader.read(2));
    if (header.fscod == 0x3) {
        // '11' is not reserved here as it is in AC-3: it means the rate comes
        // from fscod2 instead, and the frame always holds six blocks.
        const uint32_t fscod2 = reader.read(2);
        header.sample_rate = kReducedSampleRates[fscod2];
    } else {
        header.sample_rate = ac3SampleRate(header.fscod);
        reader.skip(2);                                       // numblkscod
    }

    header.acmod = static_cast<AudioCodingMode>(reader.read(3));
    header.channels = ac3ChannelCount(header.acmod);
    header.lfeon = reader.readBit() != 0;
    header.bsid = static_cast<uint8_t>(reader.read(5));
    header.flavour = flavourForBsid(header.bsid);
    header.dialnorm = static_cast<uint8_t>(reader.read(5));

    // E-AC-3 states a frame length rather than a bit rate; deriving one needs
    // the block count, which varies. Left at zero rather than guessed.
    return header.sample_rate != 0 && !reader.overrun();
}

} // namespace

bool ac3ParseFrameHeader(AC3BitReader& reader, AC3FrameHeader& header)
{
    if (reader.read(16) != kSyncWord) {
        return false;
    }

    // bsid decides which bit stream follows, and it sits at bit 40 in both --
    // after crc1, fscod and frmsizecod in AC-3, and after strmtyp,
    // substreamid, frmsiz, fscod and acmod in E-AC-3. Reading it first is the
    // only way to know which layout the rest of the frame is in.
    const size_t after_sync = reader.tell();
    reader.seek(after_sync - 16 + 40);
    const auto bsid = static_cast<uint8_t>(reader.read(5));
    const Flavour flavour = flavourForBsid(bsid);
    if (flavour == Flavour::Unknown) {
        return false;
    }
    reader.seek(after_sync);

    header = AC3FrameHeader();
    return flavour == Flavour::EAC3 ? parseEAC3(reader, header)
                                    : parseAC3(reader, header);
}

bool parseAC3FrameHeader(const uint8_t* data, size_t size, AC3FrameHeader& header)
{
    // syncinfo is 5 bytes and the longest fixed run of bsi another handful;
    // 8 bytes covers everything read below without a length check per field.
    if (!data || size < 8) {
        return false;
    }
    AC3BitReader reader(data, size);
    return ac3ParseFrameHeader(reader, header);
}

} // namespace AC3
} // namespace Codec
} // namespace PsyMP3
