/*
 * test_flac_stream_marker_with_logging.cpp - Tests for FLAC stream marker validation with logging
 * This file is part of PsyMP3.
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>

// Mock the log function before including the actual code
namespace Debug {
    std::vector<std::string> last_logs;

    template<typename... Args>
    inline void log(const char* channel, Args... args) {
        std::ostringstream oss;
        (oss << ... << args);
        std::string message = oss.str();
        last_logs.push_back(std::string("[") + channel + "] " + message);
        std::cout << "Mock Log: [" << channel << "] " << message << std::endl;
    }
}

// Ensure psymp3.h is NOT included by defining the header guard!
#define __PSYMP3_H__

// However, FLACRFC9639.cpp has: #include "psymp3.h"
// Which means it will just skip it.
// We then need to provide any missing types that FLACRFC9639.cpp relies on.

// Since FLACRFC9639.h is included in FLACRFC9639.cpp, let's include it here
// but we define TEST_MOCK_ENV so FLACRFC9639.h doesn't include psymp3.h either!
#define TEST_MOCK_ENV
#include "codecs/flac/FLACRFC9639.h"

// Forward declare what's missing
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace PsyMP3 {
    class SDLException : public std::exception {};
}

// Now include the source file!
#include "../src/codecs/flac/FLACRFC9639.cpp"

void run_tests() {
    std::cout << "Testing validateStreamMarkerWithLogging..." << std::endl;

    // Test 1: Valid marker should return true and log a success message
    {
        uint8_t valid_marker[4] = {'f', 'L', 'a', 'C'};
        Debug::last_logs.clear();

        bool result = FLACRFC9639::validateStreamMarkerWithLogging(valid_marker, "test_channel");

        assert(result == true);
        assert(Debug::last_logs.size() == 1);
        assert(Debug::last_logs[0] == "[test_channel] [validateStreamMarker] Valid FLAC stream marker found: fLaC");
        std::cout << "  ✓ Valid marker test passed" << std::endl;
    }

    // Test 2: Invalid marker should return false and log an error
    {
        uint8_t invalid_marker[4] = {'f', 'l', 'a', 'c'};
        Debug::last_logs.clear();

        bool result = FLACRFC9639::validateStreamMarkerWithLogging(invalid_marker, "test_channel");

        assert(result == false);
        // Ensure at least some logs were written, meaning errors were raised
        assert(Debug::last_logs.size() > 0);
        // Check the exact log from the source file
        assert(Debug::last_logs[0].find("[validateStreamMarker] Invalid FLAC stream marker") != std::string::npos);
        std::cout << "  ✓ Invalid marker test passed" << std::endl;
    }

    // Test 3: Nullptr marker should return false and log a specific error
    {
        Debug::last_logs.clear();

        bool result = FLACRFC9639::validateStreamMarkerWithLogging(nullptr, "test_channel");

        assert(result == false);
        assert(Debug::last_logs.size() == 1);
        assert(Debug::last_logs[0] == "[test_channel] [validateStreamMarker] nullptr marker pointer provided");
        std::cout << "  ✓ Nullptr marker test passed" << std::endl;
    }

    // Test 4: Default debug channel parameter
    {
        uint8_t invalid_marker[4] = {'0', '0', '0', '0'};
        Debug::last_logs.clear();

        bool result = FLACRFC9639::validateStreamMarkerWithLogging(invalid_marker);

        assert(result == false);
        assert(Debug::last_logs.size() > 0);
        assert(Debug::last_logs[0].find("[flac] [validateStreamMarker]") != std::string::npos);
        std::cout << "  ✓ Default debug channel test passed" << std::endl;
    }

    std::cout << "\nAll validateStreamMarkerWithLogging tests passed!" << std::endl;
}

int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "FLAC STREAM MARKER WITH LOGGING VALIDATION TESTS" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    run_tests();
    return 0;
}
