/*
 * test_iohandler_resource_management.cpp - Unit tests for IOHandler resource management
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "../include/psymp3.h"
#include "test_framework.h"

/**
 * @brief Test memory allocation failure handling
 */
bool test_memory_allocation_failure_handling() {
    TestFramework::log("Testing memory allocation failure handling...");
    
    // Create a test IOHandler instance
    class TestIOHandler : public IOHandler {
    public:
        TestIOHandler() : IOHandler() {}
        
        // Expose protected methods for testing
        bool testHandleMemoryAllocationFailure(size_t size, const std::string& context) {
            return handleMemoryAllocationFailure(size, context);
        }
        
        bool testCheckMemoryLimits(size_t bytes) {
            return checkMemoryLimits(bytes);
        }
        
        void testUpdateMemoryUsage(size_t usage) {
            updateMemoryUsage(usage);
        }
    };
    
    TestIOHandler handler;
    
    // Test 1: Normal memory allocation should succeed
    bool result1 = handler.testCheckMemoryLimits(1024); // 1KB should be fine
    if (!result1) {
        TestFramework::log("FAIL: Normal memory allocation check failed");
        return false;
    }
    
    // Test 2: Very large allocation should fail
    bool result2 = handler.testCheckMemoryLimits(SIZE_MAX); // Maximum size should fail
    if (result2) {
        TestFramework::log("FAIL: Very large memory allocation should have failed");
        return false;
    }
    
    // Test 3: Test memory allocation failure recovery
    bool recovery_result = handler.testHandleMemoryAllocationFailure(1024 * 1024, "test_context");
    // Recovery should attempt to help, result depends on system state
    TestFramework::log("Memory allocation failure recovery result: ", recovery_result ? "success" : "failed");
    
    // Test 4: Test memory usage tracking
    handler.testUpdateMemoryUsage(1024);
    handler.testUpdateMemoryUsage(0); // Should clean up
    
    TestFramework::log("Memory allocation failure handling tests passed");
    return true;
}

/**
 * @brief Test resource exhaustion handling
 */
bool test_resource_exhaustion_handling() {
    TestFramework::log("Testing resource exhaustion handling...");
    
    class TestIOHandler : public IOHandler {
    public:
        TestIOHandler() : IOHandler() {}
        
        bool testHandleResourceExhaustion(const std::string& type, const std::string& context) {
            return handleResourceExhaustion(type, context);
        }
    };
    
    TestIOHandler handler;
    
    // Test 1: Memory resource exhaustion
    bool memory_result = handler.testHandleResourceExhaustion("memory", "test_memory_exhaustion");
    TestFramework::log("Memory resource exhaustion handling: ", memory_result ? "recovered" : "failed");
    
    // Test 2: File descriptor exhaustion
    bool fd_result = handler.testHandleResourceExhaustion("file_descriptors", "test_fd_exhaustion");
    TestFramework::log("File descriptor exhaustion handling: ", fd_result ? "recovered" : "failed");
    
    // Test 3: Disk space exhaustion
    bool disk_result = handler.testHandleResourceExhaustion("disk_space", "test_disk_exhaustion");
    TestFramework::log("Disk space exhaustion handling: ", disk_result ? "recovered" : "failed");
    // Disk space exhaustion should typically not be recoverable
    
    // Test 4: Network connection exhaustion
    bool network_result = handler.testHandleResourceExhaustion("network_connections", "test_network_exhaustion");
    TestFramework::log("Network connection exhaustion handling: ", network_result ? "recovered" : "failed");
    
    // Test 5: Unknown resource type
    bool unknown_result = handler.testHandleResourceExhaustion("unknown_resource", "test_unknown");
    if (unknown_result) {
        TestFramework::log("FAIL: Unknown resource type should not be recoverable");
        return false;
    }
    
    TestFramework::log("Resource exhaustion handling tests passed");
    return true;
}

/**
 * @brief Test safe error propagation
 */
bool test_safe_error_propagation() {
    TestFramework::log("Testing safe error propagation...");
    
    class TestIOHandler : public IOHandler {
    public:
        TestIOHandler() : IOHandler() {}
        
        void testSafeErrorPropagation(int error_code, const std::string& message, std::function<void()> cleanup = nullptr) {
            safeErrorPropagation(error_code, message, cleanup);
        }
        
        int getError() const { return getLastError(); }
        bool isClosed() const { return m_closed; }
    };
    
    TestIOHandler handler;
    
    // Test 1: Error propagation without cleanup
    handler.testSafeErrorPropagation(EINVAL, "Test error without cleanup");
    if (handler.getError() != EINVAL) {
        TestFramework::log("FAIL: Error code not properly set");
        return false;
    }
    
    // Test 2: Error propagation with successful cleanup
    bool cleanup_called = false;
    auto cleanup_func = [&cleanup_called]() {
        cleanup_called = true;
        TestFramework::log("Cleanup function called successfully");
    };
    
    handler.testSafeErrorPropagation(EIO, "Test error with cleanup", cleanup_func);
    if (!cleanup_called) {
        TestFramework::log("FAIL: Cleanup function was not called");
        return false;
    }
    
    // Test 3: Error propagation with throwing cleanup
    auto throwing_cleanup = []() {
        throw std::runtime_error("Cleanup failed");
    };
    
    // This should not crash, even if cleanup throws
    handler.testSafeErrorPropagation(ENOMEM, "Test error with throwing cleanup", throwing_cleanup);
    if (handler.getError() != ENOMEM) {
        TestFramework::log("FAIL: Error should still be set even if cleanup throws");
        return false;
    }
    
    // Test 4: Fatal error should close handler
    handler.testSafeErrorPropagation(ENOMEM, "Fatal memory error");
    if (!handler.isClosed()) {
        TestFramework::log("FAIL: Fatal error should close the handler");
        return false;
    }
    
    TestFramework::log("Safe error propagation tests passed");
    return true;
}

/**
 * @brief Test destructor cleanup in error conditions
 */
bool test_destructor_cleanup() {
    TestFramework::log("Testing destructor cleanup in error conditions...");
    
    // Test that destructors work properly even when objects are in error states
    {
        class TestIOHandler : public IOHandler {
        public:
            TestIOHandler() : IOHandler() {
                // Simulate some resource allocation
                updateMemoryUsage(1024);
            }
            
            void simulateError() {
                m_error = EIO;
                m_closed = true;
            }
        };
        
        TestIOHandler* handler = new TestIOHandler();
        handler->simulateError(); // Put in error state
        
        // Destructor should handle cleanup even in error state
        delete handler; // This should not crash or leak
    }
    
    // Test memory statistics after cleanup
    auto memory_stats = IOHandler::getMemoryStats();
    TestFramework::log("Memory stats after destructor test - total usage: ", memory_stats["total_memory_usage"]);
    
    TestFramework::log("Destructor cleanup tests passed");
    return true;
}

/**
 * @brief Test memory leak prevention
 */
bool test_memory_leak_prevention() {
    TestFramework::log("Testing memory leak prevention...");
    
    auto initial_stats = IOHandler::getMemoryStats();
    size_t initial_usage = initial_stats["total_memory_usage"];
    
    // Create and destroy multiple handlers to test for leaks
    for (int i = 0; i < 10; i++) {
        class TestIOHandler : public IOHandler {
        public:
            TestIOHandler() : IOHandler() {
                updateMemoryUsage(1024 * (i + 1)); // Varying memory usage
            }
        };
        
        TestIOHandler* handler = new TestIOHandler();
        
        // Simulate some operations that might cause leaks
        handler->handleMemoryAllocationFailure(2048, "test_leak_prevention");
        handler->handleResourceExhaustion("memory", "test_leak_prevention");
        
        delete handler;
    }
    
    // Check that memory usage returned to initial level
    auto final_stats = IOHandler::getMemoryStats();
    size_t final_usage = final_stats["total_memory_usage"];
    
    if (final_usage > initial_usage + 1024) { // Allow small variance
        TestFramework::log("FAIL: Potential memory leak detected - initial: ", initial_usage, ", final: ", final_usage);
        return false;
    }
    
    TestFramework::log("Memory leak prevention tests passed");
    return true;
}

/**
 * @brief Main test function
 */
bool test_iohandler_resource_management() {
    TestFramework::log("=== IOHandler Resource Management Tests ===");
    
    bool all_passed = true;
    
    all_passed &= test_memory_allocation_failure_handling();
    all_passed &= test_resource_exhaustion_handling();
    all_passed &= test_safe_error_propagation();
    all_passed &= test_destructor_cleanup();
    all_passed &= test_memory_leak_prevention();
    
    if (all_passed) {
        TestFramework::log("=== All IOHandler Resource Management Tests PASSED ===");
    } else {
        TestFramework::log("=== Some IOHandler Resource Management Tests FAILED ===");
    }
    
    return all_passed;
}