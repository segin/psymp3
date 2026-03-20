/*
 * MediaFactory.cpp - Modern extensible media factory implementation
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "tag/ID3v2Tag.h"

namespace PsyMP3 {
namespace Demuxer {

// Static member definitions
std::map<std::string, MediaFactory::FormatRegistration> MediaFactory::s_formats;
std::map<std::string, std::string> MediaFactory::s_extension_to_format;
std::map<std::string, std::string> MediaFactory::s_mime_to_format;
bool MediaFactory::s_initialized = false;
std::mutex MediaFactory::s_factory_mutex; // Thread safety for factory operations
static std::mutex s_init_mutex; // Separate mutex for initialization

std::unique_ptr<Stream> MediaFactory::createStream(const std::string& uri) {
    // Thread-safe initialization check with double-checked locking
    if (!s_initialized) {
        std::lock_guard<std::mutex> init_lock(s_init_mutex);
        if (!s_initialized) {
            Debug::log("loader", "MediaFactory::createStream initializing default formats");
            initializeDefaultFormats();
            s_initialized = true;
        }
    }
    
    Debug::log("loader", "MediaFactory::createStream analyzing content for: ", uri);
    ContentInfo info = analyzeContent(uri);
    Debug::log("loader", "MediaFactory::createStream detected format: ", 
               info.detected_format.empty() ? "unknown" : info.detected_format,
               ", confidence: ", info.confidence);
    
    return createStreamWithContentInfo(uri, info);
}

std::unique_ptr<Stream> MediaFactory::createStreamWithMimeType(const std::string& uri, const std::string& mime_type) {
    // Thread-safe initialization check with double-checked locking
    if (!s_initialized) {
        std::lock_guard<std::mutex> init_lock(s_init_mutex);
        if (!s_initialized) {
            initializeDefaultFormats();
            s_initialized = true;
        }
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
    // Thread-safe initialization check with double-checked locking
    if (!s_initialized) {
        std::lock_guard<std::mutex> init_lock(s_init_mutex);
        if (!s_initialized) {
            Debug::log("loader", "MediaFactory::createStreamWithContentInfo initializing default formats");
            try {
                initializeDefaultFormats();
                s_initialized = true;
            } catch (const std::exception& e) {
                Debug::log("loader", "MediaFactory::createStreamWithContentInfo failed to initialize formats: ", e.what());
                throw UnsupportedMediaException("Failed to initialize media formats: " + std::string(e.what()));
            }
        }
    }
    
    // Validate input parameters
    if (uri.empty()) {
        Debug::log("loader", "MediaFactory::createStreamWithContentInfo empty URI provided");
        throw UnsupportedMediaException("Empty URI provided");
    }
    
    if (info.detected_format.empty()) {
        Debug::log("loader", "MediaFactory::createStreamWithContentInfo unable to determine format for: ", uri);
        throw UnsupportedMediaException("Unable to determine media format for: " + uri);
    }
    
    Debug::log("loader", "MediaFactory::createStreamWithContentInfo looking for format handler: ", info.detected_format);
    
    // Thread-safe format lookup
    StreamFactory factory_func;
    {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        auto it = s_formats.find(info.detected_format);
        if (it == s_formats.end()) {
            Debug::log("loader", "MediaFactory::createStreamWithContentInfo unsupported format: ", info.detected_format);
            throw UnsupportedMediaException("Unsupported media format: " + info.detected_format);
        }
        
        // Validate format registration
        if (!it->second.factory) {
            Debug::log("loader", "MediaFactory::createStreamWithContentInfo no factory function for format: ", info.detected_format);
            throw UnsupportedMediaException("No factory function registered for format: " + info.detected_format);
        }
        
        factory_func = it->second.factory;
    }
    
    try {
        Debug::log("loader", "MediaFactory::createStreamWithContentInfo creating stream for format: ", info.detected_format);
        auto stream = factory_func(uri, info);
        
        if (!stream) {
            Debug::log("loader", "MediaFactory::createStreamWithContentInfo factory returned null stream for format: ", info.detected_format);
            throw UnsupportedMediaException("Factory returned null stream for format: " + info.detected_format);
        }
        
        Debug::log("loader", "MediaFactory::createStreamWithContentInfo successfully created stream for format: ", info.detected_format);
        return stream;
        
    } catch (const std::bad_alloc& e) {
        Debug::log("loader", "MediaFactory::createStreamWithContentInfo memory allocation failed for: ", uri);
        throw UnsupportedMediaException("Memory allocation failed creating stream for " + uri);
    } catch (const UnsupportedMediaException&) {
        // Re-throw media exceptions as-is
        throw;
    } catch (const std::exception& e) {
        Debug::log("loader", "MediaFactory::createStreamWithContentInfo exception: ", e.what());
        throw UnsupportedMediaException("Failed to create stream for " + uri + ": " + e.what());
    } catch (...) {
        Debug::log("loader", "MediaFactory::createStreamWithContentInfo unknown exception for: ", uri);
        throw UnsupportedMediaException("Unknown error creating stream for " + uri);
    }
}

ContentInfo MediaFactory::analyzeContent(const std::string& uri) {
    // Thread-safe initialization check with double-checked locking
    if (!s_initialized) {
        std::lock_guard<std::mutex> init_lock(s_init_mutex);
        if (!s_initialized) {
            Debug::log("loader", "MediaFactory::analyzeContent initializing default formats");
            initializeDefaultFormats();
            s_initialized = true;
        }
    }
    
    ContentInfo best_match;
    float best_confidence = 0.0f;
    
    // Start with extension-based detection (quick and often accurate)
    Debug::log("loader", "MediaFactory::analyzeContent trying extension-based detection for: ", uri);
    auto ext_result = detectByExtension(uri);
    if (ext_result.confidence > best_confidence) {
        best_match = ext_result;
        best_confidence = ext_result.confidence;
        Debug::log("loader", "MediaFactory::analyzeContent extension detection found format: ", 
                  ext_result.detected_format, ", confidence: ", ext_result.confidence);
    }
    
    // Try content analysis with IOHandler for more detailed detection
    Debug::log("loader", "MediaFactory::analyzeContent trying content-based detection for: ", uri);
    auto handler = createIOHandler(uri);
    auto content_result = analyzeContent(handler);
    
    // Combine results - prefer higher confidence, but merge metadata
    if (content_result.confidence > best_confidence) {
        best_match = content_result;
        // Preserve extension info if we had it
        if (!ext_result.file_extension.empty()) {
            best_match.file_extension = ext_result.file_extension;
        }
    } else if (!best_match.detected_format.empty()) {
        // Keep extension-based result but enhance with any metadata from content analysis
        if (!content_result.mime_type.empty() && best_match.mime_type.empty()) {
            best_match.mime_type = content_result.mime_type;
        }
        for (const auto& [key, value] : content_result.metadata) {
            if (best_match.metadata.find(key) == best_match.metadata.end()) {
                best_match.metadata[key] = value;
            }
        }
    }
    
    return best_match;
}

ContentInfo MediaFactory::analyzeContent(std::unique_ptr<IOHandler>& handler) {
    // Thread-safe initialization check with double-checked locking
    if (!s_initialized) {
        std::lock_guard<std::mutex> init_lock(s_init_mutex);
        if (!s_initialized) {
            initializeDefaultFormats();
            s_initialized = true;
        }
    }
    
    ContentInfo best_match;
    float best_confidence = 0.0f;
    
    // Try content-specific detectors first (highest confidence) - thread-safe
    // Copy detectors to avoid holding factory mutex while calling them
    std::vector<std::pair<std::string, ContentDetector>> detectors;
    {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        for (const auto& [format_id, registration] : s_formats) {
            if (registration.detector) {
                detectors.emplace_back(format_id, registration.detector);
            }
        }
    }
    
    // Call detectors without holding factory mutex to avoid deadlocks
    for (const auto& [format_id, detector] : detectors) {
        auto result = detector(handler);
        if (result && result->confidence > best_confidence) {
            best_match = *result;
            best_confidence = result->confidence;
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
    
    // Try advanced content analysis as fallback
    if (best_confidence < 0.5f) {
        auto content_result = detectByContentAnalysis(handler);
        if (content_result.confidence > best_confidence) {
            best_match = content_result;
            best_confidence = content_result.confidence;
        }
    }
    
    return best_match;
}

void MediaFactory::registerFormat(const MediaFormat& format, StreamFactory factory) {
    std::lock_guard<std::mutex> lock(s_factory_mutex);
    registerFormatInternal(format, factory);
}

void MediaFactory::registerFormatInternal(const MediaFormat& format, StreamFactory factory) {
    // Assumes s_factory_mutex is already held
    FormatRegistration registration;
    registration.format = format;
    registration.factory = factory;
    
    s_formats[format.format_id] = registration;
    rebuildLookupTables();
}

void MediaFactory::registerContentDetector(const std::string& format_id, ContentDetector detector) {
    std::lock_guard<std::mutex> lock(s_factory_mutex);
    registerContentDetectorInternal(format_id, detector);
}

void MediaFactory::registerContentDetectorInternal(const std::string& format_id, ContentDetector detector) {
    // Assumes s_factory_mutex is already held
    auto it = s_formats.find(format_id);
    if (it != s_formats.end()) {
        it->second.detector = detector;
    }
}

void MediaFactory::unregisterFormat(const std::string& format_id) {
    std::lock_guard<std::mutex> lock(s_factory_mutex);
    s_formats.erase(format_id);
    rebuildLookupTables();
}

std::vector<MediaFormat> MediaFactory::getSupportedFormats() {
    std::lock_guard<std::mutex> lock(s_factory_mutex);
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
    std::lock_guard<std::mutex> lock(s_factory_mutex);
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
    std::lock_guard<std::mutex> lock(s_factory_mutex);
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    return s_formats.find(format_id) != s_formats.end();
}

bool MediaFactory::supportsExtension(const std::string& extension) {
    std::lock_guard<std::mutex> lock(s_factory_mutex);
    if (!s_initialized) {
        initializeDefaultFormats();
    }
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    return s_extension_to_format.find(ext) != s_extension_to_format.end();
}

bool MediaFactory::supportsMimeType(const std::string& mime_type) {
    std::lock_guard<std::mutex> lock(s_factory_mutex);
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
    std::lock_guard<std::mutex> lock(s_factory_mutex);
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
    std::lock_guard<std::mutex> lock(s_factory_mutex);
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
    std::lock_guard<std::mutex> lock(s_factory_mutex);
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
    std::lock_guard<std::mutex> lock(s_factory_mutex);
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
    
    Debug::log("loader", "MediaFactory::extractExtension processing URI: ", uri);
    
    // Remove query parameters and fragments
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path = path.substr(0, query_pos);
        Debug::log("loader", "MediaFactory::extractExtension removed query parameters: ", path);
    }
    
    size_t fragment_pos = path.find('#');
    if (fragment_pos != std::string::npos) {
        path = path.substr(0, fragment_pos);
        Debug::log("loader", "MediaFactory::extractExtension removed fragment: ", path);
    }
    
    // Find last dot
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos < path.length() - 1) {
        std::string ext = path.substr(dot_pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
        Debug::log("loader", "MediaFactory::extractExtension found extension: ", ext);
        return ext;
    }
    
    Debug::log("loader", "MediaFactory::extractExtension no extension found");
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
    // Note: This method should only be called while holding s_factory_mutex
    if (s_initialized) {
        Debug::log("loader", "MediaFactory::initializeDefaultFormats already initialized");
        return;
    }
    
    Debug::log("loader", "MediaFactory::initializeDefaultFormats initializing formats");
    
    // Register all codecs and demuxers with their respective registries
    registerAllCodecs();
    registerAllDemuxers();
    
    // Register formats based on what's available in the registries
    // This replaces hardcoded conditional compilation with registry lookups
    
#ifdef HAVE_MP3
    // MPEG Audio formats - MP3 uses legacy Stream architecture
    MediaFormat mp3_format;
    mp3_format.format_id = "mpeg_audio";
    mp3_format.display_name = "MPEG Audio";
    // Enhanced extension mappings per requirements - add MP2, .bit, .mpga extensions
    mp3_format.extensions = {"MP3", "MP2", "MPA", "MPGA", "BIT", "M2A", "MP2A"};
    // Comprehensive MIME type support for MPEG audio
    mp3_format.mime_types = {"audio/mpeg", "audio/mp3", "audio/x-mp3", "audio/mpeg3", "audio/x-mpeg", "audio/x-mpeg-3"};
    mp3_format.magic_signatures = {"ID3", "\xFF\xFB", "\xFF\xFA"};
    mp3_format.priority = 10;
    mp3_format.supports_streaming = true;
    mp3_format.supports_seeking = true;
    mp3_format.description = "MPEG-1/2 Audio Layer II/III";
    
    // MP3 uses legacy Stream architecture via Libmpg123 codec
    registerFormatInternal(mp3_format, [](const std::string& uri, const ContentInfo& info) {
        Debug::log("loader", "MediaFactory: Creating Libmpg123 stream for MP3 file: ", uri);
        Debug::log("mp3", "MediaFactory: Creating Libmpg123 stream for MP3 file: ", uri);
        return std::make_unique<PsyMP3::Codec::MP3::Libmpg123>(TagLib::String(uri.c_str()));
    });
#endif
    
#ifdef HAVE_FLAC
    // FLAC format - uses legacy Stream architecture
    MediaFormat flac_format;
    flac_format.format_id = "flac";
    flac_format.display_name = "FLAC";
    flac_format.extensions = {"FLAC"};
    // Comprehensive MIME type support for FLAC
    flac_format.mime_types = {"audio/flac", "audio/x-flac", "application/x-flac"};
    flac_format.magic_signatures = {"fLaC"};
    flac_format.priority = 10;
    flac_format.supports_streaming = true;
    flac_format.supports_seeking = true;
    flac_format.description = "Free Lossless Audio Codec";
    
    // Check if FLACDemuxer is available in registry for enhanced RFC 9639 compliance
    if (DemuxerRegistry::getInstance().isFormatSupported("flac")) {
        Debug::log("loader", "MediaFactory: Using enhanced FLACDemuxer with RFC 9639 compliance for FLAC files");
        registerFormatInternal(flac_format, [](const std::string& uri, const ContentInfo& info) {
            Debug::log("loader", "MediaFactory: Creating DemuxedStream for FLAC file: ", uri);
            Debug::log("demuxer", "MediaFactory: Creating DemuxedStream for FLAC file: ", uri);
            Debug::log("flac", "MediaFactory: Creating DemuxedStream for FLAC file: ", uri);
            
            // Route FLAC files through enhanced FLACDemuxer for RFC 9639 compliant container parsing
            // This ensures proper frame boundary detection, CRC validation, and error recovery
            return std::make_unique<DemuxedStream>(TagLib::String(uri.c_str()));
        });
    }
#ifndef HAVE_NATIVE_FLAC
    // Legacy Flac class fallback (only available when using libFLAC wrapper)
    else {
        Debug::log("loader", "MediaFactory: Using legacy Flac codec for FLAC files (RFC 9639 compliance limited)");
        registerFormatInternal(flac_format, [](const std::string& uri, const ContentInfo& info) {
            return std::make_unique<Flac>(TagLib::String(uri.c_str()));
        });
    }
#endif
#endif
    
    // Register container formats that use demuxer registry
    if (DemuxerRegistry::getInstance().isFormatSupported("ogg")) {
        Debug::log("demuxer", "MediaFactory: Registering Ogg format (OggDemuxer available in registry)");
        
        // Ogg container formats - standardized extension mappings per requirements
        MediaFormat ogg_format;
        ogg_format.format_id = "ogg";
        ogg_format.display_name = "Ogg";
        ogg_format.extensions = {"OGG", "OGA", "OPUS"}; // Added OPUS per requirements
        // Comprehensive MIME type support for web-served files
        ogg_format.mime_types = {"application/ogg", "audio/ogg", "audio/opus", "audio/vorbis", "audio/x-vorbis", "audio/x-ogg"};
        ogg_format.magic_signatures = {"OggS"};
        ogg_format.priority = 10;
        ogg_format.supports_streaming = true;
        ogg_format.supports_seeking = true;
        ogg_format.is_container = true;
        ogg_format.description = "Ogg container (Vorbis/Opus/FLAC)";
        
        registerFormatInternal(ogg_format, [](const std::string& uri, const ContentInfo& info) {
            Debug::log("loader", "MediaFactory: Creating DemuxedStream for Ogg file: ", uri);
            Debug::log("demuxer", "MediaFactory: Creating DemuxedStream for Ogg file: ", uri);
            Debug::log("ogg", "MediaFactory: Creating DemuxedStream for Ogg file: ", uri);
            // Route all Ogg files through OggDemuxer for proper container parsing
            return std::make_unique<DemuxedStream>(TagLib::String(uri.c_str()));
        });
        
        // Register enhanced content detector for Ogg files
        registerContentDetectorInternal("ogg", [](std::unique_ptr<IOHandler>& handler) -> std::optional<ContentInfo> {
            if (!handler) return std::nullopt;
            
            uint8_t buffer[512];
            long original_pos = handler->tell();
            handler->seek(0, SEEK_SET);
            size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
            handler->seek(original_pos, SEEK_SET);
            
            if (bytes_read < 4) return std::nullopt;
            
            // Check for Ogg signature
            if (buffer[0] == 'O' && buffer[1] == 'g' && buffer[2] == 'g' && buffer[3] == 'S') {
                ContentInfo info;
                info.detected_format = "ogg";
                info.confidence = 0.95f;
                
                // Detect specific codec for better routing
                std::string codec = probeOggCodec(buffer, bytes_read);
                if (!codec.empty()) {
                    info.metadata["ogg_codec"] = codec;
                    info.confidence = 0.98f; // Higher confidence with codec detection
                    
                    if (codec == "opus") {
                        info.metadata["routing_fix"] = "opus_to_ogg";
                        Debug::log("loader", "MediaFactory: Ogg content detector fixed Opus routing");
                    }
                }
                
                return info;
            }
            
            return std::nullopt;
        });
    }
    
    // Register container formats using demuxer registry
    if (DemuxerRegistry::getInstance().isFormatSupported("riff")) {
        // RIFF/WAVE formats - standardized extension mappings
        MediaFormat wave_format;
        wave_format.format_id = "wave";
        wave_format.display_name = "WAVE";
        wave_format.extensions = {"WAV", "WAVE", "BWF"};
        // Comprehensive MIME type support for WAVE
        wave_format.mime_types = {"audio/wav", "audio/wave", "audio/x-wav", "audio/vnd.wave", "audio/x-pn-wav"};
        wave_format.magic_signatures = {"RIFF"};
        wave_format.priority = 10;
        wave_format.supports_streaming = true;
        wave_format.supports_seeking = true;
        wave_format.is_container = true;
        wave_format.description = "RIFF WAVE audio";
        
        registerFormatInternal(wave_format, [](const std::string& uri, const ContentInfo& info) {
            return std::make_unique<ModernStream>(TagLib::String(uri.c_str()));
        });
    }
    
    if (DemuxerRegistry::getInstance().isFormatSupported("aiff")) {
        // AIFF formats - standardized extension mappings
        MediaFormat aiff_format;
        aiff_format.format_id = "aiff";
        aiff_format.display_name = "AIFF";
        aiff_format.extensions = {"AIF", "AIFF", "AIFC"};
        // Comprehensive MIME type support for AIFF
        aiff_format.mime_types = {"audio/aiff", "audio/x-aiff", "audio/aif", "sound/aiff"};
        aiff_format.magic_signatures = {"FORM"};
        aiff_format.priority = 10;
        aiff_format.supports_streaming = true;
        aiff_format.supports_seeking = true;
        aiff_format.is_container = true;
        aiff_format.description = "Apple AIFF audio";
        
        registerFormatInternal(aiff_format, [](const std::string& uri, const ContentInfo& info) {
            return std::make_unique<ModernStream>(TagLib::String(uri.c_str()));
        });
    }
    
    if (DemuxerRegistry::getInstance().isFormatSupported("mp4")) {
        // ISO container formats - standardized extension mappings per requirements
        MediaFormat mp4_format;
        mp4_format.format_id = "mp4";
        mp4_format.display_name = "MP4";
        mp4_format.extensions = {"MOV", "MP4", "M4A", "3GP"}; // Standardized per requirements
        // Comprehensive MIME type support for ISO containers
        mp4_format.mime_types = {"audio/mp4", "audio/m4a", "video/mp4", "video/quicktime", "audio/x-m4a", "video/x-mp4"};
        mp4_format.magic_signatures = {"ftyp"};
        mp4_format.priority = 10;
        mp4_format.supports_streaming = true;
        mp4_format.supports_seeking = true;
        mp4_format.is_container = true;
        mp4_format.description = "ISO Base Media (MP4/M4A)";
        
        registerFormatInternal(mp4_format, [](const std::string& uri, const ContentInfo& info) {
            return std::make_unique<ModernStream>(TagLib::String(uri.c_str()));
        });
    }
    
    if (DemuxerRegistry::getInstance().isFormatSupported("raw_audio")) {
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
        
        registerFormatInternal(raw_format, [](const std::string& uri, const ContentInfo& info) {
            return std::make_unique<ModernStream>(TagLib::String(uri.c_str()));
        });
    }
    
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
    
    registerFormatInternal(playlist_format, [](const std::string& uri, const ContentInfo& info) {
        return std::make_unique<NullStream>(TagLib::String(uri.c_str()));
    });
    
    rebuildLookupTables();
}

void MediaFactory::rebuildLookupTables() {
    Debug::log("loader", "MediaFactory::rebuildLookupTables rebuilding lookup tables");
    
    s_extension_to_format.clear();
    s_mime_to_format.clear();
    
    for (const auto& [format_id, registration] : s_formats) {
        // Build extension lookup
        for (const auto& ext : registration.format.extensions) {
            s_extension_to_format[ext] = format_id;
            Debug::log("loader", "MediaFactory::rebuildLookupTables registered extension ", ext, " -> ", format_id);
        }
        
        // Build MIME type lookup
        for (const auto& mime : registration.format.mime_types) {
            s_mime_to_format[mime] = format_id;
            Debug::log("loader", "MediaFactory::rebuildLookupTables registered MIME type ", mime, " -> ", format_id);
        }
    }
}

ContentInfo MediaFactory::detectByExtension(const std::string& uri) {
    ContentInfo info;
    std::string ext = extractExtension(uri);
    
    Debug::log("loader", "MediaFactory::detectByExtension extracted extension: ", ext.empty() ? "none" : ext);
    
    if (!ext.empty()) {
        std::lock_guard<std::mutex> lock(s_factory_mutex);
        auto it = s_extension_to_format.find(ext);
        if (it != s_extension_to_format.end()) {
            info.detected_format = it->second;
            info.file_extension = ext;
            info.confidence = 0.7f; // Medium confidence for extension-based detection
            
            Debug::log("loader", "MediaFactory::detectByExtension matched extension ", ext, " to format: ", it->second);
            
            // Set MIME type if available
            auto format_it = s_formats.find(it->second);
            if (format_it != s_formats.end() && !format_it->second.format.mime_types.empty()) {
                info.mime_type = format_it->second.format.mime_types[0];
                Debug::log("loader", "MediaFactory::detectByExtension set MIME type: ", info.mime_type);
            }
        } else {
            Debug::log("loader", "MediaFactory::detectByExtension no format registered for extension: ", ext);
        }
    }
    
    return info;
}

ContentInfo MediaFactory::detectByMimeType(const std::string& mime_type) {
    ContentInfo info;
    
    std::lock_guard<std::mutex> lock(s_factory_mutex);
    auto it = s_mime_to_format.find(mime_type);
    if (it != s_mime_to_format.end()) {
        info.detected_format = it->second;
        info.mime_type = mime_type;
        info.confidence = 0.9f; // High confidence for MIME type detection
    }
    
    return info;
}

std::string MediaFactory::probeOggCodec(const uint8_t* buffer, size_t buffer_size) {
    // Enhanced Ogg codec probing with improved signature detection
    if (buffer_size < 32) return "";
    
    // Look for Ogg pages and probe for codec signatures within them
    for (size_t i = 0; i < buffer_size - 4; i++) {
        if (buffer[i] == 'O' && buffer[i+1] == 'g' && 
            buffer[i+2] == 'g' && buffer[i+3] == 'S') {
            
            // Found Ogg page, look for codec signatures in the payload
            size_t search_end = std::min(i + 256, buffer_size); // Search within reasonable range
            
            // Check for Opus signature (OpusHead) - highest priority to fix routing issues
            for (size_t j = i + 4; j < search_end - 7; j++) {
                if (buffer[j] == 'O' && buffer[j+1] == 'p' && buffer[j+2] == 'u' &&
                    buffer[j+3] == 's' && buffer[j+4] == 'H' && buffer[j+5] == 'e' &&
                    buffer[j+6] == 'a' && buffer[j+7] == 'd') {
                    Debug::log("loader", "MediaFactory::probeOggCodec detected Opus codec");
                    return "opus";
                }
            }
            
            // Check for Vorbis signature (\x01vorbis)
            for (size_t j = i + 4; j < search_end - 6; j++) {
                if (buffer[j] == 0x01 && buffer[j+1] == 'v' && buffer[j+2] == 'o' &&
                    buffer[j+3] == 'r' && buffer[j+4] == 'b' && buffer[j+5] == 'i' &&
                    buffer[j+6] == 's') {
                    Debug::log("loader", "MediaFactory::probeOggCodec detected Vorbis codec");
                    return "vorbis";
                }
            }
            
            // Check for FLAC in Ogg signature (\x7FFLAC)
            for (size_t j = i + 4; j < search_end - 4; j++) {
                if (buffer[j] == 0x7F && buffer[j+1] == 'F' && 
                    buffer[j+2] == 'L' && buffer[j+3] == 'A' && buffer[j+4] == 'C') {
                    Debug::log("loader", "MediaFactory::probeOggCodec detected FLAC codec");
                    return "flac";
                }
            }
            
            // Check for Speex signature (Speex   )
            for (size_t j = i + 4; j < search_end - 7; j++) {
                if (buffer[j] == 'S' && buffer[j+1] == 'p' && buffer[j+2] == 'e' &&
                    buffer[j+3] == 'e' && buffer[j+4] == 'x' && buffer[j+5] == ' ' &&
                    buffer[j+6] == ' ' && buffer[j+7] == ' ') {
                    Debug::log("loader", "MediaFactory::probeOggCodec detected Speex codec");
                    return "speex";
                }
            }
        }
    }
    
    return ""; // No codec detected
}


ContentInfo MediaFactory::detectByMagicBytes(std::unique_ptr<IOHandler>& handler) {
    ContentInfo info;
    
    if (!handler) return info;
    
    // Read larger buffer for improved magic detection with codec-specific probing
    uint8_t buffer[512];
    long original_pos = handler->tell();
    handler->seek(0, SEEK_SET);
    size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
    
    if (bytes_read < 4) {
        handler->seek(original_pos, SEEK_SET);
        return info;
    }
    
    // Check for ID3v2 tag and skip past it to find actual audio format
    // ID3 tags can be prepended to FLAC, MP3, and other formats
    bool has_id3_tag = false;
    
    if (bytes_read >= 10 && buffer[0] == 'I' && buffer[1] == 'D' && buffer[2] == '3') {
        has_id3_tag = true;
        size_t id3_size = PsyMP3::Tag::ID3v2Tag::getTagSize(buffer);
        
        if (id3_size > 0) {
            Debug::log("loader", "MediaFactory::detectByMagicBytes found ID3v2 tag, size: ", id3_size);
            
            // Read the actual audio data after ID3 tag
            uint8_t post_id3_buffer[64];
            handler->seek(id3_size, SEEK_SET);
            size_t post_bytes = handler->read(post_id3_buffer, 1, sizeof(post_id3_buffer));
            
            if (post_bytes >= 4) {
                // Check for FLAC signature after ID3 tag
                if (post_id3_buffer[0] == 'f' && post_id3_buffer[1] == 'L' && 
                    post_id3_buffer[2] == 'a' && post_id3_buffer[3] == 'C') {
                    Debug::log("loader", "MediaFactory::detectByMagicBytes found FLAC signature after ID3 tag");
                    info.detected_format = "flac";
                    info.confidence = 0.98f;
                    info.metadata["has_id3"] = "true";
                    info.metadata["magic_signature"] = "fLaC";
                    handler->seek(original_pos, SEEK_SET);
                    return info;
                }
                
                // Check for Ogg signature after ID3 tag
                if (post_id3_buffer[0] == 'O' && post_id3_buffer[1] == 'g' && 
                    post_id3_buffer[2] == 'g' && post_id3_buffer[3] == 'S') {
                    Debug::log("loader", "MediaFactory::detectByMagicBytes found Ogg signature after ID3 tag");
                    info.detected_format = "ogg";
                    info.confidence = 0.98f;
                    info.metadata["has_id3"] = "true";
                    info.metadata["magic_signature"] = "OggS";
                    handler->seek(original_pos, SEEK_SET);
                    return info;
                }
                
                // Check for MPEG audio sync after ID3 tag (confirms it's actually MP3)
                if ((post_id3_buffer[0] == 0xFF) && ((post_id3_buffer[1] & 0xE0) == 0xE0)) {
                    Debug::log("loader", "MediaFactory::detectByMagicBytes found MPEG sync after ID3 tag");
                    info.detected_format = "mpeg_audio";
                    info.confidence = 0.98f;
                    info.metadata["has_id3"] = "true";
                    info.metadata["magic_signature"] = "ID3+sync";
                    handler->seek(original_pos, SEEK_SET);
                    return info;
                }
            }
            
            // ID3 tag found but couldn't determine format after it
            // Don't assume it's MP3 - let other detection methods handle it
            Debug::log("loader", "MediaFactory::detectByMagicBytes ID3 tag found but format after tag unclear");
        }
    }
    
    handler->seek(original_pos, SEEK_SET);
    
    // Enhanced priority-based resolution with codec-specific probing
    struct DetectionCandidate {
        std::string format_id;
        float confidence;
        int priority;
        std::string signature_matched;
        std::string codec_detected;
    };
    
    std::vector<DetectionCandidate> candidates;
    
    // Check against all registered formats and collect candidates
    // Skip ID3 signature matching if we already handled ID3 above
    for (const auto& [format_id, registration] : s_formats) {
        for (const auto& signature : registration.format.magic_signatures) {
            // Skip ID3 signature - we handle it specially above
            if (signature == "ID3" && has_id3_tag) {
                continue;
            }
            
            if (signature.length() <= bytes_read) {
                if (std::memcmp(buffer, signature.c_str(), signature.length()) == 0) {
                    DetectionCandidate candidate;
                    candidate.format_id = format_id;
                    candidate.priority = registration.format.priority;
                    candidate.signature_matched = signature;
                    
                    // Enhanced codec-specific probing for Ogg containers
                    if (format_id == "ogg" && signature == "OggS") {
                        candidate.codec_detected = probeOggCodec(buffer, bytes_read);
                        if (!candidate.codec_detected.empty()) {
                            // Boost confidence for successful codec detection
                            candidate.confidence = 0.98f;
                            Debug::log("loader", "MediaFactory::detectByMagicBytes Ogg codec detected: ", candidate.codec_detected);
                        } else {
                            candidate.confidence = 0.85f; // Lower confidence without codec detection
                        }
                    } else {
                        // Calculate confidence based on priority and signature specificity
                        if (registration.format.priority < 20) { // Highest priority formats
                            candidate.confidence = 0.95f;
                        } else if (registration.format.priority < 50) { // High priority formats
                            candidate.confidence = 0.85f;
                        } else if (registration.format.priority < 80) { // Medium priority formats
                            candidate.confidence = 0.75f;
                        } else { // Lower priority formats
                            candidate.confidence = 0.65f;
                        }
                        
                        // Boost confidence for longer, more specific signatures
                        if (signature.length() >= 8) {
                            candidate.confidence += 0.05f;
                        } else if (signature.length() >= 4) {
                            candidate.confidence += 0.02f;
                        }
                    }
                    
                    candidates.push_back(candidate);
                }
            }
        }
    }
    
    // Select best candidate using enhanced priority-based resolution
    if (!candidates.empty()) {
        // Sort by priority first (lower number = higher priority), then by confidence
        std::sort(candidates.begin(), candidates.end(), 
                  [](const DetectionCandidate& a, const DetectionCandidate& b) {
                      if (a.priority != b.priority) {
                          return a.priority < b.priority; // Lower priority number = higher priority
                      }
                      return a.confidence > b.confidence; // Higher confidence wins
                  });
        
        const auto& best = candidates[0];
        info.detected_format = best.format_id;
        info.confidence = best.confidence;
        info.metadata["magic_signature"] = best.signature_matched;
        info.metadata["priority"] = std::to_string(best.priority);
        
        // Add codec information for container formats
        if (!best.codec_detected.empty()) {
            info.metadata["detected_codec"] = best.codec_detected;
        }
        
        Debug::log("loader", "MediaFactory::detectByMagicBytes selected format: ", best.format_id, 
                  " (priority: ", best.priority, ", confidence: ", best.confidence, ")");
        
        if (!best.codec_detected.empty()) {
            Debug::log("loader", "MediaFactory::detectByMagicBytes detected codec: ", best.codec_detected);
        }
    }
    
    return info;
}

ContentInfo MediaFactory::detectByContentAnalysis(std::unique_ptr<IOHandler>& handler) {
    ContentInfo info;
    
    if (!handler) return info;
    
    // Read a larger buffer for advanced analysis
    uint8_t buffer[512];
    long original_pos = handler->tell();
    handler->seek(0, SEEK_SET);
    size_t bytes_read = handler->read(buffer, 1, sizeof(buffer));
    handler->seek(original_pos, SEEK_SET);
    
    if (bytes_read < 16) return info;
    
    // Advanced format detection based on content patterns
    
    // Check for ID3 tags - but don't assume it's MP3!
    // ID3 tags can be prepended to FLAC, MP3, and other formats
    // The actual format detection is handled by detectByMagicBytes which
    // properly skips the ID3 tag and checks the underlying format
    // So we skip ID3 detection here to avoid false positives
    
    // Check for MPEG audio sync patterns (but avoid false positives with Ogg)
    bool found_ogg = false;
    for (size_t i = 0; i < bytes_read - 4; i++) {
        if (buffer[i] == 'O' && buffer[i+1] == 'g' && buffer[i+2] == 'g' && buffer[i+3] == 'S') {
            found_ogg = true;
            break;
        }
    }
    
    // Check for FLAC signature (may appear after ID3 tag, but also check at start)
    for (size_t i = 0; i < std::min(bytes_read - 4, (size_t)256); i++) {
        if (buffer[i] == 'f' && buffer[i+1] == 'L' && 
            buffer[i+2] == 'a' && buffer[i+3] == 'C') {
            info.detected_format = "flac";
            info.confidence = 0.95f;
            info.metadata["flac_signature_found"] = "true";
            Debug::log("loader", "MediaFactory::detectByContentAnalysis found FLAC signature at offset ", i);
            return info;
        }
    }
    
    // Only check for MPEG sync if we haven't found Ogg signature
    if (!found_ogg) {
        for (size_t i = 0; i < bytes_read - 3; i++) {
            if ((buffer[i] == 0xFF) && ((buffer[i+1] & 0xE0) == 0xE0)) {
                // Additional validation to ensure it's really MPEG audio
                uint8_t version = (buffer[i+1] >> 3) & 0x03;
                uint8_t layer = (buffer[i+1] >> 1) & 0x03;
                uint8_t bitrate = (buffer[i+2] >> 4) & 0x0F;
                uint8_t samplerate = (buffer[i+2] >> 2) & 0x03;
                
                // Validate MPEG header fields
                if (version != 0x01 && layer != 0x00 && bitrate != 0x00 && 
                    bitrate != 0x0F && samplerate != 0x03) {
                    info.detected_format = "mpeg_audio";
                    info.confidence = 0.75f;
                    info.metadata["sync_pattern_found"] = "true";
                    info.metadata["mpeg_validation"] = "passed";
                    Debug::log("loader", "MediaFactory::detectByContentAnalysis validated MPEG audio sync");
                    return info;
                }
            }
        }
    }
    
    // Enhanced Ogg container codec detection to fix format routing issues
    if (bytes_read >= 32) {
        for (size_t i = 0; i < bytes_read - 4; i++) {
            if (buffer[i] == 'O' && buffer[i+1] == 'g' && 
                buffer[i+2] == 'g' && buffer[i+3] == 'S') {
                
                // Use improved codec probing
                std::string detected_codec = probeOggCodec(buffer, bytes_read);
                
                if (!detected_codec.empty()) {
                    // Route all Ogg files with detected codecs to Ogg container
                    info.detected_format = "ogg";
                    info.confidence = 0.98f; // Very high confidence with codec detection
                    info.metadata["ogg_codec"] = detected_codec;
                    
                    // Special handling for Opus routing fix
                    if (detected_codec == "opus") {
                        info.metadata["routing_fix"] = "opus_to_ogg";
                        Debug::log("loader", "MediaFactory::detectByContentAnalysis fixed Opus routing to Ogg container");
                    }
                    
                    Debug::log("loader", "MediaFactory::detectByContentAnalysis detected Ogg with codec: ", detected_codec);
                    return info;
                } else {
                    // Generic Ogg detection without codec identification
                    info.detected_format = "ogg";
                    info.confidence = 0.8f;
                    info.metadata["ogg_codec"] = "unknown";
                    Debug::log("loader", "MediaFactory::detectByContentAnalysis detected generic Ogg container");
                    return info;
                }
            }
        }
    }
    
    // Check for RIFF/WAVE with more detailed analysis
    if (bytes_read >= 12 && 
        buffer[0] == 'R' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == 'F' &&
        buffer[8] == 'W' && buffer[9] == 'A' && buffer[10] == 'V' && buffer[11] == 'E') {
        info.detected_format = "wave";
        info.confidence = 0.95f;
        info.metadata["container"] = "riff";
        return info;
    }
    
    // Check for AIFF with more detailed analysis
    if (bytes_read >= 12 && 
        buffer[0] == 'F' && buffer[1] == 'O' && buffer[2] == 'R' && buffer[3] == 'M' &&
        buffer[8] == 'A' && buffer[9] == 'I' && buffer[10] == 'F' && buffer[11] == 'F') {
        info.detected_format = "aiff";
        info.confidence = 0.95f;
        info.metadata["container"] = "iff";
        return info;
    }
    
    // Check for MP4/M4A with box analysis
    if (bytes_read >= 8) {
        for (size_t i = 0; i < bytes_read - 8; i++) {
            if (buffer[i+4] == 'f' && buffer[i+5] == 't' && buffer[i+6] == 'y' && buffer[i+7] == 'p') {
                info.detected_format = "mp4";
                info.confidence = 0.9f;
                info.metadata["container"] = "iso_base_media";
                return info;
            }
        }
    }
    
    return info;
}

std::unique_ptr<IOHandler> MediaFactory::createIOHandler(const std::string& uri) {
    if (isHttpUri(uri)) {
        return std::make_unique<HTTPIOHandler>(uri);
    } else {
        return std::make_unique<FileIOHandler>(uri);
    }
}

} // namespace Demuxer
} // namespace PsyMP3
