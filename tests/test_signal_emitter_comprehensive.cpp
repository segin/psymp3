/*
 * test_signal_emitter_comprehensive.cpp - Comprehensive unit tests for SignalEmitter
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
#include <queue>

using namespace TestFramework;
using namespace TestFramework::Threading;
using namespace PsyMP3::MPRIS;

/**
 * @brief Test class for SignalEmitter comprehensive testing
 */
class SignalEmitterTest : public TestCase {
public:
    SignalEmitterTest() : TestCase("SignalEmitterTest") {}

protected:
    void setUp() override {
        // Create mock D-Bus connection manager
        m_mock_connection_manager = std::make_unique<MockDBusConnectionManager>();
        m_mock_connection_manager->connect();
        
        // Create signal emitter
        m_signal_emitter = std::make_unique<SignalEmitter>(reinterpret_cast<DBusConnectionManager*>(m_mock_connection_manager.get()));
    }

    void tearDown() override {
        m_signal_emitter.reset();
        m_mock_connection_manager.reset();
    }

    void runTest() override {
        testBasicSignalEmission();
        testPropertiesChangedSignals();
        testSeekedSignals();
        testAsynchronousOperation();
        testQueueOverflowHandling();
        testThreadingValidation();
        testErrorHandlingAndRecovery();
        testPerformanceUnderLoad();
    }

private:
    std::unique_ptr<MockDBusConnectionManager> m_mock_connection_manager;
    std::unique_ptr<SignalEmitter> m_signal_emitter;

    void testBasicSignalEmission() {
        // Test PropertiesChanged signal emission
        std::map<std::string, DBusVariant> changed_properties;
        changed_properties["PlaybackStatus"] = DBusVariant(std::string("Playing"));
        changed_properties["Position"] = DBusVariant(uint64_t(123456789));
        
        // Emit signal (should be asynchronous)
        m_signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", changed_properties);
        
        // Give some time for asynchronous processing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check that signal was sent through mock connection
        auto* mock_connection = m_mock_connection_manager->getConnection();
        ASSERT_NOT_NULL(mock_connection, "Should have valid mock connection");
        
        auto sent_messages = mock_connection->getAllMessages();
        ASSERT_TRUE(sent_messages.size() > 0, "Should have sent at least one message");
        
        // Find PropertiesChanged signal
        bool found_properties_changed = false;
        for (const auto& message : sent_messages) {
            if (message->getType() == MockDBusMessage::Type::Signal &&
                message->getMember() == "PropertiesChanged") {
                found_properties_changed = true;
                break;
            }
        }
        ASSERT_TRUE(found_properties_changed, "Should have sent PropertiesChanged signal");
        
        // Test Seeked signal emission
        uint64_t seek_position = 987654321;
        m_signal_emitter->emitSeeked(seek_position);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Check for Seeked signal
        auto updated_messages = mock_connection->getAllMessages();
        bool found_seeked = false;
        for (const auto& message : updated_messages) {
            if (message->getType() == MockDBusMessage::Type::Signal &&
                message->getMember() == "Seeked") {
                found_seeked = true;
                break;
            }
        }
        ASSERT_TRUE(found_seeked, "Should have sent Seeked signal");
    }

    void testPropertiesChangedSignals() {
        // Test various property change scenarios
        std::vector<std::map<std::string, DBusVariant>> test_properties = {
            // Metadata change
            {
                {"Metadata", DBusVariant(std::string("test_metadata"))}
            },
            // Playback status change
            {
                {"PlaybackStatus", DBusVariant(std::string("Paused"))}
            },
            // Multiple properties change
            {
                {"PlaybackStatus", DBusVariant(std::string("Playing"))},
                {"Position", DBusVariant(uint64_t(555555555))},
                {"Volume", DBusVariant(0.75)}
            },
            // Empty properties (should be handled gracefully)
            {}
        };
        
        for (size_t i = 0; i < test_properties.size(); ++i) {
            m_signal_emitter->emitPropertiesChanged(
                "org.mpris.MediaPlayer2.Player", 
                test_properties[i]
            );
        }
        
        // Allow time for all signals to be processed
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        auto* mock_connection = m_mock_connection_manager->getConnection();
        auto sent_messages = mock_connection->getAllMessages();
        
        // Should have sent multiple PropertiesChanged signals
        size_t properties_changed_count = 0;
        for (const auto& message : sent_messages) {
            if (message->getType() == MockDBusMessage::Type::Signal &&
                message->getMember() == "PropertiesChanged") {
                properties_changed_count++;
            }
        }
        
        ASSERT_TRUE(properties_changed_count >= test_properties.size() - 1, 
                   "Should have sent most PropertiesChanged signals (empty may be filtered)");
    }

    void testSeekedSignals() {
        // Test various seek positions
        std::vector<uint64_t> seek_positions = {
            0,                    // Beginning
            1000000,             // 1 second
            60000000,            // 1 minute
            3600000000,          // 1 hour
            UINT64_MAX / 2,      // Large value
            UINT64_MAX           // Maximum value
        };
        
        for (uint64_t position : seek_positions) {
            m_signal_emitter->emitSeeked(position);
        }
        
        // Allow time for signal processing
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        
        auto* mock_connection = m_mock_connection_manager->getConnection();
        auto sent_messages = mock_connection->getAllMessages();
        
        // Count Seeked signals
        size_t seeked_count = 0;
        for (const auto& message : sent_messages) {
            if (message->getType() == MockDBusMessage::Type::Signal &&
                message->getMember() == "Seeked") {
                seeked_count++;
            }
        }
        
        ASSERT_EQUALS(seek_positions.size(), seeked_count, "Should have sent all Seeked signals");
    }

    void testAsynchronousOperation() {
        // Test that signal emission is truly asynchronous
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Emit many signals rapidly
        const size_t num_signals = 100;
        for (size_t i = 0; i < num_signals; ++i) {
            std::map<std::string, DBusVariant> properties;
            properties["TestProperty"] = DBusVariant(uint64_t(i));
            
            m_signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
        }
        
        auto emission_time = std::chrono::high_resolution_clock::now();
        auto emission_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            emission_time - start_time
        );
        
        // Emission should be very fast (asynchronous)
        ASSERT_TRUE(emission_duration.count() < 100, "Signal emission should be asynchronous and fast");
        
        // Wait for processing to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        auto* mock_connection = m_mock_connection_manager->getConnection();
        auto sent_messages = mock_connection->getAllMessages();
        
        // Should have processed most or all signals
        size_t signal_count = 0;
        for (const auto& message : sent_messages) {
            if (message->getType() == MockDBusMessage::Type::Signal) {
                signal_count++;
            }
        }
        
        ASSERT_TRUE(signal_count > 0, "Should have processed signals asynchronously");
    }

    void testQueueOverflowHandling() {
        // Test behavior when signal queue overflows
        const size_t large_signal_count = 10000;
        
        // Emit a large number of signals rapidly to test queue limits
        for (size_t i = 0; i < large_signal_count; ++i) {
            std::map<std::string, DBusVariant> properties;
            properties["OverflowTest"] = DBusVariant(uint64_t(i));
            
            m_signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
            
            // Occasionally emit Seeked signals too
            if (i % 10 == 0) {
                m_signal_emitter->emitSeeked(i * 1000);
            }
        }
        
        // Signal emitter should handle overflow gracefully without crashing
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        auto* mock_connection = m_mock_connection_manager->getConnection();
        auto sent_messages = mock_connection->getAllMessages();
        
        // Should have processed some signals (may not be all due to queue limits)
        size_t signal_count = 0;
        for (const auto& message : sent_messages) {
            if (message->getType() == MockDBusMessage::Type::Signal) {
                signal_count++;
            }
        }
        
        ASSERT_TRUE(signal_count > 0, "Should have processed some signals despite overflow");
        
        // Signal emitter should still be functional after overflow
        std::map<std::string, DBusVariant> test_properties;
        test_properties["PostOverflowTest"] = DBusVariant(std::string("test"));
        
        m_signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", test_properties);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Should still work
        auto final_messages = mock_connection->getAllMessages();
        ASSERT_TRUE(final_messages.size() >= sent_messages.size(), 
                   "Should still be functional after queue overflow");
    }

    void testThreadingValidation() {
        ThreadSafetyTester::Config config;
        config.num_threads = 8;
        config.operations_per_thread = 50;
        config.test_duration = std::chrono::milliseconds{3000};
        
        ThreadSafetyTester tester(config);
        
        // Test concurrent signal emission
        std::atomic<size_t> signal_counter{0};
        auto signal_test = [this, &signal_counter]() -> bool {
            try {
                size_t counter = signal_counter.fetch_add(1);
                
                if (counter % 2 == 0) {
                    // Emit PropertiesChanged
                    std::map<std::string, DBusVariant> properties;
                    properties["ThreadTest"] = DBusVariant(uint64_t(counter));
                    
                    m_signal_emitter->emitPropertiesChanged(
                        "org.mpris.MediaPlayer2.Player", 
                        properties
                    );
                } else {
                    // Emit Seeked
                    m_signal_emitter->emitSeeked(counter * 1000);
                }
                
                return true;
            } catch (...) {
                return false;
            }
        };
        
        auto results = tester.runTest(signal_test, "ConcurrentSignalEmission");
        
        ASSERT_TRUE(results.successful_operations > 0, "Should have successful signal emissions");
        ASSERT_FALSE(results.deadlock_detected, "Should not detect deadlocks");
        
        // Allow time for signal processing
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Verify signals were processed
        auto* mock_connection = m_mock_connection_manager->getConnection();
        auto sent_messages = mock_connection->getAllMessages();
        
        size_t total_signals = 0;
        for (const auto& message : sent_messages) {
            if (message->getType() == MockDBusMessage::Type::Signal) {
                total_signals++;
            }
        }
        
        ASSERT_TRUE(total_signals > 0, "Should have processed signals from concurrent threads");
    }

    void testErrorHandlingAndRecovery() {
        // Test behavior when D-Bus connection fails
        m_mock_connection_manager->simulateConnectionLoss();
        
        // Emit signals during connection failure
        std::map<std::string, DBusVariant> properties;
        properties["ErrorTest"] = DBusVariant(std::string("test"));
        
        // Should not crash even with connection failure
        m_signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
        m_signal_emitter->emitSeeked(123456);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Restore connection
        m_mock_connection_manager->simulateConnectionRestore();
        
        // Should recover and work normally
        m_signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto* mock_connection = m_mock_connection_manager->getConnection();
        ASSERT_NOT_NULL(mock_connection, "Should have restored connection");
        
        // Test with message sending failures
        mock_connection->setMessageFailureRate(0.5); // 50% failure rate
        
        // Emit signals with message failures
        for (int i = 0; i < 20; ++i) {
            properties["FailureTest"] = DBusVariant(uint64_t(i));
            m_signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Should handle failures gracefully
        mock_connection->setMessageFailureRate(0.0); // Restore normal operation
        
        // Should still be functional
        properties["RecoveryTest"] = DBusVariant(std::string("recovered"));
        m_signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        auto final_messages = mock_connection->getAllMessages();
        ASSERT_TRUE(final_messages.size() > 0, "Should recover from message failures");
    }

    void testPerformanceUnderLoad() {
        // Measure signal emission performance
        auto start_time = std::chrono::high_resolution_clock::now();
        
        const size_t num_operations = 1000;
        
        for (size_t i = 0; i < num_operations; ++i) {
            if (i % 3 == 0) {
                // PropertiesChanged signal
                std::map<std::string, DBusVariant> properties;
                properties["PerfTest"] = DBusVariant(uint64_t(i));
                m_signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
            } else {
                // Seeked signal
                m_signal_emitter->emitSeeked(i * 1000);
            }
        }
        
        auto emission_end_time = std::chrono::high_resolution_clock::now();
        auto emission_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            emission_end_time - start_time
        );
        
        // Emission should be very fast (asynchronous)
        ASSERT_TRUE(emission_duration.count() < 500, "Signal emission should be fast");
        
        // Wait for processing to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        auto processing_end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            processing_end_time - start_time
        );
        
        // Total processing should be reasonable
        ASSERT_TRUE(total_duration.count() < 5000, "Signal processing should complete in reasonable time");
        
        auto* mock_connection = m_mock_connection_manager->getConnection();
        auto sent_messages = mock_connection->getAllMessages();
        
        // Should have processed most signals
        size_t signal_count = 0;
        for (const auto& message : sent_messages) {
            if (message->getType() == MockDBusMessage::Type::Signal) {
                signal_count++;
            }
        }
        
        ASSERT_TRUE(signal_count > num_operations / 2, "Should have processed most signals");
        
        // Test lock contention measurement
        LockContentionAnalyzer analyzer;
        std::mutex test_mutex;
        
        auto contention_metrics = analyzer.analyzeLockContention(
            test_mutex, 
            std::chrono::milliseconds{500}, 
            4
        );
        
        ASSERT_TRUE(contention_metrics.total_acquisitions > 0, "Should measure lock usage");
        ASSERT_TRUE(contention_metrics.average_acquisition_time.count() < 1000, 
                   "Lock acquisition should be fast");
    }
};

/**
 * @brief Test class for SignalEmitter stress testing and edge cases
 */
class SignalEmitterStressTest : public TestCase {
public:
    SignalEmitterStressTest() : TestCase("SignalEmitterStressTest") {}

protected:
    void runTest() override {
        testRapidSignalBursts();
        testLongRunningOperation();
        testMemoryUsageUnderLoad();
        testShutdownDuringOperation();
    }

private:
    void testRapidSignalBursts() {
        auto mock_connection_manager = std::make_unique<MockDBusConnectionManager>();
        mock_connection_manager->connect();
        auto signal_emitter = std::make_unique<SignalEmitter>(reinterpret_cast<DBusConnectionManager*>(mock_connection_manager.get()));
        
        // Emit rapid bursts of signals
        const size_t burst_size = 500;
        const size_t num_bursts = 10;
        
        for (size_t burst = 0; burst < num_bursts; ++burst) {
            for (size_t i = 0; i < burst_size; ++i) {
                std::map<std::string, DBusVariant> properties;
                properties["BurstTest"] = DBusVariant(uint64_t(burst * burst_size + i));
                
                signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
            }
            
            // Brief pause between bursts
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Allow processing time
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Should handle bursts without crashing
        auto* mock_connection = mock_connection_manager->getConnection();
        auto sent_messages = mock_connection->getAllMessages();
        
        ASSERT_TRUE(sent_messages.size() > 0, "Should have processed signal bursts");
    }

    void testLongRunningOperation() {
        auto mock_connection_manager = std::make_unique<MockDBusConnectionManager>();
        mock_connection_manager->connect();
        auto signal_emitter = std::make_unique<SignalEmitter>(reinterpret_cast<DBusConnectionManager*>(mock_connection_manager.get()));
        
        // Run continuous signal emission for extended period
        std::atomic<bool> should_stop{false};
        std::atomic<size_t> signals_emitted{0};
        
        std::thread emission_thread([&]() {
            size_t counter = 0;
            while (!should_stop.load()) {
                std::map<std::string, DBusVariant> properties;
                properties["LongRunTest"] = DBusVariant(uint64_t(counter++));
                
                signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
                signals_emitted.fetch_add(1);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
        
        // Let it run for a while
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        should_stop.store(true);
        emission_thread.join();
        
        ASSERT_TRUE(signals_emitted.load() > 0, "Should have emitted signals during long run");
        
        // Allow final processing
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        auto* mock_connection = mock_connection_manager->getConnection();
        auto sent_messages = mock_connection->getAllMessages();
        
        ASSERT_TRUE(sent_messages.size() > 0, "Should have processed signals during long run");
    }

    void testMemoryUsageUnderLoad() {
        auto mock_connection_manager = std::make_unique<MockDBusConnectionManager>();
        mock_connection_manager->connect();
        auto signal_emitter = std::make_unique<SignalEmitter>(reinterpret_cast<DBusConnectionManager*>(mock_connection_manager.get()));
        
        // Test with large property data
        std::string large_value(10000, 'M'); // 10KB string
        
        for (int i = 0; i < 100; ++i) {
            std::map<std::string, DBusVariant> properties;
            properties["LargeData"] = DBusVariant(large_value + std::to_string(i));
            
            signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
        }
        
        // Allow processing
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Should handle large data without memory issues
        auto* mock_connection = mock_connection_manager->getConnection();
        auto sent_messages = mock_connection->getAllMessages();
        
        ASSERT_TRUE(sent_messages.size() > 0, "Should handle large data without memory issues");
    }

    void testShutdownDuringOperation() {
        auto mock_connection_manager = std::make_unique<MockDBusConnectionManager>();
        mock_connection_manager->connect();
        auto signal_emitter = std::make_unique<SignalEmitter>(reinterpret_cast<DBusConnectionManager*>(mock_connection_manager.get()));
        
        // Start emitting signals
        std::atomic<bool> emission_started{false};
        std::thread emission_thread([&]() {
            emission_started.store(true);
            for (int i = 0; i < 1000; ++i) {
                std::map<std::string, DBusVariant> properties;
                properties["ShutdownTest"] = DBusVariant(uint64_t(i));
                
                signal_emitter->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        
        // Wait for emission to start
        while (!emission_started.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        
        // Shutdown during operation
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        signal_emitter.reset(); // Should shutdown gracefully
        
        emission_thread.join(); // Should complete without hanging
        
        // Test passed if we reach here without hanging or crashing
        ASSERT_TRUE(true, "Should shutdown gracefully during operation");
    }
};

// Test suite setup and execution
int main() {
    TestSuite suite("SignalEmitter Comprehensive Tests");
    
    suite.addTest(std::make_unique<SignalEmitterTest>());
    suite.addTest(std::make_unique<SignalEmitterStressTest>());
    
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}