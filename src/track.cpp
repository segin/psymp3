/*
 * track.cpp - class implementation for track class
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

TagLib::String track::nullstr;

track::track(const TagLib::String& a_FilePath, const TagLib::String& extinf_artist, const TagLib::String& extinf_title, long extinf_duration)
    : m_FilePath(a_FilePath), m_Len(extinf_duration)
{
    // Prioritize EXTINF data if provided
    if (!extinf_artist.isEmpty()) m_Artist = extinf_artist;
    if (!extinf_title.isEmpty()) m_Title = extinf_title;

    // Then attempt to load tags from TagLib, which will fill in missing info
    // and create m_FileRef if not already done.
    loadTags();
}

void track::loadTags() { 
    if (!m_FileRef) {
        try {
            #ifdef _WIN32
                // On Windows, TagLib prefers native paths with backslashes.
                // We convert to a wide string and replace slashes before creating the FileRef.
                std::wstring wpath = m_FilePath.toWString();
                std::replace(wpath.begin(), wpath.end(), L'/', L'\\');
                m_FileRef = std::make_unique<TagLib::FileRef>(wpath.c_str());
            #else
                m_FileRef = std::make_unique<TagLib::FileRef>(m_FilePath.toCString(true));
            #endif
        } catch (std::exception& e) {
            std::cerr << "track::track(): Exception: " << e.what() << std::endl;
            m_FileRef = nullptr;
        }
    }
    if (m_FileRef && m_FileRef->tag() && m_FileRef->audioProperties()) {
        // Only set if not already set by EXTINF data
        if (m_Artist.isEmpty()) m_Artist = m_FileRef->tag()->artist();
        if (m_Title.isEmpty()) m_Title = m_FileRef->tag()->title();
        
        // Always get album from TagLib as it's not part of EXTINF
        m_Album = m_FileRef->tag()->album();
        
        // Only set length if not already set by EXTINF data
        if (m_Len == 0) m_Len = m_FileRef->audioProperties()->lengthInSeconds();
    }
}
