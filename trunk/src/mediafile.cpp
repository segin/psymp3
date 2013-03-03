/*
 * mediafile.cpp - format abstraction :)
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

Stream *MediaFile::open(TagLib::String name)
{
    TagLib::String a = name.substr(name.size() - 3).upper();
#ifdef DEBUG
    std::cout << "MediaFile::open(): " << a << std::endl;
#endif
    if(a == "MP3")
        return new Libmpg123(name);
    if(a == "OGG")
        return new Vorbis(name);
    throw InvalidMediaException("Unsupported format!");
}

