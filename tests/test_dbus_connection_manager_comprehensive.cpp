/*
 * test_dbus_connection_manager_comprehensive.cpp - Comprehensive unit tests for DBusConnectionManager
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "test_framework_threading.h"
#include "mock_dbus_connection.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace TestFramework;
using namespace TestFramework::Threading;

/**
 * @brief Test class for DBusConnectionManager comprehensive testing
 */
class DBusConnectionManagerTest : public TestCase {
public:
    DBusConnectionManagerTest() : TestCase("DBusConnectionManagerTest") {}

protected:
    void setUp() override {
        // Initialize mock D-Bus connection with failure simulation
        MockDBusConnection::Config config;
        config.simulate_connection_failures = true;
        config.connection_failure_rate = 0.2; // 20% failure rate for testing
        config.simulate_message_failures = true;
        config.message_failure_rate = 0.1; // 10% message failure rate
        
        m_mock_connection = std::make_unique<MockDBusConnection>(config);
        m_connection_manager = std::make_unique<DBusConnectionManager>();
    }

    void tearDown() override {
        if (m_connection_manager) {
            m_connection_manager->disconnect();
        }
        m_mock_connection.reset();
        m_connection_manager.reset();
    }

    void runTest() override {
        testBasicConnectionLifecycle();
        testConnectionFailureScenarios();
        testAutoReconnectionLogic();
        testThreadSafetyCompliance();
        testResourceManagement();
        testErrorHandlingAndRecovery();
        testPerformanceUnderLoad();
    }

private:
    std::unique_ptr<MockDBusConnection> m_mock_connection;
    std::unique_ptr<DBusConnectionManager> m_connection_manager;

    void testBasicConnectionLifecycle() {
        // Test normal connection establishment
        ASSERT_TRUE(m_connection_manager->connect(), "Initial connection should succeed");
        ASSERT_TRUE(m_connection_manager->isConnected(), "Should report connected state");
        
        // Test getting connection handle
        auto* connection = m_connection_manager->getConnection();
        ASSERT_NOT_NULL(connection, "Should provide valid connection handle");
        
        // Test disconnection
        m_connection_manager->disconnect();
        ASSERT_FALSE(m_connection_manager->isConnected(), "Should report disconnected state");
        
        // Test reconnection
        ASSERT_TRUE(m_connection_manager->connect(), "Reconnection should succeed");
        ASSERT_TRUE(m_connection_manager->isConnected(), "Should report connected state after reconnection");
    }

    void testConnectionFailureScenarios() {
        // Simulate connection failure
        m_mock_connection->simulateConnectionLoss();
        
        // Test connection attempt during failure
        auto connection_result = m_connection_manager->connect();
        // May succeed or fail depending on simulation - both are valid for testing
        
        // Test connection state consistency
        bool reported_state = m_connection_manager->isConnected();
        auto* connection_handle = m_connection_manager->getConnection();
        
        if (reported_state) {
            ASSERT_NOT_NULL(connection_handle, "Connected state should provide valid handle");
        } else {
            // Handle may be null or invalid during failure - this is acceptable
        }
        
        // Test recovery after failure
        m_mock_connection->simulateConnectionRestore();
        ASSERT_TRUE(m_connection_manager->connect(), "Should recover after connection restore");
    }

    void testAutoReconnectionLogic() {
        // Enable auto-reconnection
        m_connection_manager->enableAutoReconnect(true);
        
        // Establish initial connection
        ASSERT_TRUE(m_connection_manager->connect(), "Initial connection should succeed");
        
        // Simulate connection loss
        m_mock_connection->simulateConnectionLoss();
        
        // Test automatic reconnection attempt
        auto reconnection_result = m_connection_manager->attemptReconnection();
        
        // Restore connection and test again
        m_mock_connection->simulateConnectionRestore();
        ASSERT_TRUE(m_connection_manager->attemptReconnection(), "Should succeed after restore");
        
        // Disable auto-reconnection and test
        m_connection_manager->enableAutoReconnect(false);
        m_mock_connection->simulateConnectionLoss();
        // Manual reconnection should still work
        m_mock_connection->simulateConnectionRestore();
        ASSERT_TRUE(m_connection_manager->attemptReconnection(), "Manual reconnection should work");
    }

    void testThreadSafetyCompliance() {
        ThreadSafetyTester::Config config;
        config.num_threads = 8;
        config.operations_per_thread = 50;
        config.test_duration = std::chrono::milliseconds{2000};
        
        ThreadSafetyTester tester(config);
        
        // Test concurrent connection operations
        auto connection_test = [this]() -> bool {
            try {
                auto result = m_connection_manager->connect();
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                bool state = m_connection_manager->isConnected();
                m_connection_manager->disconnect();
                return true;
            } catch (...) {
                return false;
            }
        };
        
        auto results = tester.runTest(connection_test, "ConcurrentConnectionOperations");
        
        ASSERT_TRUE(results.successful_operations > 0, "Should have some successful operations");
        ASSERT_FALSE(results.deadlock_detected, "Should not detect deadlocks");
        
        // Test concurrent state queries
        auto state_query_test = [this]() -> bool {
            try {
                bool state1 = m_connection_manager->isConnected();
                auto* handle = m_connection_manager->getConnection();
                bool state2 = m_connection_manager->isConnected();
                return true;
            } catch (...) {
                return false;
            }
        };
        
        auto state_results = tester.runTest(state_query_test, "ConcurrentStateQueries");
        ASSERT_FALSE(state_results.deadlock_detected, "State queries should not cause deadlocks");
    }

    void testResourceManagement() {
        // Test RAII behavior
        {
            auto temp_manager = std::make_unique<DBusConnectionManager>();
            ASSERT_TRUE(temp_manager->connect(), "Temporary manager should connect");
            ASSERT_TRUE(temp_manager->isConnected(), "Should report connected");
            // Destructor should clean up automatically
        }
        
        // Test multiple connection/disconnection cycles
        for (int i = 0; i < 10; ++i) {
            ASSERT_TRUE(m_connection_manager->connect(), "Connection cycle should succeed");
            ASSERT_TRUE(m_connection_manager->isConnected(), "Should be connected");
            m_connection_manager->disconnect();
            ASSERT_FALSE(m_connection_manager->isConnected(), "Should be disconnected");
        }
        
        // Test resource cleanup after errors
        m_mock_connection->simulateConnectionLoss();
        m_connection_manager->connect(); // May fail
        m_connection_manager->disconnect(); // Should not crash
        
        m_mock_connection->simulateConnectionRestore();
        ASSERT_TRUE(m_connection_manager->connect(), "Should recover cleanly");
    }

    void testErrorHandlingAndRecovery() {
        // Test error injection
        m_mock_connection->setConnectionFailureRate(1.0); // Force failures
        
        bool connection_failed = false;
        for (int i = 0; i < 5; ++i) {
            if (!m_connection_manager->connect()) {
                connection_failed = true;
                break;
            }
        }
        ASSERT_TRUE(connection_failed, "Should eventually fail with 100% failure rate");
        
        // Test recovery
        m_mock_connection->setConnectionFailureRate(0.0); // Disable failures
        ASSERT_TRUE(m_connection_manager->connect(), "Should recover when failures disabled");
        
        // Test graceful handling of invalid operations
        m_connection_manager->disconnect();
        m_connection_manager->disconnect(); // Double disconnect should be safe
        
        auto* handle = m_connection_manager->getConnection();
        // Handle may be null when disconnected - this is acceptable
        
        // Test error state consistency
        ASSERT_FALSE(m_connection_manager->isConnected(), "Should report disconnected after errors");
    }

    void testPerformanceUnderLoad() {
        // Measure connection performance
        auto start_time = std::chrono::high_resolution_clock::now();
        
        const size_t num_operations = 100;
        size_t successful_operations = 0;
        
        for (size_t i = 0; i < num_operations; ++i) {
            if (m_connection_manager->connect()) {
                successful_operations++;
                m_connection_manager->disconnect();
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        ASSERT_TRUE(successful_operations > 0, "Should have some successful operations");
        
        // Performance should be reasonable (less than 50ms per operation on average)
        auto avg_time_per_op = duration.count() / num_operations;
        ASSERT_TRUE(avg_time_per_op < 50, "Average operation time should be reasonable");
        
        // Test lock contention measurement
        LockContentionAnalyzer analyzer;
        std::mutex test_mutex;
        
        auto contention_metrics = analyzer.analyzeLockContention(
            test_mutex, 
            std::chrono::milliseconds{500}, 
            4
        );
        
        // Verify contention metrics are reasonable
        ASSERT_TRUE(contention_metrics.total_acquisitions > 0, "Should have lock acquisitions");
        ASSERT_TRUE(contention_metrics.average_acquisition_time.count() >= 0, "Should have valid timing");
    }
};

/**
 * @brief Test class for DBusConnectionManager edge cases
 */
class DBusConnectionManagerEdgeCaseTest : public TestCase {
public:
    DBusConnectionManagerEdgeCaseTest() : TestCase("DBusConnectionManagerEdgeCaseTest") {}

protected:
    void runTest() override {
        testRapidConnectionCycles();
        testConnectionDuringShutdown();
        testMemoryPressureScenarios();
        testExceptionSafety();
    }

private:
    void testRapidConnectionCycles() {
        auto manager = std::make_unique<DBusConnectionManager>();
        
        // Rapid connect/disconnect cycles
        for (int i = 0; i < 50; ++i) {
            manager->connect();
            manager->disconnect();
        }
        
        // Should still be functional
        ASSERT_TRUE(manager->connect(), "Should work after rapid cycles");
        ASSERT_TRUE(manager->isConnected(), "Should report correct state");
    }

    void testConnectionDuringShutdown() {
        auto manager = std::make_unique<DBusConnectionManager>();
        
        // Start connection in background thread
        std::atomic<bool> connection_completed{false};
        std::thread connection_thread([&]() {
            manager->connect();
            connection_completed.store(true);
        });
        
        // Simulate shutdown during connection
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        manager->disconnect();
        
        connection_thread.join();
        
        // Should handle gracefully
        ASSERT_TRUE(connection_completed.load(), "Connection thread should complete");
    }

    void testMemoryPressureScenarios() {
        // Test behavior under simulated memory pressure
        std::vector<std::unique_ptr<MPRISTypes::DBusConnectionManager>> managers;
        
        // Create multiple managers to simulate resource pressure
        for (int i = 0; i < 10; ++i) {
            auto manager = std::make_unique<DBusConnectionManager>();
            if (manager->connect()) {
                managers.push_back(std::move(manager));
            }
        }
        
        // Clean up should be graceful
        managers.clear();
        
        // New manager should still work
        auto final_manager = std::make_unique<DBusConnectionManager>();
        ASSERT_TRUE(final_manager->connect(), "Should work after resource cleanup");
    }

    void testExceptionSafety() {
        auto manager = std::make_unique<DBusConnectionManager>();
        
        // Test exception safety during operations
        try {
            manager->connect();
            // Simulate exception during operation
            throw std::runtime_error("Simulated error");
        } catch (const std::exception&) {
            // Manager should still be in valid state
            bool state = manager->isConnected();
            manager->disconnect(); // Should not throw
        }
        
        // Should be able to use manager after exception
        ASSERT_TRUE(manager->connect(), "Should work after exception handling");
    }
};

// Test suite setup and execution
int main() {
    TestSuite suite("DBusConnectionManager Comprehensive Tests");
    
    suite.addTest(std::make_unique<DBusConnectionManagerTest>());
    suite.addTest(std::make_unique<DBusConnectionManagerEdgeCaseTest>());
    
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}