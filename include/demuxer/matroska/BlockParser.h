/*
 * BlockParser.h - Matroska block headers and the four lacing modes.
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

#ifndef MATROSKABLOCKPARSER_H
#define MATROSKABLOCKPARSER_H

// All necessary headers are included via psymp3.h

namespace PsyMP3 {
namespace Demuxer {
namespace Matroska {

/// How a block packs more than one frame into itself.
enum class Lacing {
    None  = 0, ///< one frame, the rest of the block
    Xiph  = 1, ///< sizes as runs of 255-terminated bytes, as Ogg does it
    Fixed = 2, ///< every frame the same size; no sizes stored at all
    EBML  = 3  ///< first size a VINT, the rest signed deltas from it
};

/// A block's header, decoded.
struct BlockHeader {
    uint64_t track_number = 0;
    /// Ticks relative to the enclosing Cluster's timestamp, and **signed**: a
    /// frame may legitimately precede the cluster it is stored in.
    int16_t timestamp_offset = 0;
    bool keyframe = false;     ///< SimpleBlock only; a Block never says
    bool invisible = false;
    bool discardable = false;  ///< SimpleBlock only
    Lacing lacing = Lacing::None;
};

/// One frame inside a block, as a view into the block's payload.
///
/// A view rather than a copy: a laced block holds several frames in one buffer
/// and copying each out would double the work of reading a file for nothing.
/// The payload must outlive the frames.
struct BlockFrame {
    const uint8_t* data = nullptr;
    size_t size = 0;
};

/// Splits a SimpleBlock or Block payload into its frames.
///
/// Every size in a laced block comes out of the file and none of them are
/// checked against the payload by the format itself, so a truncated or hostile
/// block can describe frames that run past the end of what was read. Parsing
/// fails rather than producing a frame that points outside the buffer.
class BlockParser {
public:
    /// @param data     the block's payload: everything after the element header
    /// @param size     its length
    /// @param header   filled in on success
    /// @param frames   filled with views into @p data on success
    /// @return false when the block is malformed, in which case @p frames is
    ///         empty and nothing points anywhere
    static bool parse(const uint8_t* data, size_t size, BlockHeader& header,
                      std::vector<BlockFrame>& frames);

    /// Decodes the signed variable-length integer EBML lacing uses for its
    /// deltas.
    ///
    /// Not the same encoding as an ordinary VINT: the value is read unsigned
    /// and then biased down by half its range, so a delta can be negative when
    /// a frame is smaller than the one before it. The bias depends on the
    /// width -- 63 for one byte, 8191 for two -- and using the wrong one turns
    /// every size after the first into nonsense while leaving the first
    /// correct, which reads as a codec fault rather than a container one.
    ///
    /// @return bytes consumed, or 0 if the buffer is too short or malformed.
    static size_t decodeSignedVInt(const uint8_t* data, size_t avail, int64_t& value);
};

} // namespace Matroska
} // namespace Demuxer
} // namespace PsyMP3

#endif // MATROSKABLOCKPARSER_H
