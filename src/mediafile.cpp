/*
 * mediafile.cpp - Modern media file factory implementation
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

bool MediaFile::exists(const TagLib::String& file) {
    auto filestring = 
#ifdef _WIN32
        file.toWString();
#else
        file.to8Bit(true);
#endif
    return true; // Simplified for now
}

std::vector<std::string> &MediaFile::split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> MediaFile::split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

Stream *MediaFile::open(TagLib::String name) {
    try {
        std::string uri = name.to8Bit(true);
        auto stream = MediaFactory::createStream(uri);
        
        // The MediaFactory returns unique_ptr, but MediaFile API expects raw pointer
        // This is for backward compatibility with existing code
        return stream.release();
        
    } catch (const UnsupportedMediaException& e) {
        throw InvalidMediaException(e.what());
    } catch (const ContentDetectionException& e) {
        throw InvalidMediaException(e.what());
    } catch (const std::exception& e) {
        throw InvalidMediaException("Failed to open media file: " + std::string(e.what()));
    }
}

Stream *MediaFile::openByMimeType(TagLib::String name, const std::string& mime_type) {
    try {
        std::string uri = name.to8Bit(true);
        auto stream = MediaFactory::createStreamWithMimeType(uri, mime_type);
        
        return stream.release();
        
    } catch (const UnsupportedMediaException& e) {
        throw InvalidMediaException(e.what());
    } catch (const ContentDetectionException& e) {
        throw InvalidMediaException(e.what());
    } catch (const std::exception& e) {
        throw InvalidMediaException("Failed to open media file: " + std::string(e.what()));
    }
}

std::string MediaFile::detectMimeType(TagLib::String name) {
    try {
        std::string uri = name.to8Bit(true);
        ContentInfo info = MediaFactory::analyzeContent(uri);
        return info.mime_type;
    } catch (const std::exception&) {
        return "";
    }
}

std::string MediaFile::extensionToMimeType(const std::string& extension) {
    return MediaFactory::extensionToMimeType(extension);
}

std::string MediaFile::mimeTypeToExtension(const std::string& mime_type) {
    return MediaFactory::mimeTypeToExtension(mime_type);
}

bool MediaFile::supportsExtension(const std::string& extension) {
    return MediaFactory::supportsExtension(extension);
}

bool MediaFile::supportsMimeType(const std::string& mime_type) {
    return MediaFactory::supportsMimeType(mime_type);
}

std::vector<std::string> MediaFile::getSupportedExtensions() {
    std::vector<std::string> all_extensions;
    auto formats = MediaFactory::getSupportedFormats();
    
    for (const auto& format : formats) {
        all_extensions.insert(all_extensions.end(), 
                            format.extensions.begin(), 
                            format.extensions.end());
    }
    
    return all_extensions;
}

std::vector<std::string> MediaFile::getSupportedMimeTypes() {
    std::vector<std::string> all_mime_types;
    auto formats = MediaFactory::getSupportedFormats();
    
    for (const auto& format : formats) {
        all_mime_types.insert(all_mime_types.end(), 
                            format.mime_types.begin(), 
                            format.mime_types.end());
    }
    
    return all_mime_types;
}