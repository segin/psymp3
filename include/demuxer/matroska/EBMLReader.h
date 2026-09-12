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

    /// Default ceilings from the EBML header (EBMLMaxIDLength,
    /// EBMLMaxSizeLength). A file may declare smaller ones; nothing in
    /// practice declares larger, and a larger ID would not fit a uint32_t.
    static constexpr int kMaxIdLength = 4;
    static constexpr int kMaxSizeLength = 8;

    /// Refuses to allocate more than this for one element's payload. Sizes
    /// come from the file, an eight-byte VINT reaches 2^56-2, and a truncated
    /// or hostile file will happily claim it. Anything genuinely this large in
    /// an audio file is read incrementally, not through readBinary().
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
    /// is legal EBML and means zero; Matroska relies on that to omit defaults.
    uint64_t readUInt(const EBMLElement& element);
    /// Signed integer, big-endian two's complement, sign-extended from its
    /// encoded width.
    int64_t readInt(const EBMLElement& element);
    /// IEEE 754, big-endian. Only 0, 4 and 8 byte widths exist; zero length
    /// means 0.0.
    double readFloat(const EBMLElement& element);
    /// String payload, with trailing NULs trimmed. EBML permits padding a
    /// string with them, and a CodecID compared with the padding left on
    /// matches nothing.
    std::string readString(const EBMLElement& element);
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
    ///                     means "unknown". Never meaningful for an ID.
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

    PsyMP3::IO::IOHandler* m_handler;
};

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3

#endif // EBMLREADER_H
