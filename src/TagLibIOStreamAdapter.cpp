/*
 * TagLibIOStreamAdapter.cpp - Implementation of TagLib IOStream adapter
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

#include "psymp3.h"
#include "TagLibIOStreamAdapter.h"
#include <cstdio>
#include <algorithm>

TagLibIOStreamAdapter::TagLibIOStreamAdapter(std::unique_ptr<IOHandler> handler, 
                                           const TagLib::String& name, 
                                           bool read_only)
    : m_io_handler(std::move(handler))
    , m_name(name)
    , m_read_only(read_only)
    , m_length(0)
    , m_length_cached(false)
{
    if (!m_io_handler) {
        throw std::invalid_argument("TagLibIOStreamAdapter: IOHandler cannot be null");
    }
}

TagLibIOStreamAdapter::~TagLibIOStreamAdapter()
{
    if (m_io_handler) {
        m_io_handler->close();
    }
}

TagLibFileName TagLibIOStreamAdapter::name() const
{
    return m_name.toCString(true);
}

TagLib::ByteVector TagLibIOStreamAdapter::readBlock(size_t length)
{
    if (!m_io_handler || length == 0) {
        return TagLib::ByteVector();
    }

    // Allocate buffer for reading
    std::vector<char> buffer(length);
    
    // Read data using IOHandler (returns number of elements read)
    size_t bytes_read = m_io_handler->read(buffer.data(), 1, length);
    
    if (bytes_read == 0) {
        return TagLib::ByteVector();
    }

    // Create ByteVector from read data
    return TagLib::ByteVector(buffer.data(), static_cast<unsigned int>(bytes_read));
}

void TagLibIOStreamAdapter::writeBlock(const TagLib::ByteVector& data)
{
    // Our IOHandler is currently read-only
    // This could be extended if write support is needed
    if (m_read_only) {
        // TagLib expects this to silently fail for read-only streams
        return;
    }
    
    // If write support is added to IOHandler, implement here:
    // m_io_handler->write(data.data(), 1, data.size());
}

void TagLibIOStreamAdapter::seek(TagLibOffset offset, Position p)
{
    if (!m_io_handler) {
        return;
    }

    int whence = convertSeekPosition(p);
    m_io_handler->seek(offset, whence);
}

TagLibOffset TagLibIOStreamAdapter::tell() const
{
    if (!m_io_handler) {
        return 0;
    }

    long pos = m_io_handler->tell();
    return pos >= 0 ? pos : 0;
}

TagLibOffset TagLibIOStreamAdapter::length()
{
    if (!m_length_cached) {
        calculateLength();
    }
    return m_length;
}

void TagLibIOStreamAdapter::truncate(TagLibOffset length)
{
    // Not supported by our read-only IOHandler
    // This would require extending IOHandler with truncate support
}

bool TagLibIOStreamAdapter::readOnly() const
{
    return m_read_only;
}

bool TagLibIOStreamAdapter::isOpen() const
{
    // Our IOHandler doesn't have an explicit "isOpen" check
    // We assume it's open if the handler exists and hasn't reached EOF
    return m_io_handler && !m_io_handler->eof();
}

void TagLibIOStreamAdapter::insert(const TagLib::ByteVector &data, TagLibSize start, TagLibSize replace)
{
    // Not supported by our read-only IOHandler
    // This is a complex operation that would require write support
}

void TagLibIOStreamAdapter::removeBlock(TagLibSize start, TagLibSize length)
{
    // Not supported by our read-only IOHandler
    // This is a complex operation that would require write support
}

void TagLibIOStreamAdapter::calculateLength()
{
    if (!m_io_handler) {
        m_length = 0;
        m_length_cached = true;
        return;
    }

    // Save current position
    long current_pos = m_io_handler->tell();
    if (current_pos < 0) {
        m_length = 0;
        m_length_cached = true;
        return;
    }

    // Seek to end to get length
    if (m_io_handler->seek(0, SEEK_END) == 0) {
        long end_pos = m_io_handler->tell();
        if (end_pos >= 0) {
            m_length = end_pos;
        } else {
            m_length = 0;
        }
        
        // Restore original position
        m_io_handler->seek(current_pos, SEEK_SET);
    } else {
        m_length = 0;
    }

    m_length_cached = true;
}

int TagLibIOStreamAdapter::convertSeekPosition(Position p) const
{
    switch (p) {
        case Beginning:
            return SEEK_SET;
        case Current:
            return SEEK_CUR;
        case End:
            return SEEK_END;
        default:
            return SEEK_SET;
    }
}