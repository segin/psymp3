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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psymp3.h"

std::vector<std::string> &MediaFile::split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> MediaFile::split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

Stream *MediaFile::open(TagLib::String name)
{
#ifdef _RISCOS
    std::vector<std::string> tokens = split(name.to8Bit(true), '/');
#else
    std::vector<std::string> tokens = split(name.to8Bit(true), '.');
#endif
    TagLib::String ext(TagLib::String(tokens[tokens.size() - 1]).upper());
#ifdef DEBUG
    std::cout << "MediaFile::open(): " << ext << std::endl;
#endif
    if(ext == "MP3")
        return new Libmpg123(name);
    if(ext == "OGG")
        return new Vorbis(name);
    throw InvalidMediaException("Unsupported format!");
}

