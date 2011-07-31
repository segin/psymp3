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
    m_handle = mpg123_new(NULL, NULL);
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
    mpg123_open((mpg123_handle *) m_handle, name.toCString(true));
    mpg123_getformat((mpg123_handle *) m_handle, &m_rate, &m_channels, &m_encoding);
    mpg123_format_none((mpg123_handle *) m_handle);
    mpg123_format((mpg123_handle *) m_handle, m_rate, m_channels, m_encoding);
}

unsigned int Libmpg123::getLength()
{
    return 0;
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

void *Libmpg123::getBuffer()
{
    return NULL;
}

size_t Libmpg123::getBufferLength()
{
    return 0;
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
