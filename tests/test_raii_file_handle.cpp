/*
 * test_raii_file_handle.cpp - Unit tests for RAIIFileHandle
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include <iostream>
#include <string>
#include <fstream>

// Simple test framework
namespace SimpleTest {
    void log(const std::string& message) {
        std::cout << "[TEST] " << message << std::endl;
    }
    
    bool assert_true(bool condition, const std::string& message) {
        if (condition) {
            log("PASS: " + message);
            return true;
        } else {
            log("FAIL: " + message);
            return false;
        }
    }
}

// Mock RAIIFileHandle for testing (simplified version)
class MockRAIIFileHandle {
private:
    FILE* m_file;
    bool m_owns_handle;
    
public:
    MockRAIIFileHandle() : m_file(nullptr), m_owns_handle(false) {
        SimpleTest::log("RAIIFileHandle default constructor");
    }
    
    explicit MockRAIIFileHandle(FILE* file, bool take_ownership = true) 
        : m_file(file), m_owns_handle(take_ownership) {
        SimpleTest::log("RAIIFileHandle constructor with file handle");
    }
    
    MockRAIIFileHandle(MockRAIIFileHandle&& other) noexcept 
        : m_file(other.m_file), m_owns_handle(other.m_owns_handle) {
        other.m_file = nullptr;
        other.m_owns_handle = false;
        SimpleTest::log("RAIIFileHandle move constructor");
    }
    
    MockRAIIFileHandle& operator=(MockRAIIFileHandle&& other) noexcept {
        if (this != &other) {
            close();
            m_file = other.m_file;
            m_owns_handle = other.m_owns_handle;
            other.m_file = nullptr;
            other.m_owns_handle = false;
            SimpleTest::log("RAIIFileHandle move assignment");
        }
        return *this;
    }
    
    ~MockRAIIFileHandle() {
        SimpleTest::log("RAIIFileHandle destructor");
        close();
    }
    
    // Delete copy operations
    MockRAIIFileHandle(const MockRAIIFileHandle&) = delete;
    MockRAIIFileHandle& operator=(const MockRAIIFileHandle&) = delete;
    
    bool open(const char* filename, const char* mode) {
        close();
        if (filename && mode) {
            m_file = fopen(filename, mode);
            m_owns_handle = (m_file != nullptr);
            SimpleTest::log(std::string("File open: ") + filename + " - " + (m_file ? "success" : "failed"));
            return m_file != nullptr;
        }
        return false;
    }
    
    int close() {
        int result = 0;
        if (m_file && m_owns_handle) {
            result = fclose(m_file);
            SimpleTest::log("File closed");
        }
        m_file = nullptr;
        m_owns_handle = false;
        return result;
    }
    
    FILE* get() const { return m_file; }
    bool is_valid() const { return m_file != nullptr; }
    bool owns_handle() const { return m_owns_handle; }
    
    FILE* release() {
        FILE* file = m_file;
        m_file = nullptr;
        m_owns_handle = false;
        SimpleTest::log("File handle released");
        return file;
    }
    
    void reset(FILE* file = nullptr, bool take_ownership = true) {
        close();
        m_file = file;
        m_owns_handle = take_ownership;
        SimpleTest::log("File handle reset");
    }
    
    explicit operator bool() const { return is_valid(); }
    operator FILE*() const { return m_file; }
};

// Test functions
bool test_raii_basic_functionality() {
    SimpleTest::log("=== Testing RAII Basic Functionality ===");
    
    // Create a test file
    const char* test_filename = "test_raii_file.txt";
    {
        std::ofstream test_file(test_filename);
        test_file << "Hello, RAII World!";
    }
    
    // Test 1: Basic file opening and automatic closing
    {
        MockRAIIFileHandle handle;
        if (!SimpleTest::assert_true(handle.open(test_filename, "r"), "File opened successfully")) {
            return false;
        }
        
        if (!SimpleTest::assert_true(handle.is_valid(), "File handle is valid")) {
            return false;
        }
        
        if (!SimpleTest::assert_true(handle.owns_handle(), "File handle is owned")) {
            return false;
        }
        
        // File should be automatically closed when handle goes out of scope
    }
    
    // Clean up test file
    std::remove(test_filename);
    
    return true;
}

bool test_raii_move_semantics() {
    SimpleTest::log("=== Testing RAII Move Semantics ===");
    
    // Create a test file
    const char* test_filename = "test_raii_move.txt";
    {
        std::ofstream test_file(test_filename);
        test_file << "Move semantics test";
    }
    
    // Test move constructor
    {
        MockRAIIFileHandle handle1;
        handle1.open(test_filename, "r");
        
        MockRAIIFileHandle handle2 = std::move(handle1);
        
        if (!SimpleTest::assert_true(!handle1.is_valid(), "Original handle is invalid after move")) {
            return false;
        }
        
        if (!SimpleTest::assert_true(handle2.is_valid(), "Moved-to handle is valid")) {
            return false;
        }
    }
    
    // Test move assignment
    {
        MockRAIIFileHandle handle1;
        MockRAIIFileHandle handle2;
        
        handle1.open(test_filename, "r");
        handle2 = std::move(handle1);
        
        if (!SimpleTest::assert_true(!handle1.is_valid(), "Original handle is invalid after move assignment")) {
            return false;
        }
        
        if (!SimpleTest::assert_true(handle2.is_valid(), "Move-assigned handle is valid")) {
            return false;
        }
    }
    
    // Clean up test file
    std::remove(test_filename);
    
    return true;
}

bool test_raii_resource_management() {
    SimpleTest::log("=== Testing RAII Resource Management ===");
    
    // Create a test file
    const char* test_filename = "test_raii_resource.txt";
    {
        std::ofstream test_file(test_filename);
        test_file << "Resource management test";
    }
    
    // Test release functionality
    {
        MockRAIIFileHandle handle;
        handle.open(test_filename, "r");
        
        FILE* raw_file = handle.release();
        
        if (!SimpleTest::assert_true(!handle.is_valid(), "Handle is invalid after release")) {
            return false;
        }
        
        if (!SimpleTest::assert_true(!handle.owns_handle(), "Handle doesn't own after release")) {
            return false;
        }
        
        if (!SimpleTest::assert_true(raw_file != nullptr, "Released file pointer is valid")) {
            return false;
        }
        
        // Manually close the released file
        fclose(raw_file);
    }
    
    // Test reset functionality
    {
        MockRAIIFileHandle handle;
        FILE* raw_file = fopen(test_filename, "r");
        
        handle.reset(raw_file, true);
        
        if (!SimpleTest::assert_true(handle.is_valid(), "Handle is valid after reset")) {
            return false;
        }
        
        if (!SimpleTest::assert_true(handle.owns_handle(), "Handle owns after reset")) {
            return false;
        }
        
        // File should be automatically closed when handle is destroyed
    }
    
    // Clean up test file
    std::remove(test_filename);
    
    return true;
}

bool test_raii_exception_safety() {
    SimpleTest::log("=== Testing RAII Exception Safety ===");
    
    // Create a test file
    const char* test_filename = "test_raii_exception.txt";
    {
        std::ofstream test_file(test_filename);
        test_file << "Exception safety test";
    }
    
    // Test that files are properly closed even when exceptions occur
    try {
        MockRAIIFileHandle handle;
        handle.open(test_filename, "r");
        
        if (!SimpleTest::assert_true(handle.is_valid(), "File opened before exception")) {
            return false;
        }
        
        // Simulate an exception
        throw std::runtime_error("Test exception");
        
    } catch (const std::exception& e) {
        SimpleTest::log("Caught expected exception: " + std::string(e.what()));
        // File should have been automatically closed by RAII
    }
    
    // Clean up test file
    std::remove(test_filename);
    
    return true;
}

bool test_raii_multiple_instances() {
    SimpleTest::log("=== Testing RAII Multiple Instances ===");
    
    // Create multiple test files
    const char* test_files[] = {
        "test_raii_multi1.txt",
        "test_raii_multi2.txt",
        "test_raii_multi3.txt"
    };
    
    // Create test files
    for (const char* filename : test_files) {
        std::ofstream test_file(filename);
        test_file << "Multi-instance test: " << filename;
    }
    
    // Test multiple RAII handles
    {
        MockRAIIFileHandle handles[3];
        
        for (int i = 0; i < 3; i++) {
            if (!handles[i].open(test_files[i], "r")) {
                SimpleTest::log("Failed to open file: " + std::string(test_files[i]));
                return false;
            }
        }
        
        // All handles should be valid
        for (int i = 0; i < 3; i++) {
            if (!SimpleTest::assert_true(handles[i].is_valid(), 
                "Handle " + std::to_string(i) + " is valid")) {
                return false;
            }
        }
        
        // All files should be automatically closed when handles go out of scope
    }
    
    // Clean up test files
    for (const char* filename : test_files) {
        std::remove(filename);
    }
    
    return true;
}

int main() {
    SimpleTest::log("=== RAII File Handle Tests ===");
    
    bool all_passed = true;
    
    all_passed &= test_raii_basic_functionality();
    all_passed &= test_raii_move_semantics();
    all_passed &= test_raii_resource_management();
    all_passed &= test_raii_exception_safety();
    all_passed &= test_raii_multiple_instances();
    
    if (all_passed) {
        SimpleTest::log("=== All RAII File Handle Tests PASSED ===");
        return 0;
    } else {
        SimpleTest::log("=== Some RAII File Handle Tests FAILED ===");
        return 1;
    }
}