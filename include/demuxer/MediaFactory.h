/*
 * MediaFactory.h - Modern extensible media factory architecture
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef MEDIAFACTORY_H
#define MEDIAFACTORY_H

namespace PsyMP3 {
namespace Demuxer {

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Media format descriptor with comprehensive metadata
 */
struct MediaFormat {
    std::string format_id;                    // Unique format identifier
    std::string display_name;                 // Human-readable name
    std::vector<std::string> extensions;      // File extensions
    std::vector<std::string> mime_types;      // MIME types
    std::vector<std::string> magic_signatures; // Binary signatures for detection
    int priority = 100;                       // Detection priority (lower = higher priority)
    bool supports_streaming = false;          // HTTP streaming capability
    bool supports_seeking = true;             // Seeking capability
    bool is_container = false;                // Container vs. codec format
    std::string description;                  // Technical description
};

/**
 * @brief Content detection result
 */
struct ContentInfo {
    std::string detected_format;              // Best match format ID
    std::string mime_type;                    // Detected/provided MIME type
    std::string file_extension;               // File extension (if any)
    float confidence = 0.0f;                  // Detection confidence (0.0-1.0)
    std::map<std::string, std::string> metadata; // Additional metadata
};

/**
 * @brief Stream factory function type
 */
using StreamFactory = std::function<std::unique_ptr<Stream>(const std::string& uri, const ContentInfo& info)>;

/**
 * @brief Content detector function type
 */
using ContentDetector = std::function<std::optional<ContentInfo>(std::unique_ptr<IOHandler>& handler)>;

/**
 * @brief Modern extensible media factory
 * 
 * This factory provides a flexible, plugin-based architecture for media handling:
 * - Multiple detection methods (extension, MIME, magic bytes, content analysis)
 * - HTTP streaming support with MIME type detection
 * - Plugin-based format registration
 * - Future-extensible for new formats and protocols
 * - Comprehensive metadata and capability reporting
 */
class MediaFactory {
public:
    /**
     * @brief Primary factory method - auto-detect format and create stream
     */
    static std::unique_ptr<Stream> createStream(const std::string& uri);
    
    /**
     * @brief Create stream with explicit MIME type hint
     */
    static std::unique_ptr<Stream> createStreamWithMimeType(const std::string& uri, const std::string& mime_type);
    
    /**
     * @brief Create stream with pre-analyzed content info
     */
    static std::unique_ptr<Stream> createStreamWithContentInfo(const std::string& uri, const ContentInfo& info);
    
    /**
     * @brief Analyze content and return detection results
     */
    static ContentInfo analyzeContent(const std::string& uri);
    
    /**
     * @brief Analyze content from IOHandler
     */
    static ContentInfo analyzeContent(std::unique_ptr<IOHandler>& handler);
    
    // Format registration (for plugins and dynamic formats)
    static void registerFormat(const MediaFormat& format, StreamFactory factory);
    static void registerContentDetector(const std::string& format_id, ContentDetector detector);
    static void unregisterFormat(const std::string& format_id);
    
    // Format queries and introspection
    static std::vector<MediaFormat> getSupportedFormats();
    static std::optional<MediaFormat> getFormatInfo(const std::string& format_id);
    static bool supportsFormat(const std::string& format_id);
    static bool supportsExtension(const std::string& extension);
    static bool supportsMimeType(const std::string& mime_type);
    static bool supportsStreaming(const std::string& format_id);
    
    // MIME type utilities
    static std::string extensionToMimeType(const std::string& extension);
    static std::string mimeTypeToExtension(const std::string& mime_type);
    static std::vector<std::string> getExtensionsForMimeType(const std::string& mime_type);
    static std::vector<std::string> getMimeTypesForExtension(const std::string& extension);
    
    // URI and path utilities
    static std::string extractExtension(const std::string& uri);
    static bool isHttpUri(const std::string& uri);
    static bool isLocalFile(const std::string& uri);
    
private:
    struct FormatRegistration {
        MediaFormat format;
        StreamFactory factory;
        ContentDetector detector;
    };
    
    static std::map<std::string, FormatRegistration> s_formats;
    static std::map<std::string, std::string> s_extension_to_format;
    static std::map<std::string, std::string> s_mime_to_format;
    static bool s_initialized;
    static std::mutex s_factory_mutex; // Thread safety for factory operations
    
    static void initializeDefaultFormats();
    static void rebuildLookupTables();
    static ContentInfo detectByExtension(const std::string& uri);
    static ContentInfo detectByMimeType(const std::string& mime_type);
    static ContentInfo detectByMagicBytes(std::unique_ptr<IOHandler>& handler);
    static ContentInfo detectByContentAnalysis(std::unique_ptr<IOHandler>& handler);
    static std::string probeOggCodec(const uint8_t* buffer, size_t buffer_size);
    static std::unique_ptr<IOHandler> createIOHandler(const std::string& uri);
    
    // Internal methods that assume mutex is already held
    static void registerFormatInternal(const MediaFormat& format, StreamFactory factory);
    static void registerContentDetectorInternal(const std::string& format_id, ContentDetector detector);
};

/**
 * @brief Exception thrown when media format is not supported
 */
class UnsupportedMediaException : public std::runtime_error {
public:
    explicit UnsupportedMediaException(const std::string& message) 
        : std::runtime_error(message) {}
};

/**
 * @brief Exception thrown when content detection fails
 */
class ContentDetectionException : public std::runtime_error {
public:
    explicit ContentDetectionException(const std::string& message) 
        : std::runtime_error(message) {}
};


} // namespace Demuxer
} // namespace PsyMP3
#endif // MEDIAFACTORY_H