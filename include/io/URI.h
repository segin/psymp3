/*
 * URI.h - A class for parsing and handling Uniform Resource Identifiers.
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef URI_H
#define URI_H

namespace PsyMP3 {
namespace IO {

/**
 * @brief A simple class for parsing and handling Uniform Resource Identifiers (URIs).
 *
 * This class takes a string and splits it into a scheme (e.g., "file", "http")
 * and a path. It is designed to handle common file URI variations and defaults
 * to the "file" scheme for plain local paths.
 */
class URI
{
public:
    /**
     * @brief Constructs a URI object by parsing a URI string.
     * @param uri_string The full URI string to parse.
     */
    explicit URI(const TagLib::String& uri_string);

    /**
     * @brief Gets the scheme component of the URI.
     * @return A TagLib::String containing the scheme (e.g., "file").
     */
    TagLib::String scheme() const;
    /**
     * @brief Gets the path component of the URI.
     * @return A TagLib::String containing the path.
     */
    TagLib::String path() const;

private:
    TagLib::String m_scheme;
    TagLib::String m_path;
};

} // namespace IO
} // namespace PsyMP3

#endif // URI_H
