/*
 * test_mpris_logger_basic.cpp - Basic MPRIS logging system tests
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS

#include "test_framework.h"
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>

using namespace TestFramework;

void test_basic_logging() {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Create temporary log file
    std::string temp_log_file = "/tmp/mpris_test_log_" + std::to_string(std::time(nullptr)) + ".log";
    logger.setLogFile(temp_log_file);
    logger.setLogLevel(MPRIS::LogLevel::INFO);
    logger.enableConsoleOutput(false);
    
    // Test different log levels
    logger.trace("TestComponent", "This is a trace message");
    logger.debug("TestComponent", "This is a debug message");
    logger.info("TestComponent", "This is an info message");
    logger.warn("TestComponent", "This is a warning message");
    logger.error("TestComponent", "This is an error message");
    logger.fatal("TestComponent", "This is a fatal message");
    
    // Read log file
    std::ifstream file(temp_log_file);
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string log_content = oss.str();
    file.close();
    
    // With default INFO level, only INFO, WARN, ERROR, FATAL should appear
    ASSERT_TRUE(log_content.find("This is an info message") != std::string::npos, 
                "Info message should appear in log");
    ASSERT_TRUE(log_content.find("This is a warning message") != std::string::npos, 
                "Warning message should appear in log");
    ASSERT_TRUE(log_content.find("This is an error message") != std::string::npos, 
                "Error message should appear in log");
    ASSERT_TRUE(log_content.find("This is a fatal message") != std::string::npos, 
                "Fatal message should appear in log");
    
    // TRACE and DEBUG should not appear
    ASSERT_TRUE(log_content.find("This is a trace message") == std::string::npos, 
                "Trace message should not appear in log");
    ASSERT_TRUE(log_content.find("This is a debug message") == std::string::npos, 
                "Debug message should not appear in log");
    
    // Clean up
    std::remove(temp_log_file.c_str());
}

void test_performance_metrics() {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    logger.enablePerformanceMetrics(true);
    logger.resetMetrics();
    
    // Test metrics recording
    logger.recordLockAcquisition("test_lock", 1500); // 1.5ms
    logger.recordDBusMessage(true);  // sent
    logger.recordDBusMessage(false); // received
    logger.recordPropertyUpdate();
    logger.recordSignalEmission();
    logger.recordConnectionAttempt(true);
    logger.recordConnectionAttempt(false);
    
    auto metrics = logger.getMetrics();
    
    ASSERT_EQUALS(metrics.lock_acquisitions, 1, "Lock acquisitions count");
    ASSERT_EQUALS(metrics.lock_contention_time_us, 1500, "Lock contention time");
    ASSERT_EQUALS(metrics.dbus_messages_sent, 1, "D-Bus messages sent count");
    ASSERT_EQUALS(metrics.dbus_messages_received, 1, "D-Bus messages received count");
    ASSERT_EQUALS(metrics.property_updates, 1, "Property updates count");
    ASSERT_EQUALS(metrics.signal_emissions, 1, "Signal emissions count");
    ASSERT_EQUALS(metrics.connection_attempts, 2, "Connection attempts count");
    ASSERT_EQUALS(metrics.connection_failures, 1, "Connection failures count");
}

void test_connection_state_tracking() {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Test connection state updates
    logger.updateConnectionState(MPRIS::ConnectionState::CONNECTING, "Starting connection");
    auto state = logger.getConnectionState();
    ASSERT_EQUALS(static_cast<int>(state.status), static_cast<int>(MPRIS::ConnectionState::CONNECTING), 
                  "Connection state should be CONNECTING");
    ASSERT_EQUALS(state.last_error, "Starting connection", "Last error should match");
    
    logger.updateConnectionState(MPRIS::ConnectionState::CONNECTED, "Connection established");
    state = logger.getConnectionState();
    ASSERT_EQUALS(static_cast<int>(state.status), static_cast<int>(MPRIS::ConnectionState::CONNECTED), 
                  "Connection state should be CONNECTED");
    ASSERT_EQUALS(state.reconnect_attempts, 0, "Reconnect attempts should be 0");
    
    logger.updateConnectionState(MPRIS::ConnectionState::RECONNECTING, "Connection lost");
    state = logger.getConnectionState();
    ASSERT_EQUALS(static_cast<int>(state.status), static_cast<int>(MPRIS::ConnectionState::RECONNECTING), 
                  "Connection state should be RECONNECTING");
    ASSERT_EQUALS(state.reconnect_attempts, 1, "Reconnect attempts should be 1");
}

void test_lock_timer() {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    logger.enablePerformanceMetrics(true);
    logger.resetMetrics();
    
    {
        MPRIS::LockTimer timer("test_lock");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } // Timer destructor should record the time
    
    auto metrics = logger.getMetrics();
    ASSERT_EQUALS(metrics.lock_acquisitions, 1, "Lock acquisitions should be 1");
    ASSERT_TRUE(metrics.lock_contention_time_us > 5000, "Lock contention time should be at least 5ms");
}

void test_logging_macros() {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Create temporary log file
    std::string temp_log_file = "/tmp/mpris_macro_test_" + std::to_string(std::time(nullptr)) + ".log";
    logger.setLogFile(temp_log_file);
    logger.setLogLevel(MPRIS::LogLevel::TRACE);
    logger.enableConsoleOutput(false);
    
    // Test logging macros
    MPRIS_LOG_TRACE("MacroTest", "Trace message via macro");
    MPRIS_LOG_DEBUG("MacroTest", "Debug message via macro");
    MPRIS_LOG_INFO("MacroTest", "Info message via macro");
    MPRIS_LOG_WARN("MacroTest", "Warning message via macro");
    MPRIS_LOG_ERROR("MacroTest", "Error message via macro");
    MPRIS_LOG_FATAL("MacroTest", "Fatal message via macro");
    
    // Read log file
    std::ifstream file(temp_log_file);
    std::ostringstream oss;
    oss << file.rdbuf();
    std::string log_content = oss.str();
    file.close();
    
    ASSERT_TRUE(log_content.find("Trace message via macro") != std::string::npos, 
                "Trace macro should work");
    ASSERT_TRUE(log_content.find("Debug message via macro") != std::string::npos, 
                "Debug macro should work");
    ASSERT_TRUE(log_content.find("Info message via macro") != std::string::npos, 
                "Info macro should work");
    ASSERT_TRUE(log_content.find("Warning message via macro") != std::string::npos, 
                "Warning macro should work");
    ASSERT_TRUE(log_content.find("Error message via macro") != std::string::npos, 
                "Error macro should work");
    ASSERT_TRUE(log_content.find("Fatal message via macro") != std::string::npos, 
                "Fatal macro should work");
    
    // Clean up
    std::remove(temp_log_file.c_str());
}

int main() {
    try {
        std::cout << "Running MPRIS Logger Basic Tests..." << std::endl;
        
        test_basic_logging();
        std::cout << "✓ Basic logging test passed" << std::endl;
        
        test_performance_metrics();
        std::cout << "✓ Performance metrics test passed" << std::endl;
        
        test_connection_state_tracking();
        std::cout << "✓ Connection state tracking test passed" << std::endl;
        
        test_lock_timer();
        std::cout << "✓ Lock timer test passed" << std::endl;
        
        test_logging_macros();
        std::cout << "✓ Logging macros test passed" << std::endl;
        
        std::cout << "All MPRIS Logger basic tests passed!" << std::endl;
        return 0;
        
    } catch (const TestFramework::AssertionFailure& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }
}

#else // !HAVE_DBUS

int main() {
    std::cout << "MPRIS Logger tests skipped - D-Bus support not available" << std::endl;
    return 0;
}

#endif // HAVE_DBUS