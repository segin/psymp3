/*
 * test_lastfm_config_roundtrip_properties.cpp - Property-based tests for Last.fm configuration round-trip
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
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdlib>
#include <random>
#include <unistd.h>

// ========================================
// CONFIGURATION DATA STRUCTURE
// ========================================

struct ConfigData {
    std::string username;
    std::string password;
    std::string session_key;
    std::string now_playing_url;
    std::string submission_url;
    
    bool operator==(const ConfigData& other) const {
        return username == other.username &&
               password == other.password &&
               session_key == other.session_key &&
               now_playing_url == other.now_playing_url &&
               submission_url == other.submission_url;
    }
    
    bool operator!=(const ConfigData& other) const {
        return !(*this == other);
    }
};

// ========================================
// CONFIGURATION FILE I/O
// ========================================

ConfigData parseConfigFile(const std::string& filename) {
    ConfigData config;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + filename);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        
        std::string key = line.substr(0, equals);
        std::string value = line.substr(equals + 1);
        
        if (key == "username") {
            config.username = value;
        } else if (key == "password") {
            config.password = value;
        } else if (key == "session_key") {
            config.session_key = value;
        } else if (key == "now_playing_url") {
            config.now_playing_url = value;
        } else if (key == "submission_url") {
            config.submission_url = value;
        }
    }
    
    return config;
}

void writeConfigFile(const std::string& filename, const ConfigData& config) {
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file for writing: " + filename);
    }
    
    file << "# Last.fm configuration\n";
    file << "username=" << config.username << "\n";
    file << "password=" << config.password << "\n";
    file << "session_key=" << config.session_key << "\n";
    file << "now_playing_url=" << config.now_playing_url << "\n";
    file << "submission_url=" << config.submission_url << "\n";
}

// ========================================
// RANDOM DATA GENERATORS
// ========================================

/**
 * Generate a random string with printable ASCII characters
 * Excludes newlines and equals signs to avoid breaking config format
 */
std::string generateRandomString(size_t max_length) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    std::uniform_int_distribution<> len_dist(0, max_length);
    std::uniform_int_distribution<> char_dist(32, 126);  // Printable ASCII
    
    size_t length = len_dist(gen);
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        char c = static_cast<char>(char_dist(gen));
        // Avoid newlines and equals signs
        if (c != '\n' && c != '=') {
            result += c;
        }
    }
    
    return result;
}

/**
 * Generate a random configuration
 */
ConfigData generateRandomConfig() {
    ConfigData config;
    config.username = generateRandomString(50);
    config.password = generateRandomString(100);
    config.session_key = generateRandomString(50);
    config.now_playing_url = generateRandomString(200);
    config.submission_url = generateRandomString(200);
    return config;
}

// ========================================
// PROPERTY-BASED TESTS
// ========================================

// ========================================
// PROPERTY 7: Configuration Round-Trip
// ========================================
// **Feature: lastfm-performance-optimization, Property 7: Configuration Round-Trip**
// **Validates: Requirements 6.2**
//
// For any valid configuration state (username, password, session_key, URLs),
// writing to config file and reading back SHALL produce the same configuration values.
void test_property_config_roundtrip() {
    std::cout << "\n=== Property 7: Configuration Round-Trip ===" << std::endl;
    std::cout << "Testing that config write/read produces identical values..." << std::endl;
    
    std::string temp_file = "/tmp/test_lastfm_config_prop_" + std::to_string(getpid()) + ".conf";
    
    int test_count = 0;
    int passed_count = 0;
    
    std::cout << "\n  Testing 100 random configurations:" << std::endl;
    
    for (int i = 0; i < 100; ++i) {
        try {
            // Generate random configuration
            ConfigData original = generateRandomConfig();
            
            // Write to file
            writeConfigFile(temp_file, original);
            
            // Read back from file
            ConfigData parsed = parseConfigFile(temp_file);
            
            test_count++;
            
            // Verify round-trip succeeded
            if (original == parsed) {
                passed_count++;
            } else {
                std::cerr << "  MISMATCH at iteration " << i << ":" << std::endl;
                std::cerr << "    Original username: '" << original.username << "'" << std::endl;
                std::cerr << "    Parsed username:   '" << parsed.username << "'" << std::endl;
                std::cerr << "    Original password: '" << original.password << "'" << std::endl;
                std::cerr << "    Parsed password:   '" << parsed.password << "'" << std::endl;
                std::cerr << "    Original session_key: '" << original.session_key << "'" << std::endl;
                std::cerr << "    Parsed session_key:   '" << parsed.session_key << "'" << std::endl;
                assert(false && "Configuration round-trip mismatch");
            }
            
        } catch (const std::exception& e) {
            std::cerr << "  Exception at iteration " << i << ": " << e.what() << std::endl;
            assert(false && "Exception during round-trip test");
        }
    }
    
    std::cout << "    Passed " << passed_count << "/" << test_count << " random config tests ✓" << std::endl;
    
    // Test specific edge cases
    std::cout << "\n  Testing edge cases:" << std::endl;
    
    // Empty configuration
    {
        ConfigData empty;
        empty.username = "";
        empty.password = "";
        empty.session_key = "";
        empty.now_playing_url = "";
        empty.submission_url = "";
        
        writeConfigFile(temp_file, empty);
        ConfigData parsed = parseConfigFile(temp_file);
        assert(empty == parsed);
        std::cout << "    Empty configuration ✓" << std::endl;
    }
    
    // Configuration with only username and password
    {
        ConfigData minimal;
        minimal.username = "testuser";
        minimal.password = "testpass";
        minimal.session_key = "";
        minimal.now_playing_url = "";
        minimal.submission_url = "";
        
        writeConfigFile(temp_file, minimal);
        ConfigData parsed = parseConfigFile(temp_file);
        assert(minimal == parsed);
        std::cout << "    Minimal configuration (username + password) ✓" << std::endl;
    }
    
    // Configuration with special characters
    {
        ConfigData special;
        special.username = "user@example.com";
        special.password = "p@ss!word#123$%^&*()";
        special.session_key = "abc-123_def.456~!@#$%";
        special.now_playing_url = "http://post.audioscrobbler.com/np_1.2?param=value&other=123";
        special.submission_url = "http://post.audioscrobbler.com/1.2";
        
        writeConfigFile(temp_file, special);
        ConfigData parsed = parseConfigFile(temp_file);
        assert(special == parsed);
        std::cout << "    Configuration with special characters ✓" << std::endl;
    }
    
    // Configuration with long values
    {
        ConfigData long_config;
        long_config.username = std::string(100, 'a');
        long_config.password = std::string(200, 'b');
        long_config.session_key = std::string(100, 'c');
        long_config.now_playing_url = std::string(300, 'd');
        long_config.submission_url = std::string(300, 'e');
        
        writeConfigFile(temp_file, long_config);
        ConfigData parsed = parseConfigFile(temp_file);
        assert(long_config == parsed);
        std::cout << "    Configuration with long values ✓" << std::endl;
    }
    
    // Configuration with URLs containing query parameters
    {
        ConfigData url_config;
        url_config.username = "testuser";
        url_config.password = "testpass";
        url_config.session_key = "session123";
        url_config.now_playing_url = "http://post.audioscrobbler.com/np_1.2?api_key=abc123&format=json";
        url_config.submission_url = "http://post.audioscrobbler.com/1.2?api_key=abc123&format=json";
        
        writeConfigFile(temp_file, url_config);
        ConfigData parsed = parseConfigFile(temp_file);
        assert(url_config == parsed);
        std::cout << "    Configuration with URL query parameters ✓" << std::endl;
    }
    
    // Clean up
    std::remove(temp_file.c_str());
    
    std::cout << "\n✓ Property 7: Configuration Round-Trip - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY 8: Configuration Format Consistency
// ========================================
// For any configuration, the written file SHALL contain all required keys
// in the standard format (key=value pairs).
void test_property_config_format_consistency() {
    std::cout << "\n=== Property 8: Configuration Format Consistency ===" << std::endl;
    std::cout << "Testing that written config maintains standard format..." << std::endl;
    
    std::string temp_file = "/tmp/test_lastfm_config_fmt_" + std::to_string(getpid()) + ".conf";
    
    int test_count = 0;
    
    for (int i = 0; i < 50; ++i) {
        try {
            // Generate random configuration
            ConfigData config = generateRandomConfig();
            
            // Write to file
            writeConfigFile(temp_file, config);
            
            // Read raw file content
            std::ifstream file(temp_file);
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();
            
            // Verify all required keys are present
            assert(content.find("username=") != std::string::npos);
            assert(content.find("password=") != std::string::npos);
            assert(content.find("session_key=") != std::string::npos);
            assert(content.find("now_playing_url=") != std::string::npos);
            assert(content.find("submission_url=") != std::string::npos);
            
            // Verify header comment is present
            assert(content.find("# Last.fm configuration") != std::string::npos);
            
            test_count++;
            
        } catch (const std::exception& e) {
            std::cerr << "  Exception at iteration " << i << ": " << e.what() << std::endl;
            assert(false && "Exception during format consistency test");
        }
    }
    
    std::cout << "  Verified " << test_count << " configurations maintain standard format ✓" << std::endl;
    
    // Clean up
    std::remove(temp_file.c_str());
    
    std::cout << "\n✓ Property 8: Configuration Format Consistency - ALL TESTS PASSED" << std::endl;
}

// ========================================
// PROPERTY 9: Configuration Value Preservation
// ========================================
// For any configuration value, writing and reading SHALL preserve the exact
// value without modification or truncation.
void test_property_config_value_preservation() {
    std::cout << "\n=== Property 9: Configuration Value Preservation ===" << std::endl;
    std::cout << "Testing that config values are preserved exactly..." << std::endl;
    
    std::string temp_file = "/tmp/test_lastfm_config_preserve_" + std::to_string(getpid()) + ".conf";
    
    int test_count = 0;
    
    for (int i = 0; i < 50; ++i) {
        try {
            // Generate random configuration
            ConfigData original = generateRandomConfig();
            
            // Write to file
            writeConfigFile(temp_file, original);
            
            // Read back
            ConfigData parsed = parseConfigFile(temp_file);
            
            // Verify each field is preserved exactly
            assert(original.username == parsed.username);
            assert(original.password == parsed.password);
            assert(original.session_key == parsed.session_key);
            assert(original.now_playing_url == parsed.now_playing_url);
            assert(original.submission_url == parsed.submission_url);
            
            test_count++;
            
        } catch (const std::exception& e) {
            std::cerr << "  Exception at iteration " << i << ": " << e.what() << std::endl;
            assert(false && "Exception during value preservation test");
        }
    }
    
    std::cout << "  Verified " << test_count << " configurations preserve all values ✓" << std::endl;
    
    // Clean up
    std::remove(temp_file.c_str());
    
    std::cout << "\n✓ Property 9: Configuration Value Preservation - ALL TESTS PASSED" << std::endl;
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "LAST.FM CONFIGURATION ROUND-TRIP PROPERTY-BASED TESTS" << std::endl;
    std::cout << "**Feature: lastfm-performance-optimization, Property 7: Configuration Round-Trip**" << std::endl;
    std::cout << "**Validates: Requirements 6.2**" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    try {
        // Run all property tests
        test_property_config_roundtrip();
        test_property_config_format_consistency();
        test_property_config_value_preservation();
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "✅ ALL PROPERTY TESTS PASSED" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ PROPERTY TEST FAILED" << std::endl;
        std::cerr << "Unknown exception occurred" << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    }
}
