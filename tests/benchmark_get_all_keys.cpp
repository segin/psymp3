#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <memory>
#include <optional>

// Mock types needed by DemuxerPlugin.h
namespace PsyMP3 {
    namespace Demuxer {
        struct StreamInfo {
            StreamInfo() = default;
        };

        struct ContentInfo {};

        namespace DemuxerFactory {
            typedef void* (*DemuxerFactoryFunc)(void*);
        }

        typedef std::optional<ContentInfo> (*ContentDetector)(void*);

        struct MediaFormat {
            std::string format_id;
            std::string display_name;
            std::string description;
            int32_t priority;
            std::vector<std::string> extensions;
            std::vector<std::string> magic_signatures;
            bool supports_streaming;
            bool supports_seeking;
        };
    }
}

#include "include/demuxer/DemuxerPlugin.h"

using namespace PsyMP3::Demuxer;

// Baseline implementation (no reserve)
std::vector<std::string> getAllKeys_Baseline(const ExtendedMetadata& meta) {
    std::vector<std::string> keys;
    for (const auto& [key, value] : meta.string_metadata) keys.push_back(key);
    for (const auto& [key, value] : meta.numeric_metadata) keys.push_back(key);
    for (const auto& [key, value] : meta.binary_metadata) keys.push_back(key);
    for (const auto& [key, value] : meta.float_metadata) keys.push_back(key);
    return keys;
}

int main() {
    ExtendedMetadata meta;
    for (int i = 0; i < 100; ++i) {
        meta.string_metadata["string_key_" + std::to_string(i)] = "value";
        meta.numeric_metadata["numeric_key_" + std::to_string(i)] = i;
        meta.binary_metadata["binary_key_" + std::to_string(i)] = {static_cast<uint8_t>(i)};
        meta.float_metadata["float_key_" + std::to_string(i)] = static_cast<double>(i);
    }

    const int iterations = 100000;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto keys = getAllKeys_Baseline(meta);
        volatile size_t s = keys.size();
        (void)s;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> baseline_duration = end - start;
    std::cout << "Baseline (No Reserve): " << baseline_duration.count() << " ms" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto keys = meta.getAllKeys();
        volatile size_t s = keys.size();
        (void)s;
    }
    end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> optimized_duration = end - start;
    std::cout << "Optimized (With Reserve): " << optimized_duration.count() << " ms" << std::endl;

    return 0;
}
