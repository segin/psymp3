/*
 * URI.cpp - Implementation for the URI parser class.
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
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
namespace IO {

/**
 * @brief Constructs a URI object by parsing a URI string.
 *
 * The parser handles common `file:` scheme variations (`file:///` and `file:/`) as well as generic `scheme://` formats. If no scheme is detected, it defaults to "file".
 * @param uri_string The full URI string to parse.
 */
URI::URI(const TagLib::String& uri_string)
{
    std::string s = uri_string.to8Bit(true);

    // Handle the common "file:///" case, which indicates a local file path.
    if (s.rfind("file:///", 0) == 0) {
        m_scheme = "file";
        m_path = TagLib::String(s.substr(7), TagLib::String::UTF8);
    }
    // Handle the older "file:/" case, also for local files.
    else if (s.rfind("file:/", 0) == 0) {
        m_scheme = "file";
        m_path = TagLib::String(s.substr(5), TagLib::String::UTF8);
    } else {
        // Fallback for other schemes (like http://, https://) or plain file paths.
        size_t scheme_end = s.find("://");
        if (scheme_end != std::string::npos) {
            m_scheme = TagLib::String(s.substr(0, scheme_end), TagLib::String::UTF8);
            m_path = TagLib::String(s.substr(scheme_end + 3), TagLib::String::UTF8);
        } else {
            // No scheme provided, assume it's a local file path.
            m_scheme = "file";
            m_path = uri_string;
        }
    }
}

/**
 * @brief Gets the scheme component of the URI.
 * @return A TagLib::String containing the scheme (e.g., "file").
 */
TagLib::String URI::scheme() const
{
    return m_scheme;
}

/**
 * @brief Gets the path component of the URI.
 * @return A TagLib::String containing the path.
 */
TagLib::String URI::path() const
{
    return m_path;
}

} // namespace IO
} // namespace PsyMP3
