/*
 * test_mpris_logger_performance.cpp - Performance metrics and lock contention tests
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS

#include "test_framework.h"
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <atomic>
#include <sstream>
#include <fstream>

class MPRISLoggerPerformanceTest : public TestFramework {
protected:
    void SetUp() override {
        TestFramework::SetUp();
        
        // Reset logger to default state
        auto& logger = MPRIS::MPRISLogger::getInstance();
        logger.setLogLevel(MPRIS::LogLevel::DEBUG);
        logger.enableConsoleOutput(false);
        logger.enableDebugMode(true);
        logger.enableMessageTracing(false);
        logger.enablePerformanceMetrics(true);
        logger.resetMetrics();
        
        // Create temporary log file
        m_temp_log_file = "/tmp/mpris_perf_test_" + std::to_string(std::time(nullptr)) + ".log";
        logger.setLogFile(m_temp_log_file);
    }
    
    void TearDown() override {
        // Clean up temporary log file
        std::remove(m_temp_log_file.c_str());
        TestFramework::TearDown();
    }
    
    std::string readLogFile() {
        std::ifstream file(m_temp_log_file);
        if (!file.is_open()) {
            return "";
        }
        
        std::ostringstream oss;
        oss << file.rdbuf();
        return oss.str();
    }
    
    std::string m_temp_log_file;
};

TEST_F(MPRISLoggerPerformanceTest, BasicMetricsRecording) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Record various metrics
    logger.recordLockAcquisition("test_mutex", 1000);
    logger.recordLockAcquisition("test_mutex", 2000);
    logger.recordLockAcquisition("another_mutex", 500);
    
    logger.recordDBusMessage(true);   // sent
    logger.recordDBusMessage(true);   // sent
    logger.recordDBusMessage(false);  // received
    
    logger.recordPropertyUpdate();
    logger.recordPropertyUpdate();
    logger.recordPropertyUpdate();
    
    logger.recordSignalEmission();
    logger.recordSignalEmission();
    
    logger.recordConnectionAttempt(true);   // success
    logger.recordConnectionAttempt(false);  // failure
    logger.recordConnectionAttempt(true);   // success
    
    auto metrics = logger.getMetrics();
    
    EXPECT_EQ(metrics.lock_acquisitions.load(), 3);
    EXPECT_EQ(metrics.lock_contention_time_us.load(), 3500); // 1000 + 2000 + 500
    EXPECT_EQ(metrics.dbus_messages_sent.load(), 2);
    EXPECT_EQ(metrics.dbus_messages_received.load(), 1);
    EXPECT_EQ(metrics.property_updates.load(), 3);
    EXPECT_EQ(metrics.signal_emissions.load(), 2);
    EXPECT_EQ(metrics.connection_attempts.load(), 3);
    EXPECT_EQ(metrics.connection_failures.load(), 1);
}

TEST_F(MPRISLoggerPerformanceTest, LockTimerAccuracy) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    const auto sleep_duration = std::chrono::milliseconds(50);
    
    {
        MPRIS::LockTimer timer("accuracy_test");
        std::this_thread::sleep_for(sleep_duration);
    }
    
    auto metrics = logger.getMetrics();
    
    EXPECT_EQ(metrics.lock_acquisitions.load(), 1);
    
    // Allow for some timing variance (±10ms)
    auto recorded_time_us = metrics.lock_contention_time_us.load();
    auto expected_time_us = std::chrono::duration_cast<std::chrono::microseconds>(sleep_duration).count();
    
    EXPECT_GE(recorded_time_us, expected_time_us - 10000); // At least expected - 10ms
    EXPECT_LE(recorded_time_us, expected_time_us + 10000); // At most expected + 10ms
}

TEST_F(MPRISLoggerPerformanceTest, ConcurrentMetricsRecording) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    const int num_threads = 8;
    const int operations_per_thread = 1000;
    
    std::vector<std::thread> threads;
    std::atomic<int> completed_threads{0};
    
    // Start threads that record metrics concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&logger, i, operations_per_thread, &completed_threads]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                logger.recordLockAcquisition("thread_" + std::to_string(i), j + 1);
                logger.recordDBusMessage(j % 2 == 0); // Alternate sent/received
                logger.recordPropertyUpdate();
                
                if (j % 10 == 0) {
                    logger.recordSignalEmission();
                }
                
                if (j % 100 == 0) {
                    logger.recordConnectionAttempt(j % 200 != 0); // Occasional failure
                }
            }
            completed_threads++;
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(completed_threads.load(), num_threads);
    
    auto metrics = logger.getMetrics();
    
    // Verify total counts
    EXPECT_EQ(metrics.lock_acquisitions.load(), num_threads * operations_per_thread);
    EXPECT_EQ(metrics.dbus_messages_sent.load(), num_threads * (operations_per_thread / 2));
    EXPECT_EQ(metrics.dbus_messages_received.load(), num_threads * (operations_per_thread / 2));
    EXPECT_EQ(metrics.property_updates.load(), num_threads * operations_per_thread);
    EXPECT_EQ(metrics.signal_emissions.load(), num_threads * (operations_per_thread / 10));
    
    // Connection attempts: each thread makes operations_per_thread/100 attempts
    auto expected_attempts = num_threads * (operations_per_thread / 100);
    EXPECT_EQ(metrics.connection_attempts.load(), expected_attempts);
    
    // Verify lock contention time is reasonable
    auto total_contention = metrics.lock_contention_time_us.load();
    EXPECT_GT(total_contention, 0);
    
    // Average contention per lock should be reasonable (sum of 1 to operations_per_thread)
    uint64_t expected_total_per_thread = (operations_per_thread * (operations_per_thread + 1)) / 2;
    uint64_t expected_total = num_threads * expected_total_per_thread;
    EXPECT_EQ(total_contention, expected_total);
}

TEST_F(MPRISLoggerPerformanceTest, LockContentionLogging) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Record a high-contention lock (>1ms should trigger debug log)
    logger.recordLockAcquisition("high_contention_lock", 5000); // 5ms
    
    std::string log_content = readLogFile();
    
    EXPECT_TRUE(log_content.find("Lock contention on high_contention_lock: 5000us") != std::string::npos);
}

TEST_F(MPRISLoggerPerformanceTest, MetricsToString) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Record some metrics
    logger.recordLockAcquisition("test", 1000);
    logger.recordDBusMessage(true);
    logger.recordPropertyUpdate();
    logger.recordSignalEmission();
    logger.recordConnectionAttempt(true);
    
    auto metrics = logger.getMetrics();
    std::string metrics_str = metrics.toString();
    
    EXPECT_TRUE(metrics_str.find("PerformanceMetrics {") != std::string::npos);
    EXPECT_TRUE(metrics_str.find("lock_acquisitions: 1") != std::string::npos);
    EXPECT_TRUE(metrics_str.find("lock_contention_time_us: 1000") != std::string::npos);
    EXPECT_TRUE(metrics_str.find("dbus_messages_sent: 1") != std::string::npos);
    EXPECT_TRUE(metrics_str.find("property_updates: 1") != std::string::npos);
    EXPECT_TRUE(metrics_str.find("signal_emissions: 1") != std::string::npos);
    EXPECT_TRUE(metrics_str.find("connection_attempts: 1") != std::string::npos);
    EXPECT_TRUE(metrics_str.find("connection_failures: 0") != std::string::npos);
}

TEST_F(MPRISLoggerPerformanceTest, MetricsDisabled) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Disable performance metrics
    logger.enablePerformanceMetrics(false);
    logger.resetMetrics();
    
    // Record metrics (should be ignored)
    logger.recordLockAcquisition("test", 1000);
    logger.recordDBusMessage(true);
    logger.recordPropertyUpdate();
    logger.recordSignalEmission();
    logger.recordConnectionAttempt(true);
    
    auto metrics = logger.getMetrics();
    
    // All metrics should remain at 0
    EXPECT_EQ(metrics.lock_acquisitions.load(), 0);
    EXPECT_EQ(metrics.lock_contention_time_us.load(), 0);
    EXPECT_EQ(metrics.dbus_messages_sent.load(), 0);
    EXPECT_EQ(metrics.dbus_messages_received.load(), 0);
    EXPECT_EQ(metrics.property_updates.load(), 0);
    EXPECT_EQ(metrics.signal_emissions.load(), 0);
    EXPECT_EQ(metrics.connection_attempts.load(), 0);
    EXPECT_EQ(metrics.connection_failures.load(), 0);
}

TEST_F(MPRISLoggerPerformanceTest, LockTimerNesting) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    {
        MPRIS::LockTimer outer_timer("outer_lock");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        {
            MPRIS::LockTimer inner_timer("inner_lock");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    auto metrics = logger.getMetrics();
    
    // Should have recorded 2 lock acquisitions
    EXPECT_EQ(metrics.lock_acquisitions.load(), 2);
    
    // Total time should be at least 20ms (10+5+5, allowing for timing variance)
    EXPECT_GE(metrics.lock_contention_time_us.load(), 15000); // At least 15ms
}

TEST_F(MPRISLoggerPerformanceTest, HighFrequencyMetrics) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    const int num_operations = 10000;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        logger.recordPropertyUpdate();
        if (i % 10 == 0) {
            logger.recordLockAcquisition("high_freq", 1);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    auto metrics = logger.getMetrics();
    
    EXPECT_EQ(metrics.property_updates.load(), num_operations);
    EXPECT_EQ(metrics.lock_acquisitions.load(), num_operations / 10);
    
    // Performance check: should complete in reasonable time (less than 100ms)
    EXPECT_LT(duration.count(), 100);
}

TEST_F(MPRISLoggerPerformanceTest, MetricsOverflow) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    // Test with very large values to ensure no overflow issues
    const uint64_t large_value = std::numeric_limits<uint32_t>::max();
    
    logger.recordLockAcquisition("overflow_test", large_value);
    logger.recordLockAcquisition("overflow_test", large_value);
    
    auto metrics = logger.getMetrics();
    
    EXPECT_EQ(metrics.lock_acquisitions.load(), 2);
    EXPECT_EQ(metrics.lock_contention_time_us.load(), 2 * large_value);
}

TEST_F(MPRISLoggerPerformanceTest, LockTimerExceptionSafety) {
    auto& logger = MPRIS::MPRISLogger::getInstance();
    
    try {
        MPRIS::LockTimer timer("exception_test");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        throw std::runtime_error("Test exception");
    } catch (const std::exception&) {
        // Exception caught, timer should still record metrics
    }
    
    auto metrics = logger.getMetrics();
    
    // Timer should have recorded the lock acquisition despite the exception
    EXPECT_EQ(metrics.lock_acquisitions.load(), 1);
    EXPECT_GT(metrics.lock_contention_time_us.load(), 5000); // At least 5ms
}

#else // !HAVE_DBUS

// Stub test when D-Bus is not available
TEST(MPRISLoggerPerformanceTest, DBusNotAvailable) {
    EXPECT_TRUE(true); // Placeholder test
}

#endif // HAVE_DBUS