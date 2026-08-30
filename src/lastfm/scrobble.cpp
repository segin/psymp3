/*
 * scrobble.cpp - class implementation for scrobble class
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
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

namespace PsyMP3 {
namespace LastFM {

Scrobble::Scrobble(const track& rhs) :
    // GetScrobbleArtist: first artist of a multi-valued credit — Last.fm has
    // no multi-artist model, and a joined string lands on a nonexistent
    // artist page. The full credit stays on screen via GetArtist().
    m_artist(rhs.GetScrobbleArtist().to8Bit(true)),
    m_title(rhs.GetTitle().to8Bit(true)),
    m_album(rhs.GetAlbum().to8Bit(true)),
    m_mbid(rhs.GetMusicBrainzID().to8Bit(true)),
    m_length(rhs.GetLen()),
    m_timestamp(time(nullptr))
{
    //ctor
}

Scrobble::Scrobble(const std::string& artist, const std::string& title, const std::string& album, int length, time_t timestamp,
                   const std::string& mbid) :
    m_artist(artist),
    m_title(title),
    m_album(album),
    m_mbid(mbid),
    m_length(length),
    m_timestamp(timestamp)
{
    //ctor
}

Scrobble::~Scrobble()
{
    //dtor
}

void Scrobble::appendXML(pugi::xml_node& parent) const
{
    pugi::xml_node node = parent.append_child("scrobble");
    node.append_child("artist").text().set(m_artist.c_str());
    node.append_child("title").text().set(m_title.c_str());
    node.append_child("album").text().set(m_album.c_str());
    if (!m_mbid.empty()) {
        node.append_child("mbid").text().set(m_mbid.c_str());
    }
    node.append_child("length").text().set(m_length);
    node.append_child("timestamp").text().set(static_cast<long long>(m_timestamp));
}

std::string Scrobble::toXML() const
{
    pugi::xml_document doc;
    appendXML(doc);
    std::ostringstream out;
    doc.save(out, "  ", pugi::format_default | pugi::format_no_declaration);
    return out.str();
}

Scrobble Scrobble::fromXMLNode(const pugi::xml_node& node)
{
    // as_int/as_llong yield 0 for missing or non-numeric text, which matches
    // the empty-sentinel contract (timestamp 0 marks the scrobble invalid).
    return Scrobble(node.child_value("artist"),
                    node.child_value("title"),
                    node.child_value("album"),
                    node.child("length").text().as_int(0),
                    static_cast<time_t>(node.child("timestamp").text().as_llong(0)),
                    node.child_value("mbid"));
}

Scrobble Scrobble::fromXML(const std::string& xml)
{
    pugi::xml_document doc;
    if (!doc.load_buffer(xml.data(), xml.size())) {
        std::cerr << "Failed to parse scrobble XML" << std::endl;
        return Scrobble("", "", "", 0, 0);
    }
    return fromXMLNode(doc.child("scrobble"));
}

bool Scrobble::operator==(const Scrobble& other) const
{
    return m_artist == other.m_artist &&
           m_title == other.m_title &&
           m_album == other.m_album &&
           m_mbid == other.m_mbid &&
           m_length == other.m_length &&
           m_timestamp == other.m_timestamp;
}

bool Scrobble::operator!=(const Scrobble& other) const
{
    return !(*this == other);
}

} // namespace LastFM
} // namespace PsyMP3
