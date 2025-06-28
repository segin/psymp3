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

track::track(TagLib::String a_FilePath, TagLib::String extinf_artist, TagLib::String extinf_title, long extinf_duration)
    : m_FilePath(a_FilePath), m_Len(0) // Initialize m_Len to 0, other strings are default constructed empty
{
    // Prioritize EXTINF data if provided
    if (!extinf_artist.isEmpty()) m_Artist = extinf_artist;
    if (!extinf_title.isEmpty()) m_Title = extinf_title;
    if (extinf_duration != 0) m_Len = extinf_duration;

    // Then attempt to load tags from TagLib, which will fill in missing info
    // and create m_FileRef if not already done.
    loadTags();
}

// Existing constructor, now calls loadTags() which handles prioritization
track::track(TagLib::String a_FilePath, TagLib::FileRef *a_FileRef) : m_FilePath(a_FilePath)
{
    // Take ownership of the raw pointer by moving it into a unique_ptr
    m_FileRef = std::unique_ptr<TagLib::FileRef>(a_FileRef);
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
