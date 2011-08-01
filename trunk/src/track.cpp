/*
 * track.cpp - class implementation for track class
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

track::track(TagLib::String a_FilePath)
{
    //ctor
    m_FilePath = a_FilePath;
}

track::track()
{
    //ctor
}


track::track(TagLib::String a_Artist, TagLib::String a_Title, TagLib::String a_Album, TagLib::String a_FilePath, unsigned int a_Len)
{
    //ctor
    m_Artist = a_Artist;
    m_Title = a_Title;
    m_Album = a_Album;
    m_FilePath = a_FilePath;
    m_Len = a_Len;
}

track::~track()
{
    //dtor
}

track& track::operator=(const track& rhs)
{
    if (this == &rhs) return *this; // handle self assignment
    //assignment operator
    return *this;
}