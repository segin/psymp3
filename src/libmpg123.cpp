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

Libmpg123::Libmpg123(TagLib::String name) : Stream(name)
{
    int err = MPG123_OK;
    m_handle = mpg123_new(nullptr, &err);
    if (!m_handle) {
        throw std::runtime_error("mpg123_new() failed: " + std::string(mpg123_plain_strerror(err)));
    }
    open(name);
}

Libmpg123::~Libmpg123()
{
    mpg123_close(static_cast<mpg123_handle *>(m_handle));
    mpg123_delete(static_cast<mpg123_handle *>(m_handle));
    m_handle = nullptr;
}

void Libmpg123::open(TagLib::String name)
{
    int ret;
#ifdef _WIN32
    // Use the wide-character version on Windows for better Unicode path support.
    ret = mpg123_open_w(static_cast<mpg123_handle *>(m_handle), name.toCWString());
#else
    ret = mpg123_open(static_cast<mpg123_handle *>(m_handle), name.toCString(true));
#endif
    if (ret != MPG123_OK) {
        throw BadFormatException("mpg123_open() failed: " + TagLib::String(mpg123_plain_strerror(ret)));
    }
    ret = mpg123_getformat(static_cast<mpg123_handle *>(m_handle), &m_rate, &m_channels, &m_encoding);
    if (ret != MPG123_OK) {
        throw BadFormatException("mpg123_getformat() failed: " + TagLib::String(mpg123_plain_strerror(ret)));
    }
    ret = mpg123_format_none(static_cast<mpg123_handle *>(m_handle));
    if (ret != MPG123_OK) {
        throw BadFormatException("mpg123_format_none() failed: " + TagLib::String(mpg123_plain_strerror(ret)));
    }
    ret = mpg123_format(static_cast<mpg123_handle *>(m_handle), m_rate, m_channels, MPG123_ENC_SIGNED_16);
    if (ret != MPG123_OK) {
        throw BadFormatException("mpg123_format() failed: " + TagLib::String(mpg123_plain_strerror(ret)));
    }
    m_eof = false;
}

unsigned int Libmpg123::getLength()
{
    return (int) ((long long) mpg123_length(static_cast<mpg123_handle *>(m_handle)) * 1000 / m_rate);
}

unsigned long long Libmpg123::getSLength()
{
    return mpg123_length(static_cast<mpg123_handle *>(m_handle));
}

unsigned int Libmpg123::getPosition()
{
    return (int) ((long long) mpg123_tell(static_cast<mpg123_handle *>(m_handle)) * 1000 / m_rate);
}

unsigned long long Libmpg123::getSPosition()
{
    return mpg123_tell(static_cast<mpg123_handle *>(m_handle));
}

size_t Libmpg123::getData(size_t len, void *buf)
{
    size_t actual;
    int cond;
    //std::cout << "Libmpg123::getData(): len = " << (int) len << ", buf =" << std::hex << buf << std::endl;
    cond = mpg123_read(static_cast<mpg123_handle *>(m_handle), static_cast<unsigned char *>(buf), len, &actual);
    if (cond == MPG123_DONE) {
        m_eof = true;
    } else if (cond != MPG123_OK) {
        throw BadFormatException("mpg123_read() failed: " + TagLib::String(mpg123_plain_strerror(cond)));
    }
    m_position = (long long) mpg123_tell(static_cast<mpg123_handle *>(m_handle)) * 1000 / m_rate;
    //std::cout << "Libmpg123::getData(): actual = " << (int) actual << std::endl;
    return actual;
}

void Libmpg123::seekTo(unsigned long pos)
{
    long long a = (long long) pos * m_rate / 1000;
    m_position = (long long) mpg123_seek(static_cast<mpg123_handle *>(m_handle), a, SEEK_SET) * 1000 / m_rate;
}

bool Libmpg123::eof()
{
    return m_eof;
}
