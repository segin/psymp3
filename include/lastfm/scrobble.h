/*
 * scrobble.h - Scrobble class header
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

#ifndef SCROBBLE_H
#define SCROBBLE_H

class Scrobble
{
public:
    explicit Scrobble(const track& rhs);
    Scrobble(const std::string& artist, const std::string& title, const std::string& album, int length, time_t timestamp);
    virtual ~Scrobble();
    
    // Copy constructor and assignment operator (needed for queue operations)
    Scrobble(const Scrobble& other) = default;
    Scrobble& operator=(const Scrobble& other) = default;
    
    // Move constructor and assignment operator
    Scrobble(Scrobble&& other) noexcept = default;
    Scrobble& operator=(Scrobble&& other) noexcept = default;
    
    // Accessors for track data
    std::string getArtist() const { return m_artist; }
    std::string getTitle() const { return m_title; }
    std::string getAlbum() const { return m_album; }
    int getLength() const { return m_length; }
    
    // Accessors for scrobble-specific data
    time_t getTimestamp() const { return m_timestamp; }
    void setTimestamp(time_t timestamp) { m_timestamp = timestamp; }
    
    // Helper methods for compatibility with existing code
    std::string getArtistStr() const { return m_artist; }
    std::string getTitleStr() const { return m_title; }
    std::string getAlbumStr() const { return m_album; }
    int GetLen() const { return m_length; } // For compatibility
    
    // Serialization for XML cache
    std::string toXML() const;
    static Scrobble fromXML(const std::string& xml);
    
    // Comparison operators
    bool operator==(const Scrobble& other) const;
    bool operator!=(const Scrobble& other) const;
    
private:
    std::string m_artist;
    std::string m_title;
    std::string m_album;
    int m_length;
    time_t m_timestamp;  // When the track was played (for scrobbling)
};

#endif // SCROBBLE_H
