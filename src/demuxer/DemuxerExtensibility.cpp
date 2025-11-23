/*
 * DemuxerExtensibility.cpp - Extensibility features for demuxer architecture
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Demuxer {

// DemuxerConfig implementation
bool DemuxerConfig::loadFromFile(const std::string& config_file) {
    // Implementation would parse JSON/INI/XML config file
    // For now, return false as not implemented
    return false;
}

bool DemuxerConfig::saveToFile(const std::string& config_file) const {
    // Implementation would write config to file
    // For now, return false as not implemented
    return false;
}

void DemuxerConfig::loadFromEnvironment() {
    // Load configuration from environment variables
    const char* env_val;
    
    if ((env_val = std::getenv("PSYMP3_MAX_BUFFER_SIZE")) != nullptr) {
        max_buffer_size = std::stoul(env_val);
    }
    
    if ((env_val = std::getenv("PSYMP3_CHUNK_SIZE")) != nullptr) {
        chunk_size = std::stoul(env_val);
    }
    
    if ((env_val = std::getenv("PSYMP3_ENABLE_THREADING")) != nullptr) {
        enable_threading = (std::string(env_val) == "true" || std::string(env_val) == "1");
    }
    
    if ((env_val = std::getenv("PSYMP3_MAX_THREADS")) != nullptr) {
        max_threads = std::stoul(env_val);
    }
    
    if ((env_val = std::getenv("PSYMP3_STRICT_PARSING")) != nullptr) {
        strict_parsing = (std::string(env_val) == "true" || std::string(env_val) == "1");
    }
    
    if ((env_val = std::getenv("PSYMP3_DEBUG_LOGGING")) != nullptr) {
        enable_debug_logging = (std::string(env_val) == "true" || std::string(env_val) == "1");
    }
    
    if ((env_val = std::getenv("PSYMP3_LOG_LEVEL")) != nullptr) {
        log_level = env_val;
    }
}

bool DemuxerConfig::validate() const {
    // Validate configuration values
    if (max_buffer_size == 0 || max_buffer_size > 100 * 1024 * 1024) { // Max 100MB
        return false;
    }
    
    if (chunk_size == 0 || chunk_size > max_buffer_size) {
        return false;
    }
    
    if (max_threads == 0 || max_threads > 64) { // Max 64 threads
        return false;
    }
    
    return true;
}

std::map<std::string, std::string> DemuxerConfig::toMap() const {
    std::map<std::string, std::string> config_map;
    
    config_map["max_buffer_size"] = std::to_string(max_buffer_size);
    config_map["chunk_size"] = std::to_string(chunk_size);
    config_map["read_ahead_size"] = std::to_string(read_ahead_size);
    config_map["enable_threading"] = enable_threading ? "true" : "false";
    config_map["max_threads"] = std::to_string(max_threads);
    config_map["enable_caching"] = enable_caching ? "true" : "false";
    config_map["cache_size"] = std::to_string(cache_size);
    config_map["strict_parsing"] = strict_parsing ? "true" : "false";
    config_map["max_retries"] = std::to_string(max_retries);
    config_map["retry_delay"] = std::to_string(retry_delay.count());
    config_map["enable_recovery"] = enable_recovery ? "true" : "false";
    config_map["enable_debug_logging"] = enable_debug_logging ? "true" : "false";
    config_map["log_level"] = log_level;
    
    // Add format options
    for (const auto& [key, value] : format_options) {
        config_map["format." + key] = value;
    }
    
    return config_map;
}

void DemuxerConfig::fromMap(const std::map<std::string, std::string>& config_map) {
    for (const auto& [key, value] : config_map) {
        if (key == "max_buffer_size") {
            max_buffer_size = std::stoul(value);
        } else if (key == "chunk_size") {
            chunk_size = std::stoul(value);
        } else if (key == "read_ahead_size") {
            read_ahead_size = std::stoul(value);
        } else if (key == "enable_threading") {
            enable_threading = (value == "true" || value == "1");
        } else if (key == "max_threads") {
            max_threads = std::stoul(value);
        } else if (key == "enable_caching") {
            enable_caching = (value == "true" || value == "1");
        } else if (key == "cache_size") {
            cache_size = std::stoul(value);
        } else if (key == "strict_parsing") {
            strict_parsing = (value == "true" || value == "1");
        } else if (key == "max_retries") {
            max_retries = std::stoul(value);
        } else if (key == "retry_delay") {
            retry_delay = std::chrono::milliseconds(std::stoul(value));
        } else if (key == "enable_recovery") {
            enable_recovery = (value == "true" || value == "1");
        } else if (key == "enable_debug_logging") {
            enable_debug_logging = (value == "true" || value == "1");
        } else if (key == "log_level") {
            log_level = value;
        } else if (key.substr(0, 7) == "format.") {
            format_options[key.substr(7)] = value;
        }
    }
}

// MetadataExtensionRegistry implementation
MetadataExtensionRegistry& MetadataExtensionRegistry::getInstance() {
    static MetadataExtensionRegistry instance;
    return instance;
}

void MetadataExtensionRegistry::registerExtractor(const std::string& format_id, MetadataExtractor extractor) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_extractors[format_id] = extractor;
}

void MetadataExtensionRegistry::registerValidator(const std::string& format_id, MetadataValidator validator) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_validators[format_id] = validator;
}

void MetadataExtensionRegistry::registerConverter(const std::string& source_format, 
                                                 const std::string& target_format,
                                                 MetadataConverter converter) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_converters[{source_format, target_format}] = converter;
}

std::optional<ExtendedMetadata> MetadataExtensionRegistry::extractMetadata(
    const std::vector<uint8_t>& data, const std::string& format_id) const {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_extractors.find(format_id);
    if (it != m_extractors.end()) {
        try {
            return it->second(data, format_id);
        } catch (const std::exception& e) {
            // Log error and return nullopt
            return std::nullopt;
        }
    }
    return std::nullopt;
}

bool MetadataExtensionRegistry::validateMetadata(const ExtendedMetadata& metadata, 
                                                const std::string& format_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_validators.find(format_id);
    if (it != m_validators.end()) {
        try {
            return it->second(metadata, format_id);
        } catch (const std::exception& e) {
            return false;
        }
    }
    return true; // No validator means valid by default
}std::
optional<ExtendedMetadata> MetadataExtensionRegistry::convertMetadata(
    const ExtendedMetadata& source, const std::string& source_format, 
    const std::string& target_format) const {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_converters.find({source_format, target_format});
    if (it != m_converters.end()) {
        try {
            return it->second(source, target_format);
        } catch (const std::exception& e) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::vector<std::string> MetadataExtensionRegistry::getSupportedFormats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> formats;
    for (const auto& [format_id, extractor] : m_extractors) {
        formats.push_back(format_id);
    }
    return formats;
}

std::vector<std::pair<std::string, std::string>> MetadataExtensionRegistry::getSupportedConversions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<std::string, std::string>> conversions;
    for (const auto& [pair, converter] : m_converters) {
        conversions.push_back(pair);
    }
    return conversions;
}

void MetadataExtensionRegistry::unregisterFormat(const std::string& format_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_extractors.erase(format_id);
    m_validators.erase(format_id);
    
    // Remove converters involving this format
    auto it = m_converters.begin();
    while (it != m_converters.end()) {
        if (it->first.first == format_id || it->first.second == format_id) {
            it = m_converters.erase(it);
        } else {
            ++it;
        }
    }
}

// IOHandlerRegistry implementation
IOHandlerRegistry& IOHandlerRegistry::getInstance() {
    static IOHandlerRegistry instance;
    return instance;
}

IOHandlerRegistry::IOHandlerRegistry() {
    registerBuiltInHandlers();
}

void IOHandlerRegistry::registerIOHandler(const std::string& protocol, CustomIOHandlerFactory factory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_factories[protocol] = factory;
}

void IOHandlerRegistry::unregisterIOHandler(const std::string& protocol) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_factories.erase(protocol);
}

std::unique_ptr<IOHandler> IOHandlerRegistry::createIOHandler(
    const std::string& uri, const std::map<std::string, std::string>& config) const {
    
    std::string protocol = extractProtocol(uri);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_factories.find(protocol);
    if (it != m_factories.end()) {
        try {
            return it->second(uri, config);
        } catch (const std::exception& e) {
            return nullptr;
        }
    }
    return nullptr;
}

bool IOHandlerRegistry::supportsProtocol(const std::string& protocol) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_factories.find(protocol) != m_factories.end();
}

std::vector<std::string> IOHandlerRegistry::getSupportedProtocols() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> protocols;
    for (const auto& [protocol, factory] : m_factories) {
        protocols.push_back(protocol);
    }
    return protocols;
}

std::string IOHandlerRegistry::extractProtocol(const std::string& uri) {
    size_t pos = uri.find("://");
    if (pos != std::string::npos) {
        return uri.substr(0, pos);
    }
    return "file"; // Default to file protocol
}

void IOHandlerRegistry::registerBuiltInHandlers() {
    // Register built-in file handler
    registerIOHandler("file", [](const std::string& uri, const std::map<std::string, std::string>& config) {
        // Extract file path from URI
        std::string path = uri;
        if (path.substr(0, 7) == "file://") {
            path = path.substr(7);
        }
        return std::make_unique<FileIOHandler>(TagLib::String(path.c_str()));
    });
    
    // Register built-in HTTP handler
    registerIOHandler("http", [](const std::string& uri, const std::map<std::string, std::string>& config) {
        return std::make_unique<HTTPIOHandler>(uri);
    });
    
    registerIOHandler("https", [](const std::string& uri, const std::map<std::string, std::string>& config) {
        return std::make_unique<HTTPIOHandler>(uri);
    });
}

// StreamFactoryRegistry implementation
StreamFactoryRegistry& StreamFactoryRegistry::getInstance() {
    static StreamFactoryRegistry instance;
    return instance;
}

void StreamFactoryRegistry::registerStreamFactory(const std::string& format_id, CustomStreamFactory factory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_factories[format_id] = factory;
}

void StreamFactoryRegistry::unregisterStreamFactory(const std::string& format_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_factories.erase(format_id);
}

std::unique_ptr<Stream> StreamFactoryRegistry::createStream(
    const std::string& format_id, const std::string& uri, 
    const ContentInfo& info, const std::map<std::string, std::string>& config) const {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_factories.find(format_id);
    if (it != m_factories.end()) {
        try {
            return it->second(uri, info, config);
        } catch (const std::exception& e) {
            return nullptr;
        }
    }
    return nullptr;
}

bool StreamFactoryRegistry::hasCustomFactory(const std::string& format_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_factories.find(format_id) != m_factories.end();
}

std::vector<std::string> StreamFactoryRegistry::getCustomFormats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> formats;
    for (const auto& [format_id, factory] : m_factories) {
        formats.push_back(format_id);
    }
    return formats;
}

// DemuxerConfigManager implementation
DemuxerConfigManager& DemuxerConfigManager::getInstance() {
    static DemuxerConfigManager instance;
    return instance;
}

DemuxerConfigManager::DemuxerConfigManager() {
    m_config_search_paths = {
        "./config",
        "~/.config/psymp3",
        "/etc/psymp3",
        "/usr/local/etc/psymp3"
    };
}

const DemuxerConfig& DemuxerConfigManager::getGlobalConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_global_config;
}

void DemuxerConfigManager::setGlobalConfig(const DemuxerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_global_config = config;
}

DemuxerConfig DemuxerConfigManager::getFormatConfig(const std::string& format_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_format_configs.find(format_id);
    if (it != m_format_configs.end()) {
        return mergeConfigs(m_global_config, it->second);
    }
    return m_global_config;
}

void DemuxerConfigManager::setFormatConfig(const std::string& format_id, const DemuxerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_format_configs[format_id] = config;
}

DemuxerConfig DemuxerConfigManager::getInstanceConfig(const std::string& instance_id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_instance_configs.find(instance_id);
    if (it != m_instance_configs.end()) {
        return mergeConfigs(m_global_config, it->second);
    }
    return m_global_config;
}

void DemuxerConfigManager::setInstanceConfig(const std::string& instance_id, const DemuxerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_instance_configs[instance_id] = config;
}

bool DemuxerConfigManager::loadConfigFile(const std::string& config_file) {
    DemuxerConfig config;
    if (parseConfigFile(config_file, config)) {
        setGlobalConfig(config);
        return true;
    }
    return false;
}

bool DemuxerConfigManager::saveConfigFile(const std::string& config_file) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return writeConfigFile(config_file, m_global_config);
}

void DemuxerConfigManager::loadFromEnvironment() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_global_config.loadFromEnvironment();
}

void DemuxerConfigManager::resetToDefaults() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_global_config = DemuxerConfig{};
    m_format_configs.clear();
    m_instance_configs.clear();
}std::
vector<std::string> DemuxerConfigManager::getConfigSearchPaths() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config_search_paths;
}

void DemuxerConfigManager::setConfigSearchPaths(const std::vector<std::string>& paths) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config_search_paths = paths;
}

bool DemuxerConfigManager::autoLoadConfig() {
    for (const auto& path : getConfigSearchPaths()) {
        std::string config_file = path + "/demuxer.conf";
        if (loadConfigFile(config_file)) {
            return true;
        }
    }
    return false;
}

bool DemuxerConfigManager::validateConfigurations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_global_config.validate()) {
        return false;
    }
    
    for (const auto& [format_id, config] : m_format_configs) {
        if (!config.validate()) {
            return false;
        }
    }
    
    for (const auto& [instance_id, config] : m_instance_configs) {
        if (!config.validate()) {
            return false;
        }
    }
    
    return true;
}

DemuxerConfigManager::ConfigStats DemuxerConfigManager::getConfigStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    ConfigStats stats;
    stats.total_configs = 1 + m_format_configs.size() + m_instance_configs.size();
    stats.format_configs = m_format_configs.size();
    stats.instance_configs = m_instance_configs.size();
    stats.invalid_configs = 0;
    
    // Count invalid configs
    if (!m_global_config.validate()) {
        stats.invalid_configs++;
    }
    
    for (const auto& [format_id, config] : m_format_configs) {
        if (!config.validate()) {
            stats.invalid_configs++;
        }
    }
    
    for (const auto& [instance_id, config] : m_instance_configs) {
        if (!config.validate()) {
            stats.invalid_configs++;
        }
    }
    
    return stats;
}

DemuxerConfig DemuxerConfigManager::mergeConfigs(const DemuxerConfig& base, const DemuxerConfig& override) const {
    DemuxerConfig merged = base;
    
    // Override with non-default values
    if (override.max_buffer_size != DemuxerConfig{}.max_buffer_size) {
        merged.max_buffer_size = override.max_buffer_size;
    }
    if (override.chunk_size != DemuxerConfig{}.chunk_size) {
        merged.chunk_size = override.chunk_size;
    }
    if (override.read_ahead_size != DemuxerConfig{}.read_ahead_size) {
        merged.read_ahead_size = override.read_ahead_size;
    }
    if (override.enable_threading != DemuxerConfig{}.enable_threading) {
        merged.enable_threading = override.enable_threading;
    }
    if (override.max_threads != DemuxerConfig{}.max_threads) {
        merged.max_threads = override.max_threads;
    }
    
    // Merge format options
    for (const auto& [key, value] : override.format_options) {
        merged.format_options[key] = value;
    }
    
    return merged;
}

bool DemuxerConfigManager::parseConfigFile(const std::string& file_path, DemuxerConfig& config) const {
    // Simple implementation - in practice would parse JSON/INI/XML
    return false;
}

bool DemuxerConfigManager::writeConfigFile(const std::string& file_path, const DemuxerConfig& config) const {
    // Simple implementation - in practice would write JSON/INI/XML
    return false;
}

// DemuxerExtensionPoint implementation
DemuxerExtensionPoint& DemuxerExtensionPoint::getInstance() {
    static DemuxerExtensionPoint instance;
    return instance;
}

void DemuxerExtensionPoint::registerPreParsingHook(const std::string& format_id, PreParsingHook hook) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pre_parsing_hooks[format_id].push_back(hook);
}

void DemuxerExtensionPoint::registerPostParsingHook(const std::string& format_id, PostParsingHook hook) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_post_parsing_hooks[format_id].push_back(hook);
}

void DemuxerExtensionPoint::registerErrorHook(const std::string& format_id, ErrorHook hook) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_error_hooks[format_id].push_back(hook);
}

bool DemuxerExtensionPoint::executePreParsingHooks(const std::string& format_id,
                                                  IOHandler* handler,
                                                  const DemuxerConfig& config) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_pre_parsing_hooks.find(format_id);
    if (it != m_pre_parsing_hooks.end()) {
        for (const auto& hook : it->second) {
            try {
                if (!hook(format_id, handler, config)) {
                    return false; // Hook indicated failure
                }
            } catch (const std::exception& e) {
                return false; // Hook threw exception
            }
        }
    }
    return true;
}

void DemuxerExtensionPoint::executePostParsingHooks(const std::string& format_id,
                                                   const std::vector<StreamInfo>& streams,
                                                   const DemuxerConfig& config) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_post_parsing_hooks.find(format_id);
    if (it != m_post_parsing_hooks.end()) {
        for (const auto& hook : it->second) {
            try {
                hook(format_id, streams, config);
            } catch (const std::exception& e) {
                // Log error but continue with other hooks
            }
        }
    }
}

bool DemuxerExtensionPoint::executeErrorHooks(const std::string& format_id,
                                             const std::string& error_message,
                                             const DemuxerConfig& config) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_error_hooks.find(format_id);
    if (it != m_error_hooks.end()) {
        for (const auto& hook : it->second) {
            try {
                if (hook(format_id, error_message, config)) {
                    return true; // Hook handled the error
                }
            } catch (const std::exception& e) {
                // Hook threw exception, continue with other hooks
            }
        }
    }
    return false; // No hook handled the error
}

void DemuxerExtensionPoint::unregisterHooks(const std::string& format_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pre_parsing_hooks.erase(format_id);
    m_post_parsing_hooks.erase(format_id);
    m_error_hooks.erase(format_id);
}

// ConfigScope implementation
ConfigScope::ConfigScope(const std::string& format_id, const DemuxerConfig& temp_config)
    : m_format_id(format_id), m_is_global(false) {
    
    auto& manager = DemuxerConfigManager::getInstance();
    m_original_config = manager.getFormatConfig(format_id);
    manager.setFormatConfig(format_id, temp_config);
}

ConfigScope::ConfigScope(const DemuxerConfig& temp_global_config)
    : m_is_global(true) {
    
    auto& manager = DemuxerConfigManager::getInstance();
    m_original_config = manager.getGlobalConfig();
    manager.setGlobalConfig(temp_global_config);
}

ConfigScope::~ConfigScope() {
    auto& manager = DemuxerConfigManager::getInstance();
    if (m_is_global) {
        manager.setGlobalConfig(m_original_config);
    } else {
        manager.setFormatConfig(m_format_id, m_original_config);
    }
}

// ExtensibilityUtils implementation
namespace ExtensibilityUtils {

std::map<std::string, std::string> parseConfigString(const std::string& config_str) {
    std::map<std::string, std::string> config;
    
    // Simple key=value parsing
    std::istringstream iss(config_str);
    std::string pair;
    
    while (std::getline(iss, pair, ';')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            config[key] = value;
        }
    }
    
    return config;
}

std::string formatConfigString(const std::map<std::string, std::string>& config) {
    std::ostringstream oss;
    bool first = true;
    
    for (const auto& [key, value] : config) {
        if (!first) {
            oss << ";";
        }
        oss << key << "=" << value;
        first = false;
    }
    
    return oss.str();
}

bool isValidURI(const std::string& uri) {
    // Simple URI validation
    if (uri.empty()) return false;
    
    // Check for protocol
    size_t protocol_pos = uri.find("://");
    if (protocol_pos == std::string::npos) {
        // Assume file path
        return !uri.empty();
    }
    
    std::string protocol = uri.substr(0, protocol_pos);
    return !protocol.empty() && protocol_pos + 3 < uri.length();
}

std::map<std::string, std::string> extractURIParameters(const std::string& uri) {
    std::map<std::string, std::string> params;
    
    size_t query_pos = uri.find('?');
    if (query_pos != std::string::npos && query_pos + 1 < uri.length()) {
        std::string query = uri.substr(query_pos + 1);
        return parseConfigString(query.replace(query.find('&'), 1, ";"));
    }
    
    return params;
}

std::string buildURIWithParameters(const std::string& base_uri,
                                  const std::map<std::string, std::string>& params) {
    if (params.empty()) {
        return base_uri;
    }
    
    std::string result = base_uri;
    if (result.find('?') == std::string::npos) {
        result += "?";
    } else {
        result += "&";
    }
    
    std::string param_str = formatConfigString(params);
    std::replace(param_str.begin(), param_str.end(), ';', '&');
    result += param_str;
    
    return result;
}

DemuxerConfig getDefaultConfigForFormat(const std::string& format_id) {
    DemuxerConfig config;
    
    // Format-specific defaults
    if (format_id == "ogg") {
        config.chunk_size = 32 * 1024; // Smaller chunks for Ogg
        config.enable_recovery = true;
    } else if (format_id == "mp4") {
        config.chunk_size = 128 * 1024; // Larger chunks for MP4
        config.enable_caching = true;
    } else if (format_id == "flac") {
        config.chunk_size = 64 * 1024;
        config.strict_parsing = true;
    }
    
    return config;
}

std::map<std::string, std::string> mergeConfigMaps(
    const std::map<std::string, std::string>& base,
    const std::map<std::string, std::string>& override) {
    
    std::map<std::string, std::string> merged = base;
    for (const auto& [key, value] : override) {
        merged[key] = value;
    }
    return merged;
}

} // namespace ExtensibilityUtils
} // namespace Demuxer
} // namespace PsyMP3
