#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <memory>
#include <optional>
#include <algorithm>
#include <cassert>

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

void test_getAllKeys_correctness() {
    ExtendedMetadata meta;
    meta.string_metadata["s1"] = "v1";
    meta.string_metadata["s2"] = "v2";
    meta.numeric_metadata["n1"] = 1;
    meta.numeric_metadata["n2"] = 2;
    meta.binary_metadata["b1"] = {1};
    meta.binary_metadata["b2"] = {2};
    meta.float_metadata["f1"] = 1.0;
    meta.float_metadata["f2"] = 2.0;

    std::vector<std::string> keys = meta.getAllKeys();

    assert(keys.size() == 8);

    std::vector<std::string> expected = {"s1", "s2", "n1", "n2", "b1", "b2", "f1", "f2"};

    // Maps are sorted by key, and we push them in a specific order: string, numeric, binary, float.
    // However, the test should just ensure all expected keys are present.
    for (const auto& key : expected) {
        assert(std::find(keys.begin(), keys.end(), key) != keys.end());
    }

    std::cout << "test_getAllKeys_correctness passed!" << std::endl;
}

void test_getAllKeys_empty() {
    ExtendedMetadata meta;
    std::vector<std::string> keys = meta.getAllKeys();
    assert(keys.empty());
    std::cout << "test_getAllKeys_empty passed!" << std::endl;
}

int main() {
    test_getAllKeys_correctness();
    test_getAllKeys_empty();
    return 0;
}
