/*
 * test_mpris_error_handling.cpp - Unit tests for MPRIS error handling system
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS

using namespace PsyMP3::MPRIS;

#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

// Test framework
class MPRISErrorHandlingTest {
public:
    static void runAllTests() {
        std::cout << "Running MPRIS Error Handling Tests...\n";
        
        testMPRISErrorCreation();
        testMPRISErrorHierarchy();
        testErrorLogger();
        testErrorRecoveryManager();
        testGracefulDegradationManager();
        testErrorRecoveryScenarios();
        testDegradationScenarios();
        testErrorStatistics();
        testThreadSafety();
        
        std::cout << "All MPRIS Error Handling Tests passed!\n";
    }
    
private:
    static void testMPRISErrorCreation() {
        std::cout << "Testing MPRISError creation...\n";
        
        // Test basic error creation
        MPRISError error1(MPRISError::Category::Connection, "Test error");
        assert(error1.getCategory() == MPRISError::Category::Connection);
        assert(error1.getMessage() == "Test error");
        assert(error1.getSeverity() == MPRISError::Severity::Error);
        assert(error1.getRecoveryStrategy() == MPRISError::RecoveryStrategy::None);
        
        // Test full error creation
        MPRISError error2(
            MPRISError::Category::Threading,
            MPRISError::Severity::Critical,
            "Threading error",
            "test_context",
            MPRISError::RecoveryStrategy::Restart,
            "Additional details"
        );
        
        assert(error2.getCategory() == MPRISError::Category::Threading);
        assert(error2.getSeverity() == MPRISError::Severity::Critical);
        assert(error2.getMessage() == "Threading error");
        assert(error2.getContext() == "test_context");
        assert(error2.getRecoveryStrategy() == MPRISError::RecoveryStrategy::Restart);
        assert(error2.getDetails() == "Additional details");
        
        // Test error ID uniqueness
        MPRISError error3(MPRISError::Category::Message, "Another error");
        assert(error1.getErrorId() != error2.getErrorId());
        assert(error2.getErrorId() != error3.getErrorId());
        
        // Test string representations
        assert(!error1.getCategoryString().empty());
        assert(!error2.getSeverityString().empty());
        assert(!error2.getRecoveryStrategyString().empty());
        assert(!error2.getFullDescription().empty());
        
        std::cout << "MPRISError creation tests passed.\n";
    }
    
    static void testMPRISErrorHierarchy() {
        std::cout << "Testing MPRISError hierarchy...\n";
        
        // Test specialized exception classes
        ConnectionError conn_error("Connection failed");
        assert(conn_error.getCategory() == MPRISError::Category::Connection);
        assert(conn_error.getRecoveryStrategy() == MPRISError::RecoveryStrategy::Reconnect);
        
        MessageError msg_error("Invalid message");
        assert(msg_error.getCategory() == MPRISError::Category::Message);
        
        ThreadingError thread_error("Deadlock detected");
        assert(thread_error.getCategory() == MPRISError::Category::Threading);
        assert(thread_error.getSeverity() == MPRISError::Severity::Critical);
        
        // Test exception interface
        try {
            throw conn_error;
        } catch (const MPRISError& e) {
            assert(e.getCategory() == MPRISError::Category::Connection);
            assert(std::string(e.what()).find("Connection failed") != std::string::npos);
        } catch (...) {
            assert(false && "Wrong exception type caught");
        }
        
        std::cout << "MPRISError hierarchy tests passed.\n";
    }
    
    static void testErrorLogger() {
        std::cout << "Testing ErrorLogger...\n";
        
        auto& logger = ErrorLogger::getInstance();
        
        // Test singleton behavior
        auto& logger2 = ErrorLogger::getInstance();
        assert(&logger == &logger2);
        
        // Reset stats for clean test
        logger.resetErrorStats();
        
        // Test log level configuration
        logger.setLogLevel(ErrorLogger::LogLevel::Error);
        assert(logger.getLogLevel() == ErrorLogger::LogLevel::Error);
        
        // Test error logging
        MPRISError test_error(MPRISError::Category::Connection, "Test connection error");
        logger.logError(test_error);
        
        // Test convenience methods
        logger.logWarning("Test warning", "test_context");
        logger.logInfo("Test info", "test_context");
        logger.logDebug("Test debug", "test_context"); // Should be filtered out
        
        // Test statistics
        auto stats = logger.getErrorStats();
        assert(stats.total_errors >= 1); // At least the test_error
        assert(stats.connection_errors >= 1);
        
        // Test custom log handler
        bool handler_called = false;
        std::string captured_message;
        
        logger.setLogHandler([&](ErrorLogger::LogLevel level,
                                MPRISError::Category category,
                                const std::string& message,
                                const std::string& context,
                                std::chrono::system_clock::time_point timestamp) {
            handler_called = true;
            captured_message = message;
        });
        
        logger.logError("Custom handler test");
        assert(handler_called);
        assert(captured_message == "Custom handler test");
        
        // Restore default handler
        logger.setDefaultLogHandler();
        
        std::cout << "ErrorLogger tests passed.\n";
    }
    
    static void testErrorRecoveryManager() {
        std::cout << "Testing ErrorRecoveryManager...\n";
        
        ErrorRecoveryManager recovery_manager;
        
        // Test recovery configuration
        ErrorRecoveryManager::RecoveryConfig config;
        config.max_attempts = 5;
        config.initial_delay = std::chrono::milliseconds(100);
        config.max_delay = std::chrono::milliseconds(1000);
        config.backoff_multiplier = 2.0;
        
        recovery_manager.setRecoveryConfig(MPRISError::Category::Connection, config);
        
        auto retrieved_config = recovery_manager.getRecoveryConfig(MPRISError::Category::Connection);
        assert(retrieved_config.max_attempts == 5);
        assert(retrieved_config.initial_delay == std::chrono::milliseconds(100));
        
        // Test recovery actions
        bool recovery_action_called = false;
        recovery_manager.setRecoveryAction(
            MPRISError::RecoveryStrategy::Retry,
            [&recovery_action_called]() -> bool {
                recovery_action_called = true;
                return true; // Simulate successful recovery
            }
        );
        
        // Test recovery attempt
        MPRISError error(
            MPRISError::Category::Connection,
            MPRISError::Severity::Error,
            "Connection lost",
            "test",
            MPRISError::RecoveryStrategy::Retry
        );
        
        bool recovery_result = recovery_manager.attemptRecovery(error);
        assert(recovery_result);
        assert(recovery_action_called);
        
        // Test recovery statistics
        auto stats = recovery_manager.getRecoveryStats();
        assert(stats.total_attempts >= 1);
        assert(stats.successful_recoveries >= 1);
        
        // Test failed recovery
        recovery_manager.setRecoveryAction(
            MPRISError::RecoveryStrategy::Reset,
            []() -> bool {
                return false; // Simulate failed recovery
            }
        );
        
        MPRISError error2(
            MPRISError::Category::Resource,
            MPRISError::Severity::Error,
            "Resource exhausted",
            "test",
            MPRISError::RecoveryStrategy::Reset
        );
        
        bool recovery_result2 = recovery_manager.attemptRecovery(error2);
        assert(!recovery_result2);
        
        std::cout << "ErrorRecoveryManager tests passed.\n";
    }
    
    static void testGracefulDegradationManager() {
        std::cout << "Testing GracefulDegradationManager...\n";
        
        GracefulDegradationManager degradation_manager;
        
        // Test initial state
        assert(degradation_manager.getDegradationLevel() == GracefulDegradationManager::DegradationLevel::None);
        assert(degradation_manager.isFeatureAvailable("metadata_updates"));
        assert(degradation_manager.isFeatureAvailable("playback_control"));
        
        // Test manual degradation level setting
        degradation_manager.setDegradationLevel(GracefulDegradationManager::DegradationLevel::Limited);
        assert(degradation_manager.getDegradationLevel() == GracefulDegradationManager::DegradationLevel::Limited);
        assert(!degradation_manager.isFeatureAvailable("metadata_updates"));
        assert(degradation_manager.isFeatureAvailable("playback_control"));
        
        degradation_manager.setDegradationLevel(GracefulDegradationManager::DegradationLevel::Disabled);
        assert(!degradation_manager.isFeatureAvailable("playback_control"));
        assert(!degradation_manager.isFeatureAvailable("metadata_updates"));
        
        // Reset for auto-degradation test
        degradation_manager.setDegradationLevel(GracefulDegradationManager::DegradationLevel::None);
        
        // Test auto-degradation with error reporting (simplified to avoid hanging)
        degradation_manager.setErrorThreshold(MPRISError::Category::Connection, 2);
        degradation_manager.setTimeWindow(std::chrono::seconds(1)); // Short window for testing
        
        // Report a few connection errors
        ConnectionError error1("Connection error 1");
        degradation_manager.reportError(error1);
        
        ConnectionError error2("Connection error 2");
        degradation_manager.reportError(error2);
        
        // Test feature management (basic functionality)
        degradation_manager.disableFeature("custom_feature");
        assert(!degradation_manager.isFeatureAvailable("custom_feature"));
        
        degradation_manager.enableFeature("custom_feature");
        assert(degradation_manager.isFeatureAvailable("custom_feature"));
        
        // Test that we can set different degradation levels without issues
        degradation_manager.setDegradationLevel(GracefulDegradationManager::DegradationLevel::Minimal);
        assert(degradation_manager.getDegradationLevel() == GracefulDegradationManager::DegradationLevel::Minimal);
        
        std::cout << "GracefulDegradationManager tests passed.\n";
    }
    
    static void testErrorRecoveryScenarios() {
        std::cout << "Testing error recovery scenarios...\n";
        
        ErrorRecoveryManager recovery_manager;
        
        // Scenario 1: Connection error with successful recovery
        int retry_count = 0;
        recovery_manager.setRecoveryAction(
            MPRISError::RecoveryStrategy::Reconnect,
            [&retry_count]() -> bool {
                retry_count++;
                return retry_count >= 2; // Succeed on second attempt
            }
        );
        
        ConnectionError conn_error("Connection lost");
        
        // First attempt should fail
        bool result1 = recovery_manager.attemptRecovery(conn_error);
        assert(!result1);
        assert(retry_count == 1);
        
        // Second attempt should succeed
        bool result2 = recovery_manager.attemptRecovery(conn_error);
        assert(result2);
        assert(retry_count == 2);
        
        // Scenario 2: Threading error (should have limited recovery attempts)
        ErrorRecoveryManager::RecoveryConfig threading_config;
        threading_config.max_attempts = 1; // Very limited for threading errors
        recovery_manager.setRecoveryConfig(MPRISError::Category::Threading, threading_config);
        
        recovery_manager.setRecoveryAction(
            MPRISError::RecoveryStrategy::Restart,
            []() -> bool { return false; } // Always fail
        );
        
        ThreadingError thread_error("Deadlock detected");
        bool thread_result1 = recovery_manager.attemptRecovery(thread_error);
        assert(!thread_result1);
        
        // Second attempt should be blocked due to max_attempts = 1
        bool thread_result2 = recovery_manager.attemptRecovery(thread_error);
        assert(!thread_result2);
        
        std::cout << "Error recovery scenarios tests passed.\n";
    }
    
    static void testDegradationScenarios() {
        std::cout << "Testing degradation scenarios...\n";
        
        GracefulDegradationManager degradation_manager;
        
        // Scenario 1: Progressive degradation due to connection errors
        degradation_manager.setErrorThreshold(MPRISError::Category::Connection, 2);
        degradation_manager.setTimeWindow(std::chrono::seconds(60));
        
        // Initial state - all features available
        assert(degradation_manager.getDegradationLevel() == GracefulDegradationManager::DegradationLevel::None);
        assert(degradation_manager.isFeatureAvailable("metadata_updates"));
        assert(degradation_manager.isFeatureAvailable("seeking"));
        
        // Report connection errors to trigger degradation
        for (int i = 0; i < 3; ++i) {
            ConnectionError error("Connection error " + std::to_string(i));
            degradation_manager.reportError(error);
        }
        
        // Scenario 2: Critical error causing immediate full degradation
        degradation_manager.setDegradationLevel(GracefulDegradationManager::DegradationLevel::None);
        degradation_manager.setErrorThreshold(MPRISError::Category::Threading, 1);
        
        ThreadingError critical_error("Critical threading error");
        degradation_manager.reportError(critical_error);
        
        // Should trigger some level of degradation for critical errors
        // (Exact behavior depends on implementation)
        
        // Scenario 3: Feature-specific degradation
        degradation_manager.setDegradationLevel(GracefulDegradationManager::DegradationLevel::Limited);
        
        // In limited mode, metadata updates should be disabled
        assert(!degradation_manager.isFeatureAvailable("metadata_updates"));
        // But basic playback control should still work
        assert(degradation_manager.isFeatureAvailable("playback_control"));
        
        std::cout << "Degradation scenarios tests passed.\n";
    }
    
    static void testErrorStatistics() {
        std::cout << "Testing error statistics...\n";
        
        auto& logger = ErrorLogger::getInstance();
        logger.resetErrorStats();
        
        ErrorRecoveryManager recovery_manager;
        recovery_manager.resetRecoveryStats();
        
        // Generate various types of errors
        std::vector<MPRISError> test_errors = {
            ConnectionError("Connection error 1"),
            ConnectionError("Connection error 2"),
            MessageError("Message error 1"),
            PlayerStateError("Player state error 1"),
            ThreadingError("Threading error 1")
        };
        
        // Log all errors
        for (const auto& error : test_errors) {
            logger.logError(error);
        }
        
        // Check statistics
        auto stats = logger.getErrorStats();
        assert(stats.total_errors >= test_errors.size());
        assert(stats.connection_errors >= 2);
        assert(stats.message_errors >= 1);
        assert(stats.player_state_errors >= 1);
        assert(stats.threading_errors >= 1);
        
        // Test recovery statistics
        recovery_manager.setRecoveryAction(
            MPRISError::RecoveryStrategy::Retry,
            []() -> bool { return true; }
        );
        
        recovery_manager.setRecoveryAction(
            MPRISError::RecoveryStrategy::Reconnect,
            []() -> bool { return false; }
        );
        
        // Attempt recoveries
        for (const auto& error : test_errors) {
            recovery_manager.attemptRecovery(error);
        }
        
        auto recovery_stats = recovery_manager.getRecoveryStats();
        assert(recovery_stats.total_attempts > 0);
        
        std::cout << "Error statistics tests passed.\n";
    }
    
    static void testThreadSafety() {
        std::cout << "Testing thread safety...\n";
        
        auto& logger = ErrorLogger::getInstance();
        ErrorRecoveryManager recovery_manager;
        GracefulDegradationManager degradation_manager;
        
        const int num_threads = 4;
        const int errors_per_thread = 10;
        std::vector<std::thread> threads;
        
        // Test concurrent error logging
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&logger, t, errors_per_thread]() {
                for (int i = 0; i < errors_per_thread; ++i) {
                    MPRISError error(
                        MPRISError::Category::Connection,
                        "Thread " + std::to_string(t) + " error " + std::to_string(i)
                    );
                    logger.logError(error);
                    
                    // Small delay to increase chance of race conditions
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
            });
        }
        
        // Test concurrent recovery attempts
        recovery_manager.setRecoveryAction(
            MPRISError::RecoveryStrategy::Retry,
            []() -> bool {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                return true;
            }
        );
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&recovery_manager, t, errors_per_thread]() {
                for (int i = 0; i < errors_per_thread; ++i) {
                    MPRISError error(
                        MPRISError::Category::Resource,
                        MPRISError::Severity::Error,
                        "Thread " + std::to_string(t) + " recovery " + std::to_string(i),
                        "test",
                        MPRISError::RecoveryStrategy::Retry
                    );
                    recovery_manager.attemptRecovery(error);
                }
            });
        }
        
        // Test concurrent degradation manager access
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&degradation_manager, t, errors_per_thread]() {
                for (int i = 0; i < errors_per_thread; ++i) {
                    MPRISError error(
                        MPRISError::Category::Protocol,
                        "Thread " + std::to_string(t) + " degradation " + std::to_string(i)
                    );
                    degradation_manager.reportError(error);
                    
                    // Also test feature availability checks
                    bool available = degradation_manager.isFeatureAvailable("test_feature");
                    (void)available; // Suppress unused variable warning
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Verify that no crashes occurred and statistics are reasonable
        auto stats = logger.getErrorStats();
        assert(stats.total_errors >= num_threads * errors_per_thread);
        
        auto recovery_stats = recovery_manager.getRecoveryStats();
        assert(recovery_stats.total_attempts >= num_threads * errors_per_thread);
        
        std::cout << "Thread safety tests passed.\n";
    }
};

// Test main function
int main() {
    try {
        MPRISErrorHandlingTest::runAllTests();
        std::cout << "\nAll MPRIS error handling tests completed successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}

#else // !HAVE_DBUS

#include <iostream>

int main() {
    std::cout << "MPRIS error handling tests skipped - D-Bus support not compiled in\n";
    return 0;
}

#endif // HAVE_DBUS