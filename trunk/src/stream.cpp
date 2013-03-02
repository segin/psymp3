/*
 * stream.cpp - Stream functionality base class
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psymp3.h"

Stream::Stream()
{
    /* Not sure if I need this... */
}

Stream::Stream(TagLib::String name) : m_path(name)
{
    m_tags = new TagLib::FileRef(name.toCString(true));
}

Stream::~Stream()
{
    if (m_tags) delete m_tags;
}

void Stream::open(TagLib::String name)
{
    // Base class - do nothing.
    return;
}

TagLib::String Stream::getArtist()
{
    if(!m_tags) return TagLib::String::null;
    return m_tags->tag()->artist();
}

TagLib::String Stream::getTitle()
{
    if(!m_tags) return TagLib::String::null;
    return m_tags->tag()->title();
}

TagLib::String Stream::getAlbum()
{
    if(!m_tags) return TagLib::String::null;
    return m_tags->tag()->album();
}

/* Note that the base class version falls back to TagLib, which is inaccurate.
 * Having all this generic functionality in TagLib as well as the children codec
 * classes is to make writing child codecs easier by providing generic, working
 * functionality until the codec version is fully implemented.
 */
unsigned int Stream::getLength()
{
    if(!m_tags) return 0;
    return m_tags->audioProperties()->length() * 1000; // * 1000 to make msec
}

/* As the data we're getting from TagLib is inaccurate but percise, we can
 * simply multiply it by the sample rate to get the approximate length in
 * samples.
 */
unsigned long long Stream::getSLength()
{
    if(!m_tags) return 0;
    return m_tags->audioProperties()->length() * m_tags->audioProperties()->bitrate();
}

/* TagLib provides this information in a generic manner for a mulitude of
 * different formats. This will aid greatly in implementing child codec
 * classes by providing an already-working mechanism for grabbing the data.
 * However, eventually, the codec class must override these classes if
 * possible. Best to get the data from the codec and decoder libraries.
 */
unsigned int Stream::getChannels()
{
    if(!m_tags) return 0;
    return m_tags->audioProperties()->channels();
}

unsigned int Stream::getRate()
{
    if(!m_tags) return 0;
    return m_tags->audioProperties()->sampleRate();
}

unsigned int Stream::getBitrate()
{
    if(!m_tags) return 0;
    return m_tags->audioProperties()->bitrate();
}

/* This is the sample encoding. So far, we can force all output to be
 * signed 16-bit little endian. As such, this is, so far, unneeded and
 * implemented solely as a stub. However, it may be needed in the future,
 * so for now, I leave it in place.
 */
unsigned int Stream::getEncoding()
{
    return 0;
}

unsigned int Stream::getPosition()
{
    return m_position;
}

unsigned long long Stream::getSPosition()
{
    return m_sposition;
}
