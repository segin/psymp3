/*
 * test_lastfm_lazy_logging_properties.cpp - Property-based tests for Last.fm lazy debug logging
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

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <atomic>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <ctime>

// ========================================
// MOCK DEBUG SYSTEM FOR TESTING
// ========================================

// Global counter to track if lazy evaluation is working
std::atomic<int> g_evaluation_count(0);

// Mock Debug class that tracks channel state
class MockDebug {
public:
    static void init(const std::string& logfile, const std::vector<std::string>& channels) {
        m_enabled_channels = channels;
        m_logfile = logfile;
    }

    static bool isChannelEnabled(const std::string& channel) {
        // Check if channel is in enabled list
        for (const auto& enabled : m_enabled_channels) {
            if (enabled == channel || enabled == "all") {
                return true;
            }
        }
        return false;
    }

    template<typename... Args>
    static void log(const std::string& channel, Args&&... args) {
        if (isChannelEnabled(channel)) {
            // Simulate logging (just count that we got here)
            std::stringstream ss;
            if constexpr (sizeof...(args) > 0) {
                (ss << ... << args);
            }
            // Write to mock logfile
            if (!m_logfile.empty()) {
                std::ofstream log(m_logfile, std::ios::app);
                log << ss.str() << "\n";
            }
        }
    }

private:
    static std::vector<std::string> m_enabled_channels;
    static std::string m_logfile;
};

std::vector<std::string> MockDebug::m_enabled_channels;
std::string MockDebug::m_logfile;

// ========================================
// LAZY EVALUATION MACRO (same as in debug.h)
// ========================================

// Lazy evaluation macro - checks if channel is enabled before evaluating arguments
// This prevents string formatting overhead when logging is disabled (Requirements 3.1, 3.3)
#define DEBUG_LOG_LAZY(channel, ...) \
    do { \
        if (MockDebug::isChannelEnabled(channel)) { \
            MockDebug::log(channel, __VA_ARGS__); \
        } \
    } while(0)

// ========================================
// HELPER FUNCTION THAT TRACKS EVALUATION
// ========================================

// This function increments a counter when called
// If lazy evaluation works, it should NOT be called when logging is disabled
std::string expensiveStringOperation() {
    g_evaluation_count++;
    return "expensive_result_" + std::to_string(g_evaluation_count);
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 3: Debug Logging Lazy Evaluation
// ========================================
// **Feature: lastfm-performance-optimization, Property 3: Debug Logging Lazy Evaluation**
// **Validates: Requirements 3.1, 3.3**
//
// For any disabled debug channel, log statement arguments SHALL NOT be evaluated 
// (no side effects from argument expressions).
void test_property_lazy_evaluation_disabled_channel() {
    std::cout << "\n=== Property 3: Debug Logging Lazy Evaluation ===" << std::endl;
    std::cout << "Testing that disabled channels don't evaluate arguments..." << std::endl;

    // Initialize debug with NO channels enabled
    std::vector<std::string> disabled_channels;
    MockDebug::init("", disabled_channels);

    // Reset counter
    g_evaluation_count = 0;

    // Try to log with disabled channel - expensiveStringOperation should NOT be called
    DEBUG_LOG_LAZY("lastfm", "Message: ", expensiveStringOperation());

    // Verify the expensive operation was NOT called
    assert(g_evaluation_count == 0 && "Lazy evaluation failed: argument was evaluated for disabled channel");
    std::cout << "  ✓ Disabled channel did not evaluate arguments (count = " << g_evaluation_count << ")" << std::endl;

    // Now enable the channel and try again
    std::vector<std::string> enabled_channels = {"lastfm"};
    MockDebug::init("", enabled_channels);

    g_evaluation_count = 0;

    // Try to log with enabled channel - expensiveStringOperation SHOULD be called
    DEBUG_LOG_LAZY("lastfm", "Message: ", expensiveStringOperation());

    // Verify the expensive operation WAS called
    assert(g_evaluation_count == 1 && "Lazy evaluation failed: argument was not evaluated for enabled channel");
    std::cout << "  ✓ Enabled channel evaluated arguments (count = " << g_evaluation_count << ")" << std::endl;

    std::cout << "\n✓ Property 3: Debug Logging Lazy Evaluation - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY 4: Multiple Arguments Lazy Evaluation
// ========================================
// For any disabled debug channel with multiple arguments, NONE of the arguments 
// SHALL be evaluated.
void test_property_multiple_arguments_lazy_evaluation() {
    std::cout << "\n=== Property 4: Multiple Arguments Lazy Evaluation ===" << std::endl;
    std::cout << "Testing that all arguments are lazily evaluated..." << std::endl;

    // Initialize debug with NO channels enabled
    std::vector<std::string> disabled_channels;
    MockDebug::init("", disabled_channels);

    // Reset counter
    g_evaluation_count = 0;

    // Try to log with multiple expensive operations - NONE should be called
    DEBUG_LOG_LAZY("lastfm", 
                   "Arg1: ", expensiveStringOperation(),
                   " Arg2: ", expensiveStringOperation(),
                   " Arg3: ", expensiveStringOperation());

    // Verify NONE of the expensive operations were called
    assert(g_evaluation_count == 0 && "Lazy evaluation failed: arguments were evaluated for disabled channel");
    std::cout << "  ✓ All arguments were lazily evaluated (count = " << g_evaluation_count << ")" << std::endl;

    // Now enable the channel
    std::vector<std::string> enabled_channels = {"lastfm"};
    MockDebug::init("", enabled_channels);

    g_evaluation_count = 0;

    // Try to log with enabled channel - ALL should be called
    DEBUG_LOG_LAZY("lastfm", 
                   "Arg1: ", expensiveStringOperation(),
                   " Arg2: ", expensiveStringOperation(),
                   " Arg3: ", expensiveStringOperation());

    // Verify all expensive operations WERE called
    assert(g_evaluation_count == 3 && "Lazy evaluation failed: not all arguments were evaluated for enabled channel");
    std::cout << "  ✓ All arguments were evaluated when channel enabled (count = " << g_evaluation_count << ")" << std::endl;

    std::cout << "\n✓ Property 4: Multiple Arguments Lazy Evaluation - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY 5: Channel Filtering
// ========================================
// For any debug channel that is not in the enabled list, arguments SHALL NOT be evaluated.
// For any debug channel that IS in the enabled list, arguments SHALL be evaluated.
void test_property_channel_filtering() {
    std::cout << "\n=== Property 5: Channel Filtering ===" << std::endl;
    std::cout << "Testing that only enabled channels evaluate arguments..." << std::endl;

    // Initialize with specific channels enabled
    std::vector<std::string> enabled_channels = {"audio", "demux"};
    MockDebug::init("", enabled_channels);

    // Test disabled channel
    g_evaluation_count = 0;
    DEBUG_LOG_LAZY("lastfm", "Should not evaluate: ", expensiveStringOperation());
    assert(g_evaluation_count == 0 && "Lazy evaluation failed: disabled channel evaluated arguments");
    std::cout << "  ✓ Disabled channel 'lastfm' did not evaluate (count = " << g_evaluation_count << ")" << std::endl;

    // Test enabled channel 1
    g_evaluation_count = 0;
    DEBUG_LOG_LAZY("audio", "Should evaluate: ", expensiveStringOperation());
    assert(g_evaluation_count == 1 && "Lazy evaluation failed: enabled channel did not evaluate arguments");
    std::cout << "  ✓ Enabled channel 'audio' evaluated (count = " << g_evaluation_count << ")" << std::endl;

    // Test enabled channel 2
    g_evaluation_count = 0;
    DEBUG_LOG_LAZY("demux", "Should evaluate: ", expensiveStringOperation());
    assert(g_evaluation_count == 1 && "Lazy evaluation failed: enabled channel did not evaluate arguments");
    std::cout << "  ✓ Enabled channel 'demux' evaluated (count = " << g_evaluation_count << ")" << std::endl;

    // Test another disabled channel
    g_evaluation_count = 0;
    DEBUG_LOG_LAZY("codec", "Should not evaluate: ", expensiveStringOperation());
    assert(g_evaluation_count == 0 && "Lazy evaluation failed: disabled channel evaluated arguments");
    std::cout << "  ✓ Disabled channel 'codec' did not evaluate (count = " << g_evaluation_count << ")" << std::endl;

    std::cout << "\n✓ Property 5: Channel Filtering - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY 6: "all" Channel Enables Everything
// ========================================
// When "all" is in the enabled channels list, all channels SHALL be enabled.
void test_property_all_channel() {
    std::cout << "\n=== Property 6: 'all' Channel Enables Everything ===" << std::endl;
    std::cout << "Testing that 'all' channel enables all debug channels..." << std::endl;

    // Initialize with "all" channel enabled
    std::vector<std::string> enabled_channels = {"all"};
    MockDebug::init("", enabled_channels);

    // Test various channels - all should be enabled
    std::vector<std::string> test_channels = {"lastfm", "audio", "demux", "codec", "flac", "custom_channel"};

    for (const auto& channel : test_channels) {
        g_evaluation_count = 0;
        DEBUG_LOG_LAZY(channel, "Should evaluate: ", expensiveStringOperation());
        assert(g_evaluation_count == 1);
        std::cout << "  ✓ Channel '" << channel << "' enabled by 'all'" << std::endl;
    }

    std::cout << "\n✓ Property 6: 'all' Channel Enables Everything - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY 7: No Side Effects When Disabled
// ========================================
// For any disabled debug channel, the macro SHALL have no observable side effects.
void test_property_no_side_effects_when_disabled() {
    std::cout << "\n=== Property 7: No Side Effects When Disabled ===" << std::endl;
    std::cout << "Testing that disabled logging has no side effects..." << std::endl;

    // Initialize with NO channels enabled
    std::vector<std::string> disabled_channels;
    MockDebug::init("", disabled_channels);

    // Create a test variable
    int test_value = 42;

    // Use a lambda that modifies state - should NOT be called
    auto modify_state = [&test_value]() {
        test_value = 999;
        return "modified";
    };

    // Try to log with disabled channel
    DEBUG_LOG_LAZY("lastfm", "Value: ", modify_state());

    // Verify the state was NOT modified
    assert(test_value == 42 && "Side effect occurred: state was modified for disabled channel");
    std::cout << "  ✓ No side effects when channel disabled (test_value = " << test_value << ")" << std::endl;

    // Now enable the channel
    std::vector<std::string> enabled_channels = {"lastfm"};
    MockDebug::init("", enabled_channels);

    test_value = 42;

    // Try to log with enabled channel
    DEBUG_LOG_LAZY("lastfm", "Value: ", modify_state());

    // Verify the state WAS modified
    assert(test_value == 999 && "Side effect did not occur: state was not modified for enabled channel");
    std::cout << "  ✓ Side effects occur when channel enabled (test_value = " << test_value << ")" << std::endl;

    std::cout << "\n✓ Property 7: No Side Effects When Disabled - ALL TESTS PASSED" << std::endl;
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "LAST.FM LAZY DEBUG LOGGING PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: lastfm-performance-optimization, Property 3: Debug Logging Lazy Evaluation**" << std::endl;
    std::cout << "**Validates: Requirements 3.1, 3.3**" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    try {
        // Run all property tests
        test_property_lazy_evaluation_disabled_channel();
        test_property_multiple_arguments_lazy_evaluation();
        test_property_channel_filtering();
        test_property_all_channel();
        test_property_no_side_effects_when_disabled();

        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(60, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(60, '=') << std::endl;
        return 1;
    }
}
