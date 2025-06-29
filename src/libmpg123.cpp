/*
 * libmpg123.cpp - Extends the Stream base class to decode MP3s.
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

// Callback functions for libmpg123 to read from our FILE* handle.
// These must have C linkage and cannot be member functions.

// Read callback
static ssize_t read_callback(void *handle, void *buffer, size_t count)
{
    return static_cast<IOHandler*>(handle)->read(buffer, 1, count);
}

// Seek callback
static off_t lseek_callback(void *handle, off_t offset, int whence)
{
    auto* handler = static_cast<IOHandler*>(handle);
    if (handler->seek(static_cast<long>(offset), whence) != 0) {
        return -1;
    }
    return handler->tell();
}

// Cleanup callback
static void cleanup_callback(void *handle)
{
    // The IOHandler's lifetime is managed by the Libmpg123 class's unique_ptr.
    // This callback can ensure the underlying handle is closed if necessary.
    static_cast<IOHandler*>(handle)->close();
}

Libmpg123::Libmpg123(TagLib::String name) : Stream(name), m_mpg_handle(nullptr)
{
    int err = MPG123_OK;
    m_mpg_handle = mpg123_new(nullptr, &err);
    if (!m_mpg_handle) {
        throw std::runtime_error("mpg123_new() failed: " + std::string(mpg123_plain_strerror(err)));
    }
    mpg123_param(m_mpg_handle, MPG123_ADD_FLAGS, MPG123_QUIET, 0);
    open(name);
}

Libmpg123::~Libmpg123()
{
    // mpg123_close will trigger our cleanup_callback, which closes the file handle.
    mpg123_close(m_mpg_handle);
    mpg123_delete(m_mpg_handle);
}

void Libmpg123::open(TagLib::String name)
{
    // Use the new URI and IOHandler abstraction layer.
    URI uri(name);
    if (uri.scheme() == "file") {
        m_handler = std::make_unique<FileIOHandler>(uri.path());
    } else {
        // In the future, a factory could create other IOHandlers here.
        throw InvalidMediaException("Unsupported URI scheme for MP3: " + uri.scheme());
    }

    // Replace the default I/O with our callback-based reader.
    mpg123_replace_reader_handle(m_mpg_handle, read_callback, lseek_callback, cleanup_callback);

    // "Open" the handle, which now uses our callbacks and the provided IOHandler* handle.
    int ret = mpg123_open_handle(m_mpg_handle, m_handler.get());
    if (ret != MPG123_OK) {
        throw InvalidMediaException("mpg123_open_handle() failed: " + TagLib::String(mpg123_plain_strerror(ret)));
    }

    ret = mpg123_getformat(m_mpg_handle, &m_rate, &m_channels, &m_encoding);
    if (ret != MPG123_OK) {
        throw BadFormatException("mpg123_getformat() failed: " + TagLib::String(mpg123_plain_strerror(ret)));
    }
    ret = mpg123_format_none(m_mpg_handle);
    if (ret != MPG123_OK) {
        throw BadFormatException("mpg123_format_none() failed: " + TagLib::String(mpg123_plain_strerror(ret)));
    }
    ret = mpg123_format(m_mpg_handle, m_rate, m_channels, MPG123_ENC_SIGNED_16);
    if (ret != MPG123_OK) {
        throw BadFormatException("mpg123_format() failed: " + TagLib::String(mpg123_plain_strerror(ret)));
    }
    m_eof = false;
}

unsigned int Libmpg123::getLength()
{
    return (int) ((long long) mpg123_length(m_mpg_handle) * 1000 / m_rate);
}

unsigned long long Libmpg123::getSLength()
{
    return mpg123_length(m_mpg_handle);
}

unsigned int Libmpg123::getPosition()
{
    return (int) ((long long) mpg123_tell(m_mpg_handle) * 1000 / m_rate);
}

unsigned long long Libmpg123::getSPosition()
{
    return mpg123_tell(m_mpg_handle);
}

size_t Libmpg123::getData(size_t len, void *buf)
{
    size_t actual;
    int cond;
    //std::cout << "Libmpg123::getData(): len = " << (int) len << ", buf =" << std::hex << buf << std::endl;
    cond = mpg123_read(m_mpg_handle, static_cast<unsigned char *>(buf), len, &actual);
    if (cond == MPG123_DONE) {
        m_eof = true;
    } else if (cond != MPG123_OK) {
        throw BadFormatException("mpg123_read() failed: " + TagLib::String(mpg123_plain_strerror(cond)));
    }
    m_position = (long long) mpg123_tell(m_mpg_handle) * 1000 / m_rate;
    //std::cout << "Libmpg123::getData(): actual = " << (int) actual << std::endl;
    return actual;
}

void Libmpg123::seekTo(unsigned long pos)
{
    long long a = (long long) pos * m_rate / 1000;
    m_position = (long long) mpg123_seek(m_mpg_handle, a, SEEK_SET) * 1000 / m_rate;
    m_eof = false; // A seek operation means we are no longer at the end of the file.
}

bool Libmpg123::eof()
{
    return m_eof;
}
