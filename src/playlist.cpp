/*
 * playlist.cpp - class implementation for playlist class
 * This file is part of PsyMP3.
 * Copyright © 2011-2024 Kirn Gill <segin2005@gmail.com>
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

Playlist::~Playlist()
{
    //dtor
}

bool Playlist::addFile(TagLib::String path)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    TagLib::FileRef *fileref;
    track *ntrk;
    try {
        std::cout << "Attempting open of " << path << std::endl;
        fileref = new TagLib::FileRef(path.toCString(true));
    } catch (const std::exception& e) { // Revert to catching std::exception, as TagLib::FileRef::NotAFile is not a standard TagLib exception.
        std::cerr << "Playlist::addFile(): Cannot add file " << path << ": " << e.what() << std::endl;
	    return false;
    }
    // Construct track directly in the vector, passing the raw pointer.
    // The track constructor will take ownership of 'fileref' via unique_ptr.
    tracks.emplace_back(path, fileref);
    return true;
}

long Playlist::getPosition(void)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_position;
}

long Playlist::entries(void)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return tracks.size();
}

bool Playlist::setPosition(long position)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if(position < tracks.size()) {
        m_position = position;
        return true;
    } else {
        return false;
    }
}

TagLib::String Playlist::setPositionAndJump(long position)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    setPosition(position);
    return getTrack(position);
}

TagLib::String Playlist::getTrack(long position)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if(position < tracks.size()) {
        std::string path = tracks[position].GetFilePath().to8Bit(true);
        return tracks[position].GetFilePath();
    } else {
        return "";
    }
}

TagLib::String Playlist::next()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return getTrack(++m_position);
}

TagLib::String Playlist::prev()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_position == 0) m_position++;
    return getTrack(--m_position);
}

std::unique_ptr<Playlist> Playlist::loadPlaylist(TagLib::String path)
{
    auto nply = std::make_unique<Playlist>();
    // TODO: Implement actual playlist loading logic here, populating nply
    return nply;
}

void Playlist::savePlaylist(TagLib::String path)
{

}