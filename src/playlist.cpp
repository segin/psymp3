/*
 * playlist.cpp - class implementation for playlist class
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
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

Playlist::Playlist(std::vector<std::string> args)
{
    parseArgs(args);
}

Playlist::Playlist(track trk)
{
    // M3U ctor
}

Playlist::~Playlist()
{
    //dtor
}

void Playlist::parseArgs(std::vector<std::string> args)
{
    for(int i = 1; i < args.size(); i++) {
        addFile(args[i]);
    }
}

bool Playlist::addFile(TagLib::String path)
{
    TagLib::FileRef *fileref;
    track *ntrk;
    try {
        std::cout << "Attempting open of " << path << std::endl;
        fileref = new TagLib::FileRef(path.toCString(true));
    } catch (std::exception& e) {
        std::cerr << "Playlist::addFile(): Cannot add file " << path << ": " << e.what() << std::endl;
	    return false;
    }
    ntrk = new track(path, fileref);
    tracks.push_back(*ntrk);
    return true;
}

long Playlist::getPosition(void)
{
    return m_position;
}

long Playlist::entries(void)
{
    return tracks.size();
}

bool Playlist::setPosition(long position)
{
    if(position < tracks.size()) {
        m_position = position;
        return true;
    } else {
        return false;
    }
}

TagLib::String Playlist::setPositionAndJump(long position)
{
    setPosition(position);
    return getTrack(position);
}

TagLib::String Playlist::getTrack(long position)
{
    if(position < tracks.size()) {
        std::string path = tracks[position].GetFilePath().to8Bit(true);
        return tracks[position].GetFilePath();
    } else {
        return "";
    }
}

TagLib::String Playlist::next()
{
    return getTrack(++m_position);
}

TagLib::String Playlist::prev()
{
    if (m_position == 0) m_position++;
    return getTrack(--m_position);
}

Playlist Playlist::loadPlaylist(TagLib::String path)
{
    Playlist nply;

    return nply;
}

void Playlist::savePlaylist(TagLib::String path)
{

}