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

namespace {
// RFC 3986 percent-decoding: "%XX" -> byte. Leaves malformed escapes verbatim.
std::string percentDecode(const std::string& in)
{
    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            int hi = hexVal(in[i + 1]);
            int lo = hexVal(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out += in[i];
    }
    return out;
}
} // namespace

/**
 * @brief Constructs a URI object by parsing a URI string.
 *
 * The parser handles common `file:` scheme variations (`file:///` and `file:/`) as well as generic `scheme://` formats. If no scheme is detected, it defaults to "file".
 * @param uri_string The full URI string to parse.
 */
URI::URI(const TagLib::String& uri_string)
{
    std::string s = uri_string.to8Bit(true);

    // file: URIs per RFC 8089. Accepted forms: file:///path, file://host/path,
    // and the legacy file:/path. The authority (host) component, when present,
    // must NOT be injected into the filesystem path — empty and "localhost"
    // both mean the local host. Paths are percent-decoded.
    if (s.rfind("file:", 0) == 0) {
        m_scheme = "file";
        std::string rest = s.substr(5); // everything after "file:"
        std::string path;
        if (rest.rfind("//", 0) == 0) {
            // Authority present: //[authority]/path. The authority runs from
            // after "//" to the next "/"; the path is the remainder.
            std::string after_authority = rest.substr(2);
            size_t path_start = after_authority.find('/');
            path = (path_start == std::string::npos) ? std::string("/")
                                                      : after_authority.substr(path_start);
        } else {
            // file:/path (no authority) or a relative file:path.
            path = rest;
        }
        m_path = TagLib::String(percentDecode(path), TagLib::String::UTF8);
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
