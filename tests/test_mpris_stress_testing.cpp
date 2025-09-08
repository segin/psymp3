/*
 * test_mpris_stress_testing.cpp - Stress tests for high-concurrency MPRIS scenarios
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
#include "mock_player.h"

using namespace TestFramework;

/**
 * Stress test for high-frequency MPRIS operations
 */
bool test_high_frequency_operations() {
    std::cout << "Testing high-frequency MPRIS operations..." << std::endl;
    
    auto player = MockPlayerFactory::createPerformanceTestPlayer();
    MockDBusConnection::Config config;
    config.enable_message_logging = false; // Disable logging for performance
    MockDBusConnection dbus_connection(config);
    
    ASSERT_TRUE(dbus_connection.connect(), "D-Bus connection should succeed");
    
    const size_t num_operations = 10000;
    const size_t num_threads = 8;
    
    Threading::ThreadSafetyTester::Config test_config;
    test_config.num_threads = num_threads;
    test_config.operations_per_thread = num_operations / num_threads;
    test_config.test_duration = std::chrono::milliseconds{5000}; // 5 seconds max
    test_config.enable_random_delays = false; // No delays for performance test
    
    Threading::ThreadSafetyTester tester(test_config);
    
    // Test function that performs various MPRIS operations
    std::atomic<size_t> operation_counter{0};
    auto test_func = [&]() -> bool {
        size_t op_index = operation_counter.fetch_add(1);
        
        switch (op_index % 6) {
            case 0: return player->play();
            case 1: return player->pause();
            case 2: return player->stop();
            case 3: {
                player->seekTo((op_index % 180) * 1000000); // Seek to different positions
                return true;
            }
            case 4: {
                // Send D-Bus message
                auto message = MockDBusMessageFactory::createPlayMethodCall();
                return dbus_connection.sendMessage(std::move(message));
            }
            case 5: {
                // Get player state (read operation)
                player->getState();
                player->getPosition();
                return true;
            }
            default: return false;
        }
    };
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto results = tester.runTest(test_func, "High-frequency operations");
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    ASSERT_TRUE(results.total_operations > 0, "Should have completed operations");
    ASSERT_TRUE(results.successful_operations > results.total_operations * 0.95, "Should have >95% success rate");
    
    double ops_per_second = static_cast<double>(results.total_operations) / (total_duration.count() / 1000.0);
    
    std::cout << "High-frequency test results:" << std::endl;
    std::cout << "  Total operations: " << results.total_operations << std::endl;
    std::cout << "  Successful operations: " << results.successful_operations << std::endl;
    std::cout << "  Success rate: " << (static_cast<double>(results.successful_operations) / results.total_operations * 100.0) << "%" << std::endl;
    std::cout << "  Operations per second: " << ops_per_second << std::endl;
    std::cout << "  Average operation time: " << results.average_operation_time.count() << "ms" << std::endl;
    
    // Validate that player state is still consistent
    ASSERT_TRUE(player->validateState(), "Player state should be valid after stress test");
    
    std::cout << "✓ High-frequency operations stress test passed" << std::endl;
    return true;
}

/**
 * Stress test for lock contention under heavy load
 */
bool test_lock_contention_stress() {
    std::cout << "Testing lock contention under heavy load..." << std::endl;
    
    auto player = MockPlayerFactory::createThreadSafetyTestPlayer();
    
    const size_t num_threads = 16; // High thread count
    const size_t operations_per_thread = 1000;
    
    Threading::ThreadSafetyTester::Config config;
    config.num_threads = num_threads;
    config.operations_per_thread = operations_per_thread;
    config.test_duration = std::chrono::milliseconds{10000}; // 10 seconds max
    config.enable_random_delays = true;
    config.min_delay = std::chrono::microseconds{1};
    config.max_delay = std::chrono::microseconds{10};
    
    Threading::ThreadSafetyTester tester(config);
    
    // Test function that creates high lock contention
    std::atomic<size_t> operation_counter{0};
    auto contention_test = [&]() -> bool {
        size_t op_index = operation_counter.fetch_add(1);
        
        // Mix of operations that require different lock patterns
        switch (op_index % 8) {
            case 0: return player->play();
            case 1: return player->pause();
            case 2: return player->stop();
            case 3: {
                player->seekTo(op_index * 1000); // Frequent seeking
                return true;
            }
            case 4: {
                // Read operations (should be fast but still need locks)
                player->getState();
                return true;
            }
            case 5: {
                player->getPosition();
                return true;
            }
            case 6: {
                player->getCurrentTrack();
                return true;
            }
            case 7: {
                // Batch operation
                std::vector<std::string> ops = {"play", "seek:5000000", "pause"};
                player->performBatchOperations(ops);
                return true;
            }
            default: return false;
        }
    };
    
    auto results = tester.runTest(contention_test, "Lock contention stress");
    
    ASSERT_TRUE(results.total_operations > 0, "Should have completed operations");
    ASSERT_FALSE(results.deadlock_detected, "Should not detect deadlocks");
    
    // Check lock contention metrics
    size_t contention_count = player->getLockContentionCount();
    double contention_ratio = static_cast<double>(contention_count) / results.total_operations;
    
    std::cout << "Lock contention stress test results:" << std::endl;
    std::cout << "  Total operations: " << results.total_operations << std::endl;
    std::cout << "  Lock contentions detected: " << contention_count << std::endl;
    std::cout << "  Contention ratio: " << (contention_ratio * 100.0) << "%" << std::endl;
    std::cout << "  Average operation time: " << results.average_operation_time.count() << "ms" << std::endl;
    std::cout << "  Max operation time: " << results.max_operation_time.count() << "ms" << std::endl;
    
    // Validate final state
    ASSERT_TRUE(player->validateState(), "Player state should be valid after contention test");
    
    std::cout << "✓ Lock contention stress test passed" << std::endl;
    return true;
}

/**
 * Stress test for D-Bus message throughput
 */
bool test_dbus_message_throughput() {
    std::cout << "Testing D-Bus message throughput..." << std::endl;
    
    MockDBusConnection::Config config;
    config.enable_message_logging = false;
    config.simulate_message_failures = false;
    config.max_message_queue_size = 10000; // Large queue for throughput test
    
    MockDBusConnection connection(config);
    ASSERT_TRUE(connection.connect(), "D-Bus connection should succeed");
    
    const size_t num_messages = 50000;
    const size_t num_threads = 8;
    
    std::atomic<size_t> messages_sent{0};
    std::atomic<size_t> send_failures{0};
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Create sender threads
    std::vector<std::thread> sender_threads;
    for (size_t t = 0; t < num_threads; ++t) {
        sender_threads.emplace_back([&, t]() {
            size_t messages_per_thread = num_messages / num_threads;
            
            for (size_t i = 0; i < messages_per_thread; ++i) {
                // Create different types of messages
                std::unique_ptr<MockDBusMessage> message;
                
                switch (i % 4) {
                    case 0:
                        message = MockDBusMessageFactory::createPlayMethodCall();
                        break;
                    case 1:
                        message = MockDBusMessageFactory::createPauseMethodCall();
                        break;
                    case 2:
                        message = MockDBusMessageFactory::createSeekMethodCall(i * 1000);
                        break;
                    case 3: {
                        std::map<std::string, std::string> props = {
                            {"PlaybackStatus", "Playing"},
                            {"Position", std::to_string(i * 1000000)}
                        };
                        message = MockDBusMessageFactory::createPropertiesChangedSignal(
                            "org.mpris.MediaPlayer2.Player", props);
                        break;
                    }
                }
                
                if (connection.sendMessage(std::move(message))) {
                    messages_sent.fetch_add(1);
                } else {
                    send_failures.fetch_add(1);
                }
            }
        });
    }
    
    // Wait for all senders to complete
    for (auto& thread : sender_threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    double messages_per_second = static_cast<double>(messages_sent.load()) / (duration.count() / 1000.0);
    double success_rate = static_cast<double>(messages_sent.load()) / (messages_sent.load() + send_failures.load());
    
    ASSERT_TRUE(messages_sent.load() > 0, "Should have sent messages");
    ASSERT_TRUE(success_rate > 0.95, "Should have >95% success rate");
    
    std::cout << "D-Bus message throughput results:" << std::endl;
    std::cout << "  Messages sent: " << messages_sent.load() << std::endl;
    std::cout << "  Send failures: " << send_failures.load() << std::endl;
    std::cout << "  Success rate: " << (success_rate * 100.0) << "%" << std::endl;
    std::cout << "  Duration: " << duration.count() << "ms" << std::endl;
    std::cout << "  Messages per second: " << messages_per_second << std::endl;
    
    // Verify connection statistics
    auto stats = connection.getStatistics();
    ASSERT_EQUALS(messages_sent.load(), stats.messages_sent, "Statistics should match sent messages");
    
    std::cout << "✓ D-Bus message throughput test passed" << std::endl;
    return true;
}

/**
 * Stress test for memory usage under load
 */
bool test_memory_usage_stress() {
    std::cout << "Testing memory usage under stress..." << std::endl;
    
    // Create multiple mock components to test memory usage
    std::vector<std::unique_ptr<MockPlayer>> players;
    std::vector<std::unique_ptr<MockDBusConnection>> connections;
    
    const size_t num_components = 100;
    
    // Create many mock components
    for (size_t i = 0; i < num_components; ++i) {
        players.push_back(MockPlayerFactory::createBasicPlayer());
        
        MockDBusConnection::Config config;
        config.enable_message_logging = false;
        connections.push_back(std::make_unique<MockDBusConnection>(config));
    }
    
    // Perform operations on all components
    for (size_t i = 0; i < num_components; ++i) {
        ASSERT_TRUE(players[i]->play(), "Player should play successfully");
        ASSERT_TRUE(connections[i]->connect(), "Connection should succeed");
        
        // Create some messages
        for (int j = 0; j < 10; ++j) {
            auto message = MockDBusMessageFactory::createPlayMethodCall();
            connections[i]->sendMessage(std::move(message));
        }
    }
    
    // Verify all components are still functional
    for (size_t i = 0; i < num_components; ++i) {
        ASSERT_EQUALS(PlayerState::Playing, players[i]->getState(), "Player should be in playing state");
        ASSERT_TRUE(connections[i]->isConnected(), "Connection should be connected");
        
        auto stats = connections[i]->getStatistics();
        ASSERT_EQUALS(10u, stats.messages_sent, "Should have sent 10 messages per connection");
    }
    
    std::cout << "Memory usage stress test results:" << std::endl;
    std::cout << "  Created " << num_components << " mock players" << std::endl;
    std::cout << "  Created " << num_components << " mock D-Bus connections" << std::endl;
    std::cout << "  Sent " << (num_components * 10) << " total messages" << std::endl;
    std::cout << "  All components remain functional" << std::endl;
    
    // Clean up (destructors will be called automatically)
    players.clear();
    connections.clear();
    
    std::cout << "✓ Memory usage stress test passed" << std::endl;
    return true;
}

/**
 * Stress test for connection instability simulation
 */
bool test_connection_instability_stress() {
    std::cout << "Testing connection instability stress..." << std::endl;
    
    MockDBusConnection::Config config;
    config.simulate_connection_failures = true;
    config.connection_failure_rate = 0.3; // 30% failure rate
    config.simulate_message_failures = true;
    config.message_failure_rate = 0.1; // 10% message failure rate
    
    MockDBusConnectionManager manager;
    manager.setConnectionConfig(config);
    manager.enableAutoReconnect(true);
    
    const size_t num_operations = 1000;
    size_t successful_operations = 0;
    size_t connection_attempts = 0;
    size_t reconnection_attempts = 0;
    
    for (size_t i = 0; i < num_operations; ++i) {
        // Try to connect if not connected
        if (!manager.isConnected()) {
            connection_attempts++;
            if (manager.connect()) {
                // Connection successful
            } else {
                // Connection failed, try reconnection
                reconnection_attempts++;
                if (manager.attemptReconnection()) {
                    // Reconnection successful
                }
            }
        }
        
        // Try to send a message if connected
        if (manager.isConnected()) {
            auto message = MockDBusMessageFactory::createPlayMethodCall();
            if (manager.getConnection()->sendMessage(std::move(message))) {
                successful_operations++;
            }
        }
        
        // Randomly simulate connection loss
        if (i % 50 == 0) {
            manager.simulateConnectionLoss();
        }
        
        // Small delay to simulate real-world timing
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    double success_rate = static_cast<double>(successful_operations) / num_operations;
    
    std::cout << "Connection instability stress test results:" << std::endl;
    std::cout << "  Total operations attempted: " << num_operations << std::endl;
    std::cout << "  Successful operations: " << successful_operations << std::endl;
    std::cout << "  Success rate: " << (success_rate * 100.0) << "%" << std::endl;
    std::cout << "  Connection attempts: " << connection_attempts << std::endl;
    std::cout << "  Reconnection attempts: " << reconnection_attempts << std::endl;
    
    // We expect some failures due to simulated instability, but not complete failure
    ASSERT_TRUE(successful_operations > 0, "Should have some successful operations");
    ASSERT_TRUE(success_rate > 0.1, "Success rate should be > 10% even with instability");
    
    std::cout << "✓ Connection instability stress test passed" << std::endl;
    return true;
}

/**
 * Comprehensive stress test combining all scenarios
 */
bool test_comprehensive_stress() {
    std::cout << "Running comprehensive stress test..." << std::endl;
    
    const size_t num_threads = 12;
    const size_t operations_per_thread = 500;
    const auto test_duration = std::chrono::milliseconds{15000}; // 15 seconds
    
    // Create multiple mock components
    auto player = MockPlayerFactory::createRealisticPlayer();
    MockDBusConnection::Config config;
    config.simulate_connection_failures = true;
    config.connection_failure_rate = 0.05; // 5% failure rate
    config.simulate_message_failures = true;
    config.message_failure_rate = 0.02; // 2% message failure rate
    config.enable_message_logging = false;
    
    MockDBusConnection dbus_connection(config);
    ASSERT_TRUE(dbus_connection.connect(), "Initial D-Bus connection should succeed");
    
    Threading::ThreadSafetyTester::Config test_config;
    test_config.num_threads = num_threads;
    test_config.operations_per_thread = operations_per_thread;
    test_config.test_duration = test_duration;
    test_config.enable_random_delays = true;
    test_config.min_delay = std::chrono::microseconds{1};
    test_config.max_delay = std::chrono::microseconds{50};
    
    Threading::ThreadSafetyTester tester(test_config);
    
    // Comprehensive test function
    std::atomic<size_t> operation_counter{0};
    std::atomic<size_t> dbus_operations{0};
    std::atomic<size_t> player_operations{0};
    
    auto comprehensive_test = [&]() -> bool {
        size_t op_index = operation_counter.fetch_add(1);
        
        try {
            switch (op_index % 10) {
                case 0: case 1: case 2: {
                    // Player operations (30%)
                    player_operations.fetch_add(1);
                    switch (op_index % 3) {
                        case 0: return player->play();
                        case 1: return player->pause();
                        case 2: return player->stop();
                    }
                    break;
                }
                case 3: case 4: {
                    // Seeking operations (20%)
                    player_operations.fetch_add(1);
                    player->seekTo((op_index % 180) * 1000000);
                    return true;
                }
                case 5: case 6: case 7: {
                    // D-Bus operations (30%)
                    dbus_operations.fetch_add(1);
                    std::unique_ptr<MockDBusMessage> message;
                    switch (op_index % 3) {
                        case 0:
                            message = MockDBusMessageFactory::createPlayMethodCall();
                            break;
                        case 1:
                            message = MockDBusMessageFactory::createPauseMethodCall();
                            break;
                        case 2:
                            message = MockDBusMessageFactory::createSeekMethodCall(op_index * 1000);
                            break;
                    }
                    return dbus_connection.sendMessage(std::move(message));
                }
                case 8: {
                    // Read operations (10%)
                    player->getState();
                    player->getPosition();
                    player->getCurrentTrack();
                    return true;
                }
                case 9: {
                    // Playlist operations (10%)
                    if (op_index % 2 == 0) {
                        player->nextTrack();
                    } else {
                        player->prevTrack();
                    }
                    return true;
                }
                default:
                    return false;
            }
        } catch (const std::exception& e) {
            // Log exception but don't fail the test
            return false;
        }
        
        return false;
    };
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto results = tester.runTest(comprehensive_test, "Comprehensive stress test");
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    ASSERT_TRUE(results.total_operations > 0, "Should have completed operations");
    ASSERT_FALSE(results.deadlock_detected, "Should not detect deadlocks");
    
    // Calculate performance metrics
    double ops_per_second = static_cast<double>(results.total_operations) / (actual_duration.count() / 1000.0);
    double success_rate = static_cast<double>(results.successful_operations) / results.total_operations;
    
    std::cout << "Comprehensive stress test results:" << std::endl;
    std::cout << "  Test duration: " << actual_duration.count() << "ms" << std::endl;
    std::cout << "  Total operations: " << results.total_operations << std::endl;
    std::cout << "  Successful operations: " << results.successful_operations << std::endl;
    std::cout << "  Failed operations: " << results.failed_operations << std::endl;
    std::cout << "  Success rate: " << (success_rate * 100.0) << "%" << std::endl;
    std::cout << "  Operations per second: " << ops_per_second << std::endl;
    std::cout << "  Player operations: " << player_operations.load() << std::endl;
    std::cout << "  D-Bus operations: " << dbus_operations.load() << std::endl;
    std::cout << "  Average operation time: " << results.average_operation_time.count() << "ms" << std::endl;
    std::cout << "  Max operation time: " << results.max_operation_time.count() << "ms" << std::endl;
    
    // Validate final state
    ASSERT_TRUE(player->validateState(), "Player state should be valid after comprehensive stress test");
    
    // Check statistics
    auto player_stats = player->getStatistics();
    auto dbus_stats = dbus_connection.getStatistics();
    
    std::cout << "  Player statistics:" << std::endl;
    std::cout << "    Play calls: " << player_stats.play_calls << std::endl;
    std::cout << "    Pause calls: " << player_stats.pause_calls << std::endl;
    std::cout << "    Stop calls: " << player_stats.stop_calls << std::endl;
    std::cout << "    Seek calls: " << player_stats.seek_calls << std::endl;
    std::cout << "  D-Bus statistics:" << std::endl;
    std::cout << "    Messages sent: " << dbus_stats.messages_sent << std::endl;
    std::cout << "    Messages failed: " << dbus_stats.messages_failed << std::endl;
    
    // We expect high success rate even under stress
    ASSERT_TRUE(success_rate > 0.8, "Success rate should be > 80% even under comprehensive stress");
    
    std::cout << "✓ Comprehensive stress test passed" << std::endl;
    return true;
}

/**
 * Main stress test runner
 */
int main() {
    std::cout << "Running MPRIS Stress Tests..." << std::endl << std::endl;
    
    bool all_passed = true;
    
    try {
        all_passed &= test_high_frequency_operations();
        all_passed &= test_lock_contention_stress();
        all_passed &= test_dbus_message_throughput();
        all_passed &= test_memory_usage_stress();
        all_passed &= test_connection_instability_stress();
        all_passed &= test_comprehensive_stress();
        
    } catch (const std::exception& e) {
        std::cout << "✗ Stress test failed with exception: " << e.what() << std::endl;
        all_passed = false;
    }
    
    std::cout << std::endl;
    if (all_passed) {
        std::cout << "✓ All MPRIS stress tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some MPRIS stress tests FAILED!" << std::endl;
        return 1;
    }
}