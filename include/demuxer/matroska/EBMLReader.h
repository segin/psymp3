/*
 * EBMLReader.h - EBML element reader, the substrate Matroska is built on.
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

#ifndef EBMLREADER_H
#define EBMLREADER_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Demuxer {
namespace Matroska {

/// One element header: what it is, and where its payload lives.
///
/// EBML is all elements, nested arbitrarily: an ID, a size, and either raw
/// payload or more elements. Nothing here interprets the ID -- that belongs to
/// the Matroska layer above, which is the only part that knows an ID of
/// 0x1549A966 means Segment Info.
struct EBMLElement {
    /// The ID exactly as encoded, marker bits included. That is how IDs are
    /// written in the Matroska specification (the EBML header is 0x1A45DFA3,
    /// not the 0x0A45DFA3 you would get by stripping the marker), so keeping
    /// them lets the tables above read like the spec.
    uint32_t id = 0;
    /// Payload length in bytes. Meaningless when @ref unknown_size is set.
    uint64_t size = 0;
    /// The size field was all value bits: this element runs until something
    /// else ends it. Live-muxed WebM writes its Segment and Clusters this way,
    /// because a muxer streaming to a socket cannot know the length in advance.
    bool unknown_size = false;
    /// The ID is one EBML forbids: its value bits are all ones, or all zeros
    /// at two bytes or more (RFC 8794 5). Only damage produces one, so the
    /// size after it is not to be trusted either. The one-byte 0x80 is legal
    /// (RFC 9559 4.2).
    bool invalid_id = false;
    /// Offset of the first byte of the ID.
    uint64_t header_offset = 0;
    /// Offset of the first byte of the payload.
    uint64_t data_offset = 0;

    /// One past the last payload byte. Only meaningful for a known size.
    uint64_t end() const { return data_offset + size; }
};

/// Reads EBML elements from an IOHandler.
///
/// Deliberately not a Demuxer: it has no notion of streams, seeking by time or
/// codecs, and being free of all that is what lets it be tested directly
/// against a handful of bytes in memory.
///
/// The reader does not own the handler. MatroskaDemuxer owns it and outlives
/// every reader it hands out.
class EBMLReader {
public:
    explicit EBMLReader(PsyMP3::IO::IOHandler* handler);

    /// The widest ID and size a Matroska file may use. RFC 9559 4.3 fixes
    /// EBMLMaxIDLength at 4 and caps EBMLMaxSizeLength at 8. EBML itself lets
    /// a header declare larger values (RFC 8794 11.2.4, 11.2.5), but Matroska
    /// does not, and a longer ID would not fit a uint32_t anyway. A file may
    /// declare a smaller size limit. The header's values are not read, so
    /// these two apply to every file.
    static constexpr int kMaxIdLength = 4;
    static constexpr int kMaxSizeLength = 8;

    /// Refuses to allocate more than this for one element's payload. Sizes
    /// come from the file, an eight-byte VINT reaches 2^56-2, and a truncated
    /// or hostile file will happily claim it. Nothing is read in pieces:
    /// CodecPrivate, every string and every Block of the track being played
    /// come through readBinary() or readString(), so a larger one is refused,
    /// and for a Block that ends playback. A block on another track is never
    /// read past the few bytes that hold its track number (MatroskaDemuxer
    /// checks that first), so a video frame past this size does not stop the
    /// audio.
    static constexpr uint64_t kMaxBinarySize = 64u * 1024 * 1024;

    /// Reads the next element header at the current position.
    ///
    /// Returns false at a clean end of input -- which is not an error, it is
    /// how every loop over a master element's children terminates. Throws when
    /// there are bytes but they are not a well-formed header.
    bool readElementHeader(EBMLElement& out);

    /// @name Payload readers
    /// Each consumes exactly `element.size` bytes and leaves the position at
    /// the end of the element, so a caller can read a value and carry straight
    /// on to its sibling.
    /// @{

    /// Unsigned integer, big-endian, 0 to 8 bytes wide. A zero-length integer
    /// reads as 0. That is its value only when the schema declares no default:
    /// an empty element that has one takes the default (RFC 8794 6.1), and
    /// applying it is left to the caller, which knows the schema. RFC 9559 4.4
    /// forbids Matroska writers to write an element empty when its default is
    /// not 0, because older readers made exactly this mistake.
    uint64_t readUInt(const EBMLElement& element);
    /// Signed integer, big-endian two's complement, sign-extended from its
    /// encoded width.
    int64_t readInt(const EBMLElement& element);
    /// IEEE 754, big-endian. Only 0, 4 and 8 byte widths exist; zero length
    /// reads as 0.0, and a declared default is again the caller's to apply.
    double readFloat(const EBMLElement& element);
    /// String payload, cut at the first NUL. RFC 8794 13 has a NUL and
    /// everything after it in the element ignored, so "eb\0l" reads as "eb". A
    /// writer may end a string with NULs, typically to overwrite a value in
    /// place with a shorter one, and a CodecID compared with them left on
    /// matches nothing.
    std::string readString(const EBMLElement& element);
    /// UTF-8 payload, cut at the first NUL as readString does, with invalid
    /// sequences repaired. For the elements RFC 9559 types utf-8 -- titles,
    /// names, tag names and values -- where readString is for the ASCII
    /// string elements such as CodecID.
    std::string readUTF8(const EBMLElement& element);
    /// Raw payload. Refuses anything past kMaxBinarySize.
    std::vector<uint8_t> readBinary(const EBMLElement& element);
    /// Nanoseconds since 2001-01-01T00:00:00 UTC -- the EBML epoch, which is
    /// not the Unix one. Zero length means the epoch itself.
    int64_t readDate(const EBMLElement& element);
    /// @}

    /// Steps over an element's payload without reading it.
    void skip(const EBMLElement& element);

    /// Decodes a variable-length integer from a buffer.
    ///
    /// The width is carried in the leading zeros of the first byte: 1xxxxxxx is
    /// one byte, 01xxxxxx two, and so on to eight. A first byte of zero would
    /// mean a width past eight and is rejected.
    ///
    /// @param keep_marker  IDs keep the marker bit, sizes strip it. That is the
    ///                     one difference between decoding the two halves of an
    ///                     element header, and getting it backwards yields IDs
    ///                     that match nothing and sizes that are wildly large.
    /// @param unknown      Set when every value bit is 1, which for a size
    ///                     means "unknown". In an ID the same pattern makes
    ///                     the ID invalid (RFC 8794 5); readElementHeader
    ///                     does not ask for the flag there.
    /// @return bytes consumed, or 0 if @p avail is too short or the encoding
    ///         is invalid.
    ///
    /// Static and buffer-based because block lacing needs the same decoder
    /// against an in-memory frame rather than the file.
    static size_t decodeVInt(const uint8_t* data, size_t avail, uint64_t& value,
                             bool keep_marker, bool* unknown = nullptr);

    /// Width in bytes a VINT starting with @p first_byte occupies, or 0 if that
    /// byte cannot start one.
    static int vintLength(uint8_t first_byte);

    uint64_t tell();
    void seek(uint64_t offset);

private:
    /// Reads a VINT from the handler rather than a buffer.
    bool readVInt(uint64_t& value, int max_length, bool keep_marker,
                  bool* unknown, int& consumed);
    /// Reads exactly @p length payload bytes into @p out.
    void readPayload(const EBMLElement& element, uint8_t* out, size_t length);
    /// Reads a whole payload, refusing one the file cannot hold before
    /// allocating anything for it.
    std::vector<uint8_t> readBounded(const EBMLElement& element);

    PsyMP3::IO::IOHandler* m_handler;
};

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3

#endif // EBMLREADER_H
