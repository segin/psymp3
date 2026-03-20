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

/**
 * @brief Checks whether the given media file path refers to an accessible file.
 *
 * Currently always returns `true` (simplified placeholder implementation).
 *
 * @param file Path to the media file.
 * @return `true` if the file is accessible.
 */
bool MediaFile::exists(const TagLib::String& file) {
    auto filestring = 
#ifdef _WIN32
        file.toWString();
#else
        file.to8Bit(true);
#endif
    return true; // Simplified for now
}

/**
 * @brief Splits a string by a delimiter, appending results to an existing vector.
 * @param s     The input string.
 * @param delim Delimiter character.
 * @param elems Output vector to which tokens are appended.
 * @return Reference to `elems`.
 */
std::vector<std::string> &MediaFile::split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

/**
 * @brief Splits a string by a delimiter and returns the result as a new vector.
 * @param s     The input string.
 * @param delim Delimiter character.
 * @return A vector of tokens.
 */
std::vector<std::string> MediaFile::split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

/**
 * @brief Opens a media file and returns a decoded `Stream`.
 *
 * Delegates to `PsyMP3::Demuxer::MediaFactory::createStream`. Logs the URI
 * to the `loader` and `demuxer` debug channels. Rethrows demuxer-specific
 * exceptions as `InvalidMediaException`.
 *
 * @param name URI or filesystem path of the media file to open.
 * @return An owning `unique_ptr` to the opened `Stream`.
 * @throws InvalidMediaException if the file cannot be opened or the format is unsupported.
 */
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

/**
 * @brief Opens a media file with an explicit MIME type hint.
 *
 * Bypasses format auto-detection and passes the supplied MIME type directly
 * to `MediaFactory::createStreamWithMimeType`.
 *
 * @param name      URI or filesystem path.
 * @param mime_type Explicit MIME type (e.g., `"audio/ogg"`).
 * @return An owning `unique_ptr` to the opened `Stream`.
 * @throws InvalidMediaException if the file cannot be opened.
 */
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

/**
 * @brief Determines the MIME type of a file by analysing its content.
 * @param name URI or filesystem path.
 * @return MIME type string, or an empty string if detection fails.
 */
std::string MediaFile::detectMimeType(TagLib::String name) {
    try {
        std::string uri = name.to8Bit(true);
        PsyMP3::Demuxer::ContentInfo info = PsyMP3::Demuxer::MediaFactory::analyzeContent(uri);
        return info.mime_type;
    } catch (const std::exception&) {
        return "";
    }
}

/**
 * @brief Maps a file extension to its canonical MIME type string.
 * @param extension File extension without the leading dot (e.g., `"mp3"`).
 * @return Corresponding MIME type, or an empty string if unknown.
 */
std::string MediaFile::extensionToMimeType(const std::string& extension) {
    return PsyMP3::Demuxer::MediaFactory::extensionToMimeType(extension);
}

/**
 * @brief Maps a MIME type to its canonical file extension.
 * @param mime_type MIME type string (e.g., `"audio/mpeg"`).
 * @return Corresponding file extension (without dot), or an empty string if unknown.
 */
std::string MediaFile::mimeTypeToExtension(const std::string& mime_type) {
    return PsyMP3::Demuxer::MediaFactory::mimeTypeToExtension(mime_type);
}

/**
 * @brief Checks whether the given file extension is supported by the demuxer framework.
 * @param extension File extension without the leading dot.
 * @return `true` if the extension is recognised.
 */
bool MediaFile::supportsExtension(const std::string& extension) {
    return PsyMP3::Demuxer::MediaFactory::supportsExtension(extension);
}

/**
 * @brief Checks whether the given MIME type is supported by the demuxer framework.
 * @param mime_type MIME type string.
 * @return `true` if the MIME type is recognised.
 */
bool MediaFile::supportsMimeType(const std::string& mime_type) {
    return PsyMP3::Demuxer::MediaFactory::supportsMimeType(mime_type);
}

/**
 * @brief Returns all file extensions supported by the demuxer framework.
 * @return A vector of extension strings (without leading dots).
 */
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

/**
 * @brief Returns all MIME types supported by the demuxer framework.
 * @return A vector of MIME type strings.
 */
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