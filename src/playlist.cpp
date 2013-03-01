/*
 * playlist.cpp - class implementation for playlist class
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

Playlist::Playlist(std::vector<std::string> args)
{
    parseArgs(args);
}

Playlist::Playlist(TagLib::String playlist)
{
    // M3U ctor
}

Playlist::~Playlist()
{
    //dtor
}

void Playlist::parseArgs(std::vector<std::string> args)
{
    for(int i = 0; i < args.size(); i++) {
        addFile(args[i]);
    }
}

bool Playlist::addFile(TagLib::String path)
{
    TagLib::FileRef *fileref;
    track *ntrk;
    try {
        fileref = new TagLib::FileRef(path.toCString(true));
    } catch (std::exception& e) {
        std::cerr << "Playlist::addFile(): Cannot add file " << path << ": " << e.what() << std::endl;
	return false;
    }
    ntrk = new track(path, fileref);
    tracks.push_back(*ntrk);
}

long Playlist::getPosition(void)
{
    return m_position;
}

bool Playlist::setPosition(long position);
{
    if(position =< tracks.size()) {
        m_position = position;
        return true;
    } else {
        return false;
    }
}
