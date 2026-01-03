/*
 * mediafile.cpp - Modern media file factory implementation
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

std::unique_ptr<Stream> MediaFile::open(TagLib::String name) {
    try {
        std::string uri = name.to8Bit(true);
        Debug::log("loader", "MediaFile::open called for file: ", uri);
        Debug::log("demuxer", "MediaFile::open called for file: ", uri);
        
        // Check if the file has an .ogg extension
        std::string ext;
        size_t dot_pos = uri.find_last_of('.');
        if (dot_pos != std::string::npos) {
            ext = uri.substr(dot_pos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == "ogg" || ext == "oga") {
                Debug::log("loader", "MediaFile::open detected Ogg file by extension: ", uri);
                Debug::log("ogg", "MediaFile::open detected Ogg file by extension: ", uri);
                Debug::log("demuxer", "MediaFile::open detected Ogg file by extension: ", uri);
            }
        }
        
        auto stream = PsyMP3::Demuxer::MediaFactory::createStream(uri);
        Debug::log("loader", "MediaFile::open successfully created stream for: ", uri);
        return stream;
        
    } catch (const PsyMP3::Demuxer::UnsupportedMediaException& e) {
        Debug::log("demuxer", "MediaFile::open UnsupportedMediaException: ", e.what());
        throw InvalidMediaException(e.what());
    } catch (const PsyMP3::Demuxer::ContentDetectionException& e) {
        Debug::log("demuxer", "MediaFile::open ContentDetectionException: ", e.what());
        throw InvalidMediaException(e.what());
    } catch (const std::exception& e) {
        Debug::log("demuxer", "MediaFile::open std::exception: ", e.what());
        throw InvalidMediaException("Failed to open media file: " + std::string(e.what()));
    }
}

std::unique_ptr<Stream> MediaFile::openByMimeType(TagLib::String name, const std::string& mime_type) {
    try {
        std::string uri = name.to8Bit(true);
        return PsyMP3::Demuxer::MediaFactory::createStreamWithMimeType(uri, mime_type);
        
    } catch (const PsyMP3::Demuxer::UnsupportedMediaException& e) {
        throw InvalidMediaException(e.what());
    } catch (const PsyMP3::Demuxer::ContentDetectionException& e) {
        throw InvalidMediaException(e.what());
    } catch (const std::exception& e) {
        throw InvalidMediaException("Failed to open media file: " + std::string(e.what()));
    }
    // Unreachable - all paths throw or return
    return nullptr;
}

std::string MediaFile::detectMimeType(TagLib::String name) {
    try {
        std::string uri = name.to8Bit(true);
        PsyMP3::Demuxer::ContentInfo info = PsyMP3::Demuxer::MediaFactory::analyzeContent(uri);
        return info.mime_type;
    } catch (const std::exception&) {
        return "";
    }
}

std::string MediaFile::extensionToMimeType(const std::string& extension) {
    return PsyMP3::Demuxer::MediaFactory::extensionToMimeType(extension);
}

std::string MediaFile::mimeTypeToExtension(const std::string& mime_type) {
    return PsyMP3::Demuxer::MediaFactory::mimeTypeToExtension(mime_type);
}

bool MediaFile::supportsExtension(const std::string& extension) {
    return PsyMP3::Demuxer::MediaFactory::supportsExtension(extension);
}

bool MediaFile::supportsMimeType(const std::string& mime_type) {
    return PsyMP3::Demuxer::MediaFactory::supportsMimeType(mime_type);
}

std::vector<std::string> MediaFile::getSupportedExtensions() {
    std::vector<std::string> all_extensions;
    auto formats = PsyMP3::Demuxer::MediaFactory::getSupportedFormats();
    
    for (const auto& format : formats) {
        all_extensions.insert(all_extensions.end(), 
                            format.extensions.begin(), 
                            format.extensions.end());
    }
    
    return all_extensions;
}

std::vector<std::string> MediaFile::getSupportedMimeTypes() {
    std::vector<std::string> all_mime_types;
    auto formats = PsyMP3::Demuxer::MediaFactory::getSupportedFormats();
    
    for (const auto& format : formats) {
        all_mime_types.insert(all_mime_types.end(), 
                            format.mime_types.begin(), 
                            format.mime_types.end());
    }
    
    return all_mime_types;
}