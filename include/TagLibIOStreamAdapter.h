/*
 * TagLibIOStreamAdapter.h - Adapter to use IOHandler with TagLib
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef TAGLIBIOSTREAMADAPTER_H
#define TAGLIBIOSTREAMADAPTER_H

#include "IOHandler.h"
#include <taglib/tiostream.h>
#include <taglib/tstring.h>
#include <memory>

// Type compatibility for different TagLib versions
#ifdef TAGLIB_MAJOR_VERSION
  #if TAGLIB_MAJOR_VERSION >= 2
    // TagLib 2.x uses offset_t and proper namespacing
    using TagLibOffset = TagLib::offset_t;
    using TagLibFileName = TagLib::FileName;
    using TagLibSize = size_t;
  #else
    // TagLib 1.x uses long and unsigned long
    using TagLibOffset = long;
    using TagLibFileName = TagLib::FileName;
    using TagLibSize = unsigned long;
  #endif
#else
  // Fallback for older TagLib without version macros
  using TagLibOffset = long;
  using TagLibFileName = TagLib::FileName;
  using TagLibSize = unsigned long;
#endif

/**
 * @brief Adapter class that allows TagLib to use our IOHandler system.
 * 
 * This class bridges the gap between TagLib's IOStream interface and our
 * IOHandler system. It provides unified file I/O for both audio decoding
 * and metadata reading, solving Unicode filename issues and eliminating
 * the need for multiple file handles to the same file.
 */
class TagLibIOStreamAdapter : public TagLib::IOStream
{
public:
    /**
     * @brief Constructs a TagLib IOStream adapter.
     * @param handler The IOHandler to wrap (takes ownership)
     * @param name The stream name for TagLib
     * @param read_only Whether the stream is read-only
     */
    explicit TagLibIOStreamAdapter(std::unique_ptr<IOHandler> handler, 
                                  const TagLib::String& name, 
                                  bool read_only = true);

    /**
     * @brief Destructor - ensures proper cleanup of IOHandler.
     */
    ~TagLibIOStreamAdapter() override;

    // TagLib::IOStream interface implementation
    TagLibFileName name() const override;
    TagLib::ByteVector readBlock(size_t length) override;
    void writeBlock(const TagLib::ByteVector& data) override;
    void seek(TagLibOffset offset, Position p) override;
    TagLibOffset tell() const override;
    TagLibOffset length() override;
    void truncate(TagLibOffset length) override;
    bool readOnly() const override;
    bool isOpen() const override;
    void insert(const TagLib::ByteVector &data, TagLibSize start = 0, TagLibSize replace = 0) override;
    void removeBlock(TagLibSize start = 0, TagLibSize length = 0) override;

private:
    std::unique_ptr<IOHandler> m_io_handler;
    TagLib::String m_name;
    bool m_read_only;
    TagLibOffset m_length;
    bool m_length_cached;

    /**
     * @brief Calculates and caches the stream length.
     */
    void calculateLength();

    /**
     * @brief Converts TagLib seek position to standard whence values.
     * @param p TagLib position enum
     * @return Standard SEEK_* value
     */
    int convertSeekPosition(Position p) const;
};

#endif // TAGLIBIOSTREAMADAPTER_H