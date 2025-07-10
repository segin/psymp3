/*
 * MediaFactory.cpp - Modern extensible media factory implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// Static member definitions
std::map<std::string, MediaFactory::FormatRegistration> MediaFactory::s_formats;
std::map<std::string, std::string> MediaFactory::s_extension_to_format;
std::map<std::string, std::string> MediaFactory::s_mime_to_format;
bool MediaFactory::s_initialized = false;

std::unique_ptr<Stream> MediaFactory::createStream(const std::string& uri) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    
    ContentInfo info = analyzeContent(uri);
    return createStreamWithContentInfo(uri, info);
}

std::unique_ptr<Stream> MediaFactory::createStreamWithMimeType(const std::string& uri, const std::string& mime_type) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    
    // Start with MIME type detection
    ContentInfo info = detectByMimeType(mime_type);
    info.mime_type = mime_type;
    
    // Enhance with file extension if available
    std::string ext = extractExtension(uri);
    if (!ext.empty()) {
        info.file_extension = ext;
    }
    
    // If MIME detection failed, fall back to full analysis
    if (info.detected_format.empty()) {
        info = analyzeContent(uri);
        info.mime_type = mime_type; // Preserve provided MIME type
    }
    
    return createStreamWithContentInfo(uri, info);
}

std::unique_ptr<Stream> MediaFactory::createStreamWithContentInfo(const std::string& uri, const ContentInfo& info) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    
    if (info.detected_format.empty()) {
        throw UnsupportedMediaException("Unable to determine media format for: " + uri);
    }
    
    auto it = s_formats.find(info.detected_format);
    if (it == s_formats.end()) {
        throw UnsupportedMediaException("Unsupported media format: " + info.detected_format);
    }
    
    try {
        return it->second.factory(uri, info);
    } catch (const std::exception& e) {
        throw UnsupportedMediaException("Failed to create stream for " + uri + ": " + e.what());
    }
}

ContentInfo MediaFactory::analyzeContent(const std::string& uri) {
    auto handler = createIOHandler(uri);
    return analyzeContent(handler);
}

ContentInfo MediaFactory::analyzeContent(std::unique_ptr<IOHandler>& handler) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    
    ContentInfo best_match;
    float best_confidence = 0.0f;
    
    // Try content-specific detectors first (highest confidence)
    for (const auto& [format_id, registration] : s_formats) {
        if (registration.detector) {
            auto result = registration.detector(handler);
            if (result && result->confidence > best_confidence) {
                best_match = *result;
                best_confidence = result->confidence;
            }
        }
    }
    
    // Try magic byte detection
    if (best_confidence < 0.8f) {
        auto magic_result = detectByMagicBytes(handler);
        if (magic_result.confidence > best_confidence) {
            best_match = magic_result;
            best_confidence = magic_result.confidence;
        }
    }
    
    return best_match;
}

void MediaFactory::registerFormat(const MediaFormat& format, StreamFactory factory) {
    FormatRegistration registration;
    registration.format = format;
    registration.factory = factory;
    
    s_formats[format.format_id] = registration;
    rebuildLookupTables();
}

void MediaFactory::registerContentDetector(const std::string& format_id, ContentDetector detector) {
    auto it = s_formats.find(format_id);
    if (it != s_formats.end()) {
        it->second.detector = detector;
    }
}

void MediaFactory::unregisterFormat(const std::string& format_id) {
    s_formats.erase(format_id);
    rebuildLookupTables();
}

std::vector<MediaFormat> MediaFactory::getSupportedFormats() {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    
    std::vector<MediaFormat> formats;
    for (const auto& [id, registration] : s_formats) {
        formats.push_back(registration.format);
    }
    return formats;
}

std::optional<MediaFormat> MediaFactory::getFormatInfo(const std::string& format_id) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    
    auto it = s_formats.find(format_id);
    if (it != s_formats.end()) {
        return it->second.format;
    }
    return std::nullopt;
}

bool MediaFactory::supportsFormat(const std::string& format_id) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    return s_formats.find(format_id) != s_formats.end();
}

bool MediaFactory::supportsExtension(const std::string& extension) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    return s_extension_to_format.find(ext) != s_extension_to_format.end();
}

bool MediaFactory::supportsMimeType(const std::string& mime_type) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    return s_mime_to_format.find(mime_type) != s_mime_to_format.end();
}

bool MediaFactory::supportsStreaming(const std::string& format_id) {
    auto format = getFormatInfo(format_id);
    return format && format->supports_streaming;
}

std::string MediaFactory::extensionToMimeType(const std::string& extension) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    
    auto it = s_extension_to_format.find(ext);
    if (it != s_extension_to_format.end()) {
        auto format_it = s_formats.find(it->second);
        if (format_it != s_formats.end() && !format_it->second.format.mime_types.empty()) {
            return format_it->second.format.mime_types[0];
        }
    }
    return "";
}

std::string MediaFactory::mimeTypeToExtension(const std::string& mime_type) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    
    auto it = s_mime_to_format.find(mime_type);
    if (it != s_mime_to_format.end()) {
        auto format_it = s_formats.find(it->second);
        if (format_it != s_formats.end() && !format_it->second.format.extensions.empty()) {
            return format_it->second.format.extensions[0];
        }
    }
    return "";
}

std::vector<std::string> MediaFactory::getExtensionsForMimeType(const std::string& mime_type) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    
    auto it = s_mime_to_format.find(mime_type);
    if (it != s_mime_to_format.end()) {
        auto format_it = s_formats.find(it->second);
        if (format_it != s_formats.end()) {
            return format_it->second.format.extensions;
        }
    }
    return {};
}

std::vector<std::string> MediaFactory::getMimeTypesForExtension(const std::string& extension) {
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    
    auto it = s_extension_to_format.find(ext);
    if (it != s_extension_to_format.end()) {
        auto format_it = s_formats.find(it->second);
        if (format_it != s_formats.end()) {
            return format_it->second.format.mime_types;
        }
    }
    return {};
}

std::string MediaFactory::extractExtension(const std::string& uri) {
    // Handle URLs and file paths
    std::string path = uri;
    
    // Remove query parameters and fragments
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path = path.substr(0, query_pos);
    }
    
    size_t fragment_pos = path.find('#');
    if (fragment_pos != std::string::npos) {
        path = path.substr(0, fragment_pos);
    }
    
    // Find last dot
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos < path.length() - 1) {
        std::string ext = path.substr(dot_pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
        return ext;
    }
    
    return "";
}

bool MediaFactory::isHttpUri(const std::string& uri) {
    // Support both HTTP and HTTPS
    return uri.substr(0, 7) == "http://" || uri.substr(0, 8) == "https://";
}

bool MediaFactory::isLocalFile(const std::string& uri) {
    return !isHttpUri(uri) && uri.find("://") == std::string::npos;
}

void MediaFactory::initializeDefaultFormats() {
    if (s_initialized) return;
    
    // MPEG Audio formats
    MediaFormat mp3_format;
    mp3_format.format_id = "mpeg_audio";
    mp3_format.display_name = "MPEG Audio";
    mp3_format.extensions = {"MP3", "MP2", "MPA"};
    mp3_format.mime_types = {"audio/mpeg", "audio/mp3"};
    mp3_format.magic_signatures = {"ID3", "\xFF\xFB", "\xFF\xFA"};
    mp3_format.priority = 10;
    mp3_format.supports_streaming = true;
    mp3_format.supports_seeking = true;
    mp3_format.description = "MPEG-1/2 Audio Layer II/III";
    
    registerFormat(mp3_format, [](const std::string& uri, const ContentInfo& info) {
        return std::make_unique<Libmpg123>(TagLib::String(uri.c_str()));
    });
    
    // FLAC format
    MediaFormat flac_format;
    flac_format.format_id = "flac";
    flac_format.display_name = "FLAC";
    flac_format.extensions = {"FLAC", "FLA"};
    flac_format.mime_types = {"audio/flac", "audio/x-flac"};
    flac_format.magic_signatures = {"fLaC"};
    flac_format.priority = 10;
    flac_format.supports_streaming = true;
    flac_format.supports_seeking = true;
    flac_format.description = "Free Lossless Audio Codec";
    
    registerFormat(flac_format, [](const std::string& uri, const ContentInfo& info) {
        return std::make_unique<Flac>(TagLib::String(uri.c_str()));
    });
    
    // Standalone Opus format (for .opus files that might not be in Ogg containers)
    MediaFormat opus_format;
    opus_format.format_id = "opus";
    opus_format.display_name = "Opus";
    opus_format.extensions = {"OPUS"};
    opus_format.mime_types = {"audio/opus"};
    opus_format.magic_signatures = {"OggS"}; // Opus files are typically in Ogg containers
    opus_format.priority = 15; // Higher priority than generic Ogg to catch .opus files first
    opus_format.supports_streaming = true;
    opus_format.supports_seeking = true;
    opus_format.description = "Opus Audio Codec";
    
    registerFormat(opus_format, [](const std::string& uri, const ContentInfo& info) {
        return std::make_unique<OpusFile>(TagLib::String(uri.c_str()));
    });
    
    // Ogg container formats (Vorbis, FLAC-in-Ogg)
    MediaFormat ogg_format;
    ogg_format.format_id = "ogg";
    ogg_format.display_name = "Ogg";
    ogg_format.extensions = {"OGG", "OGA"};
    ogg_format.mime_types = {"application/ogg", "audio/ogg", "audio/vorbis"};
    ogg_format.magic_signatures = {"OggS"};
    ogg_format.priority = 10;
    ogg_format.supports_streaming = true;
    ogg_format.supports_seeking = true;
    ogg_format.is_container = true;
    ogg_format.description = "Ogg container (Vorbis/FLAC)";
    
    registerFormat(ogg_format, [](const std::string& uri, const ContentInfo& info) {
        // Route all Ogg files through OggDemuxer for proper container parsing
        return std::make_unique<DemuxedStream>(TagLib::String(uri.c_str()));
    });
    
    // RIFF/WAVE formats
    MediaFormat wave_format;
    wave_format.format_id = "wave";
    wave_format.display_name = "WAVE";
    wave_format.extensions = {"WAV", "WAVE", "BWF"};
    wave_format.mime_types = {"audio/wav", "audio/wave", "audio/x-wav"};
    wave_format.magic_signatures = {"RIFF"};
    wave_format.priority = 10;
    wave_format.supports_streaming = true;
    wave_format.supports_seeking = true;
    wave_format.is_container = true;
    wave_format.description = "RIFF WAVE audio";
    
    registerFormat(wave_format, [](const std::string& uri, const ContentInfo& info) {
        return std::make_unique<ModernStream>(TagLib::String(uri.c_str()));
    });
    
    // AIFF formats
    MediaFormat aiff_format;
    aiff_format.format_id = "aiff";
    aiff_format.display_name = "AIFF";
    aiff_format.extensions = {"AIF", "AIFF", "AIFC"};
    aiff_format.mime_types = {"audio/aiff", "audio/x-aiff"};
    aiff_format.magic_signatures = {"FORM"};
    aiff_format.priority = 10;
    aiff_format.supports_streaming = true;
    aiff_format.supports_seeking = true;
    aiff_format.is_container = true;
    aiff_format.description = "Apple AIFF audio";
    
    registerFormat(aiff_format, [](const std::string& uri, const ContentInfo& info) {
        return std::make_unique<ModernStream>(TagLib::String(uri.c_str()));
    });
    
    // MP4/M4A formats
    MediaFormat mp4_format;
    mp4_format.format_id = "mp4";
    mp4_format.display_name = "MP4";
    mp4_format.extensions = {"MP4", "M4A", "M4B", "M4P"};
    mp4_format.mime_types = {"audio/mp4", "audio/m4a", "video/mp4"};
    mp4_format.magic_signatures = {"ftyp"};
    mp4_format.priority = 10;
    mp4_format.supports_streaming = true;
    mp4_format.supports_seeking = true;
    mp4_format.is_container = true;
    mp4_format.description = "ISO Base Media (MP4/M4A)";
    
    registerFormat(mp4_format, [](const std::string& uri, const ContentInfo& info) {
        return std::make_unique<ModernStream>(TagLib::String(uri.c_str()));
    });
    
    // Raw audio formats
    MediaFormat raw_format;
    raw_format.format_id = "raw_audio";
    raw_format.display_name = "Raw Audio";
    raw_format.extensions = {"PCM", "RAW", "AL", "ALAW", "UL", "ULAW", "MULAW", "AU", "SND"};
    raw_format.mime_types = {"audio/pcm", "audio/raw", "audio/alaw", "audio/ulaw", "audio/basic"};
    raw_format.priority = 90; // Lower priority since no magic signature
    raw_format.supports_streaming = true;
    raw_format.supports_seeking = true;
    raw_format.description = "Raw PCM/A-law/μ-law audio";
    
    registerFormat(raw_format, [](const std::string& uri, const ContentInfo& info) {
        return std::make_unique<ModernStream>(TagLib::String(uri.c_str()));
    });
    
    // Playlist formats
    MediaFormat playlist_format;
    playlist_format.format_id = "playlist";
    playlist_format.display_name = "Playlist";
    playlist_format.extensions = {"M3U", "M3U8"};
    playlist_format.mime_types = {"application/vnd.apple.mpegurl", "application/x-mpegurl", "audio/x-mpegurl"};
    playlist_format.priority = 50;
    playlist_format.supports_streaming = true;
    playlist_format.supports_seeking = false;
    playlist_format.description = "M3U/M3U8 playlists";
    
    registerFormat(playlist_format, [](const std::string& uri, const ContentInfo& info) {
        return std::make_unique<NullStream>(TagLib::String(uri.c_str()));
    });
    
    rebuildLookupTables();
    s_initialized = true;
}

void MediaFactory::rebuildLookupTables() {
    s_extension_to_format.clear();
    s_mime_to_format.clear();
    
    for (const auto& [format_id, registration] : s_formats) {
        // Build extension lookup
        for (const auto& ext : registration.format.extensions) {
            s_extension_to_format[ext] = format_id;
        }
        
        // Build MIME type lookup
        for (const auto& mime : registration.format.mime_types) {
            s_mime_to_format[mime] = format_id;
        }
    }
}

ContentInfo MediaFactory::detectByExtension(const std::string& uri) {
    ContentInfo info;
    std::string ext = extractExtension(uri);
    
    if (!ext.empty()) {
        auto it = s_extension_to_format.find(ext);
        if (it != s_extension_to_format.end()) {
            info.detected_format = it->second;
            info.file_extension = ext;
            info.confidence = 0.7f; // Medium confidence for extension-based detection
            
            // Set MIME type if available
            auto format_it = s_formats.find(it->second);
            if (format_it != s_formats.end() && !format_it->second.format.mime_types.empty()) {
                info.mime_type = format_it->second.format.mime_types[0];
            }
        }
    }
    
    return info;
}

ContentInfo MediaFactory::detectByMimeType(const std::string& mime_type) {
    ContentInfo info;
    
    auto it = s_mime_to_format.find(mime_type);
    if (it != s_mime_to_format.end()) {
        info.detected_format = it->second;
        info.mime_type = mime_type;
        info.confidence = 0.9f; // High confidence for MIME type detection
    }
    
    return info;
}

ContentInfo MediaFactory::detectByMagicBytes(std::unique_ptr<IOHandler>& handler) {
    ContentInfo info;
    
    if (!handler) return info;
    
    // Read first 16 bytes for magic detection
    uint8_t buffer[16];
    long original_pos = handler->tell();
    handler->seek(0, SEEK_SET);
    size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
    handler->seek(original_pos, SEEK_SET);
    
    if (bytes_read < 4) return info;
    
    float best_confidence = 0.0f;
    
    // Check against all registered formats
    for (const auto& [format_id, registration] : s_formats) {
        for (const auto& signature : registration.format.magic_signatures) {
            if (signature.length() <= bytes_read) {
                if (std::memcmp(buffer, signature.c_str(), signature.length()) == 0) {
                    if (registration.format.priority < 50) { // High priority formats
                        info.detected_format = format_id;
                        info.confidence = 0.95f;
                        best_confidence = 0.95f;
                        break;
                    } else if (0.8f > best_confidence) {
                        info.detected_format = format_id;
                        info.confidence = 0.8f;
                        best_confidence = 0.8f;
                    }
                }
            }
        }
        if (best_confidence >= 0.95f) break;
    }
    
    return info;
}

ContentInfo MediaFactory::detectByContentAnalysis(std::unique_ptr<IOHandler>& handler) {
    // Placeholder for advanced content analysis
    // This could include more sophisticated format detection
    ContentInfo info;
    return info;
}

std::unique_ptr<IOHandler> MediaFactory::createIOHandler(const std::string& uri) {
    if (isHttpUri(uri)) {
        return std::make_unique<HTTPIOHandler>(uri);
    } else {
        return std::make_unique<FileIOHandler>(uri);
    }
}