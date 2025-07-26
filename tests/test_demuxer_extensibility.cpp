/*
 * test_demuxer_extensibility.cpp - Tests for demuxer extensibility features
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <optional>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <set>
#include <sstream>

// Mock the required types for testing
struct ExtendedMetadata {
    std::string format_id;
    std::map<std::string, std::string> string_metadata;
    std::map<std::string, int64_t> numeric_metadata;
    std::map<std::string, std::vector<uint8_t>> binary_metadata;
    std::map<std::string, double> float_metadata;
    
    void setString(const std::string& key, const std::string& value) {
        string_metadata[key] = value;
    }
    
    std::string getString(const std::string& key, const std::string& default_value = "") const {
        auto it = string_metadata.find(key);
        return (it != string_metadata.end()) ? it->second : default_value;
    }
    
    void setNumeric(const std::string& key, int64_t value) {
        numeric_metadata[key] = value;
    }
    
    int64_t getNumeric(const std::string& key, int64_t default_value = 0) const {
        auto it = numeric_metadata.find(key);
        return (it != numeric_metadata.end()) ? it->second : default_value;
    }
    
    void setBinary(const std::string& key, const std::vector<uint8_t>& value) {
        binary_metadata[key] = value;
    }
    
    std::vector<uint8_t> getBinary(const std::string& key) const {
        auto it = binary_metadata.find(key);
        return (it != binary_metadata.end()) ? it->second : std::vector<uint8_t>();
    }
    
    void setFloat(const std::string& key, double value) {
        float_metadata[key] = value;
    }
    
    double getFloat(const std::string& key, double default_value = 0.0) const {
        auto it = float_metadata.find(key);
        return (it != float_metadata.end()) ? it->second : default_value;
    }
    
    bool hasKey(const std::string& key) const {
        return string_metadata.find(key) != string_metadata.end() ||
               numeric_metadata.find(key) != numeric_metadata.end() ||
               binary_metadata.find(key) != binary_metadata.end() ||
               float_metadata.find(key) != float_metadata.end();
    }
    
    void clear() {
        string_metadata.clear();
        numeric_metadata.clear();
        binary_metadata.clear();
        float_metadata.clear();
    }
};

// Test DemuxerConfig
struct DemuxerConfig {
    size_t max_buffer_size = 1024 * 1024;
    size_t chunk_size = 64 * 1024;
    size_t read_ahead_size = 256 * 1024;
    bool enable_threading = true;
    size_t max_threads = 4;
    bool enable_caching = true;
    size_t cache_size = 512 * 1024;
    bool strict_parsing = false;
    size_t max_retries = 3;
    std::chrono::milliseconds retry_delay{100};
    bool enable_recovery = true;
    std::map<std::string, std::string> format_options;
    bool enable_debug_logging = false;
    std::string log_level = "info";
    std::vector<std::string> debug_categories;
    
    void setFormatOption(const std::string& format_id, const std::string& key, const std::string& value) {
        format_options[format_id + "." + key] = value;
    }
    
    std::string getFormatOption(const std::string& format_id, const std::string& key, 
                               const std::string& default_value = "") const {
        auto it = format_options.find(format_id + "." + key);
        return (it != format_options.end()) ? it->second : default_value;
    }
    
    bool hasFormatOption(const std::string& format_id, const std::string& key) const {
        return format_options.find(format_id + "." + key) != format_options.end();
    }
    
    bool validate() const {
        if (max_buffer_size == 0 || max_buffer_size > 100 * 1024 * 1024) {
            return false;
        }
        if (chunk_size == 0 || chunk_size > max_buffer_size) {
            return false;
        }
        if (max_threads == 0 || max_threads > 64) {
            return false;
        }
        return true;
    }
    
    std::map<std::string, std::string> toMap() const {
        std::map<std::string, std::string> config_map;
        config_map["max_buffer_size"] = std::to_string(max_buffer_size);
        config_map["chunk_size"] = std::to_string(chunk_size);
        config_map["enable_threading"] = enable_threading ? "true" : "false";
        config_map["max_threads"] = std::to_string(max_threads);
        config_map["strict_parsing"] = strict_parsing ? "true" : "false";
        
        for (const auto& [key, value] : format_options) {
            config_map["format." + key] = value;
        }
        
        return config_map;
    }
    
    void fromMap(const std::map<std::string, std::string>& config_map) {
        for (const auto& [key, value] : config_map) {
            if (key == "max_buffer_size") {
                max_buffer_size = std::stoul(value);
            } else if (key == "chunk_size") {
                chunk_size = std::stoul(value);
            } else if (key == "enable_threading") {
                enable_threading = (value == "true" || value == "1");
            } else if (key == "max_threads") {
                max_threads = std::stoul(value);
            } else if (key == "strict_parsing") {
                strict_parsing = (value == "true" || value == "1");
            } else if (key.substr(0, 7) == "format.") {
                format_options[key.substr(7)] = value;
            }
        }
    }
};

// Test MetadataExtensionRegistry
class MetadataExtensionRegistry {
public:
    using MetadataExtractor = std::function<ExtendedMetadata(const std::vector<uint8_t>&, const std::string&)>;
    using MetadataValidator = std::function<bool(const ExtendedMetadata&, const std::string&)>;
    using MetadataConverter = std::function<ExtendedMetadata(const ExtendedMetadata&, const std::string&)>;
    
    static MetadataExtensionRegistry& getInstance() {
        static MetadataExtensionRegistry instance;
        return instance;
    }
    
    void registerExtractor(const std::string& format_id, MetadataExtractor extractor) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_extractors[format_id] = extractor;
    }
    
    void registerValidator(const std::string& format_id, MetadataValidator validator) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_validators[format_id] = validator;
    }
    
    void registerConverter(const std::string& source_format, const std::string& target_format,
                          MetadataConverter converter) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_converters[{source_format, target_format}] = converter;
    }
    
    std::optional<ExtendedMetadata> extractMetadata(const std::vector<uint8_t>& data,
                                                   const std::string& format_id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_extractors.find(format_id);
        if (it != m_extractors.end()) {
            try {
                return it->second(data, format_id);
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
    
    bool validateMetadata(const ExtendedMetadata& metadata, const std::string& format_id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_validators.find(format_id);
        if (it != m_validators.end()) {
            try {
                return it->second(metadata, format_id);
            } catch (...) {
                return false;
            }
        }
        return true;
    }
    
    std::vector<std::string> getSupportedFormats() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> formats;
        for (const auto& [format_id, extractor] : m_extractors) {
            formats.push_back(format_id);
        }
        return formats;
    }
    
    void unregisterFormat(const std::string& format_id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_extractors.erase(format_id);
        m_validators.erase(format_id);
    }
    
private:
    mutable std::mutex m_mutex;
    std::map<std::string, MetadataExtractor> m_extractors;
    std::map<std::string, MetadataValidator> m_validators;
    std::map<std::pair<std::string, std::string>, MetadataConverter> m_converters;
};

// Test functions
void testDemuxerConfig() {
    std::cout << "Testing DemuxerConfig..." << std::endl;
    
    DemuxerConfig config;
    
    // Test default values
    assert(config.max_buffer_size == 1024 * 1024);
    assert(config.chunk_size == 64 * 1024);
    assert(config.enable_threading == true);
    assert(config.max_threads == 4);
    assert(config.strict_parsing == false);
    
    // Test validation
    assert(config.validate());
    
    // Test invalid config
    config.max_buffer_size = 0;
    assert(!config.validate());
    config.max_buffer_size = 1024 * 1024; // Reset
    
    // Test format options
    config.setFormatOption("ogg", "quality", "high");
    assert(config.hasFormatOption("ogg", "quality"));
    assert(config.getFormatOption("ogg", "quality") == "high");
    assert(config.getFormatOption("ogg", "nonexistent", "default") == "default");
    
    // Test serialization
    auto config_map = config.toMap();
    assert(config_map["max_buffer_size"] == "1048576");
    assert(config_map["enable_threading"] == "true");
    assert(config_map["format.ogg.quality"] == "high");
    
    // Test deserialization
    DemuxerConfig config2;
    config2.fromMap(config_map);
    assert(config2.max_buffer_size == config.max_buffer_size);
    assert(config2.enable_threading == config.enable_threading);
    assert(config2.getFormatOption("ogg", "quality") == "high");
    
    std::cout << "DemuxerConfig tests passed!" << std::endl;
}

void testMetadataExtensionRegistry() {
    std::cout << "Testing MetadataExtensionRegistry..." << std::endl;
    
    auto& registry = MetadataExtensionRegistry::getInstance();
    
    // Test singleton
    auto& registry2 = MetadataExtensionRegistry::getInstance();
    assert(&registry == &registry2);
    
    // Register test extractor
    registry.registerExtractor("test_format", [](const std::vector<uint8_t>& data, const std::string& format_id) {
        ExtendedMetadata metadata;
        metadata.format_id = format_id;
        metadata.setString("extracted", "true");
        metadata.setNumeric("data_size", static_cast<int64_t>(data.size()));
        return metadata;
    });
    
    // Register test validator
    registry.registerValidator("test_format", [](const ExtendedMetadata& metadata, const std::string& format_id) {
        return metadata.hasKey("extracted") && metadata.getString("extracted") == "true";
    });
    
    // Test extraction
    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
    auto extracted = registry.extractMetadata(test_data, "test_format");
    assert(extracted.has_value());
    assert(extracted->format_id == "test_format");
    assert(extracted->getString("extracted") == "true");
    assert(extracted->getNumeric("data_size") == 4);
    
    // Test validation
    assert(registry.validateMetadata(*extracted, "test_format"));
    
    ExtendedMetadata invalid_metadata;
    invalid_metadata.format_id = "test_format";
    assert(!registry.validateMetadata(invalid_metadata, "test_format"));
    
    // Test supported formats
    auto formats = registry.getSupportedFormats();
    assert(formats.size() >= 1);
    assert(std::find(formats.begin(), formats.end(), "test_format") != formats.end());
    
    // Test unregistration
    registry.unregisterFormat("test_format");
    auto extracted_after = registry.extractMetadata(test_data, "test_format");
    assert(!extracted_after.has_value());
    
    std::cout << "MetadataExtensionRegistry tests passed!" << std::endl;
}

void testExtensibilityUtils() {
    std::cout << "Testing ExtensibilityUtils..." << std::endl;
    
    // Test config string parsing
    std::string config_str = "key1=value1;key2=value2;key3=value3";
    std::map<std::string, std::string> expected = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"}
    };
    
    // Simple parsing function for test
    auto parseConfigString = [](const std::string& config_str) {
        std::map<std::string, std::string> config;
        std::istringstream iss(config_str);
        std::string pair;
        
        while (std::getline(iss, pair, ';')) {
            size_t eq_pos = pair.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = pair.substr(0, eq_pos);
                std::string value = pair.substr(eq_pos + 1);
                config[key] = value;
            }
        }
        return config;
    };
    
    auto parsed = parseConfigString(config_str);
    assert(parsed.size() == 3);
    assert(parsed["key1"] == "value1");
    assert(parsed["key2"] == "value2");
    assert(parsed["key3"] == "value3");
    
    // Test URI validation
    auto isValidURI = [](const std::string& uri) {
        if (uri.empty()) return false;
        size_t protocol_pos = uri.find("://");
        if (protocol_pos == std::string::npos) {
            return !uri.empty(); // Assume file path
        }
        std::string protocol = uri.substr(0, protocol_pos);
        return !protocol.empty() && protocol_pos + 3 < uri.length();
    };
    
    assert(isValidURI("http://example.com/file.mp3"));
    assert(isValidURI("file:///path/to/file.mp3"));
    assert(isValidURI("/path/to/file.mp3"));
    assert(!isValidURI(""));
    assert(!isValidURI("http://"));
    
    std::cout << "ExtensibilityUtils tests passed!" << std::endl;
}

int main() {
    std::cout << "Running demuxer extensibility tests..." << std::endl;
    
    try {
        testDemuxerConfig();
        testMetadataExtensionRegistry();
        testExtensibilityUtils();
        
        std::cout << "All extensibility tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}