/*
 * DemuxerExtensibility.h - Extensibility features for demuxer architecture
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

#ifndef DEMUXER_EXTENSIBILITY_H
#define DEMUXER_EXTENSIBILITY_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Custom stream factory function type
 */
using CustomStreamFactory = std::function<std::unique_ptr<Stream>(const std::string& uri, 
                                                                 const ContentInfo& info,
                                                                 const std::map<std::string, std::string>& config)>;

/**
 * @brief Custom IOHandler factory function type
 */
using CustomIOHandlerFactory = std::function<std::unique_ptr<IOHandler>(const std::string& uri,
                                                                       const std::map<std::string, std::string>& config)>;

/**
 * @brief Runtime configuration for demuxer behavior
 */
struct DemuxerConfig {
    // Buffer management
    size_t max_buffer_size = 1024 * 1024;      // 1MB default
    size_t chunk_size = 64 * 1024;             // 64KB default
    size_t read_ahead_size = 256 * 1024;       // 256KB default
    
    // Performance tuning
    bool enable_threading = true;
    size_t max_threads = 4;
    bool enable_caching = true;
    size_t cache_size = 512 * 1024;            // 512KB default
    
    // Error handling
    bool strict_parsing = false;
    size_t max_retries = 3;
    std::chrono::milliseconds retry_delay{100};
    bool enable_recovery = true;
    
    // Format-specific options
    std::map<std::string, std::string> format_options;
    
    // Debugging and logging
    bool enable_debug_logging = false;
    std::string log_level = "info";
    std::vector<std::string> debug_categories;
    
    /**
     * @brief Set format-specific option
     */
    void setFormatOption(const std::string& format_id, const std::string& key, const std::string& value) {
        format_options[format_id + "." + key] = value;
    }
    
    /**
     * @brief Get format-specific option
     */
    std::string getFormatOption(const std::string& format_id, const std::string& key, 
                               const std::string& default_value = "") const {
        auto it = format_options.find(format_id + "." + key);
        return (it != format_options.end()) ? it->second : default_value;
    }
    
    /**
     * @brief Check if format-specific option exists
     */
    bool hasFormatOption(const std::string& format_id, const std::string& key) const {
        return format_options.find(format_id + "." + key) != format_options.end();
    }
    
    /**
     * @brief Load configuration from file
     */
    bool loadFromFile(const std::string& config_file);
    
    /**
     * @brief Save configuration to file
     */
    bool saveToFile(const std::string& config_file) const;
    
    /**
     * @brief Load configuration from environment variables
     */
    void loadFromEnvironment();
    
    /**
     * @brief Validate configuration values
     */
    bool validate() const;
    
    /**
     * @brief Get configuration as key-value map
     */
    std::map<std::string, std::string> toMap() const;
    
    /**
     * @brief Load configuration from key-value map
     */
    void fromMap(const std::map<std::string, std::string>& config_map);
};

/**
 * @brief Format-specific metadata extension registry
 */
class MetadataExtensionRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static MetadataExtensionRegistry& getInstance();
    
    /**
     * @brief Metadata extractor function type
     */
    using MetadataExtractor = std::function<ExtendedMetadata(const std::vector<uint8_t>& data,
                                                            const std::string& format_id)>;
    
    /**
     * @brief Metadata validator function type
     */
    using MetadataValidator = std::function<bool(const ExtendedMetadata& metadata,
                                               const std::string& format_id)>;
    
    /**
     * @brief Metadata converter function type
     */
    using MetadataConverter = std::function<ExtendedMetadata(const ExtendedMetadata& source,
                                                           const std::string& target_format)>;
    
    /**
     * @brief Register metadata extractor for format
     */
    void registerExtractor(const std::string& format_id, MetadataExtractor extractor);
    
    /**
     * @brief Register metadata validator for format
     */
    void registerValidator(const std::string& format_id, MetadataValidator validator);
    
    /**
     * @brief Register metadata converter between formats
     */
    void registerConverter(const std::string& source_format, const std::string& target_format,
                          MetadataConverter converter);
    
    /**
     * @brief Extract metadata using registered extractor
     */
    std::optional<ExtendedMetadata> extractMetadata(const std::vector<uint8_t>& data,
                                                   const std::string& format_id) const;
    
    /**
     * @brief Validate metadata using registered validator
     */
    bool validateMetadata(const ExtendedMetadata& metadata, const std::string& format_id) const;
    
    /**
     * @brief Convert metadata between formats
     */
    std::optional<ExtendedMetadata> convertMetadata(const ExtendedMetadata& source,
                                                   const std::string& source_format,
                                                   const std::string& target_format) const;
    
    /**
     * @brief Get supported formats for extraction
     */
    std::vector<std::string> getSupportedFormats() const;
    
    /**
     * @brief Get supported conversion pairs
     */
    std::vector<std::pair<std::string, std::string>> getSupportedConversions() const;
    
    /**
     * @brief Unregister all extensions for format
     */
    void unregisterFormat(const std::string& format_id);
    
private:
    MetadataExtensionRegistry() = default;
    
    mutable std::mutex m_mutex;
    std::map<std::string, MetadataExtractor> m_extractors;
    std::map<std::string, MetadataValidator> m_validators;
    std::map<std::pair<std::string, std::string>, MetadataConverter> m_converters;
};

/**
 * @brief Extensible IOHandler registry
 */
class IOHandlerRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static IOHandlerRegistry& getInstance();
    
    /**
     * @brief Register custom IOHandler factory for protocol
     */
    void registerIOHandler(const std::string& protocol, CustomIOHandlerFactory factory);
    
    /**
     * @brief Unregister IOHandler for protocol
     */
    void unregisterIOHandler(const std::string& protocol);
    
    /**
     * @brief Create IOHandler for URI
     */
    std::unique_ptr<IOHandler> createIOHandler(const std::string& uri,
                                              const std::map<std::string, std::string>& config = {}) const;
    
    /**
     * @brief Check if protocol is supported
     */
    bool supportsProtocol(const std::string& protocol) const;
    
    /**
     * @brief Get supported protocols
     */
    std::vector<std::string> getSupportedProtocols() const;
    
    /**
     * @brief Extract protocol from URI
     */
    static std::string extractProtocol(const std::string& uri);
    
private:
    IOHandlerRegistry();
    
    mutable std::mutex m_mutex;
    std::map<std::string, CustomIOHandlerFactory> m_factories;
    
    void registerBuiltInHandlers();
};

/**
 * @brief Extensible stream factory registry
 */
class StreamFactoryRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static StreamFactoryRegistry& getInstance();
    
    /**
     * @brief Register custom stream factory for format
     */
    void registerStreamFactory(const std::string& format_id, CustomStreamFactory factory);
    
    /**
     * @brief Unregister stream factory for format
     */
    void unregisterStreamFactory(const std::string& format_id);
    
    /**
     * @brief Create stream using registered factory
     */
    std::unique_ptr<Stream> createStream(const std::string& format_id,
                                        const std::string& uri,
                                        const ContentInfo& info,
                                        const std::map<std::string, std::string>& config = {}) const;
    
    /**
     * @brief Check if format has custom factory
     */
    bool hasCustomFactory(const std::string& format_id) const;
    
    /**
     * @brief Get formats with custom factories
     */
    std::vector<std::string> getCustomFormats() const;
    
private:
    StreamFactoryRegistry() = default;
    
    mutable std::mutex m_mutex;
    std::map<std::string, CustomStreamFactory> m_factories;
};

/**
 * @brief Runtime configuration manager
 */
class DemuxerConfigManager {
public:
    /**
     * @brief Get singleton instance
     */
    static DemuxerConfigManager& getInstance();
    
    /**
     * @brief Get global configuration
     */
    const DemuxerConfig& getGlobalConfig() const;
    
    /**
     * @brief Set global configuration
     */
    void setGlobalConfig(const DemuxerConfig& config);
    
    /**
     * @brief Get format-specific configuration
     */
    DemuxerConfig getFormatConfig(const std::string& format_id) const;
    
    /**
     * @brief Set format-specific configuration
     */
    void setFormatConfig(const std::string& format_id, const DemuxerConfig& config);
    
    /**
     * @brief Get configuration for specific demuxer instance
     */
    DemuxerConfig getInstanceConfig(const std::string& instance_id) const;
    
    /**
     * @brief Set configuration for specific demuxer instance
     */
    void setInstanceConfig(const std::string& instance_id, const DemuxerConfig& config);
    
    /**
     * @brief Load configuration from file
     */
    bool loadConfigFile(const std::string& config_file);
    
    /**
     * @brief Save configuration to file
     */
    bool saveConfigFile(const std::string& config_file) const;
    
    /**
     * @brief Load configuration from environment
     */
    void loadFromEnvironment();
    
    /**
     * @brief Reset to default configuration
     */
    void resetToDefaults();
    
    /**
     * @brief Get configuration search paths
     */
    std::vector<std::string> getConfigSearchPaths() const;
    
    /**
     * @brief Set configuration search paths
     */
    void setConfigSearchPaths(const std::vector<std::string>& paths);
    
    /**
     * @brief Auto-load configuration from search paths
     */
    bool autoLoadConfig();
    
    /**
     * @brief Validate all configurations
     */
    bool validateConfigurations() const;
    
    /**
     * @brief Get configuration statistics
     */
    struct ConfigStats {
        size_t total_configs;
        size_t format_configs;
        size_t instance_configs;
        size_t invalid_configs;
    };
    ConfigStats getConfigStats() const;
    
private:
    DemuxerConfigManager();
    
    mutable std::mutex m_mutex;
    DemuxerConfig m_global_config;
    std::map<std::string, DemuxerConfig> m_format_configs;
    std::map<std::string, DemuxerConfig> m_instance_configs;
    std::vector<std::string> m_config_search_paths;
    
    DemuxerConfig mergeConfigs(const DemuxerConfig& base, const DemuxerConfig& override) const;
    bool parseConfigFile(const std::string& file_path, DemuxerConfig& config) const;
    bool writeConfigFile(const std::string& file_path, const DemuxerConfig& config) const;
};

/**
 * @brief Extension point for custom demuxer behaviors
 */
class DemuxerExtensionPoint {
public:
    /**
     * @brief Pre-parsing hook function type
     */
    using PreParsingHook = std::function<bool(const std::string& format_id,
                                            IOHandler* handler,
                                            const DemuxerConfig& config)>;
    
    /**
     * @brief Post-parsing hook function type
     */
    using PostParsingHook = std::function<void(const std::string& format_id,
                                             const std::vector<StreamInfo>& streams,
                                             const DemuxerConfig& config)>;
    
    /**
     * @brief Error handling hook function type
     */
    using ErrorHook = std::function<bool(const std::string& format_id,
                                       const std::string& error_message,
                                       const DemuxerConfig& config)>;
    
    /**
     * @brief Get singleton instance
     */
    static DemuxerExtensionPoint& getInstance();
    
    /**
     * @brief Register pre-parsing hook
     */
    void registerPreParsingHook(const std::string& format_id, PreParsingHook hook);
    
    /**
     * @brief Register post-parsing hook
     */
    void registerPostParsingHook(const std::string& format_id, PostParsingHook hook);
    
    /**
     * @brief Register error handling hook
     */
    void registerErrorHook(const std::string& format_id, ErrorHook hook);
    
    /**
     * @brief Execute pre-parsing hooks
     */
    bool executePreParsingHooks(const std::string& format_id,
                               IOHandler* handler,
                               const DemuxerConfig& config) const;
    
    /**
     * @brief Execute post-parsing hooks
     */
    void executePostParsingHooks(const std::string& format_id,
                                const std::vector<StreamInfo>& streams,
                                const DemuxerConfig& config) const;
    
    /**
     * @brief Execute error handling hooks
     */
    bool executeErrorHooks(const std::string& format_id,
                          const std::string& error_message,
                          const DemuxerConfig& config) const;
    
    /**
     * @brief Unregister all hooks for format
     */
    void unregisterHooks(const std::string& format_id);
    
private:
    DemuxerExtensionPoint() = default;
    
    mutable std::mutex m_mutex;
    std::map<std::string, std::vector<PreParsingHook>> m_pre_parsing_hooks;
    std::map<std::string, std::vector<PostParsingHook>> m_post_parsing_hooks;
    std::map<std::string, std::vector<ErrorHook>> m_error_hooks;
};

/**
 * @brief RAII configuration scope for temporary config changes
 */
class ConfigScope {
public:
    explicit ConfigScope(const std::string& format_id, const DemuxerConfig& temp_config);
    explicit ConfigScope(const DemuxerConfig& temp_global_config);
    ~ConfigScope();
    
    // Non-copyable, non-movable
    ConfigScope(const ConfigScope&) = delete;
    ConfigScope& operator=(const ConfigScope&) = delete;
    ConfigScope(ConfigScope&&) = delete;
    ConfigScope& operator=(ConfigScope&&) = delete;
    
private:
    std::string m_format_id;
    DemuxerConfig m_original_config;
    bool m_is_global;
};

/**
 * @brief Utility functions for extensibility
 */
namespace ExtensibilityUtils {
    
    /**
     * @brief Parse configuration string
     */
    std::map<std::string, std::string> parseConfigString(const std::string& config_str);
    
    /**
     * @brief Format configuration as string
     */
    std::string formatConfigString(const std::map<std::string, std::string>& config);
    
    /**
     * @brief Validate URI format
     */
    bool isValidURI(const std::string& uri);
    
    /**
     * @brief Extract parameters from URI
     */
    std::map<std::string, std::string> extractURIParameters(const std::string& uri);
    
    /**
     * @brief Build URI with parameters
     */
    std::string buildURIWithParameters(const std::string& base_uri,
                                      const std::map<std::string, std::string>& params);
    
    /**
     * @brief Get default configuration for format
     */
    DemuxerConfig getDefaultConfigForFormat(const std::string& format_id);
    
    /**
     * @brief Merge configuration maps
     */
    std::map<std::string, std::string> mergeConfigMaps(
        const std::map<std::string, std::string>& base,
        const std::map<std::string, std::string>& override);
    
    /**
     * @brief Convert string to typed value
     */
    template<typename T>
    T convertConfigValue(const std::string& value, const T& default_value);
    
    /**
     * @brief Convert typed value to string
     */
    template<typename T>
    std::string convertToConfigString(const T& value);
}

#endif // DEMUXER_EXTENSIBILITY_H