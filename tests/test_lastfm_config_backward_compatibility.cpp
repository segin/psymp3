/*
 * test_lastfm_config_backward_compatibility.cpp - Configuration file backward compatibility tests
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
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

// ========================================
// CONFIGURATION FILE FORMAT UTILITIES
// ========================================

/**
 * Parse a Last.fm configuration file in the standard format
 * Format: key=value pairs, one per line, # for comments
 */
struct ConfigData {
    std::string username;
    std::string password_hash;
    std::string session_key;
    std::string now_playing_url;
    std::string submission_url;
    
    bool operator==(const ConfigData& other) const {
        return username == other.username &&
               password_hash == other.password_hash &&
               session_key == other.session_key &&
               now_playing_url == other.now_playing_url &&
               submission_url == other.submission_url;
    }
    
    bool operator!=(const ConfigData& other) const {
        return !(*this == other);
    }
};

ConfigData parseConfigFile(const std::string& filename) {
    ConfigData config;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + filename);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        
        std::string key = line.substr(0, equals);
        std::string value = line.substr(equals + 1);
        
        if (key == "username") {
            config.username = value;
        } else if (key == "password") {
            // For backward compatibility tests, we mock the hashing transition.
            // This verifies that legacy 'password' entries are correctly identified
            // and transformed into the hash field.
            config.password_hash = "mock_hash_of_" + value;
        } else if (key == "password_hash") {
            config.password_hash = value;
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
    // password_hash is not persisted for security reasons
    file << "session_key=" << config.session_key << "\n";
    file << "now_playing_url=" << config.now_playing_url << "\n";
    file << "submission_url=" << config.submission_url << "\n";
}

// ========================================
// TEST CASES
// ========================================

/**
 * Test 1: Parse existing configuration file format
 * Verifies that existing config files can be read correctly
 */
bool test_parse_existing_config_format() {
    std::cout << "\n=== Test 1: Parse Existing Configuration File Format ===" << std::endl;
    
    // Create a temporary config file with standard format
    std::string temp_file = "/tmp/test_lastfm_config_" + std::to_string(getpid()) + ".conf";
    
    try {
        // Write a config file in the standard format
        ConfigData original;
        original.username = "testuser";
        original.password_hash = "testpass123";
        original.session_key = "abc123def456";
        original.now_playing_url = "http://post.audioscrobbler.com/np_1.2";
        original.submission_url = "http://post.audioscrobbler.com/1.2";
        
        // Manually write config file with password_hash to verify we can still read legacy files
        {
            std::ofstream file(temp_file);
            file << "# Last.fm configuration\n";
            file << "username=" << original.username << "\n";
            file << "password_hash=" << original.password_hash << "\n";
            file << "session_key=" << original.session_key << "\n";
            file << "now_playing_url=" << original.now_playing_url << "\n";
            file << "submission_url=" << original.submission_url << "\n";
        }
        
        // Parse it back
        ConfigData parsed = parseConfigFile(temp_file);
        
        // Verify all fields match
        assert(parsed.username == original.username);
        assert(parsed.password_hash == original.password_hash);
        assert(parsed.session_key == original.session_key);
        assert(parsed.now_playing_url == original.now_playing_url);
        assert(parsed.submission_url == original.submission_url);
        
        std::cout << "  ✓ Successfully parsed existing config format" << std::endl;
        std::cout << "    Username: " << parsed.username << std::endl;
        std::cout << "    Session Key: " << parsed.session_key << std::endl;
        
        // Clean up
        std::remove(temp_file.c_str());
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed to parse config: " << e.what() << std::endl;
        std::remove(temp_file.c_str());
        return false;
    }
}

/**
 * Test 2: Write configuration file maintains format
 * Verifies that written config files maintain the standard format
 */
bool test_write_config_maintains_format() {
    std::cout << "\n=== Test 2: Write Configuration File Maintains Format ===" << std::endl;
    
    std::string temp_file = "/tmp/test_lastfm_config_write_" + std::to_string(getpid()) + ".conf";
    
    try {
        // Create config data
        ConfigData original;
        original.username = "myuser";
        original.password_hash = "mypassword";
        original.session_key = "session123";
        original.now_playing_url = "http://post.audioscrobbler.com/np_1.2";
        original.submission_url = "http://post.audioscrobbler.com/1.2";
        
        // Write it
        writeConfigFile(temp_file, original);
        
        // Read the raw file content
        std::ifstream file(temp_file);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        // Verify format contains expected keys
        assert(content.find("username=") != std::string::npos);
        // password_hash must NOT be present
        assert(content.find("password_hash=") == std::string::npos);
        assert(content.find("session_key=") != std::string::npos);
        assert(content.find("now_playing_url=") != std::string::npos);
        assert(content.find("submission_url=") != std::string::npos);
        
        // Verify values are present
        assert(content.find("myuser") != std::string::npos);
        // Password hash value should NOT be present
        assert(content.find("mypassword") == std::string::npos);
        assert(content.find("session123") != std::string::npos);
        
        // Parse it back to verify round-trip
        ConfigData parsed = parseConfigFile(temp_file);

        // password_hash is not persisted
        assert(parsed.password_hash.empty());
        original.password_hash.clear();

        assert(parsed == original);
        
        std::cout << "  ✓ Written config file maintains standard format" << std::endl;
        std::cout << "  ✓ Round-trip parsing successful" << std::endl;
        
        // Clean up
        std::remove(temp_file.c_str());
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed to write config: " << e.what() << std::endl;
        std::remove(temp_file.c_str());
        return false;
    }
}

/**
 * Test 3: Handle missing optional fields
 * Verifies that config files with missing fields are handled gracefully
 */
bool test_handle_missing_optional_fields() {
    std::cout << "\n=== Test 3: Handle Missing Optional Fields ===" << std::endl;
    
    std::string temp_file = "/tmp/test_lastfm_config_missing_" + std::to_string(getpid()) + ".conf";
    
    try {
        // Write a minimal config file (only username and password)
        std::ofstream file(temp_file);
        file << "# Minimal Last.fm configuration\n";
        file << "username=testuser\n";
        file << "password=testpass\n"; // Legacy key
        file.close();
        
        // Parse it
        ConfigData parsed = parseConfigFile(temp_file);
        
        // Verify required fields are present and migration occurred
        assert(parsed.username == "testuser");
        assert(parsed.password_hash == "mock_hash_of_testpass");
        
        // Optional fields should be empty
        assert(parsed.session_key.empty());
        assert(parsed.now_playing_url.empty());
        assert(parsed.submission_url.empty());
        
        std::cout << "  ✓ Successfully handled config with missing optional fields" << std::endl;
        
        // Clean up
        std::remove(temp_file.c_str());
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed to handle missing fields: " << e.what() << std::endl;
        std::remove(temp_file.c_str());
        return false;
    }
}

/**
 * Test 4: Handle special characters in values
 * Verifies that config values with special characters are preserved
 */
bool test_handle_special_characters() {
    std::cout << "\n=== Test 4: Handle Special Characters in Values ===" << std::endl;
    
    std::string temp_file = "/tmp/test_lastfm_config_special_" + std::to_string(getpid()) + ".conf";
    
    try {
        // Create config with special characters
        ConfigData original;
        original.username = "user@example.com";
        original.password_hash = "p@ss!word#123";
        original.session_key = "abc-123_def.456";
        original.now_playing_url = "http://post.audioscrobbler.com/np_1.2?param=value";
        original.submission_url = "http://post.audioscrobbler.com/1.2";
        
        // Write and parse
        writeConfigFile(temp_file, original);
        ConfigData parsed = parseConfigFile(temp_file);
        
        // Verify special characters are preserved
        assert(parsed.username == original.username);
        // password_hash is not persisted
        assert(parsed.password_hash.empty());
        assert(parsed.session_key == original.session_key);
        assert(parsed.now_playing_url == original.now_playing_url);
        
        std::cout << "  ✓ Special characters preserved in config values" << std::endl;
        
        // Clean up
        std::remove(temp_file.c_str());
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed to handle special characters: " << e.what() << std::endl;
        std::remove(temp_file.c_str());
        return false;
    }
}

/**
 * Test 5: Handle comments and blank lines
 * Verifies that config files with comments and blank lines are parsed correctly
 */
bool test_handle_comments_and_blank_lines() {
    std::cout << "\n=== Test 5: Handle Comments and Blank Lines ===" << std::endl;
    
    std::string temp_file = "/tmp/test_lastfm_config_comments_" + std::to_string(getpid()) + ".conf";
    
    try {
        // Write a config file with comments and blank lines
        std::ofstream file(temp_file);
        file << "# Last.fm configuration file\n";
        file << "# This is a comment\n";
        file << "\n";
        file << "username=testuser\n";
        file << "# Another comment\n";
        file << "\n";
        file << "password=testpass\n";
        file << "\n";
        file << "# Session information\n";
        file << "session_key=abc123\n";
        file << "now_playing_url=http://post.audioscrobbler.com/np_1.2\n";
        file << "submission_url=http://post.audioscrobbler.com/1.2\n";
        file.close();
        
        // Parse it
        ConfigData parsed = parseConfigFile(temp_file);
        
        // Verify values are correctly extracted and migrated
        assert(parsed.username == "testuser");
        assert(parsed.password_hash == "mock_hash_of_testpass");
        assert(parsed.session_key == "abc123");
        assert(parsed.now_playing_url == "http://post.audioscrobbler.com/np_1.2");
        assert(parsed.submission_url == "http://post.audioscrobbler.com/1.2");
        
        std::cout << "  ✓ Comments and blank lines handled correctly" << std::endl;
        
        // Clean up
        std::remove(temp_file.c_str());
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed to handle comments: " << e.what() << std::endl;
        std::remove(temp_file.c_str());
        return false;
    }
}

/**
 * Test 6: Empty values are preserved
 * Verifies that empty configuration values are handled correctly
 */
bool test_empty_values_preserved() {
    std::cout << "\n=== Test 6: Empty Values Are Preserved ===" << std::endl;
    
    std::string temp_file = "/tmp/test_lastfm_config_empty_" + std::to_string(getpid()) + ".conf";
    
    try {
        // Create config with some empty values
        ConfigData original;
        original.username = "testuser";
        original.password_hash = "";  // Empty password
        original.session_key = "";  // Empty session key
        original.now_playing_url = "http://post.audioscrobbler.com/np_1.2";
        original.submission_url = "";  // Empty submission URL
        
        // Write and parse
        writeConfigFile(temp_file, original);
        ConfigData parsed = parseConfigFile(temp_file);
        
        // Verify empty values are preserved
        assert(parsed.username == "testuser");
        assert(parsed.password_hash.empty());
        assert(parsed.session_key.empty());
        assert(parsed.now_playing_url == "http://post.audioscrobbler.com/np_1.2");
        assert(parsed.submission_url.empty());
        
        std::cout << "  ✓ Empty values correctly preserved" << std::endl;
        
        // Clean up
        std::remove(temp_file.c_str());
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed to handle empty values: " << e.what() << std::endl;
        std::remove(temp_file.c_str());
        return false;
    }
}

// ========================================
// MAIN TEST RUNNER
// ========================================
int main() {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "LAST.FM CONFIGURATION FILE BACKWARD COMPATIBILITY TESTS" << std::endl;
    std::cout << "Requirements: 6.1, 6.2" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    try {
        // Run all tests
        if (test_parse_existing_config_format()) passed++; else failed++;
        if (test_write_config_maintains_format()) passed++; else failed++;
        if (test_handle_missing_optional_fields()) passed++; else failed++;
        if (test_handle_special_characters()) passed++; else failed++;
        if (test_handle_comments_and_blank_lines()) passed++; else failed++;
        if (test_empty_values_preserved()) passed++; else failed++;
        
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "RESULTS: " << passed << " passed, " << failed << " failed" << std::endl;
        
        if (failed == 0) {
            std::cout << "✅ ALL BACKWARD COMPATIBILITY TESTS PASSED" << std::endl;
            std::cout << std::string(70, '=') << std::endl;
            return 0;
        } else {
            std::cout << "❌ SOME TESTS FAILED" << std::endl;
            std::cout << std::string(70, '=') << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(70, '=') << std::endl;
        std::cerr << "❌ TEST SUITE FAILED" << std::endl;
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cerr << std::string(70, '=') << std::endl;
        return 1;
    }
}
