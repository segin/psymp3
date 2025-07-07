/*
 * mediafile.h - Header for MediaFile namespace
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
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

#ifndef MEDIAFILE_H
#define MEDIAFILE_H

namespace MediaFile {
    // Primary factory methods
    Stream *open(TagLib::String name);
    Stream *openByMimeType(TagLib::String name, const std::string& mime_type);
    
    // MIME type detection and mapping
    std::string detectMimeType(TagLib::String name);
    std::string extensionToMimeType(const std::string& extension);
    std::string mimeTypeToExtension(const std::string& mime_type);
    
    // Utility functions
    std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
    std::vector<std::string> split(const std::string &s, char delim);
    bool exists(const TagLib::String& file);
    
    // Format support queries
    bool supportsExtension(const std::string& extension);
    bool supportsMimeType(const std::string& mime_type);
    std::vector<std::string> getSupportedExtensions();
    std::vector<std::string> getSupportedMimeTypes();
}

#endif // MEDIAFILE_H
