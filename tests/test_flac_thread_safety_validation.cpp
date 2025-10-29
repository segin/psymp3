/*
 * test_flac_thread_safety_validation.cpp - FLAC demuxer thread safety validation test
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

/**
 * @brief Test FLAC demuxer thread safety validation
 * 
 * This test validates that the FLAC demuxer correctly implements the public/private
 * lock pattern and maintains thread safety under concurrent access.
 */
int main()
{
    Debug::log("test", "=== FLAC Thread Safety Validation Test ===");
    
    try {
        // Create FLAC demuxer with test file
        auto handler = std::make_unique<FileIOHandler>("data/11 Everlong.flac");
        
        // Check if file exists by trying to get file size
        if (handler->getFileSize() <= 0) {
            Debug::log("test", "Failed to open test file or file is empty");
            return 1;
        }
        
        FLACDemuxer demuxer(std::move(handler));
        
        // Test 1: Validate thread safety implementation
        Debug::log("test", "Test 1: Validating thread safety implementation");
        bool thread_safety_valid = demuxer.validateThreadSafetyImplementation();
        
        if (!thread_safety_valid) {
            Debug::log("test", "FAIL: Thread safety validation failed");
            return 1;
        }
        
        Debug::log("test", "PASS: Thread safety implementation validated");
        
        // Test 2: Get detailed thread safety validation results
        Debug::log("test", "Test 2: Getting detailed validation results");
        auto validation = demuxer.validateThreadSafety();
        
        Debug::log("test", "Thread safety compliance score: ", validation.getComplianceScore(), "%");
        Debug::log("test", "Public methods with locks: ", validation.public_methods_with_locks);
        Debug::log("test", "Private unlocked methods: ", validation.private_unlocked_methods);
        
        if (!validation.isValid()) {
            Debug::log("test", "FAIL: Thread safety validation not fully compliant");
            return 1;
        }
        
        Debug::log("test", "PASS: Thread safety validation fully compliant");
        
        // Test 3: Create thread safety documentation
        Debug::log("test", "Test 3: Creating thread safety documentation");
        demuxer.createThreadSafetyDocumentation();
        Debug::log("test", "PASS: Thread safety documentation created");
        
        // Test 4: Parse container (this tests the actual thread safety implementation)
        Debug::log("test", "Test 4: Testing container parsing with thread safety");
        if (!demuxer.parseContainer()) {
            Debug::log("test", "FAIL: Container parsing failed");
            return 1;
        }
        
        Debug::log("test", "PASS: Container parsing successful");
        
        // Test 5: Concurrent access simulation (basic test)
        Debug::log("test", "Test 5: Basic concurrent access test");
        
        std::atomic<bool> test_running{true};
        std::atomic<int> errors{0};
        
        // Thread 1: Repeatedly get stream info
        std::thread thread1([&demuxer, &test_running, &errors]() {
            while (test_running.load()) {
                try {
                    auto streams = demuxer.getStreams();
                    if (streams.empty()) {
                        errors.fetch_add(1);
                    }
                } catch (...) {
                    errors.fetch_add(1);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        
        // Thread 2: Repeatedly get duration and position
        std::thread thread2([&demuxer, &test_running, &errors]() {
            while (test_running.load()) {
                try {
                    uint64_t duration = demuxer.getDuration();
                    uint64_t position = demuxer.getPosition();
                    if (duration == 0) {
                        errors.fetch_add(1);
                    }
                } catch (...) {
                    errors.fetch_add(1);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        
        // Thread 3: Repeatedly check EOF status
        std::thread thread3([&demuxer, &test_running, &errors]() {
            while (test_running.load()) {
                try {
                    bool eof = demuxer.isEOF();
                    // EOF status is valid regardless of value
                } catch (...) {
                    errors.fetch_add(1);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        
        // Run concurrent access test for 100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        test_running.store(false);
        
        // Wait for threads to complete
        thread1.join();
        thread2.join();
        thread3.join();
        
        if (errors.load() > 0) {
            Debug::log("test", "FAIL: Concurrent access test had ", errors.load(), " errors");
            return 1;
        }
        
        Debug::log("test", "PASS: Concurrent access test completed without errors");
        
        Debug::log("test", "=== All Thread Safety Tests Passed ===");
        return 0;
        
    } catch (const std::exception& e) {
        Debug::log("test", "FAIL: Exception during thread safety test: ", e.what());
        return 1;
    } catch (...) {
        Debug::log("test", "FAIL: Unknown exception during thread safety test");
        return 1;
    }
}