/*
 * DemuxerPlugin.h - Plugin architecture for demuxer extensibility
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

#ifndef DEMUXER_PLUGIN_H
#define DEMUXER_PLUGIN_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Plugin API version for ABI compatibility
 */
#define DEMUXER_PLUGIN_API_VERSION 1

/**
 * @brief Plugin information structure for stable ABI
 */
struct DemuxerPluginInfo {
    uint32_t api_version;           // Plugin API version
    const char* plugin_name;        // Plugin name (null-terminated)
    const char* plugin_version;     // Plugin version (null-terminated)
    const char* author;             // Plugin author (null-terminated)
    const char* description;        // Plugin description (null-terminated)
    uint32_t format_count;          // Number of formats supported
};

/**
 * @brief Format information structure for stable ABI
 */
struct DemuxerPluginFormat {
    const char* format_id;          // Format identifier (null-terminated)
    const char* format_name;        // Human-readable name (null-terminated)
    const char* description;        // Format description (null-terminated)
    const char** extensions;        // Null-terminated array of extensions
    uint32_t extension_count;       // Number of extensions
    const uint8_t* magic_signature; // Magic bytes for detection
    uint32_t signature_size;        // Size of magic signature
    uint32_t signature_offset;      // Offset of signature in file
    int32_t priority;               // Detection priority
    uint32_t capabilities;          // Format capabilities (bitfield)
};

/**
 * @brief Format capability flags
 */
enum DemuxerPluginCapabilities {
    DEMUXER_CAP_STREAMING = 0x01,   // Supports streaming
    DEMUXER_CAP_SEEKING = 0x02,     // Supports seeking
    DEMUXER_CAP_METADATA = 0x04,    // Supports metadata extraction
    DEMUXER_CAP_MULTIPLE_STREAMS = 0x08, // Supports multiple streams
    DEMUXER_CAP_CHAPTERS = 0x10,    // Supports chapters
    DEMUXER_CAP_THUMBNAILS = 0x20   // Supports embedded thumbnails
};

/**
 * @brief Plugin demuxer factory function type for stable ABI
 */
typedef void* (*DemuxerPluginFactoryFunc)(void* io_handler);

/**
 * @brief Plugin content detector function type for stable ABI
 */
typedef int (*DemuxerPluginDetectorFunc)(void* io_handler, void* content_info);

/**
 * @brief Plugin initialization function type
 */
typedef int (*DemuxerPluginInitFunc)(const DemuxerPluginInfo** plugin_info,
                                    const DemuxerPluginFormat** formats,
                                    DemuxerPluginFactoryFunc* factory_func,
                                    DemuxerPluginDetectorFunc* detector_func);

/**
 * @brief Plugin cleanup function type
 */
typedef void (*DemuxerPluginCleanupFunc)(void);

/**
 * @brief Extended metadata structure for format-specific information
 */
struct ExtendedMetadata {
    std::string format_id;                              // Format identifier
    std::map<std::string, std::string> string_metadata; // String metadata
    std::map<std::string, int64_t> numeric_metadata;    // Numeric metadata
    std::map<std::string, std::vector<uint8_t>> binary_metadata; // Binary metadata
    std::map<std::string, double> float_metadata;       // Floating-point metadata
    
    /**
     * @brief Set string metadata
     */
    void setString(const std::string& key, const std::string& value) {
        string_metadata[key] = value;
    }
    
    /**
     * @brief Get string metadata
     */
    std::string getString(const std::string& key, const std::string& default_value = "") const {
        auto it = string_metadata.find(key);
        return (it != string_metadata.end()) ? it->second : default_value;
    }
    
    /**
     * @brief Set numeric metadata
     */
    void setNumeric(const std::string& key, int64_t value) {
        numeric_metadata[key] = value;
    }
    
    /**
     * @brief Get numeric metadata
     */
    int64_t getNumeric(const std::string& key, int64_t default_value = 0) const {
        auto it = numeric_metadata.find(key);
        return (it != numeric_metadata.end()) ? it->second : default_value;
    }
    
    /**
     * @brief Set binary metadata
     */
    void setBinary(const std::string& key, const std::vector<uint8_t>& value) {
        binary_metadata[key] = value;
    }
    
    /**
     * @brief Get binary metadata
     */
    std::vector<uint8_t> getBinary(const std::string& key) const {
        auto it = binary_metadata.find(key);
        return (it != binary_metadata.end()) ? it->second : std::vector<uint8_t>();
    }
    
    /**
     * @brief Set floating-point metadata
     */
    void setFloat(const std::string& key, double value) {
        float_metadata[key] = value;
    }
    
    /**
     * @brief Get floating-point metadata
     */
    double getFloat(const std::string& key, double default_value = 0.0) const {
        auto it = float_metadata.find(key);
        return (it != float_metadata.end()) ? it->second : default_value;
    }
    
    /**
     * @brief Check if metadata key exists
     */
    bool hasKey(const std::string& key) const {
        return string_metadata.find(key) != string_metadata.end() ||
               numeric_metadata.find(key) != numeric_metadata.end() ||
               binary_metadata.find(key) != binary_metadata.end() ||
               float_metadata.find(key) != float_metadata.end();
    }
    
    /**
     * @brief Clear all metadata
     */
    void clear() {
        string_metadata.clear();
        numeric_metadata.clear();
        binary_metadata.clear();
        float_metadata.clear();
    }
    
    /**
     * @brief Get all keys
     */
    std::vector<std::string> getAllKeys() const {
        std::vector<std::string> keys;
        for (const auto& [key, value] : string_metadata) keys.push_back(key);
        for (const auto& [key, value] : numeric_metadata) keys.push_back(key);
        for (const auto& [key, value] : binary_metadata) keys.push_back(key);
        for (const auto& [key, value] : float_metadata) keys.push_back(key);
        return keys;
    }
};

/**
 * @brief Enhanced StreamInfo with extensible metadata support
 */
struct ExtendedStreamInfo : public StreamInfo {
    ExtendedMetadata extended_metadata;  // Format-specific metadata
    
    /**
     * @brief Constructor from base StreamInfo
     */
    ExtendedStreamInfo(const StreamInfo& base) : StreamInfo(base) {}
    
    /**
     * @brief Default constructor
     */
    ExtendedStreamInfo() = default;
    
    /**
     * @brief Set format-specific metadata
     */
    void setFormatMetadata(const std::string& key, const std::string& value) {
        extended_metadata.setString(key, value);
    }
    
    /**
     * @brief Get format-specific metadata
     */
    std::string getFormatMetadata(const std::string& key, const std::string& default_value = "") const {
        return extended_metadata.getString(key, default_value);
    }
    
    /**
     * @brief Check if format-specific metadata exists
     */
    bool hasFormatMetadata(const std::string& key) const {
        return extended_metadata.hasKey(key);
    }
};

/**
 * @brief Plugin manager for dynamic format registration
 */
class DemuxerPluginManager {
public:
    /**
     * @brief Get singleton instance
     */
    static DemuxerPluginManager& getInstance();
    
    /**
     * @brief Load plugin from shared library
     * @param plugin_path Path to plugin shared library
     * @return true if plugin loaded successfully
     */
    bool loadPlugin(const std::string& plugin_path);
    
    /**
     * @brief Unload plugin
     * @param plugin_name Name of plugin to unload
     * @return true if plugin unloaded successfully
     */
    bool unloadPlugin(const std::string& plugin_name);
    
    /**
     * @brief Register custom demuxer factory
     * @param format_id Format identifier
     * @param factory_func Factory function
     * @param format_info Format information
     * @return true if registration successful
     */
    bool registerCustomDemuxer(const std::string& format_id,
                              DemuxerFactory::DemuxerFactoryFunc factory_func,
                              const MediaFormat& format_info);
    
    /**
     * @brief Register custom content detector
     * @param format_id Format identifier
     * @param detector_func Content detector function
     * @return true if registration successful
     */
    bool registerCustomDetector(const std::string& format_id,
                               ContentDetector detector_func);
    
    /**
     * @brief Unregister custom format
     * @param format_id Format identifier
     * @return true if unregistration successful
     */
    bool unregisterCustomFormat(const std::string& format_id);
    
    /**
     * @brief Get loaded plugin information
     */
    struct LoadedPluginInfo {
        std::string plugin_name;
        std::string plugin_version;
        std::string author;
        std::string description;
        std::vector<std::string> supported_formats;
        std::string library_path;
        void* library_handle;
    };
    std::vector<LoadedPluginInfo> getLoadedPlugins() const;
    
    /**
     * @brief Get custom format information
     */
    std::vector<MediaFormat> getCustomFormats() const;
    
    /**
     * @brief Check if format is provided by plugin
     * @param format_id Format identifier
     * @return true if format is from plugin
     */
    bool isPluginFormat(const std::string& format_id) const;
    
    /**
     * @brief Scan directory for plugins
     * @param plugin_dir Directory to scan
     * @return Number of plugins loaded
     */
    int scanPluginDirectory(const std::string& plugin_dir);
    
    /**
     * @brief Set plugin search paths
     * @param paths Vector of directory paths to search
     */
    void setPluginSearchPaths(const std::vector<std::string>& paths);
    
    /**
     * @brief Get plugin search paths
     */
    std::vector<std::string> getPluginSearchPaths() const;
    
    /**
     * @brief Auto-load plugins from search paths
     * @return Number of plugins loaded
     */
    int autoLoadPlugins();
    
    /**
     * @brief Validate plugin ABI compatibility
     * @param plugin_info Plugin information
     * @return true if compatible
     */
    bool validatePluginABI(const DemuxerPluginInfo* plugin_info) const;
    
    /**
     * @brief Get plugin statistics
     */
    struct PluginStats {
        size_t total_plugins_loaded;
        size_t total_formats_registered;
        size_t custom_detectors_registered;
        size_t failed_loads;
    };
    PluginStats getPluginStats() const;
    
private:
    DemuxerPluginManager();
    ~DemuxerPluginManager();
    
    // Thread safety
    mutable std::mutex m_mutex;
    
    // Plugin management
    struct PluginHandle {
        std::string plugin_name;
        std::string library_path;
        void* library_handle;
        DemuxerPluginInfo plugin_info;
        std::vector<DemuxerPluginFormat> formats;
        DemuxerPluginFactoryFunc factory_func;
        DemuxerPluginDetectorFunc detector_func;
        DemuxerPluginCleanupFunc cleanup_func;
    };
    std::map<std::string, std::unique_ptr<PluginHandle>> m_loaded_plugins;
    
    // Custom registrations
    std::map<std::string, MediaFormat> m_custom_formats;
    std::map<std::string, DemuxerFactory::DemuxerFactoryFunc> m_custom_factories;
    std::map<std::string, ContentDetector> m_custom_detectors;
    
    // Plugin search paths
    std::vector<std::string> m_search_paths;
    
    // Statistics
    mutable PluginStats m_stats;
    
    // Helper methods
    void* loadLibrary(const std::string& path);
    void unloadLibrary(void* handle);
    void* getSymbol(void* handle, const std::string& symbol_name);
    bool registerPluginFormats(PluginHandle* plugin);
    void unregisterPluginFormats(PluginHandle* plugin);
    std::string getPluginFileName(const std::string& plugin_name) const;
    bool isValidPluginFile(const std::string& file_path) const;
    
    // ABI conversion helpers
    MediaFormat convertPluginFormat(const DemuxerPluginFormat& plugin_format) const;
    DemuxerFactory::DemuxerFactoryFunc wrapPluginFactory(DemuxerPluginFactoryFunc plugin_factory) const;
    ContentDetector wrapPluginDetector(DemuxerPluginDetectorFunc plugin_detector) const;
};

/**
 * @brief RAII plugin loader for automatic cleanup
 */
class PluginLoader {
public:
    explicit PluginLoader(const std::string& plugin_path);
    ~PluginLoader();
    
    /**
     * @brief Check if plugin loaded successfully
     */
    bool isLoaded() const { return m_loaded; }
    
    /**
     * @brief Get plugin name
     */
    const std::string& getPluginName() const { return m_plugin_name; }
    
    /**
     * @brief Get error message if loading failed
     */
    const std::string& getErrorMessage() const { return m_error_message; }
    
private:
    std::string m_plugin_path;
    std::string m_plugin_name;
    std::string m_error_message;
    bool m_loaded;
};

/**
 * @brief Macro for plugin entry point definition
 */
#define DEMUXER_PLUGIN_ENTRY_POINT(plugin_info_var, formats_var, factory_func, detector_func) \
    extern "C" { \
        int demuxer_plugin_init(const DemuxerPluginInfo** plugin_info, \
                               const DemuxerPluginFormat** formats, \
                               DemuxerPluginFactoryFunc* factory_func_ptr, \
                               DemuxerPluginDetectorFunc* detector_func_ptr) { \
            if (plugin_info) *plugin_info = &plugin_info_var; \
            if (formats) *formats = formats_var; \
            if (factory_func_ptr) *factory_func_ptr = factory_func; \
            if (detector_func_ptr) *detector_func_ptr = detector_func; \
            return 1; \
        } \
        void demuxer_plugin_cleanup(void) { \
            /* Plugin-specific cleanup code here */ \
        } \
    }

#endif // DEMUXER_PLUGIN_H