/*
 * libmpg123.cpp - Extends the Stream base class to decode MP3s.
 * This file is part of PsyMP3.
 * Copyright © 2011 Kirn Gill <segin2005@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "psymp3.h"

Libmpg123::Libmpg123(TagLib::String name) : Stream(name)
{
    int err = MPG123_OK;
    m_handle = mpg123_new(NULL, &err);
    if (!m_handle) {
        std::cerr << "mpg123_new() failed: " << mpg123_plain_strerror(err) << std::endl;
    }
    open(name);
}

Libmpg123::~Libmpg123()
{
    mpg123_close((mpg123_handle *) m_handle);
    mpg123_delete((mpg123_handle *) m_handle);
    m_handle = NULL;
}

void Libmpg123::open(TagLib::String name)
{
    int ret;
    ret = mpg123_open((mpg123_handle *) m_handle, name.toCString(true));
    if (ret == MPG123_OK) {
        std::cerr << "mpg123_open() failed: " << ret << std::endl;
    }
    ret = mpg123_getformat((mpg123_handle *) m_handle, &m_rate, &m_channels, &m_encoding);
    if (ret == -1) {

    }
    ret = mpg123_format_none((mpg123_handle *) m_handle);
    if (ret == -1) {

    }
    ret = mpg123_format((mpg123_handle *) m_handle, m_rate, m_channels, m_encoding);
    if (ret == -1) {

    }
}

unsigned int Libmpg123::getChannels()
{
    return m_channels;
}

unsigned int Libmpg123::getRate()
{
    return m_rate;
}

unsigned int Libmpg123::getEncoding()
{
    return 0;
}

size_t Libmpg123::getData(size_t len, void *buf)
{
    size_t actual;
    std::cout << "Libmpg123::getData(): len = " << (int) len << ", buf =" << std::hex << buf << std::endl;
    mpg123_read((mpg123_handle *) m_handle, (unsigned char *) buf, len, &actual);
    std::cout << "Libmpg123::getData(): actual = " << (int) actual << std::endl;
    return actual;
}

void Libmpg123::init()
{
    mpg123_init();
    atexit(fini);
}

void Libmpg123::fini()
{
    mpg123_exit();
}
