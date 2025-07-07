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
    std::ostringstream xml;
    xml << "<scrobble>"
        << "<artist>" << m_artist << "</artist>"
        << "<title>" << m_title << "</title>"
        << "<album>" << m_album << "</album>"
        << "<length>" << m_length << "</length>"
        << "<timestamp>" << m_timestamp << "</timestamp>"
        << "</scrobble>";
    return xml.str();
}

Scrobble Scrobble::fromXML(const std::string& xml)
{
    // Simple XML parsing for scrobble data
    std::string artist, title, album;
    int length = 0;
    time_t timestamp = 0;
    
    // Extract artist
    size_t start = xml.find("<artist>") + 8;
    size_t end = xml.find("</artist>");
    if (start != std::string::npos && end != std::string::npos) {
        artist = xml.substr(start, end - start);
    }
    
    // Extract title
    start = xml.find("<title>") + 7;
    end = xml.find("</title>");
    if (start != std::string::npos && end != std::string::npos) {
        title = xml.substr(start, end - start);
    }
    
    // Extract album
    start = xml.find("<album>") + 7;
    end = xml.find("</album>");
    if (start != std::string::npos && end != std::string::npos) {
        album = xml.substr(start, end - start);
    }
    
    // Extract length
    start = xml.find("<length>") + 8;
    end = xml.find("</length>");
    if (start != std::string::npos && end != std::string::npos) {
        length = std::stoi(xml.substr(start, end - start));
    }
    
    // Extract timestamp
    start = xml.find("<timestamp>") + 11;
    end = xml.find("</timestamp>");
    if (start != std::string::npos && end != std::string::npos) {
        timestamp = std::stoll(xml.substr(start, end - start));
    }
    
    return Scrobble(artist, title, album, length, timestamp);
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
