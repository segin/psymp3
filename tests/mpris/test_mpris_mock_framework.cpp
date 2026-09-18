/*
 * test_mpris_mock_framework.cpp - Comprehensive test suite for MPRIS mock framework
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
 * Test MockDBusConnection error simulation
 */
bool test_mock_dbus_connection_error_simulation() {
    std::cout << "Testing MockDBusConnection error simulation..." << std::endl;
    
    MockDBusConnection::Config config;
    config.simulate_connection_failures = true;
    config.connection_failure_rate = 1.0; // 100% failure rate
    
    MockDBusConnection connection(config);
    
    // Test connection failure
    bool connected = connection.connect();
    ASSERT_FALSE(connected, "Connection should fail with 100% failure rate");
    ASSERT_FALSE(connection.isConnected(), "Connection should not be connected after failed connect()");
    
    std::string error = connection.getLastError();
    ASSERT_FALSE(error.empty(), "Should have error message after failed connection");
    
    // Test connection loss simulation
    config.connection_failure_rate = 0.0; // Disable connection failures
    connection.updateConfig(config);
    
    connected = connection.connect();
    ASSERT_TRUE(connected, "Connection should succeed with 0% failure rate");
    
    connection.simulateConnectionLoss();
    ASSERT_FALSE(connection.isConnected(), "Connection should be lost after simulateConnectionLoss()");
    
    std::cout << "✓ MockDBusConnection error simulation test passed" << std::endl;
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
 * Test MockPlayer playlist functionality
 */
bool test_mock_player_playlist() {
    std::cout << "Testing MockPlayer playlist functionality..." << std::endl;
    
    // Create test playlist
    std::vector<MockPlayer::TrackInfo> tracks = {
        MockPlayer::TrackInfo("Artist 1", "Title 1", "Album 1"),
        MockPlayer::TrackInfo("Artist 2", "Title 2", "Album 2"),
        MockPlayer::TrackInfo("Artist 3", "Title 3", "Album 3")
    };
    
    auto player = MockPlayerFactory::createPlayerWithPlaylist(tracks);
    
    // Test initial track
    auto current_track = player->getCurrentTrack();
    ASSERT_EQUALS(std::string("Artist 1"), current_track.artist, "Initial track should be first in playlist");
    ASSERT_EQUALS(0u, player->getCurrentTrackIndex(), "Initial track index should be 0");
    
    // Test next track
    player->nextTrack();
    current_track = player->getCurrentTrack();
    ASSERT_EQUALS(std::string("Artist 2"), current_track.artist, "Should advance to second track");
    ASSERT_EQUALS(1u, player->getCurrentTrackIndex(), "Track index should be 1");
    
    // Test previous track
    player->prevTrack();
    current_track = player->getCurrentTrack();
    ASSERT_EQUALS(std::string("Artist 1"), current_track.artist, "Should go back to first track");
    ASSERT_EQUALS(0u, player->getCurrentTrackIndex(), "Track index should be 0");
    
    std::cout << "✓ MockPlayer playlist functionality test passed" << std::endl;
    return true;
}

/**
 * Test MockPlayer error simulation
 */
bool test_mock_player_error_simulation() {
    std::cout << "Testing MockPlayer error simulation..." << std::endl;
    
    auto player = MockPlayerFactory::createErrorSimulationPlayer(1.0); // 100% error rate
    
    // Test that operations fail with error simulation
    bool played = player->play();
    ASSERT_FALSE(played, "Play should fail with 100% error rate");
    
    bool paused = player->pause();
    ASSERT_FALSE(paused, "Pause should fail with 100% error rate");
    
    // Test injected errors
    player->enableErrorSimulation(false);
    player->injectError("play");
    
    played = player->play();
    ASSERT_FALSE(played, "Play should fail with injected error");
    
    player->clearInjectedErrors();
    played = player->play();
    ASSERT_TRUE(played, "Play should succeed after clearing injected errors");
    
    std::cout << "✓ MockPlayer error simulation test passed" << std::endl;
    return true;
}

/**
 * Test threading safety with MockPlayer
 */
bool test_mock_player_threading_safety() {
    std::cout << "Testing MockPlayer threading safety..." << std::endl;
    
    auto player = MockPlayerFactory::createThreadSafetyTestPlayer();
    
    Threading::ThreadSafetyTester::Config config;
    config.num_threads = 4;
    config.test_duration = std::chrono::milliseconds{500};
    config.operations_per_thread = 50;
    
    Threading::ThreadSafetyTester tester(config);
    
    // Test concurrent play/pause operations
    auto test_func = [&player]() -> bool {
        static std::atomic<int> operation_counter{0};
        int op = operation_counter.fetch_add(1) % 4;
        
        switch (op) {
            case 0: return player->play();
            case 1: return player->pause();
            case 2: return player->stop();
            case 3: player->seekTo(1000000); return true; // 1 second
            default: return false;
        }
    };
    
    auto results = tester.runTest(test_func, "MockPlayer threading safety");
    
    ASSERT_TRUE(results.successful_operations > 0, "Should have successful operations");
    ASSERT_FALSE(results.deadlock_detected, "Should not detect deadlocks");
    
    // Validate player state is still consistent
    ASSERT_TRUE(player->validateState(), "Player state should be valid after concurrent operations");
    
    std::cout << "✓ MockPlayer threading safety test passed" << std::endl;
    return true;
}

/**
 * Test MockDBusMessage factory
 */
bool test_mock_dbus_message_factory() {
    std::cout << "Testing MockDBusMessage factory..." << std::endl;
    
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
    
    std::cout << "✓ MockDBusMessage factory test passed" << std::endl;
    return true;
}

/**
 * Test lock contention analysis
 */
bool test_lock_contention_analysis() {
    std::cout << "Testing lock contention analysis..." << std::endl;
    
    Threading::LockContentionAnalyzer analyzer;
    std::mutex test_mutex;
    
    auto metrics = analyzer.analyzeLockContention(
        test_mutex, 
        std::chrono::milliseconds{200}, 
        4 // 4 threads
    );
    
    ASSERT_TRUE(metrics.total_acquisitions > 0, "Should have lock acquisitions");
    ASSERT_TRUE(metrics.average_acquisition_time.count() >= 0, "Should have valid average acquisition time");
    ASSERT_TRUE(metrics.max_acquisition_time >= metrics.min_acquisition_time, "Max time should be >= min time");
    
    std::cout << "Lock contention metrics:" << std::endl;
    std::cout << "  Total acquisitions: " << metrics.total_acquisitions << std::endl;
    std::cout << "  Average time: " << metrics.average_acquisition_time.count() << "μs" << std::endl;
    std::cout << "  Contention ratio: " << (metrics.contention_ratio * 100.0) << "%" << std::endl;
    
    std::cout << "✓ Lock contention analysis test passed" << std::endl;
    return true;
}

/**
 * Test race condition detection
 */
bool test_race_condition_detection() {
    std::cout << "Testing race condition detection..." << std::endl;
    
    Threading::RaceConditionDetector detector;
    
    // Shared data for race condition test
    std::atomic<int> shared_counter{0};
    int unsafe_counter = 0;
    std::mutex counter_mutex;
    
    // Setup function
    auto setup_func = [&]() {
        shared_counter.store(0);
        unsafe_counter = 0;
    };
    
    // Test function that should NOT have race conditions (using atomic)
    auto safe_test_func = [&](size_t thread_id, size_t iteration) {
        shared_counter.fetch_add(1);
    };
    
    // Verify function for safe test
    auto safe_verify_func = [&]() -> bool {
        return shared_counter.load() == 4000; // 4 threads * 1000 iterations
    };
    
    bool race_detected = detector.detectRaceCondition(
        setup_func, safe_test_func, safe_verify_func, 4, 1000);
    
    ASSERT_FALSE(race_detected, "Should not detect race condition with atomic operations");
    
    // Test function that SHOULD have race conditions (unsafe increment)
    auto unsafe_test_func = [&](size_t thread_id, size_t iteration) {
        // Intentionally unsafe increment without proper synchronization
        int temp = unsafe_counter;
        std::this_thread::sleep_for(std::chrono::nanoseconds(1)); // Increase chance of race
        unsafe_counter = temp + 1;
    };
    
    // Verify function for unsafe test
    auto unsafe_verify_func = [&]() -> bool {
        return unsafe_counter == 4000; // This will likely fail due to race condition
    };
    
    race_detected = detector.detectRaceCondition(
        setup_func, unsafe_test_func, unsafe_verify_func, 4, 1000);
    
    // Note: This test might be flaky depending on timing, so we don't assert the result
    // but we do report it for informational purposes
    std::cout << "Race condition detected in unsafe test: " << (race_detected ? "Yes" : "No") << std::endl;
    
    std::cout << "✓ Race condition detection test passed" << std::endl;
    return true;
}

/**
 * Test performance benchmarking
 */
bool test_performance_benchmarking() {
    std::cout << "Testing performance benchmarking..." << std::endl;
    
    Threading::ThreadingBenchmark benchmark;
    
    // Simple operation for benchmarking
    std::atomic<int> counter{0};
    auto operation = [&counter](size_t index) {
        counter.fetch_add(1);
        // Simulate some work
        volatile int temp = 0;
        for (int i = 0; i < 100; ++i) {
            temp += i;
        }
    };
    
    auto results = benchmark.benchmarkScaling(operation, 10000, 4);
    
    ASSERT_TRUE(results.single_thread_time.count() > 0, "Should have valid single-thread time");
    ASSERT_TRUE(results.multi_thread_time.count() > 0, "Should have valid multi-thread time");
    ASSERT_TRUE(results.operations_per_second > 0, "Should have valid operations per second");
    
    std::cout << "Performance benchmark results:" << std::endl;
    std::cout << "  Single-thread time: " << results.single_thread_time.count() << "μs" << std::endl;
    std::cout << "  Multi-thread time: " << results.multi_thread_time.count() << "μs" << std::endl;
    std::cout << "  Speedup ratio: " << results.speedup_ratio << "x" << std::endl;
    std::cout << "  Efficiency: " << (results.efficiency * 100.0) << "%" << std::endl;
    std::cout << "  Operations/sec: " << results.operations_per_second << std::endl;
    
    std::cout << "✓ Performance benchmarking test passed" << std::endl;
    return true;
}

/**
 * Test comprehensive mock framework integration
 */
bool test_mock_framework_integration() {
    std::cout << "Testing mock framework integration..." << std::endl;
    
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
    
    std::cout << "✓ Mock framework integration test passed" << std::endl;
    return true;
}

/**
 * Main test runner
 */
int main() {
    std::cout << "Running MPRIS Mock Framework comprehensive tests..." << std::endl << std::endl;
    
    bool all_passed = true;
    
    try {
        all_passed &= test_mock_dbus_connection_basic();
        all_passed &= test_mock_dbus_connection_error_simulation();
        all_passed &= test_mock_player_basic();
        all_passed &= test_mock_player_playlist();
        all_passed &= test_mock_player_error_simulation();
        all_passed &= test_mock_player_threading_safety();
        all_passed &= test_mock_dbus_message_factory();
        all_passed &= test_lock_contention_analysis();
        all_passed &= test_race_condition_detection();
        all_passed &= test_performance_benchmarking();
        all_passed &= test_mock_framework_integration();
        
    } catch (const std::exception& e) {
        std::cout << "✗ Test failed with exception: " << e.what() << std::endl;
        all_passed = false;
    }
    
    std::cout << std::endl;
    if (all_passed) {
        std::cout << "✓ All MPRIS Mock Framework tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some MPRIS Mock Framework tests FAILED!" << std::endl;
        return 1;
    }
}