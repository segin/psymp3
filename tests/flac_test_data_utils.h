/*
 * flac_test_data_utils.h - Common utilities for FLAC test data validation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#ifndef FLAC_TEST_DATA_UTILS_H
#define FLAC_TEST_DATA_UTILS_H

#include <vector>
#include <string>
#include <fstream>
#include <iostream>

/**
 * @brief Common test data file paths for FLAC validation
 */
class FLACTestDataUtils {
public:
    /**
     * @brief Get list of available FLAC test files
     * @return Vector of test file paths in order of preference
     */
    static std::vector<std::string> getTestFiles() {
        return {
            "tests/data/11 life goes by.flac",
            "tests/data/RADIO GA GA.flac", 
            "tests/data/11 Everlong.flac"
        };
    }
    
    /**
     * @brief Find the first available test file
     * @return Path to first available test file, empty string if none found
     */
    static std::string findAvailableTestFile() {
        auto files = getTestFiles();
        for (const auto& file : files) {
            if (fileExists(file)) {
                return file;
            }
        }
        return "";
    }
    
    /**
     * @brief Get all available test files
     * @return Vector of paths to all existing test files
     */
    static std::vector<std::string> getAvailableTestFiles() {
        std::vector<std::string> available;
        auto files = getTestFiles();
        for (const auto& file : files) {
            if (fileExists(file)) {
                available.push_back(file);
            }
        }
        return available;
    }
    
    /**
     * @brief Check if a file exists
     * @param path File path to check
     * @return true if file exists and is readable
     */
    static bool fileExists(const std::string& path) {
        std::ifstream file(path);
        return file.good();
    }
    
    /**
     * @brief Get file size in bytes
     * @param path File path
     * @return File size in bytes, 0 if file doesn't exist
     */
    static size_t getFileSize(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return 0;
        return static_cast<size_t>(file.tellg());
    }
    
    /**
     * @brief Print test file information
     * @param testName Name of the test for logging
     */
    static void printTestFileInfo(const std::string& testName) {
        std::cout << "=== " << testName << " - Test File Information ===" << std::endl;
        auto files = getTestFiles();
        for (const auto& file : files) {
            std::cout << "File: " << file;
            if (fileExists(file)) {
                size_t size = getFileSize(file);
                std::cout << " (EXISTS, " << size << " bytes)";
            } else {
                std::cout << " (NOT FOUND)";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
    
    /**
     * @brief Validate that at least one test file is available
     * @param testName Name of the test for error reporting
     * @return true if at least one test file is available
     */
    static bool validateTestDataAvailable(const std::string& testName) {
        auto available = getAvailableTestFiles();
        if (available.empty()) {
            std::cerr << "ERROR: No FLAC test data files found for " << testName << std::endl;
            std::cerr << "Expected files in tests/data/:" << std::endl;
            auto files = getTestFiles();
            for (const auto& file : files) {
                std::cerr << "  - " << file << std::endl;
            }
            return false;
        }
        return true;
    }
};

#endif // FLAC_TEST_DATA_UTILS_H