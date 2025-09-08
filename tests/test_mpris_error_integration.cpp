/*
 * test_mpris_error_integration.cpp - Integration test for MPRIS error handling components
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS

#include <cassert>
#include <iostream>

// Integration test class for error handling components
class MPRISErrorIntegrationTest {
public:
    static void runAllTests() {
        std::cout << "Running MPRIS Error Integration Tests...\n";
        
        testErrorLoggerIntegration();
        testErrorRecoveryIntegration();
        testGracefulDegradationIntegration();
        testComponentInteraction();
        
        std::cout << "All MPRIS Error Integration Tests passed!\n";
    }
    
private:
    static void testErrorLoggerIntegration() {
        std::cout << "Testing ErrorLogger integration...\n";
        
        auto& logger = MPRISTypes::ErrorLogger::getInstance();
        
        // Reset for clean test
        logger.resetErrorStats();
        
        // Test logging different error types
        MPRISTypes::ConnectionError conn_error("Integration test connection error");
        MPRISTypes::MessageError msg_error("Integration test message error");
        MPRISTypes::ThreadingError thread_error("Integration test threading error");
        
        logger.logError(conn_error);
        logger.logError(msg_error);
        logger.logError(thread_error);
        
        // Verify statistics
        auto stats = logger.getErrorStats();
        assert(stats.total_errors >= 3);
        assert(stats.connection_errors >= 1);
        assert(stats.message_errors >= 1);
        assert(stats.threading_errors >= 1);
        
        std::cout << "ErrorLogger integration tests passed.\n";
    }
    
    static void testErrorRecoveryIntegration() {
        std::cout << "Testing ErrorRecoveryManager integration...\n";
        
        MPRISTypes::ErrorRecoveryManager recovery_manager;
        
        // Set up recovery actions
        bool retry_called = false;
        bool reconnect_called = false;
        
        recovery_manager.setRecoveryAction(
            MPRISTypes::MPRISError::RecoveryStrategy::Retry,
            [&retry_called]() -> bool {
                retry_called = true;
                return true;
            }
        );
        
        recovery_manager.setRecoveryAction(
            MPRISTypes::MPRISError::RecoveryStrategy::Reconnect,
            [&reconnect_called]() -> bool {
                reconnect_called = true;
                return false; // Simulate failure
            }
        );
        
        // Test recovery attempts
        MPRISTypes::MPRISError retry_error(
            MPRISTypes::MPRISError::Category::Message,
            MPRISTypes::MPRISError::Severity::Warning,
            "Retry test error",
            "integration_test",
            MPRISTypes::MPRISError::RecoveryStrategy::Retry
        );
        
        MPRISTypes::MPRISError reconnect_error(
            MPRISTypes::MPRISError::Category::Connection,
            MPRISTypes::MPRISError::Severity::Error,
            "Reconnect test error",
            "integration_test",
            MPRISTypes::MPRISError::RecoveryStrategy::Reconnect
        );
        
        bool retry_result = recovery_manager.attemptRecovery(retry_error);
        bool reconnect_result = recovery_manager.attemptRecovery(reconnect_error);
        
        assert(retry_result == true);
        assert(reconnect_result == false);
        assert(retry_called);
        assert(reconnect_called);
        
        // Check statistics
        auto stats = recovery_manager.getRecoveryStats();
        assert(stats.total_attempts >= 2);
        assert(stats.successful_recoveries >= 1);
        assert(stats.failed_recoveries >= 1);
        
        std::cout << "ErrorRecoveryManager integration tests passed.\n";
    }
    
    static void testGracefulDegradationIntegration() {
        std::cout << "Testing GracefulDegradationManager integration...\n";
        
        MPRISTypes::GracefulDegradationManager degradation_manager;
        
        // Test initial state
        assert(degradation_manager.getDegradationLevel() == MPRISTypes::GracefulDegradationManager::DegradationLevel::None);
        
        // Test error reporting and auto-degradation
        degradation_manager.setErrorThreshold(MPRISTypes::MPRISError::Category::Connection, 2);
        degradation_manager.setTimeWindow(std::chrono::seconds(10));
        
        // Report errors
        MPRISTypes::ConnectionError error1("Connection error 1");
        MPRISTypes::ConnectionError error2("Connection error 2");
        
        degradation_manager.reportError(error1);
        degradation_manager.reportError(error2);
        
        // Test manual degradation
        degradation_manager.setDegradationLevel(MPRISTypes::GracefulDegradationManager::DegradationLevel::Limited);
        assert(degradation_manager.getDegradationLevel() == MPRISTypes::GracefulDegradationManager::DegradationLevel::Limited);
        
        // Test feature availability
        assert(!degradation_manager.isFeatureAvailable("metadata_updates"));
        assert(degradation_manager.isFeatureAvailable("playback_control"));
        
        // Test further degradation
        degradation_manager.setDegradationLevel(MPRISTypes::GracefulDegradationManager::DegradationLevel::Disabled);
        assert(!degradation_manager.isFeatureAvailable("playback_control"));
        
        std::cout << "GracefulDegradationManager integration tests passed.\n";
    }
    
    static void testComponentInteraction() {
        std::cout << "Testing component interaction...\n";
        
        auto& logger = MPRISTypes::ErrorLogger::getInstance();
        MPRISTypes::ErrorRecoveryManager recovery_manager;
        MPRISTypes::GracefulDegradationManager degradation_manager;
        
        // Reset for clean test
        logger.resetErrorStats();
        recovery_manager.resetRecoveryStats();
        
        // Set up recovery action
        recovery_manager.setRecoveryAction(
            MPRISTypes::MPRISError::RecoveryStrategy::Degrade,
            [&degradation_manager]() -> bool {
                auto current_level = degradation_manager.getDegradationLevel();
                auto new_level = static_cast<MPRISTypes::GracefulDegradationManager::DegradationLevel>(
                    std::min(static_cast<int>(current_level) + 1, 
                            static_cast<int>(MPRISTypes::GracefulDegradationManager::DegradationLevel::Disabled))
                );
                
                if (new_level != current_level) {
                    degradation_manager.setDegradationLevel(new_level);
                    return true;
                }
                return false;
            }
        );
        
        // Create error that triggers degradation
        MPRISTypes::MPRISError degrade_error(
            MPRISTypes::MPRISError::Category::Resource,
            MPRISTypes::MPRISError::Severity::Error,
            "Resource exhaustion",
            "integration_test",
            MPRISTypes::MPRISError::RecoveryStrategy::Degrade
        );
        
        // Log error and attempt recovery
        logger.logError(degrade_error);
        bool recovery_result = recovery_manager.attemptRecovery(degrade_error);
        
        assert(recovery_result == true);
        assert(degradation_manager.getDegradationLevel() == MPRISTypes::GracefulDegradationManager::DegradationLevel::Limited);
        
        // Verify statistics were updated
        auto error_stats = logger.getErrorStats();
        auto recovery_stats = recovery_manager.getRecoveryStats();
        
        assert(error_stats.total_errors >= 1);
        assert(recovery_stats.total_attempts >= 1);
        assert(recovery_stats.successful_recoveries >= 1);
        
        std::cout << "Component interaction tests passed.\n";
    }
};

// Test main function
int main() {
    try {
        MPRISErrorIntegrationTest::runAllTests();
        std::cout << "\nAll MPRIS error integration tests completed successfully!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Integration test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Integration test failed with unknown exception" << std::endl;
        return 1;
    }
}

#else // !HAVE_DBUS

#include <iostream>

int main() {
    std::cout << "MPRIS error integration tests skipped - D-Bus support not compiled in\n";
    return 0;
}

#endif // HAVE_DBUS