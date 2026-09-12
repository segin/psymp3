/*
 * MatroskaElements.h - Matroska element IDs and the codec identifier mapping.
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

#ifndef MATROSKAELEMENTS_H
#define MATROSKAELEMENTS_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Demuxer {
namespace Matroska {

/// Element IDs, written the way the Matroska specification writes them: with
/// the EBML marker bits left on, which is what EBMLReader hands back.
namespace Id {

constexpr uint32_t EBMLHeader      = 0x1A45DFA3;
constexpr uint32_t EBMLVersion     = 0x4286;
constexpr uint32_t EBMLReadVersion = 0x42F7;
constexpr uint32_t DocType         = 0x4282;
constexpr uint32_t DocTypeVersion  = 0x4287;
constexpr uint32_t DocTypeReadVersion = 0x4285;

constexpr uint32_t Segment  = 0x18538067;
constexpr uint32_t SeekHead = 0x114D9B74;
constexpr uint32_t Seek     = 0x4DBB;
constexpr uint32_t SeekID   = 0x53AB;
constexpr uint32_t SeekPosition = 0x53AC;

constexpr uint32_t Info           = 0x1549A966;
constexpr uint32_t TimestampScale = 0x2AD7B1;
constexpr uint32_t Duration       = 0x4489;
constexpr uint32_t MuxingApp      = 0x4D80;
constexpr uint32_t WritingApp     = 0x5741;
constexpr uint32_t Title          = 0x7BA9;
constexpr uint32_t SegmentUUID    = 0x73A4;

constexpr uint32_t Tracks          = 0x1654AE6B;
constexpr uint32_t TrackEntry      = 0xAE;
constexpr uint32_t TrackNumber     = 0xD7;
constexpr uint32_t TrackUID        = 0x73C5;
constexpr uint32_t TrackType       = 0x83;
constexpr uint32_t FlagEnabled     = 0xB9;
constexpr uint32_t FlagDefault     = 0x88;
constexpr uint32_t FlagForced      = 0x55AA;
constexpr uint32_t FlagLacing      = 0x9C;
constexpr uint32_t DefaultDuration = 0x23E383;
constexpr uint32_t TrackName       = 0x536E;
constexpr uint32_t Language        = 0x22B59C;
constexpr uint32_t LanguageBCP47   = 0x22B59D;
constexpr uint32_t CodecID         = 0x86;
constexpr uint32_t CodecPrivate    = 0x63A2;
constexpr uint32_t CodecName       = 0x258688;
constexpr uint32_t CodecDelay      = 0x56AA;
constexpr uint32_t SeekPreRoll     = 0x56BB;

constexpr uint32_t Audio                   = 0xE1;
constexpr uint32_t SamplingFrequency       = 0xB5;
constexpr uint32_t OutputSamplingFrequency = 0x78B5;
constexpr uint32_t Channels                = 0x9F;
constexpr uint32_t BitDepth                = 0x6264;

constexpr uint32_t Cluster        = 0x1F43B675;
constexpr uint32_t Timestamp      = 0xE7;
constexpr uint32_t SimpleBlock    = 0xA3;
constexpr uint32_t BlockGroup     = 0xA0;
constexpr uint32_t Block          = 0xA1;
constexpr uint32_t BlockDuration  = 0x9B;
constexpr uint32_t DiscardPadding = 0x75A2;

constexpr uint32_t Cues              = 0x1C53BB6B;
constexpr uint32_t CuePoint          = 0xBB;
constexpr uint32_t CueTime           = 0xB3;
constexpr uint32_t CueTrackPositions = 0xB7;
constexpr uint32_t CueTrack          = 0xF7;
constexpr uint32_t CueClusterPosition = 0xF1;
constexpr uint32_t CueRelativePosition = 0xF0;

constexpr uint32_t Tags      = 0x1254C367;
constexpr uint32_t Tag       = 0x7373;
constexpr uint32_t Targets   = 0x63C0;
constexpr uint32_t SimpleTag = 0x67C8;
constexpr uint32_t TagName   = 0x45A3;
constexpr uint32_t TagString = 0x4487;

constexpr uint32_t Attachments = 0x1941A469;

/// Padding and integrity, present anywhere and carrying nothing a demuxer
/// wants. Skipped wherever children are walked.
constexpr uint32_t Void  = 0xEC;
constexpr uint32_t CRC32 = 0xBF;

} // namespace Id

/// TrackType values. Only Audio is playable here; the rest exist so a video
/// track in a .mkv can be recognised and passed over rather than mistaken for
/// something to decode.
namespace TrackType {
constexpr uint64_t Video    = 1;
constexpr uint64_t Audio    = 2;
constexpr uint64_t Complex  = 3;
constexpr uint64_t Logo     = 0x10;
constexpr uint64_t Subtitle = 0x11;
constexpr uint64_t Buttons  = 0x12;
constexpr uint64_t Control  = 0x20;
constexpr uint64_t Metadata = 0x21;
} // namespace TrackType

/// The codec_name the rest of PsyMP3 dispatches on for a Matroska CodecID, or
/// an empty string when nothing in the tree decodes it.
///
/// Returning empty rather than guessing is deliberate: a track PsyMP3 cannot
/// decode should be reported by name, not opened and then failed at the first
/// packet.
std::string codecNameForId(const std::string& codec_id);

/// True when @p codec_id names raw PCM stored most significant byte first.
/// A_PCM/INT/BIG is the only such case, and reading it host-order does not
/// sound subtly wrong -- it decodes to static, exactly as AIFF did.
bool codecIsBigEndianPCM(const std::string& codec_id);

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3

#endif // MATROSKAELEMENTS_H
