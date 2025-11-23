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

#ifndef TAGLIBIOHANDLERADAPTER_H
#define TAGLIBIOHANDLERADAPTER_H

// No direct includes - all includes should be in psymp3.h

// Type compatibility for different TagLib versions
#ifdef TAGLIB_MAJOR_VERSION
  #if TAGLIB_MAJOR_VERSION >= 2
    // TagLib 2.x: insert(data, offset_t start, size_t replace), removeBlock(offset_t start, size_t length)
    using TagLibOffset = TagLib::offset_t;
    using TagLibFileName = TagLib::FileName;
    using TagLibInsertStart = TagLib::offset_t;
    using TagLibInsertReplace = size_t;
    using TagLibRemoveStart = TagLib::offset_t;
    using TagLibRemoveLength = size_t;
  #else
    // TagLib 1.x: insert(data, unsigned long start, unsigned long replace), removeBlock(unsigned long start, unsigned long length)
    using TagLibOffset = long;
    using TagLibFileName = TagLib::FileName;
    using TagLibInsertStart = unsigned long;
    using TagLibInsertReplace = unsigned long;
    using TagLibRemoveStart = unsigned long;
    using TagLibRemoveLength = unsigned long;
  #endif
#else
  // Fallback for older TagLib without version macros
  using TagLibOffset = long;
  using TagLibFileName = TagLib::FileName;
  using TagLibInsertStart = unsigned long;
  using TagLibInsertReplace = unsigned long;
  using TagLibRemoveStart = unsigned long;
  using TagLibRemoveLength = unsigned long;
#endif

namespace PsyMP3 {
namespace IO {

/**
 * @brief Adapter class that allows TagLib to use our IOHandler system.
 * 
 * This class bridges the gap between TagLib's IOStream interface and our
 * IOHandler system. It provides unified file I/O for both audio decoding
 * and metadata reading, solving Unicode filename issues and eliminating
 * the need for multiple file handles to the same file.
 */
class TagLibIOHandlerAdapter : public TagLib::IOStream
{
public:
    /**
     * @brief Constructs a TagLib IOStream adapter.
     * @param handler The IOHandler to wrap (takes ownership)
     * @param name The stream name for TagLib
     * @param read_only Whether the stream is read-only
     */
    explicit TagLibIOHandlerAdapter(std::unique_ptr<IOHandler> handler, 
                                  const TagLib::String& name, 
                                  bool read_only = true);

    /**
     * @brief Destructor - ensures proper cleanup of IOHandler.
     */
    ~TagLibIOHandlerAdapter() override;

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
    void insert(const TagLib::ByteVector &data, TagLibInsertStart start = 0, TagLibInsertReplace replace = 0) override;
    void removeBlock(TagLibRemoveStart start = 0, TagLibRemoveLength length = 0) override;

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

} // namespace IO
} // namespace PsyMP3

#endif // TAGLIBIOHANDLERADAPTER_H
