/*
 * test_demuxer_plugin_simple.cpp - Simple unit tests for demuxer plugin architecture
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

// Mock the required types for testing
struct StreamInfo {
    uint32_t stream_id = 0;
    std::string codec_type;
    std::string codec_name;
    uint32_t codec_tag = 0;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
    uint32_t bitrate = 0;
    std::vector<uint8_t> codec_data;
    uint64_t duration_samples = 0;
    uint64_t duration_ms = 0;
    std::string artist;
    std::string title;
    std::string album;
    
    StreamInfo() = default;
    StreamInfo(uint32_t id, const std::string& type, const std::string& name)
        : stream_id(id), codec_type(type), codec_name(name) {}
};

struct MediaFormat {
    std::string format_id;
    std::string display_name;
    std::vector<std::string> extensions;
    std::vector<std::string> mime_types;
    std::vector<std::string> magic_signatures;
    int priority = 100;
    bool supports_streaming = false;
    bool supports_seeking = true;
    bool is_container = false;
    std::string description;
};

struct ContentInfo {
    std::string detected_format;
    std::string mime_type;
    std::string file_extension;
    float confidence = 0.0f;
    std::map<std::string, std::string> metadata;
};

class IOHandler {
public:
    virtual ~IOHandler() = default;
    virtual size_t read(void* buffer, size_t size, size_t count) = 0;
    virtual int seek(long offset, int whence) = 0;
    virtual long tell() = 0;
    virtual int close() = 0;
    virtual bool eof() = 0;
};

class Demuxer {
public:
    explicit Demuxer(std::unique_ptr<IOHandler> handler) : m_handler(std::move(handler)) {}
    virtual ~Demuxer() = default;
    virtual bool parseContainer() = 0;
    virtual std::vector<StreamInfo> getStreams() const = 0;
    virtual StreamInfo getStreamInfo(uint32_t stream_id) const = 0;
    
protected:
    std::unique_ptr<IOHandler> m_handler;
};

class Stream {
public:
    virtual ~Stream() = default;
    virtual size_t getData(size_t len, void *buf) = 0;
    virtual void seekTo(unsigned long pos) = 0;
    virtual bool eof() = 0;
    virtual unsigned int getLength() = 0;
};

// Mock factory functions
using StreamFactory = std::function<std::unique_ptr<Stream>(const std::string& uri, const ContentInfo& info)>;
using ContentDetector = std::function<std::optional<ContentInfo>(std::unique_ptr<IOHandler>& handler)>;

namespace DemuxerFactory {
    using DemuxerFactoryFunc = std::function<std::unique_ptr<Demuxer>(std::unique_ptr<IOHandler>)>;
}

// Include the plugin architecture headers
#include "../include/DemuxerPlugin.h"

// Simple test functions
void testExtendedMetadata() {
    std::cout << "Testing ExtendedMetadata..." << std::endl;
    
    ExtendedMetadata metadata;
    metadata.format_id = "test_format";
    
    // Test string metadata
    metadata.setString("title", "Test Title");
    assert(metadata.getString("title") == "Test Title");
    assert(metadata.getString("nonexistent", "default") == "default");
    
    // Test numeric metadata
    metadata.setNumeric("duration", 12345);
    assert(metadata.getNumeric("duration") == 12345L);
    assert(metadata.getNumeric("nonexistent", 999) == 999L);
    
    // Test binary metadata
    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
    metadata.setBinary("codec_data", test_data);
    auto retrieved_data = metadata.getBinary("codec_data");
    assert(retrieved_data.size() == 4);
    assert(retrieved_data[0] == 0x01);
    
    // Test float metadata
    metadata.setFloat("sample_rate", 44100.0);
    assert(metadata.getFloat("sample_rate") == 44100.0);
    
    // Test key existence
    assert(metadata.hasKey("title"));
    assert(metadata.hasKey("duration"));
    assert(metadata.hasKey("codec_data"));
    assert(metadata.hasKey("sample_rate"));
    assert(!metadata.hasKey("nonexistent"));
    
    // Test get all keys
    auto all_keys = metadata.getAllKeys();
    assert(all_keys.size() == 4);
    
    // Test clear
    metadata.clear();
    assert(!metadata.hasKey("title"));
    assert(metadata.getAllKeys().size() == 0);
    
    std::cout << "ExtendedMetadata tests passed!" << std::endl;
}

void testExtendedStreamInfo() {
    std::cout << "Testing ExtendedStreamInfo..." << std::endl;
    
    // Create base StreamInfo
    StreamInfo base_info;
    base_info.stream_id = 1;
    base_info.codec_type = "audio";
    base_info.codec_name = "test_codec";
    base_info.sample_rate = 44100;
    base_info.channels = 2;
    
    // Create ExtendedStreamInfo
    ExtendedStreamInfo extended_info(base_info);
    
    // Verify base properties are preserved
    assert(extended_info.stream_id == 1);
    assert(extended_info.codec_type == "audio");
    assert(extended_info.codec_name == "test_codec");
    assert(extended_info.sample_rate == 44100);
    assert(extended_info.channels == 2);
    
    // Test format-specific metadata
    extended_info.setFormatMetadata("encoder", "Test Encoder v1.0");
    assert(extended_info.getFormatMetadata("encoder") == "Test Encoder v1.0");
    assert(extended_info.hasFormatMetadata("encoder"));
    assert(!extended_info.hasFormatMetadata("nonexistent"));
    
    std::cout << "ExtendedStreamInfo tests passed!" << std::endl;
}

void testPluginManagerSingleton() {
    std::cout << "Testing DemuxerPluginManager singleton..." << std::endl;
    
    // Test singleton pattern
    auto& manager1 = DemuxerPluginManager::getInstance();
    auto& manager2 = DemuxerPluginManager::getInstance();
    
    assert(&manager1 == &manager2);
    
    std::cout << "DemuxerPluginManager singleton test passed!" << std::endl;
}

void testPluginSearchPaths() {
    std::cout << "Testing plugin search paths..." << std::endl;
    
    auto& manager = DemuxerPluginManager::getInstance();
    
    // Get initial search paths
    auto initial_paths = manager.getPluginSearchPaths();
    assert(initial_paths.size() > 0);
    
    // Set custom search paths
    std::vector<std::string> custom_paths = {"/custom/path1", "/custom/path2"};
    manager.setPluginSearchPaths(custom_paths);
    
    // Verify paths were set
    auto current_paths = manager.getPluginSearchPaths();
    assert(current_paths.size() == 2);
    assert(current_paths[0] == "/custom/path1");
    assert(current_paths[1] == "/custom/path2");
    
    // Restore initial paths
    manager.setPluginSearchPaths(initial_paths);
    
    std::cout << "Plugin search paths test passed!" << std::endl;
}

void testPluginStats() {
    std::cout << "Testing plugin statistics..." << std::endl;
    
    auto& manager = DemuxerPluginManager::getInstance();
    
    // Get initial stats
    auto initial_stats = manager.getPluginStats();
    
    // Register a custom format to change stats
    MediaFormat test_format;
    test_format.format_id = "stats_test_format";
    test_format.display_name = "Stats Test Format";
    
    auto factory_func = [](std::unique_ptr<IOHandler> handler) -> std::unique_ptr<Demuxer> {
        return nullptr;
    };
    
    bool registered = manager.registerCustomDemuxer("stats_test_format", factory_func, test_format);
    assert(registered);
    
    // Get updated stats
    auto updated_stats = manager.getPluginStats();
    
    // Verify stats changed
    assert(updated_stats.total_formats_registered == initial_stats.total_formats_registered + 1);
    
    // Clean up
    manager.unregisterCustomFormat("stats_test_format");
    
    std::cout << "Plugin statistics test passed!" << std::endl;
}

int main() {
    std::cout << "Running DemuxerPlugin tests..." << std::endl;
    
    try {
        testExtendedMetadata();
        testExtendedStreamInfo();
        testPluginManagerSingleton();
        testPluginSearchPaths();
        testPluginStats();
        
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}