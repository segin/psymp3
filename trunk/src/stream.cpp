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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "psymp3.h"

Stream::Stream()
{

}

Stream::Stream(TagLib::String name)
{
    m_tags = new TagLib::FileRef(name.toCString(true));
    m_path = name;
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

unsigned int Stream::getLength()
{
    if(!m_tags) return 0;
    return m_tags->audioProperties()->length() * 1000; // * 1000 to make msec
}

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

unsigned int Stream::getEncoding()
{
    return 0;
}
