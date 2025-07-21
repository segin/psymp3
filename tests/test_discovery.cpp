/*
 * test_discovery.cpp - Implementation of test discovery engine
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "test_discovery.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <iostream>
#include <sys/stat.h>
#include <dirent.h>
#include <cctype>

namespace TestFramework {

    // ========================================
    // TEST DISCOVERY IMPLEMENTATION
    // ========================================
    
    TestDiscovery::TestDiscovery(const std::string& test_directory) 
        : m_test_directory(test_directory), m_default_timeout(30000), m_cache_valid(false) {
    }
    
    std::vector<TestInfo> TestDiscovery::discoverTests() {
        if (!m_cache_valid) {
            m_discovered_tests.clear();
            
            std::vector<std::string> test_files = scanDirectory(m_test_directory);
            
            for (const std::string& file_path : test_files) {
                if (isTestFile(file_path)) {
                    TestInfo info;
                    info.name = extractTestName(file_path);
                    info.source_path = file_path;
                    info.executable_path = getExecutablePath(info.name);
                    info.metadata = parseTestMetadata(file_path);
                    info.is_built = isTestBuilt(info);
                    info.last_modified = getFileModTime(file_path);
                    
                    // Apply custom timeout if set
                    auto timeout_it = m_custom_timeouts.find(info.name);
                    if (timeout_it != m_custom_timeouts.end()) {
                        info.metadata.timeout = timeout_it->second;
                    }
                    
                    m_discovered_tests.push_back(info);
                }
            }
            
            m_cache_valid = true;
        }
        
        return m_discovered_tests;
    }
    
    std::vector<TestInfo> TestDiscovery::discoverTests(const std::string& pattern) {
        std::vector<TestInfo> all_tests = discoverTests();
        std::vector<TestInfo> matching_tests;
        
        for (const TestInfo& test : all_tests) {
            if (matchesPattern(pattern, test.name)) {
                matching_tests.push_back(test);
            }
        }
        
        return matching_tests;
    }
    
    bool TestDiscovery::isTestFile(const std::string& filename) const {
        // Extract just the filename from the path
        size_t last_slash = filename.find_last_of("/\\");
        std::string basename = (last_slash != std::string::npos) ? 
                              filename.substr(last_slash + 1) : filename;
        
        // Check if it starts with "test_" and ends with ".cpp"
        return basename.length() > 9 && // "test_*.cpp" minimum length
               basename.substr(0, 5) == "test_" &&
               basename.substr(basename.length() - 4) == ".cpp";
    }
    
    TestMetadata TestDiscovery::parseTestMetadata(const std::string& filepath) {
        return MetadataParser::parseFromFile(filepath);
    }
    
    std::vector<std::string> TestDiscovery::resolveDependencies(const std::string& source_path) {
        std::vector<std::string> includes = parseIncludes(source_path);
        return includesToDependencies(includes);
    }
    
    bool TestDiscovery::isTestBuilt(const TestInfo& test_info) const {
        if (!fileExists(test_info.executable_path)) {
            return false;
        }
        
        // Check if executable is newer than source
        auto exe_time = getFileModTime(test_info.executable_path);
        auto src_time = getFileModTime(test_info.source_path);
        
        return exe_time >= src_time;
    }
    
    std::string TestDiscovery::getExecutablePath(const std::string& test_name) const {
        return m_test_directory + "/" + test_name;
    }
    
    void TestDiscovery::setTestTimeout(const std::string& test_name, std::chrono::milliseconds timeout) {
        m_custom_timeouts[test_name] = timeout;
        m_cache_valid = false; // Invalidate cache to pick up new timeout
    }
    
    void TestDiscovery::setDefaultTimeout(std::chrono::milliseconds timeout) {
        m_default_timeout = timeout;
        m_cache_valid = false; // Invalidate cache
    }
    
    std::vector<std::string> TestDiscovery::getTestNames() {
        std::vector<TestInfo> tests = discoverTests();
        std::vector<std::string> names;
        names.reserve(tests.size());
        
        for (const TestInfo& test : tests) {
            names.push_back(test.name);
        }
        
        return names;
    }
    
    TestInfo TestDiscovery::getTestInfo(const std::string& test_name) {
        std::vector<TestInfo> tests = discoverTests();
        
        for (const TestInfo& test : tests) {
            if (test.name == test_name) {
                return test;
            }
        }
        
        return TestInfo(); // Return empty TestInfo if not found
    }
    
    std::vector<TestInfo> TestDiscovery::filterByTags(const std::vector<TestInfo>& tests,
                                                     const std::vector<std::string>& required_tags,
                                                     const std::vector<std::string>& excluded_tags) {
        std::vector<TestInfo> filtered;
        
        for (const TestInfo& test : tests) {
            bool include = true;
            
            // Check required tags
            for (const std::string& required_tag : required_tags) {
                bool has_tag = std::find(test.metadata.tags.begin(), test.metadata.tags.end(), required_tag) 
                              != test.metadata.tags.end();
                if (!has_tag) {
                    include = false;
                    break;
                }
            }
            
            // Check excluded tags
            if (include) {
                for (const std::string& excluded_tag : excluded_tags) {
                    bool has_tag = std::find(test.metadata.tags.begin(), test.metadata.tags.end(), excluded_tag) 
                                  != test.metadata.tags.end();
                    if (has_tag) {
                        include = false;
                        break;
                    }
                }
            }
            
            if (include) {
                filtered.push_back(test);
            }
        }
        
        return filtered;
    }
    
    std::vector<TestInfo> TestDiscovery::getParallelSafeTests(const std::vector<TestInfo>& tests) {
        std::vector<TestInfo> parallel_safe;
        
        for (const TestInfo& test : tests) {
            if (test.metadata.parallel_safe) {
                parallel_safe.push_back(test);
            }
        }
        
        return parallel_safe;
    }
    
    std::vector<TestInfo> TestDiscovery::getSequentialTests(const std::vector<TestInfo>& tests) {
        std::vector<TestInfo> sequential;
        
        for (const TestInfo& test : tests) {
            if (!test.metadata.parallel_safe) {
                sequential.push_back(test);
            }
        }
        
        return sequential;
    }
    
    void TestDiscovery::refresh() {
        m_cache_valid = false;
        m_discovered_tests.clear();
    }
    
    std::map<std::string, int> TestDiscovery::getStatistics() {
        std::vector<TestInfo> tests = discoverTests();
        std::map<std::string, int> stats;
        
        stats["total_files"] = tests.size();
        stats["built_tests"] = 0;
        stats["parallel_safe"] = 0;
        stats["sequential"] = 0;
        
        for (const TestInfo& test : tests) {
            if (test.is_built) {
                stats["built_tests"]++;
            }
            if (test.metadata.parallel_safe) {
                stats["parallel_safe"]++;
            } else {
                stats["sequential"]++;
            }
        }
        
        return stats;
    }
    
    // ========================================
    // PRIVATE HELPER METHODS
    // ========================================
    
    std::vector<std::string> TestDiscovery::scanDirectory(const std::string& directory) {
        std::vector<std::string> files;
        
        DIR* dir = opendir(directory.c_str());
        if (dir == nullptr) {
            std::cerr << "Warning: Could not open directory: " << directory << std::endl;
            return files;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            
            // Skip . and .. entries
            if (filename == "." || filename == "..") {
                continue;
            }
            
            std::string full_path = directory + "/" + filename;
            
            // Check if it's a regular file
            struct stat file_stat;
            if (stat(full_path.c_str(), &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
                files.push_back(full_path);
            }
        }
        
        closedir(dir);
        return files;
    }
    
    std::string TestDiscovery::extractTestName(const std::string& filename) const {
        size_t last_slash = filename.find_last_of("/\\");
        std::string basename = (last_slash != std::string::npos) ? 
                              filename.substr(last_slash + 1) : filename;
        
        // Remove .cpp extension
        if (basename.length() > 4 && basename.substr(basename.length() - 4) == ".cpp") {
            return basename.substr(0, basename.length() - 4);
        }
        
        return basename;
    }
    
    std::map<std::string, std::string> TestDiscovery::parseMetadataComments(const std::string& filepath) {
        std::map<std::string, std::string> metadata;
        std::ifstream file(filepath);
        
        if (!file.is_open()) {
            return metadata;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            if (MetadataParser::isMetadataComment(line)) {
                auto key_value = MetadataParser::extractMetadata(line);
                if (!key_value.first.empty()) {
                    metadata[key_value.first] = key_value.second;
                }
            }
        }
        
        return metadata;
    }
    
    std::vector<std::string> TestDiscovery::parseIncludes(const std::string& filepath) {
        std::vector<std::string> includes;
        std::ifstream file(filepath);
        
        if (!file.is_open()) {
            return includes;
        }
        
        std::string line;
        std::regex include_regex(R"(^\s*#include\s*[<"]([^>"]+)[>"])");
        
        while (std::getline(file, line)) {
            std::smatch match;
            if (std::regex_match(line, match, include_regex)) {
                includes.push_back(match[1].str());
            }
        }
        
        return includes;
    }
    
    std::vector<std::string> TestDiscovery::includesToDependencies(const std::vector<std::string>& includes) {
        std::vector<std::string> dependencies;
        
        for (const std::string& include : includes) {
            // Convert header includes to potential object file dependencies
            if (include.find("../include/") == 0) {
                // Local project header
                std::string header_name = include.substr(11); // Remove "../include/"
                if (header_name.length() > 2 && header_name.substr(header_name.length() - 2) == ".h") {
                    std::string obj_name = header_name.substr(0, header_name.length() - 2) + ".o";
                    dependencies.push_back(obj_name);
                }
            }
        }
        
        return dependencies;
    }
    
    bool TestDiscovery::matchesPattern(const std::string& pattern, const std::string& text) const {
        // Simple glob matching with * and ? wildcards
        std::string regex_pattern = pattern;
        
        // Escape special regex characters except * and ?
        std::string special_chars = ".^$+{}[]|()\\";
        for (char c : special_chars) {
            size_t pos = 0;
            std::string char_str(1, c);
            std::string escaped = "\\" + char_str;
            while ((pos = regex_pattern.find(char_str, pos)) != std::string::npos) {
                regex_pattern.replace(pos, 1, escaped);
                pos += 2;
            }
        }
        
        // Convert glob wildcards to regex
        size_t pos = 0;
        while ((pos = regex_pattern.find("*", pos)) != std::string::npos) {
            regex_pattern.replace(pos, 1, ".*");
            pos += 2;
        }
        
        pos = 0;
        while ((pos = regex_pattern.find("?", pos)) != std::string::npos) {
            regex_pattern.replace(pos, 1, ".");
            pos += 1;
        }
        
        std::regex pattern_regex("^" + regex_pattern + "$");
        return std::regex_match(text, pattern_regex);
    }
    
    std::chrono::system_clock::time_point TestDiscovery::getFileModTime(const std::string& filepath) const {
        struct stat file_stat;
        if (stat(filepath.c_str(), &file_stat) == 0) {
            return std::chrono::system_clock::from_time_t(file_stat.st_mtime);
        }
        return std::chrono::system_clock::time_point{};
    }
    
    bool TestDiscovery::fileExists(const std::string& filepath) const {
        struct stat file_stat;
        return stat(filepath.c_str(), &file_stat) == 0;
    }
    
    std::vector<std::string> TestDiscovery::parseCommaSeparated(const std::string& input) const {
        std::vector<std::string> result;
        std::stringstream ss(input);
        std::string item;
        
        while (std::getline(ss, item, ',')) {
            result.push_back(trim(item));
        }
        
        return result;
    }
    
    std::string TestDiscovery::trim(const std::string& str) const {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            return "";
        }
        
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
    
    TestDiscovery::CompilationResult TestDiscovery::attemptCompilation(const TestInfo& test_info) {
        CompilationResult result;
        
        // Check if source file exists
        if (!fileExists(test_info.source_path)) {
            result.error_output = "Source file does not exist: " + test_info.source_path;
            return result;
        }
        
        // Build compilation command similar to what make would use
        std::ostringstream cmd;
        cmd << "cd " << m_test_directory << " && ";
        cmd << "g++ -DHAVE_CONFIG_H -I. -I../include ";
        cmd << "-I../include -I/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT ";
        cmd << "-I/usr/include/taglib -I/usr/include/freetype2 -I/usr/include/libpng16 ";
        cmd << "-g -O2 -c -o " << test_info.name << ".o " << test_info.source_path;
        cmd << " 2>&1"; // Redirect stderr to stdout for capture
        
        result.command_used = cmd.str();
        
        // Execute compilation command
        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) {
            result.error_output = "Failed to execute compilation command";
            return result;
        }
        
        // Capture output
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result.error_output += buffer;
        }
        
        result.exit_code = pclose(pipe);
        result.success = (result.exit_code == 0);
        
        return result;
    }
    
    std::vector<TestDiscovery::DependencyInfo> TestDiscovery::checkDependencies(const TestInfo& test_info) {
        std::vector<DependencyInfo> dependencies;
        
        // Check for rect.o dependency (common for most tests)
        DependencyInfo rect_dep;
        rect_dep.name = "rect.o";
        rect_dep.available = fileExists("../src/rect.o");
        if (!rect_dep.available) {
            rect_dep.suggestion = "Run 'make -C ../src rect.o' to build the rect object file";
        }
        dependencies.push_back(rect_dep);
        
        // Check for test utilities library
        DependencyInfo utils_dep;
        utils_dep.name = "libtest_utilities.a";
        utils_dep.available = fileExists("libtest_utilities.a");
        if (!utils_dep.available) {
            utils_dep.suggestion = "Run 'make libtest_utilities.a' to build the test utilities library";
        }
        dependencies.push_back(utils_dep);
        
        // Check for system dependencies based on includes
        std::vector<std::string> includes = parseIncludes(test_info.source_path);
        for (const std::string& include : includes) {
            if (include.find("SDL") != std::string::npos) {
                DependencyInfo sdl_dep;
                sdl_dep.name = "SDL development headers";
                // Simple check - this could be more sophisticated
                sdl_dep.available = fileExists("/usr/include/SDL/SDL.h") || fileExists("/usr/include/SDL2/SDL.h");
                if (!sdl_dep.available) {
                    sdl_dep.suggestion = "Install SDL development package: sudo apt-get install libsdl1.2-dev";
                }
                dependencies.push_back(sdl_dep);
                break;
            }
        }
        
        return dependencies;
    }

    // ========================================
    // METADATA PARSER IMPLEMENTATION
    // ========================================
    
    TestMetadata MetadataParser::parseFromFile(const std::string& filepath) {
        std::vector<std::string> comment_lines;
        std::ifstream file(filepath);
        
        if (!file.is_open()) {
            TestMetadata metadata;
            metadata.source_file = filepath;
            return metadata;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            if (isMetadataComment(line)) {
                comment_lines.push_back(line);
            }
        }
        
        TestMetadata metadata = parseFromComments(comment_lines);
        metadata.source_file = filepath;
        return metadata;
    }
    
    TestMetadata MetadataParser::parseFromComments(const std::vector<std::string>& comment_lines) {
        TestMetadata metadata;
        
        for (const std::string& line : comment_lines) {
            auto key_value = extractMetadata(line);
            if (key_value.first.empty()) {
                continue;
            }
            
            const std::string& key = key_value.first;
            const std::string& value = key_value.second;
            
            if (key == "test-name") {
                metadata.name = value;
            } else if (key == "test-description") {
                metadata.description = value;
            } else if (key == "test-tags") {
                std::stringstream ss(value);
                std::string tag;
                while (std::getline(ss, tag, ',')) {
                    // Trim whitespace
                    tag.erase(0, tag.find_first_not_of(" \t"));
                    tag.erase(tag.find_last_not_of(" \t") + 1);
                    if (!tag.empty()) {
                        metadata.tags.push_back(tag);
                    }
                }
            } else if (key == "test-timeout") {
                metadata.timeout = parseTimeout(value);
            } else if (key == "test-author") {
                metadata.author = value;
            } else if (key == "test-parallel") {
                metadata.parallel_safe = parseBool(value);
            } else if (key == "test-dependencies") {
                std::stringstream ss(value);
                std::string dep;
                while (std::getline(ss, dep, ',')) {
                    // Trim whitespace
                    dep.erase(0, dep.find_first_not_of(" \t"));
                    dep.erase(dep.find_last_not_of(" \t") + 1);
                    if (!dep.empty()) {
                        metadata.dependencies.push_back(dep);
                    }
                }
            } else if (key == "test-created") {
                metadata.created_date = value;
            }
        }
        
        return metadata;
    }
    
    std::pair<std::string, std::string> MetadataParser::extractMetadata(const std::string& comment) {
        // Look for pattern: // @key: value or /* @key: value */
        std::regex metadata_regex(R"(^\s*(?://|/\*)\s*@([^:]+):\s*(.+?)(?:\s*\*/)?$)");
        std::smatch match;
        
        if (std::regex_match(comment, match, metadata_regex)) {
            std::string key = match[1].str();
            std::string value = match[2].str();
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            return std::make_pair(key, value);
        }
        
        return std::make_pair("", "");
    }
    
    bool MetadataParser::isMetadataComment(const std::string& line) {
        // Check if line contains @key: pattern
        std::regex metadata_regex(R"(^\s*(?://|/\*)\s*@[^:]+:)");
        return std::regex_search(line, metadata_regex);
    }
    
    bool MetadataParser::parseBool(const std::string& value) {
        std::string lower_value = value;
        std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
        
        return lower_value == "true" || lower_value == "yes" || lower_value == "1" || lower_value == "on";
    }
    
    std::chrono::milliseconds MetadataParser::parseTimeout(const std::string& value) {
        std::regex timeout_regex(R"((\d+)\s*(ms|s|m)?)");
        std::smatch match;
        
        if (std::regex_match(value, match, timeout_regex)) {
            int number = std::stoi(match[1].str());
            std::string unit = match[2].str();
            
            if (unit.empty() || unit == "ms") {
                return std::chrono::milliseconds(number);
            } else if (unit == "s") {
                return std::chrono::milliseconds(number * 1000);
            } else if (unit == "m") {
                return std::chrono::milliseconds(number * 60 * 1000);
            }
        }
        
        // Default to treating as milliseconds
        try {
            return std::chrono::milliseconds(std::stoi(value));
        } catch (...) {
            return std::chrono::milliseconds(30000); // Default 30 seconds
        }
    }

} // namespace TestFramework