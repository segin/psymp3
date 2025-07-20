/*
 * test_discovery.h - Test discovery engine for PsyMP3 test harness
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef TEST_DISCOVERY_H
#define TEST_DISCOVERY_H

#include <string>
#include <vector>
#include <chrono>
#include <map>
#include <set>

namespace TestFramework {

    // ========================================
    // TEST METADATA STRUCTURES
    // ========================================
    
    /**
     * @brief Metadata information parsed from test source files
     */
    struct TestMetadata {
        std::string name;                           ///< Human-readable test name
        std::string description;                    ///< Test description from comments
        std::vector<std::string> tags;              ///< Test categorization tags
        std::chrono::milliseconds timeout;          ///< Maximum execution time
        std::vector<std::string> dependencies;      ///< Required object files/libraries
        bool parallel_safe;                         ///< Can run in parallel with other tests
        std::string author;                         ///< Test author from comments
        std::string created_date;                   ///< Creation date from comments
        std::string source_file;                    ///< Source file path
        
        TestMetadata() : timeout(30000), parallel_safe(true) {} // Default 30 second timeout
    };
    
    /**
     * @brief Information about a discovered test executable
     */
    struct TestInfo {
        std::string name;                           ///< Test executable name
        std::string executable_path;                ///< Full path to executable
        std::string source_path;                    ///< Path to source file
        TestMetadata metadata;                      ///< Parsed metadata
        bool is_built;                              ///< Whether executable exists
        std::chrono::system_clock::time_point last_modified; ///< Last modification time
        
        TestInfo() : is_built(false) {}
        TestInfo(const std::string& test_name) : name(test_name), is_built(false) {}
    };

    // ========================================
    // TEST DISCOVERY ENGINE
    // ========================================
    
    /**
     * @brief Engine for discovering and cataloging test files
     * 
     * Scans the tests directory for test files, parses metadata from source
     * comments, resolves dependencies, and maintains a catalog of available tests.
     */
    class TestDiscovery {
    public:
        /**
         * @brief Constructor
         * @param test_directory Directory to scan for tests (default: "tests")
         */
        explicit TestDiscovery(const std::string& test_directory = "tests");
        
        /**
         * @brief Discover all test files in the configured directory
         * @return Vector of TestInfo for all discovered tests
         */
        std::vector<TestInfo> discoverTests();
        
        /**
         * @brief Discover tests matching a specific pattern
         * @param pattern Glob-style pattern to match test names
         * @return Vector of TestInfo for matching tests
         */
        std::vector<TestInfo> discoverTests(const std::string& pattern);
        
        /**
         * @brief Check if a filename represents a test file
         * @param filename Name of file to check
         * @return true if file matches test naming convention
         */
        bool isTestFile(const std::string& filename) const;
        
        /**
         * @brief Parse metadata from a test source file
         * @param filepath Path to source file
         * @return TestMetadata parsed from file comments
         */
        TestMetadata parseTestMetadata(const std::string& filepath);
        
        /**
         * @brief Resolve dependencies for a test file
         * @param source_path Path to test source file
         * @return Vector of dependency names (object files, libraries)
         */
        std::vector<std::string> resolveDependencies(const std::string& source_path);
        
        /**
         * @brief Check if test executable exists and is up to date
         * @param test_info TestInfo to check
         * @return true if executable exists and is newer than source
         */
        bool isTestBuilt(const TestInfo& test_info) const;
        
        /**
         * @brief Get the expected executable path for a test
         * @param test_name Name of the test
         * @return Expected path to test executable
         */
        std::string getExecutablePath(const std::string& test_name) const;
        
        /**
         * @brief Set custom timeout for specific tests
         * @param test_name Name of test
         * @param timeout Timeout in milliseconds
         */
        void setTestTimeout(const std::string& test_name, std::chrono::milliseconds timeout);
        
        /**
         * @brief Set default timeout for all tests
         * @param timeout Default timeout in milliseconds
         */
        void setDefaultTimeout(std::chrono::milliseconds timeout);
        
        /**
         * @brief Get list of all test names
         * @return Vector of test names
         */
        std::vector<std::string> getTestNames();
        
        /**
         * @brief Get test info by name
         * @param test_name Name of test to find
         * @return TestInfo if found, empty TestInfo if not found
         */
        TestInfo getTestInfo(const std::string& test_name);
        
        /**
         * @brief Filter tests by tags
         * @param tests Vector of tests to filter
         * @param required_tags Tags that must be present
         * @param excluded_tags Tags that must not be present
         * @return Filtered vector of tests
         */
        std::vector<TestInfo> filterByTags(const std::vector<TestInfo>& tests, 
                                          const std::vector<std::string>& required_tags = {},
                                          const std::vector<std::string>& excluded_tags = {});
        
        /**
         * @brief Get tests that can run in parallel
         * @param tests Vector of tests to filter
         * @return Vector of parallel-safe tests
         */
        std::vector<TestInfo> getParallelSafeTests(const std::vector<TestInfo>& tests);
        
        /**
         * @brief Get tests that must run sequentially
         * @param tests Vector of tests to filter
         * @return Vector of tests that require sequential execution
         */
        std::vector<TestInfo> getSequentialTests(const std::vector<TestInfo>& tests);
        
        /**
         * @brief Refresh discovery cache
         * 
         * Forces re-scanning of test directory and re-parsing of metadata.
         * Call this if test files have been added, removed, or modified.
         */
        void refresh();
        
        /**
         * @brief Get discovery statistics
         * @return Map of statistics (total_files, valid_tests, built_tests, etc.)
         */
        std::map<std::string, int> getStatistics();
        
    private:
        std::string m_test_directory;                           ///< Directory to scan
        std::chrono::milliseconds m_default_timeout;           ///< Default test timeout
        std::map<std::string, std::chrono::milliseconds> m_custom_timeouts; ///< Per-test timeouts
        std::vector<TestInfo> m_discovered_tests;              ///< Cache of discovered tests
        bool m_cache_valid;                                     ///< Whether cache is valid
        
        /**
         * @brief Scan directory for test files
         * @param directory Directory to scan
         * @return Vector of test file paths
         */
        std::vector<std::string> scanDirectory(const std::string& directory);
        
        /**
         * @brief Extract test name from filename
         * @param filename Source filename
         * @return Test name (without extension)
         */
        std::string extractTestName(const std::string& filename) const;
        
        /**
         * @brief Parse metadata comment block from source file
         * @param filepath Path to source file
         * @return Raw metadata strings from comments
         */
        std::map<std::string, std::string> parseMetadataComments(const std::string& filepath);
        
        /**
         * @brief Parse include statements to determine dependencies
         * @param filepath Path to source file
         * @return Vector of included header files
         */
        std::vector<std::string> parseIncludes(const std::string& filepath);
        
        /**
         * @brief Convert include paths to object file dependencies
         * @param includes Vector of include paths
         * @return Vector of required object files
         */
        std::vector<std::string> includesToDependencies(const std::vector<std::string>& includes);
        
        /**
         * @brief Check if pattern matches string (simple glob matching)
         * @param pattern Pattern with * and ? wildcards
         * @param text String to match against
         * @return true if pattern matches
         */
        bool matchesPattern(const std::string& pattern, const std::string& text) const;
        
        /**
         * @brief Get file modification time
         * @param filepath Path to file
         * @return Last modification time
         */
        std::chrono::system_clock::time_point getFileModTime(const std::string& filepath) const;
        
        /**
         * @brief Check if file exists
         * @param filepath Path to file
         * @return true if file exists
         */
        bool fileExists(const std::string& filepath) const;
        
        /**
         * @brief Parse comma-separated values from string
         * @param input Input string with comma-separated values
         * @return Vector of trimmed values
         */
        std::vector<std::string> parseCommaSeparated(const std::string& input) const;
        
        /**
         * @brief Trim whitespace from string
         * @param str String to trim
         * @return Trimmed string
         */
        std::string trim(const std::string& str) const;
    };

    // ========================================
    // METADATA COMMENT PARSER
    // ========================================
    
    /**
     * @brief Parser for extracting metadata from source file comments
     * 
     * Recognizes special comment blocks with metadata:
     * 
     * // @test-name: Rectangle Area Validation
     * // @test-description: Tests area calculation methods
     * // @test-tags: rect, area, validation
     * // @test-timeout: 5000
     * // @test-author: John Doe
     * // @test-parallel: true
     * // @test-dependencies: rect.o, utility.o
     */
    class MetadataParser {
    public:
        /**
         * @brief Parse metadata from source file
         * @param filepath Path to source file
         * @return TestMetadata extracted from comments
         */
        static TestMetadata parseFromFile(const std::string& filepath);
        
        /**
         * @brief Parse metadata from comment lines
         * @param comment_lines Vector of comment lines
         * @return TestMetadata extracted from comments
         */
        static TestMetadata parseFromComments(const std::vector<std::string>& comment_lines);
        
        /**
         * @brief Check if line is a metadata comment
         * @param line Line to check
         * @return true if line contains metadata
         */
        static bool isMetadataComment(const std::string& line);
        
        /**
         * @brief Extract metadata key-value pairs from comment
         * @param comment Single comment line
         * @return Pair of key and value, or empty strings if not metadata
         */
        static std::pair<std::string, std::string> extractMetadata(const std::string& comment);
        
    private:
        
        /**
         * @brief Parse boolean value from string
         * @param value String value ("true", "false", "yes", "no", "1", "0")
         * @return Boolean value
         */
        static bool parseBool(const std::string& value);
        
        /**
         * @brief Parse timeout value from string
         * @param value String value (number optionally followed by "ms", "s", "m")
         * @return Timeout in milliseconds
         */
        static std::chrono::milliseconds parseTimeout(const std::string& value);
    };

} // namespace TestFramework

#endif // TEST_DISCOVERY_H