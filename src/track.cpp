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

track::track(TagLib::String a_FilePath, TagLib::FileRef *a_FileRef)
{
    m_FilePath = a_FilePath;
    if (a_FileRef)
        ;
    else {
        try {
            a_FileRef = new TagLib::FileRef(a_FilePath.toCString(true));
        } catch (std::exception& e) {
            std::cerr << "track::track(): Exception: " << e.what() << std::endl;
            a_FileRef = (TagLib::FileRef *) NULL;
        }
    }
    if (a_FileRef) {
        m_Artist = a_FileRef->tag()->artist();
        m_Title = a_FileRef->tag()->title();
        m_Album = a_FileRef->tag()->album();
        m_Len = a_FileRef->audioProperties()->length();
    }
}

