/*
 * test_mock_framework_standalone.cpp - Standalone test for mock framework components
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
 * Test MockDBusConnection basic functionality
 */
bool test_mock_dbus_connection_basic() {
    std::cout << "Testing MockDBusConnection basic functionality..." << std::endl;
    
    MockDBusConnection::Config config;
    config.auto_connect = true;
    config.simulate_connection_failures = false;
    
    MockDBusConnection connection(config);
    
    // Test initial state
    ASSERT_FALSE(connection.isConnected(), "Connection should not be connected initially");
    ASSERT_EQUALS(MockDBusConnection::State::Disconnected, connection.getState(), "Initial state should be Disconnected");
    
    // Test connection
    bool connected = connection.connect();
    ASSERT_TRUE(connected, "Connection should succeed");
    ASSERT_TRUE(connection.isConnected(), "Connection should be connected after connect()");
    ASSERT_EQUALS(MockDBusConnection::State::Connected, connection.getState(), "State should be Connected");
    
    // Test service name registration
    bool name_requested = connection.requestName("org.mpris.MediaPlayer2.test");
    ASSERT_TRUE(name_requested, "Service name request should succeed");
    
    auto owned_names = connection.getOwnedNames();
    ASSERT_EQUALS(1u, owned_names.size(), "Should own one service name");
    ASSERT_EQUALS(std::string("org.mpris.MediaPlayer2.test"), owned_names[0], "Should own the requested service name");
    
    // Test message sending
    auto message = MockDBusMessageFactory::createPlayMethodCall();
    bool sent = connection.sendMessage(std::move(message));
    ASSERT_TRUE(sent, "Message sending should succeed");
    
    // Test statistics
    auto stats = connection.getStatistics();
    ASSERT_EQUALS(1u, stats.messages_sent, "Should have sent one message");
    ASSERT_EQUALS(1u, stats.connection_attempts, "Should have one connection attempt");
    
    // Test disconnection
    connection.disconnect();
    ASSERT_FALSE(connection.isConnected(), "Connection should be disconnected after disconnect()");
    
    std::cout << "✓ MockDBusConnection basic functionality test passed" << std::endl;
    return true;
}

/**
 * Test MockPlayer basic functionality
 */
bool test_mock_player_basic() {
    std::cout << "Testing MockPlayer basic functionality..." << std::endl;
    
    auto player = MockPlayerFactory::createBasicPlayer();
    
    // Test initial state
    ASSERT_EQUALS(PlayerState::Stopped, player->getState(), "Initial state should be Stopped");
    ASSERT_EQUALS(0u, player->getPosition(), "Initial position should be 0");
    
    // Test play
    bool played = player->play();
    ASSERT_TRUE(played, "Play should succeed");
    ASSERT_EQUALS(PlayerState::Playing, player->getState(), "State should be Playing after play()");
    
    // Test pause
    bool paused = player->pause();
    ASSERT_TRUE(paused, "Pause should succeed");
    ASSERT_EQUALS(PlayerState::Paused, player->getState(), "State should be Paused after pause()");
    
    // Test stop
    bool stopped = player->stop();
    ASSERT_TRUE(stopped, "Stop should succeed");
    ASSERT_EQUALS(PlayerState::Stopped, player->getState(), "State should be Stopped after stop()");
    ASSERT_EQUALS(0u, player->getPosition(), "Position should be 0 after stop()");
    
    // Test seeking
    player->play();
    player->seekTo(30000000); // 30 seconds
    ASSERT_EQUALS(30000000u, player->getPosition(), "Position should be 30 seconds after seek");
    
    std::cout << "✓ MockPlayer basic functionality test passed" << std::endl;
    return true;
}

/**
 * Test threading safety utilities
 */
bool test_threading_utilities() {
    std::cout << "Testing threading utilities..." << std::endl;
    
    auto player = MockPlayerFactory::createThreadSafetyTestPlayer();
    
    // Test thread safety tester
    Threading::ThreadSafetyTester::Config config;
    config.num_threads = 4;
    config.operations_per_thread = 100;
    config.test_duration = std::chrono::milliseconds{1000};
    
    Threading::ThreadSafetyTester tester(config);
    
    auto test_func = [&player]() -> bool {
        static std::atomic<int> counter{0};
        int op = counter.fetch_add(1) % 3;
        
        switch (op) {
            case 0: return player->play();
            case 1: return player->pause();
            case 2: return player->stop();
            default: return false;
        }
    };
    
    auto results = tester.runTest(test_func, "Threading safety test");
    
    ASSERT_FALSE(results.deadlock_detected, "Should not detect deadlocks");
    ASSERT_TRUE(results.total_operations > 0, "Should have completed operations");
    
    std::cout << "  Threading test completed " << results.total_operations << " operations" << std::endl;
    
    // Test lock contention analyzer
    Threading::LockContentionAnalyzer analyzer;
    std::mutex test_mutex;
    
    auto metrics = analyzer.analyzeLockContention(test_mutex, std::chrono::milliseconds{200}, 4);
    
    ASSERT_TRUE(metrics.total_acquisitions > 0, "Should have lock acquisitions");
    
    std::cout << "  Lock contention: " << metrics.total_acquisitions << " acquisitions, " 
              << (metrics.contention_ratio * 100.0) << "% contention" << std::endl;
    
    std::cout << "✓ Threading utilities test passed" << std::endl;
    return true;
}

/**
 * Test performance benchmarking
 */
bool test_performance_benchmarking() {
    std::cout << "Testing performance benchmarking..." << std::endl;
    
    auto player = MockPlayerFactory::createPerformanceTestPlayer();
    MockDBusConnection::Config config;
    config.enable_message_logging = false;
    MockDBusConnection dbus_connection(config);
    
    ASSERT_TRUE(dbus_connection.connect(), "D-Bus connection should succeed");
    
    // Benchmark player operations
    const size_t num_operations = 10000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < num_operations; ++i) {
        player->play();
        player->pause();
        player->getState();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    double ops_per_second = static_cast<double>(num_operations * 3) / (duration.count() / 1000000.0);
    
    std::cout << "  Player operations: " << ops_per_second << " ops/sec" << std::endl;
    
    // Benchmark D-Bus message throughput
    start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < num_operations; ++i) {
        auto message = MockDBusMessageFactory::createPlayMethodCall();
        dbus_connection.sendMessage(std::move(message));
    }
    
    end_time = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    double messages_per_second = static_cast<double>(num_operations) / (duration.count() / 1000000.0);
    
    std::cout << "  D-Bus messages: " << messages_per_second << " msg/sec" << std::endl;
    
    // Verify statistics
    auto stats = dbus_connection.getStatistics();
    ASSERT_EQUALS(num_operations, stats.messages_sent, "Message count should match");
    
    std::cout << "✓ Performance benchmarking test passed" << std::endl;
    return true;
}

/**
 * Test error simulation
 */
bool test_error_simulation() {
    std::cout << "Testing error simulation..." << std::endl;
    
    // Test player error simulation
    auto player = MockPlayerFactory::createErrorSimulationPlayer(0.5); // 50% error rate
    
    size_t successful_operations = 0;
    size_t total_operations = 100;
    
    for (size_t i = 0; i < total_operations; ++i) {
        if (player->play()) {
            successful_operations++;
        }
    }
    
    double success_rate = static_cast<double>(successful_operations) / total_operations;
    
    std::cout << "  Player error simulation: " << (success_rate * 100.0) << "% success rate" << std::endl;
    
    // Should have some failures with 50% error rate
    ASSERT_TRUE(success_rate < 0.8, "Should have some failures with error simulation");
    ASSERT_TRUE(success_rate > 0.2, "Should have some successes even with errors");
    
    // Test D-Bus error simulation
    MockDBusConnection::Config dbus_config;
    dbus_config.simulate_message_failures = true;
    dbus_config.message_failure_rate = 0.3; // 30% failure rate
    
    MockDBusConnection dbus_connection(dbus_config);
    dbus_connection.connect();
    
    size_t successful_messages = 0;
    
    for (size_t i = 0; i < total_operations; ++i) {
        auto message = MockDBusMessageFactory::createPlayMethodCall();
        if (dbus_connection.sendMessage(std::move(message))) {
            successful_messages++;
        }
    }
    
    double message_success_rate = static_cast<double>(successful_messages) / total_operations;
    
    std::cout << "  D-Bus error simulation: " << (message_success_rate * 100.0) << "% success rate" << std::endl;
    
    // Should have some failures with 30% error rate
    ASSERT_TRUE(message_success_rate < 0.9, "Should have some failures with D-Bus error simulation");
    ASSERT_TRUE(message_success_rate > 0.5, "Should have some successes even with D-Bus errors");
    
    std::cout << "✓ Error simulation test passed" << std::endl;
    return true;
}

/**
 * Test message factory
 */
bool test_message_factory() {
    std::cout << "Testing message factory..." << std::endl;
    
    // Test method call creation
    auto play_msg = MockDBusMessageFactory::createPlayMethodCall();
    ASSERT_NOT_NULL(play_msg.get(), "Play message should be created");
    ASSERT_EQUALS(MockDBusMessage::Type::MethodCall, play_msg->getType(), "Should be method call type");
    ASSERT_EQUALS(std::string("org.mpris.MediaPlayer2.Player"), play_msg->getInterface(), "Should have correct interface");
    ASSERT_EQUALS(std::string("Play"), play_msg->getMember(), "Should have correct member");
    ASSERT_TRUE(play_msg->isValid(), "Play message should be valid");
    
    // Test seek method call with arguments
    auto seek_msg = MockDBusMessageFactory::createSeekMethodCall(5000000); // 5 seconds
    ASSERT_NOT_NULL(seek_msg.get(), "Seek message should be created");
    ASSERT_EQUALS(std::string("Seek"), seek_msg->getMember(), "Should have correct member");
    auto int64_args = seek_msg->getInt64Arguments();
    ASSERT_EQUALS(1u, int64_args.size(), "Should have one int64 argument");
    ASSERT_EQUALS(5000000, int64_args[0], "Should have correct seek offset");
    
    // Test signal creation
    std::map<std::string, std::string> properties = {
        {"PlaybackStatus", "Playing"},
        {"Position", "30000000"}
    };
    auto signal_msg = MockDBusMessageFactory::createPropertiesChangedSignal(
        "org.mpris.MediaPlayer2.Player", properties);
    ASSERT_NOT_NULL(signal_msg.get(), "Signal message should be created");
    ASSERT_EQUALS(MockDBusMessage::Type::Signal, signal_msg->getType(), "Should be signal type");
    
    // Test error response
    auto error_msg = MockDBusMessageFactory::createErrorResponse(
        "org.mpris.MediaPlayer2.Player.Error", "Test error message");
    ASSERT_NOT_NULL(error_msg.get(), "Error message should be created");
    ASSERT_EQUALS(MockDBusMessage::Type::Error, error_msg->getType(), "Should be error type");
    ASSERT_EQUALS(std::string("org.mpris.MediaPlayer2.Player.Error"), error_msg->getErrorName(), "Should have correct error name");
    
    std::cout << "✓ Message factory test passed" << std::endl;
    return true;
}

/**
 * Test comprehensive integration
 */
bool test_comprehensive_integration() {
    std::cout << "Testing comprehensive integration..." << std::endl;
    
    // Create mock components
    auto player = MockPlayerFactory::createBasicPlayer();
    MockDBusConnection::Config dbus_config;
    dbus_config.enable_message_logging = false; // Reduce noise
    MockDBusConnection dbus_connection(dbus_config);
    
    // Set up callbacks to test integration
    bool state_change_called = false;
    bool position_change_called = false;
    
    player->setStateChangeCallback([&](PlayerState old_state, PlayerState new_state) {
        state_change_called = true;
    });
    
    player->setPositionChangeCallback([&](uint64_t old_pos, uint64_t new_pos) {
        position_change_called = true;
    });
    
    // Test D-Bus connection
    ASSERT_TRUE(dbus_connection.connect(), "D-Bus connection should succeed");
    ASSERT_TRUE(dbus_connection.requestName("org.mpris.MediaPlayer2.test"), "Service name request should succeed");
    
    // Test player operations
    ASSERT_TRUE(player->play(), "Player play should succeed");
    ASSERT_TRUE(state_change_called, "State change callback should be called");
    
    player->seekTo(5000000); // 5 seconds
    ASSERT_TRUE(position_change_called, "Position change callback should be called");
    
    // Test D-Bus message handling
    auto play_message = MockDBusMessageFactory::createPlayMethodCall();
    ASSERT_TRUE(dbus_connection.sendMessage(std::move(play_message)), "D-Bus message send should succeed");
    
    // Verify statistics
    auto player_stats = player->getStatistics();
    ASSERT_TRUE(player_stats.play_calls > 0, "Player should have play calls recorded");
    ASSERT_TRUE(player_stats.seek_calls > 0, "Player should have seek calls recorded");
    
    auto dbus_stats = dbus_connection.getStatistics();
    ASSERT_TRUE(dbus_stats.messages_sent > 0, "D-Bus should have messages sent recorded");
    
    std::cout << "✓ Comprehensive integration test passed" << std::endl;
    return true;
}

/**
 * Main test runner
 */
int main() {
    std::cout << "Running Mock Framework Standalone Tests..." << std::endl;
    std::cout << "=========================================" << std::endl << std::endl;
    
    bool all_passed = true;
    
    try {
        all_passed &= test_mock_dbus_connection_basic();
        all_passed &= test_mock_player_basic();
        all_passed &= test_threading_utilities();
        all_passed &= test_performance_benchmarking();
        all_passed &= test_error_simulation();
        all_passed &= test_message_factory();
        all_passed &= test_comprehensive_integration();
        
    } catch (const std::exception& e) {
        std::cout << "✗ Test failed with exception: " << e.what() << std::endl;
        all_passed = false;
    }
    
    std::cout << std::endl;
    if (all_passed) {
        std::cout << "✓ All mock framework standalone tests PASSED!" << std::endl;
        std::cout << "The MPRIS mock framework core components are working correctly." << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some mock framework standalone tests FAILED!" << std::endl;
        return 1;
    }
}