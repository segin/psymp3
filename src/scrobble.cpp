/*
 * scrobble.cpp - class implementation for scrobble class
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

Scrobble::Scrobble(const track& rhs) :
    m_artist(rhs.GetArtist().to8Bit(true)),
    m_title(rhs.GetTitle().to8Bit(true)),
    m_album(rhs.GetAlbum().to8Bit(true)),
    m_length(rhs.GetLen()),
    m_timestamp(time(nullptr))
{
    //ctor
}

Scrobble::Scrobble(const std::string& artist, const std::string& title, const std::string& album, int length, time_t timestamp) :
    m_artist(artist),
    m_title(title),
    m_album(album),
    m_length(length),
    m_timestamp(timestamp)
{
    //ctor
}

Scrobble::~Scrobble()
{
    //dtor
}

std::string Scrobble::toXML() const
{
    XMLUtil::Element scrobbleElement("scrobble");
    
    scrobbleElement.children.emplace_back("artist", m_artist);
    scrobbleElement.children.emplace_back("title", m_title);
    scrobbleElement.children.emplace_back("album", m_album);
    scrobbleElement.children.emplace_back("length", std::to_string(m_length));
    scrobbleElement.children.emplace_back("timestamp", std::to_string(m_timestamp));
    
    return XMLUtil::generateXML(scrobbleElement);
}

Scrobble Scrobble::fromXML(const std::string& xml)
{
    try {
        XMLUtil::Element scrobbleElement = XMLUtil::parseXML(xml);
        
        std::string artist = XMLUtil::getChildText(scrobbleElement, "artist");
        std::string title = XMLUtil::getChildText(scrobbleElement, "title");
        std::string album = XMLUtil::getChildText(scrobbleElement, "album");
        
        std::string lengthStr = XMLUtil::getChildText(scrobbleElement, "length");
        std::string timestampStr = XMLUtil::getChildText(scrobbleElement, "timestamp");
        
        int length = lengthStr.empty() ? 0 : std::stoi(lengthStr);
        time_t timestamp = timestampStr.empty() ? 0 : std::stoll(timestampStr);
        
        return Scrobble(artist, title, album, length, timestamp);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse scrobble XML: " << e.what() << std::endl;
        return Scrobble("", "", "", 0, 0);
    }
}

bool Scrobble::operator==(const Scrobble& other) const
{
    return m_artist == other.m_artist &&
           m_title == other.m_title &&
           m_album == other.m_album &&
           m_length == other.m_length &&
           m_timestamp == other.m_timestamp;
}

bool Scrobble::operator!=(const Scrobble& other) const
{
    return !(*this == other);
}
