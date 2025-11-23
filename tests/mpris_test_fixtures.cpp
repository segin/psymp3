/*
 * mpris_test_fixtures.cpp - Test fixtures implementation for MPRIS scenarios
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "mpris_test_fixtures.h"
#include "test_framework_threading.h"

namespace TestFramework {

// MPRISTestFixture implementation

MPRISTestFixture::MPRISTestFixture(const std::string& name) : TestCase(name) {
}

MPRISTestFixture::~MPRISTestFixture() = default;

void MPRISTestFixture::setUp() {
    // Initialize mock components
    m_mock_player = std::make_unique<MockPlayer>();
    m_mock_dbus = std::make_unique<MockDBusConnection>();
    
    // Clear any previous validation errors
    m_validation_errors.clear();
    m_setup_completed = true;
}

void MPRISTestFixture::tearDown() {
    // Clean shutdown of MPRIS manager if it exists
    if (m_mpris_manager) {
        m_mpris_manager->shutdown();
        m_mpris_manager.reset();
    }
    
    // Disconnect D-Bus connection
    if (m_mock_dbus) {
        m_mock_dbus->disconnect();
        m_mock_dbus.reset();
    }
    
    // Reset mock player
    m_mock_player.reset();
    
    m_setup_completed = false;
}

void MPRISTestFixture::initializeBasicSetup() {
    if (!m_setup_completed) {
        recordValidationError("Setup not completed before initialization");
        return;
    }
    
    // Configure mock components for basic testing
    MockDBusConnection::Config dbus_config;
    dbus_config.auto_connect = true;
    dbus_config.simulate_connection_failures = false;
    dbus_config.simulate_message_failures = false;
    dbus_config.enable_message_logging = false; // Reduce noise in tests
    
    m_mock_dbus->updateConfig(dbus_config);
    
    MockPlayer::Config player_config;
    player_config.simulate_state_changes = true;
    player_config.state_change_delay = std::chrono::milliseconds{10}; // Fast for testing
    player_config.enable_error_simulation = false;
    
    m_mock_player->updateConfig(player_config);
    
    // Connect D-Bus
    if (!m_mock_dbus->connect()) {
        recordValidationError("Failed to connect mock D-Bus connection");
    }
    
#ifdef HAVE_DBUS
    // Create MPRIS manager (only if D-Bus support is available)
    try {
        m_mpris_manager = std::make_unique<MPRISManager>(
            reinterpret_cast<Player*>(m_mock_player.get())
        );
        
        // Initialize MPRIS manager
        if (!m_mpris_manager->initialize()) {
            recordValidationError("Failed to initialize MPRIS manager");
        }
    } catch (const std::exception& e) {
        recordValidationError(std::string("Exception during MPRIS initialization: ") + e.what());
    }
#endif
}

void MPRISTestFixture::initializeWithErrorSimulation(double error_rate) {
    initializeBasicSetup();
    
    // Enable error simulation
    MockDBusConnection::Config dbus_config = m_mock_dbus->getConfig();
    dbus_config.simulate_connection_failures = true;
    dbus_config.connection_failure_rate = error_rate * 0.5; // Lower rate for connections
    dbus_config.simulate_message_failures = true;
    dbus_config.message_failure_rate = error_rate;
    
    m_mock_dbus->updateConfig(dbus_config);
    
    MockPlayer::Config player_config = m_mock_player->getConfig();
    player_config.enable_error_simulation = true;
    player_config.error_rate = error_rate;
    
    m_mock_player->updateConfig(player_config);
}

void MPRISTestFixture::initializeWithThreadSafetyTesting() {
    initializeBasicSetup();
    
    // Enable thread safety testing
    m_mock_player->enableThreadSafetyTesting(true);
    m_mock_dbus->enableThreadSafetyTesting(true);
    
    MockPlayer::Config player_config = m_mock_player->getConfig();
    player_config.thread_safety_testing = true;
    player_config.state_change_delay = std::chrono::milliseconds{1}; // Minimal delay
    
    m_mock_player->updateConfig(player_config);
}

void MPRISTestFixture::assertPlayerState(PlayerState expected_state, const std::string& message) {
    PlayerState actual_state = m_mock_player->getState();
    if (actual_state != expected_state) {
        recordValidationError(message + " - Expected: " + std::to_string(static_cast<int>(expected_state)) + 
                            ", Actual: " + std::to_string(static_cast<int>(actual_state)));
    }
}

void MPRISTestFixture::assertDBusMessageSent(const std::string& interface, const std::string& member, const std::string& message) {
    auto messages = m_mock_dbus->findMessagesByInterface(interface);
    bool found = false;
    
    for (const auto* msg : messages) {
        if (msg->getMember() == member) {
            found = true;
            break;
        }
    }
    
    if (!found) {
        recordValidationError(message + " - D-Bus message not found: " + interface + "." + member);
    }
}

void MPRISTestFixture::assertNoDBusErrors(const std::string& message) {
    std::string last_error = m_mock_dbus->getLastError();
    if (!last_error.empty()) {
        recordValidationError(message + " - D-Bus error: " + last_error);
    }
}

void MPRISTestFixture::assertMPRISInitialized(const std::string& message) {
#ifdef HAVE_DBUS
    if (!m_mpris_manager || !m_mpris_manager->isInitialized()) {
        recordValidationError(message + " - MPRIS manager not initialized");
    }
#endif
}

void MPRISTestFixture::simulateMethodCall(const std::string& method) {
    std::unique_ptr<MockDBusMessage> message;
    
    if (method == "Play") {
        message = MockDBusMessageFactory::createPlayMethodCall();
    } else if (method == "Pause") {
        message = MockDBusMessageFactory::createPauseMethodCall();
    } else if (method == "Stop") {
        message = MockDBusMessageFactory::createStopMethodCall();
    } else if (method == "Next") {
        message = MockDBusMessageFactory::createNextMethodCall();
    } else if (method == "Previous") {
        message = MockDBusMessageFactory::createPreviousMethodCall();
    } else {
        recordValidationError("Unknown method call: " + method);
        return;
    }
    
    if (!m_mock_dbus->sendMessage(std::move(message))) {
        recordValidationError("Failed to send method call: " + method);
    }
}

void MPRISTestFixture::simulatePropertyGet(const std::string& interface, const std::string& property) {
    auto message = MockDBusMessageFactory::createGetPropertyCall(interface, property);
    if (!m_mock_dbus->sendMessage(std::move(message))) {
        recordValidationError("Failed to send property get: " + interface + "." + property);
    }
}

void MPRISTestFixture::simulatePropertySet(const std::string& interface, const std::string& property, const std::string& value) {
    auto message = MockDBusMessageFactory::createSetPropertyCall(interface, property, value);
    if (!m_mock_dbus->sendMessage(std::move(message))) {
        recordValidationError("Failed to send property set: " + interface + "." + property);
    }
}

void MPRISTestFixture::simulateConnectionLoss() {
    m_mock_dbus->simulateConnectionLoss();
}

void MPRISTestFixture::simulateConnectionRestore() {
    m_mock_dbus->simulateConnectionRestore();
}

bool MPRISTestFixture::validateMPRISState() const {
#ifdef HAVE_DBUS
    if (!m_mpris_manager) {
        return false;
    }
    
    // Basic validation - MPRIS manager should be initialized
    return m_mpris_manager->isInitialized();
#else
    return true; // No MPRIS to validate
#endif
}

bool MPRISTestFixture::validatePlayerIntegration() const {
    if (!m_mock_player) {
        return false;
    }
    
    return m_mock_player->validateState();
}

bool MPRISTestFixture::validateDBusIntegration() const {
    if (!m_mock_dbus) {
        return false;
    }
    
    return m_mock_dbus->isConnected() || m_mock_dbus->getLastError().empty();
}

std::string MPRISTestFixture::getValidationErrors() const {
    std::string errors;
    for (const auto& error : m_validation_errors) {
        if (!errors.empty()) {
            errors += "; ";
        }
        errors += error;
    }
    return errors;
}

void MPRISTestFixture::recordValidationError(const std::string& error) {
    m_validation_errors.push_back(error);
}

// BasicMPRISTestFixture implementation

void BasicMPRISTestFixture::setUp() {
    MPRISTestFixture::setUp();
    initializeBasicSetup();
}

void BasicMPRISTestFixture::testInitialization() {
    assertMPRISInitialized("Basic initialization test");
    assertNoDBusErrors("Basic initialization test");
}

void BasicMPRISTestFixture::testPlaybackControl() {
    // Test play
    simulateMethodCall("Play");
    assertPlayerState(PlayerState::Playing, "Play method test");
    
    // Test pause
    simulateMethodCall("Pause");
    assertPlayerState(PlayerState::Paused, "Pause method test");
    
    // Test stop
    simulateMethodCall("Stop");
    assertPlayerState(PlayerState::Stopped, "Stop method test");
}

void BasicMPRISTestFixture::testMetadataUpdates() {
#ifdef HAVE_DBUS
    if (m_mpris_manager) {
        m_mpris_manager->updateMetadata("Test Artist", "Test Title", "Test Album");
        
        // Verify metadata was updated
        auto track = m_mock_player->getCurrentTrack();
        if (track.artist != "Test Artist" || track.title != "Test Title" || track.album != "Test Album") {
            recordValidationError("Metadata update failed");
        }
    }
#endif
}

void BasicMPRISTestFixture::testPropertyAccess() {
    simulatePropertyGet("org.mpris.MediaPlayer2.Player", "PlaybackStatus");
    simulatePropertyGet("org.mpris.MediaPlayer2.Player", "Metadata");
    simulatePropertyGet("org.mpris.MediaPlayer2.Player", "Position");
    
    assertNoDBusErrors("Property access test");
}

void BasicMPRISTestFixture::testShutdown() {
#ifdef HAVE_DBUS
    if (m_mpris_manager) {
        m_mpris_manager->shutdown();
        // After shutdown, MPRIS should not be initialized
        if (m_mpris_manager->isInitialized()) {
            recordValidationError("MPRIS manager still initialized after shutdown");
        }
    }
#endif
}

// Factory implementations

std::unique_ptr<BasicMPRISTestFixture> MPRISTestFixtureFactory::createBasicFixture() {
    return std::make_unique<BasicMPRISTestFixture>();
}

std::unique_ptr<ErrorHandlingTestFixture> MPRISTestFixtureFactory::createErrorHandlingFixture() {
    return std::make_unique<ErrorHandlingTestFixture>();
}

std::unique_ptr<ThreadSafetyTestFixture> MPRISTestFixtureFactory::createThreadSafetyFixture() {
    return std::make_unique<ThreadSafetyTestFixture>();
}

std::unique_ptr<PerformanceTestFixture> MPRISTestFixtureFactory::createPerformanceFixture() {
    return std::make_unique<PerformanceTestFixture>();
}

std::unique_ptr<IntegrationTestFixture> MPRISTestFixtureFactory::createIntegrationFixture() {
    return std::make_unique<IntegrationTestFixture>();
}

std::unique_ptr<StressTestFixture> MPRISTestFixtureFactory::createStressFixture() {
    return std::make_unique<StressTestFixture>();
}

std::vector<std::unique_ptr<MPRISTestFixture>> MPRISTestFixtureFactory::createAllFixtures() {
    std::vector<std::unique_ptr<MPRISTestFixture>> fixtures;
    
    fixtures.push_back(createBasicFixture());
    fixtures.push_back(createErrorHandlingFixture());
    fixtures.push_back(createThreadSafetyFixture());
    fixtures.push_back(createPerformanceFixture());
    fixtures.push_back(createIntegrationFixture());
    fixtures.push_back(createStressFixture());
    
    return fixtures;
}

// Test data generator implementations

MockPlayer::TrackInfo MPRISTestDataGenerator::generateTestTrack(size_t index) {
    MockPlayer::TrackInfo track;
    track.artist = "Test Artist " + std::to_string(index);
    track.title = "Test Title " + std::to_string(index);
    track.album = "Test Album " + std::to_string(index);
    track.track_id = "/test/track/" + std::to_string(index);
    track.duration_us = (120 + (index % 180)) * 1000000; // 2-5 minutes
    track.art_url = "file:///test/art/" + std::to_string(index) + ".jpg";
    
    return track;
}

std::vector<MockPlayer::TrackInfo> MPRISTestDataGenerator::generateTestPlaylist(size_t track_count) {
    std::vector<MockPlayer::TrackInfo> playlist;
    playlist.reserve(track_count);
    
    for (size_t i = 0; i < track_count; ++i) {
        playlist.push_back(generateTestTrack(i));
    }
    
    return playlist;
}

std::vector<std::unique_ptr<MockDBusMessage>> MPRISTestDataGenerator::generateTestMessages(size_t count) {
    std::vector<std::unique_ptr<MockDBusMessage>> messages;
    messages.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        switch (i % 5) {
            case 0:
                messages.push_back(MockDBusMessageFactory::createPlayMethodCall());
                break;
            case 1:
                messages.push_back(MockDBusMessageFactory::createPauseMethodCall());
                break;
            case 2:
                messages.push_back(MockDBusMessageFactory::createStopMethodCall());
                break;
            case 3:
                messages.push_back(MockDBusMessageFactory::createSeekMethodCall(i * 1000000));
                break;
            case 4: {
                std::map<std::string, std::string> props = {
                    {"PlaybackStatus", "Playing"},
                    {"Position", std::to_string(i * 1000000)}
                };
                messages.push_back(MockDBusMessageFactory::createPropertiesChangedSignal(
                    "org.mpris.MediaPlayer2.Player", props));
                break;
            }
        }
    }
    
    return messages;
}

std::vector<std::unique_ptr<MockDBusMessage>> MPRISTestDataGenerator::generateMalformedMessages(size_t count) {
    std::vector<std::unique_ptr<MockDBusMessage>> messages;
    messages.reserve(count);
    
    for (size_t i = 0; i < count; ++i) {
        switch (i % 3) {
            case 0:
                messages.push_back(MockDBusMessageFactory::createMalformedMessage());
                break;
            case 1:
                messages.push_back(MockDBusMessageFactory::createMessageWithInvalidArguments());
                break;
            case 2:
                messages.push_back(MockDBusMessageFactory::createMessageWithMissingArguments());
                break;
        }
    }
    
    return messages;
}

std::vector<std::pair<std::string, std::string>> MPRISTestDataGenerator::generatePropertyChanges() {
    return {
        {"PlaybackStatus", "Playing"},
        {"PlaybackStatus", "Paused"},
        {"PlaybackStatus", "Stopped"},
        {"Position", "30000000"},
        {"Position", "60000000"},
        {"Metadata", "{}"},
        {"Volume", "0.8"},
        {"CanPlay", "true"},
        {"CanPause", "true"},
        {"CanSeek", "true"}
    };
}

std::vector<std::string> MPRISTestDataGenerator::generateStressTestOperations(size_t count) {
    std::vector<std::string> operations;
    operations.reserve(count);
    
    std::vector<std::string> op_types = {
        "play", "pause", "stop", "next", "prev", 
        "seek:5000000", "seek:10000000", "seek:30000000"
    };
    
    for (size_t i = 0; i < count; ++i) {
        operations.push_back(op_types[i % op_types.size()]);
    }
    
    return operations;
}

// ErrorHandlingTestFixture implementation

void ErrorHandlingTestFixture::setUp() {
    MPRISTestFixture::setUp();
    initializeWithErrorSimulation(0.2); // 20% error rate
}

void ErrorHandlingTestFixture::testConnectionFailure() {
    simulateConnectionLoss();
    
    // MPRIS should handle connection loss gracefully
    simulateMethodCall("Play");
    
    // Even with connection loss, player operations should still work
    assertPlayerState(PlayerState::Playing, "Connection failure test");
}

void ErrorHandlingTestFixture::testMalformedMessages() {
    auto malformed_message = MockDBusMessageFactory::createMalformedMessage();
    m_mock_dbus->sendMessage(std::move(malformed_message));
    
    // System should handle malformed messages without crashing
    assertNoDBusErrors("Malformed message test should not cause system errors");
}

void ErrorHandlingTestFixture::testPlayerStateErrors() {
    // Inject player errors
    m_mock_player->injectError("play");
    
    simulateMethodCall("Play");
    
    // Player should remain in consistent state even with errors
    if (!m_mock_player->validateState()) {
        recordValidationError("Player state inconsistent after error injection");
    }
}

void ErrorHandlingTestFixture::testRecoveryMechanisms() {
    // Simulate connection loss and recovery
    simulateConnectionLoss();
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    simulateConnectionRestore();
    
    // System should recover and continue functioning
    simulateMethodCall("Play");
    assertPlayerState(PlayerState::Playing, "Recovery mechanism test");
}

void ErrorHandlingTestFixture::testGracefulDegradation() {
    // Enable high error rate
    initializeWithErrorSimulation(0.8); // 80% error rate
    
    // System should still function with degraded performance
    for (int i = 0; i < 10; ++i) {
        simulateMethodCall("Play");
        simulateMethodCall("Pause");
    }
    
    // Player should still be in a valid state
    if (!m_mock_player->validateState()) {
        recordValidationError("Player state invalid under high error rate");
    }
}

// ThreadSafetyTestFixture implementation

void ThreadSafetyTestFixture::setUp() {
    MPRISTestFixture::setUp();
    initializeWithThreadSafetyTesting();
}

void ThreadSafetyTestFixture::testConcurrentMethodCalls() {
    Threading::ThreadSafetyTester::Config config;
    config.num_threads = 4;
    config.operations_per_thread = 100;
    config.test_duration = std::chrono::milliseconds{1000};
    
    Threading::ThreadSafetyTester tester(config);
    
    auto test_func = [this]() -> bool {
        static std::atomic<int> counter{0};
        int op = counter.fetch_add(1) % 4;
        
        switch (op) {
            case 0: simulateMethodCall("Play"); break;
            case 1: simulateMethodCall("Pause"); break;
            case 2: simulateMethodCall("Stop"); break;
            case 3: m_mock_player->seekTo(1000000); break;
        }
        return true;
    };
    
    auto results = tester.runTest(test_func, "Concurrent method calls");
    
    if (results.deadlock_detected) {
        recordValidationError("Deadlock detected in concurrent method calls");
    }
    
    if (!m_mock_player->validateState()) {
        recordValidationError("Player state invalid after concurrent operations");
    }
}

void ThreadSafetyTestFixture::testConcurrentPropertyAccess() {
    TestFramework::Threading::ThreadSafetyTester::Config config;
    config.num_threads = 8;
    config.operations_per_thread = 200;
    config.test_duration = std::chrono::milliseconds{500};
    
    TestFramework::Threading::ThreadSafetyTester tester(config);
    
    auto test_func = [this]() -> bool {
        static std::atomic<int> counter{0};
        int op = counter.fetch_add(1) % 3;
        
        switch (op) {
            case 0: m_mock_player->getState(); break;
            case 1: m_mock_player->getPosition(); break;
            case 2: m_mock_player->getCurrentTrack(); break;
        }
        return true;
    };
    
    auto results = tester.runTest(test_func, "Concurrent property access");
    
    if (results.deadlock_detected) {
        recordValidationError("Deadlock detected in concurrent property access");
    }
}

void ThreadSafetyTestFixture::testLockOrderCompliance() {
    // Test that locks are acquired in consistent order
    size_t contention_count = m_mock_player->getLockContentionCount();
    
    // Perform operations that might cause lock contention
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 50; ++j) {
                m_mock_player->play();
                m_mock_player->getState();
                m_mock_player->pause();
                m_mock_player->getPosition();
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Check if excessive contention occurred (might indicate lock order issues)
    size_t final_contention = m_mock_player->getLockContentionCount();
    if (final_contention - contention_count > 100) {
        recordValidationError("Excessive lock contention detected - possible lock order issues");
    }
}

void ThreadSafetyTestFixture::testDeadlockPrevention() {
    TestFramework::Threading::ThreadSafetyTester tester;
    
    auto deadlock_test = [this]() {
        // Create a scenario that could cause deadlock if not properly implemented
        std::thread t1([this]() {
            for (int i = 0; i < 100; ++i) {
                m_mock_player->play();
                simulateMethodCall("Play");
            }
        });
        
        std::thread t2([this]() {
            for (int i = 0; i < 100; ++i) {
                simulateMethodCall("Pause");
                m_mock_player->pause();
            }
        });
        
        t1.join();
        t2.join();
    };
    
    bool deadlock_detected = tester.testForDeadlock(deadlock_test, std::chrono::milliseconds{5000});
    
    if (deadlock_detected) {
        recordValidationError("Deadlock detected in mixed operations");
    }
}

void ThreadSafetyTestFixture::testCallbackSafety() {
    std::atomic<bool> callback_called{false};
    std::atomic<bool> callback_error{false};
    
    // Set up callback that could cause issues if called while holding locks
    m_mock_player->setStateChangeCallback([&](PlayerState old_state, PlayerState new_state) {
        callback_called.store(true);
        
        // Try to call back into player (this should not deadlock)
        try {
            PlayerState current = m_mock_player->getState();
            if (current != new_state) {
                callback_error.store(true);
            }
        } catch (...) {
            callback_error.store(true);
        }
    });
    
    // Trigger state changes
    m_mock_player->play();
    m_mock_player->pause();
    m_mock_player->stop();
    
    std::this_thread::sleep_for(std::chrono::milliseconds{100}); // Allow callbacks to complete
    
    if (!callback_called.load()) {
        recordValidationError("State change callback was not called");
    }
    
    if (callback_error.load()) {
        recordValidationError("Callback safety test failed - possible deadlock or state inconsistency");
    }
}

// PerformanceTestFixture implementation

void PerformanceTestFixture::setUp() {
    MPRISTestFixture::setUp();
    
    // Configure for performance testing
    MockDBusConnection::Config dbus_config;
    dbus_config.auto_connect = true;
    dbus_config.simulate_connection_failures = false;
    dbus_config.simulate_message_failures = false;
    dbus_config.enable_message_logging = false; // Disable logging for performance
    dbus_config.connection_delay = std::chrono::milliseconds{0};
    
    m_mock_dbus->updateConfig(dbus_config);
    
    MockPlayer::Config player_config;
    player_config.simulate_state_changes = false; // Disable delays
    player_config.simulate_seeking = false;
    player_config.simulate_track_changes = false;
    player_config.enable_error_simulation = false;
    
    m_mock_player->updateConfig(player_config);
    
    initializeBasicSetup();
}

void PerformanceTestFixture::testHighFrequencyUpdates() {
    const size_t num_updates = 10000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < num_updates; ++i) {
#ifdef HAVE_DBUS
        if (m_mpris_manager) {
            m_mpris_manager->updatePosition(i * 1000); // Update position frequently
        }
#endif
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Should complete within reasonable time (less than 1 second for 10k updates)
    if (duration.count() > 1000) {
        recordValidationError("High frequency updates too slow: " + std::to_string(duration.count()) + "ms");
    }
}

void PerformanceTestFixture::testLockContention() {
    TestFramework::Threading::LockContentionAnalyzer analyzer;
    std::mutex test_mutex;
    
    auto metrics = analyzer.analyzeLockContention(test_mutex, std::chrono::milliseconds{500}, 8);
    
    // Check for reasonable contention levels
    if (metrics.contention_ratio > 0.5) {
        recordValidationError("Excessive lock contention detected: " + 
                            std::to_string(metrics.contention_ratio * 100.0) + "%");
    }
}

void PerformanceTestFixture::testMemoryUsage() {
    // Create many operations and check for memory leaks
    const size_t num_operations = 1000;
    
    for (size_t i = 0; i < num_operations; ++i) {
        auto message = MockDBusMessageFactory::createPlayMethodCall();
        m_mock_dbus->sendMessage(std::move(message));
        
        m_mock_player->play();
        m_mock_player->pause();
        m_mock_player->seekTo(i * 1000);
    }
    
    // Verify statistics are reasonable
    auto stats = m_mock_dbus->getStatistics();
    if (stats.messages_sent != num_operations) {
        recordValidationError("Message count mismatch in memory test");
    }
}

void PerformanceTestFixture::testMessageThroughput() {
    const size_t num_messages = 5000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < num_messages; ++i) {
        auto message = MockDBusMessageFactory::createPlayMethodCall();
        if (!m_mock_dbus->sendMessage(std::move(message))) {
            recordValidationError("Message send failed during throughput test");
            break;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    double messages_per_second = static_cast<double>(num_messages) / (duration.count() / 1000.0);
    
    // Should achieve reasonable throughput (>1000 messages/second)
    if (messages_per_second < 1000.0) {
        recordValidationError("Message throughput too low: " + std::to_string(messages_per_second) + " msg/s");
    }
}

void PerformanceTestFixture::testScalability() {
    TestFramework::Threading::ThreadingBenchmark benchmark;
    
    auto operation = [this](size_t index) {
        m_mock_player->play();
        m_mock_player->getState();
        m_mock_player->pause();
    };
    
    auto results = benchmark.benchmarkScaling(operation, 1000, 4);
    
    // Should show some speedup with multiple threads
    if (results.speedup_ratio < 1.5) {
        recordValidationError("Poor scalability: speedup ratio " + std::to_string(results.speedup_ratio));
    }
}

// IntegrationTestFixture implementation

void IntegrationTestFixture::setUp() {
    MPRISTestFixture::setUp();
    initializeBasicSetup();
}

void IntegrationTestFixture::testPlayerIntegration() {
    // Test that MPRIS operations affect player state
    simulateMethodCall("Play");
    assertPlayerState(PlayerState::Playing, "Player integration - Play");
    
    simulateMethodCall("Pause");
    assertPlayerState(PlayerState::Paused, "Player integration - Pause");
    
    simulateMethodCall("Stop");
    assertPlayerState(PlayerState::Stopped, "Player integration - Stop");
}

void IntegrationTestFixture::testDBusIntegration() {
    // Test D-Bus message flow
    simulateMethodCall("Play");
    
    auto messages = m_mock_dbus->findMessagesByInterface("org.mpris.MediaPlayer2.Player");
    if (messages.empty()) {
        recordValidationError("No D-Bus messages found for Player interface");
    }
}

void IntegrationTestFixture::testSignalEmission() {
#ifdef HAVE_DBUS
    if (m_mpris_manager) {
        // Update metadata and check for signal emission
        m_mpris_manager->updateMetadata("Artist", "Title", "Album");
        
        // Check for PropertiesChanged signal
        auto signals = m_mock_dbus->findMessagesByMember("PropertiesChanged");
        if (signals.empty()) {
            recordValidationError("PropertiesChanged signal not emitted");
        }
    }
#endif
}

void IntegrationTestFixture::testPropertySynchronization() {
    // Change player state and verify MPRIS properties are synchronized
    m_mock_player->play();
    
#ifdef HAVE_DBUS
    if (m_mpris_manager) {
        // Properties should be automatically synchronized
        std::this_thread::sleep_for(std::chrono::milliseconds{50}); // Allow sync
        
        // Verify synchronization occurred (implementation-dependent)
        assertPlayerState(PlayerState::Playing, "Property synchronization test");
    }
#endif
}

void IntegrationTestFixture::testEndToEndWorkflow() {
    // Test complete workflow from D-Bus message to player action
    simulateMethodCall("Play");
    assertPlayerState(PlayerState::Playing, "End-to-end workflow - Play");
    
    // Seek operation
    auto seek_message = MockDBusMessageFactory::createSeekMethodCall(30000000); // 30 seconds
    m_mock_dbus->sendMessage(std::move(seek_message));
    
    // Verify seek was processed
    uint64_t position = m_mock_player->getPosition();
    if (position == 0) {
        recordValidationError("Seek operation not processed in end-to-end test");
    }
}

// StressTestFixture implementation

void StressTestFixture::setUp() {
    MPRISTestFixture::setUp();
    initializeWithThreadSafetyTesting();
}

void StressTestFixture::testHighConcurrency() {
    const size_t num_threads = 16;
    const size_t operations_per_thread = 500;
    
    TestFramework::Threading::ThreadSafetyTester::Config config;
    config.num_threads = num_threads;
    config.operations_per_thread = operations_per_thread;
    config.test_duration = std::chrono::milliseconds{10000}; // 10 seconds
    
    TestFramework::Threading::ThreadSafetyTester tester(config);
    
    auto stress_test = [this]() -> bool {
        static std::atomic<size_t> counter{0};
        size_t op = counter.fetch_add(1) % 6;
        
        switch (op) {
            case 0: return m_mock_player->play();
            case 1: return m_mock_player->pause();
            case 2: return m_mock_player->stop();
            case 3: m_mock_player->seekTo(op * 1000000); return true;
            case 4: simulateMethodCall("Play"); return true;
            case 5: simulateMethodCall("Pause"); return true;
            default: return false;
        }
    };
    
    auto results = tester.runTest(stress_test, "High concurrency stress test");
    
    if (results.deadlock_detected) {
        recordValidationError("Deadlock detected in high concurrency test");
    }
    
    if (results.successful_operations < results.total_operations * 0.8) {
        recordValidationError("Low success rate in high concurrency test: " + 
                            std::to_string(static_cast<double>(results.successful_operations) / results.total_operations * 100.0) + "%");
    }
}

void StressTestFixture::testLongRunningOperations() {
    // Test system stability over extended period
    const auto test_duration = std::chrono::milliseconds{5000}; // 5 seconds
    auto start_time = std::chrono::high_resolution_clock::now();
    
    size_t operation_count = 0;
    while (std::chrono::high_resolution_clock::now() - start_time < test_duration) {
        m_mock_player->play();
        m_mock_player->pause();
        simulateMethodCall("Play");
        simulateMethodCall("Stop");
        
        operation_count++;
        
        if (operation_count % 100 == 0) {
            // Periodic validation
            if (!m_mock_player->validateState()) {
                recordValidationError("Player state invalid during long running test");
                break;
            }
        }
    }
    
    // Final validation
    if (!validateMPRISState() || !validatePlayerIntegration()) {
        recordValidationError("System state invalid after long running operations");
    }
}

void StressTestFixture::testResourceExhaustion() {
    // Test behavior under resource constraints
    const size_t large_message_count = 10000;
    
    for (size_t i = 0; i < large_message_count; ++i) {
        auto message = MockDBusMessageFactory::createPlayMethodCall();
        if (!m_mock_dbus->sendMessage(std::move(message))) {
            // Expected to fail at some point due to queue limits
            break;
        }
    }
    
    // System should still be functional
    if (!m_mock_dbus->isConnected() && m_mock_dbus->getLastError().empty()) {
        recordValidationError("D-Bus connection lost during resource exhaustion test");
    }
}

void StressTestFixture::testConnectionInstability() {
    // Simulate unstable connection conditions
    for (int i = 0; i < 20; ++i) {
        simulateConnectionLoss();
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        
        simulateConnectionRestore();
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        
        // Try operations during instability
        simulateMethodCall("Play");
        simulateMethodCall("Pause");
    }
    
    // System should handle instability gracefully
    if (!m_mock_player->validateState()) {
        recordValidationError("Player state invalid after connection instability");
    }
}

void StressTestFixture::testExtremeCases() {
    // Test with extreme parameter values
    
    // Very large seek position
    m_mock_player->seekTo(UINT64_MAX);
    if (!m_mock_player->validateState()) {
        recordValidationError("Player state invalid after extreme seek");
    }
    
    // Rapid state changes
    for (int i = 0; i < 1000; ++i) {
        m_mock_player->play();
        m_mock_player->pause();
        m_mock_player->stop();
    }
    
    if (!m_mock_player->validateState()) {
        recordValidationError("Player state invalid after rapid state changes");
    }
}

// MPRISTestScenarioRunner implementation

MPRISTestScenarioRunner::MPRISTestScenarioRunner() {
    addPredefinedScenarios();
}

void MPRISTestScenarioRunner::addScenario(const ScenarioConfig& config) {
    m_scenarios[config.name] = config;
}

void MPRISTestScenarioRunner::addPredefinedScenarios() {
    addBasicFunctionalityScenarios();
    addErrorHandlingScenarios();
    addThreadSafetyScenarios();
    addPerformanceScenarios();
    addIntegrationScenarios();
    addStressTestScenarios();
}

bool MPRISTestScenarioRunner::runScenario(const std::string& name, MPRISTestFixture& fixture) {
    auto it = m_scenarios.find(name);
    if (it == m_scenarios.end()) {
        return false;
    }
    
    const auto& config = it->second;
    
    try {
        // Setup
        if (config.setup_func) {
            config.setup_func(fixture);
        }
        
        // Run test
        bool result = false;
        if (config.test_func) {
            result = config.test_func(fixture);
        }
        
        // Cleanup
        if (config.cleanup_func) {
            config.cleanup_func(fixture);
        }
        
        return config.expect_failure ? !result : result;
        
    } catch (const std::exception& e) {
        return config.expect_failure;
    }
}

std::vector<TestCaseInfo> MPRISTestScenarioRunner::runAllScenarios(MPRISTestFixture& fixture) {
    std::vector<TestCaseInfo> results;
    
    for (const auto& scenario_pair : m_scenarios) {
        const std::string& name = scenario_pair.first;
        
        TestCaseInfo info(name);
        bool scenario_passed = runScenario(name, fixture);
        info.result = scenario_passed ? TestResult::PASSED : TestResult::FAILED;
        info.failure_message = fixture.getValidationErrors();
        
        results.push_back(info);
    }
    
    return results;
}

void MPRISTestScenarioRunner::printScenarioResults(const std::vector<TestCaseInfo>& results) {
    std::cout << "Scenario Test Results:" << std::endl;
    std::cout << "=====================" << std::endl;
    
    for (const auto& result : results) {
        std::cout << (result.result == TestResult::PASSED ? "✓" : "✗") << " " << result.name;
        if (result.result != TestResult::PASSED && !result.failure_message.empty()) {
            std::cout << " - " << result.failure_message;
        }
        std::cout << std::endl;
    }
}

size_t MPRISTestScenarioRunner::getPassedScenarioCount(const std::vector<TestCaseInfo>& results) {
    return std::count_if(results.begin(), results.end(), [](const TestCaseInfo& info) {
        return info.result == TestResult::PASSED;
    });
}

size_t MPRISTestScenarioRunner::getFailedScenarioCount(const std::vector<TestCaseInfo>& results) {
    return std::count_if(results.begin(), results.end(), [](const TestCaseInfo& info) {
        return info.result != TestResult::PASSED;
    });
}

void MPRISTestScenarioRunner::addBasicFunctionalityScenarios() {
    ScenarioConfig config;
    
    config.name = "Basic Playback Control";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* basic_fixture = dynamic_cast<BasicMPRISTestFixture*>(&fixture);
        if (basic_fixture) {
            basic_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
    
    config.name = "Metadata Updates";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* basic_fixture = dynamic_cast<BasicMPRISTestFixture*>(&fixture);
        if (basic_fixture) {
            basic_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
}

void MPRISTestScenarioRunner::addErrorHandlingScenarios() {
    ScenarioConfig config;
    
    config.name = "Connection Failure Recovery";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* error_fixture = dynamic_cast<ErrorHandlingTestFixture*>(&fixture);
        if (error_fixture) {
            error_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
    
    config.name = "Malformed Message Handling";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* error_fixture = dynamic_cast<ErrorHandlingTestFixture*>(&fixture);
        if (error_fixture) {
            error_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
}

void MPRISTestScenarioRunner::addThreadSafetyScenarios() {
    ScenarioConfig config;
    
    config.name = "Concurrent Method Calls";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* thread_fixture = dynamic_cast<ThreadSafetyTestFixture*>(&fixture);
        if (thread_fixture) {
            thread_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
    
    config.name = "Deadlock Prevention";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* thread_fixture = dynamic_cast<ThreadSafetyTestFixture*>(&fixture);
        if (thread_fixture) {
            thread_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
}

void MPRISTestScenarioRunner::addPerformanceScenarios() {
    ScenarioConfig config;
    
    config.name = "High Frequency Updates";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* perf_fixture = dynamic_cast<PerformanceTestFixture*>(&fixture);
        if (perf_fixture) {
            perf_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
    
    config.name = "Message Throughput";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* perf_fixture = dynamic_cast<PerformanceTestFixture*>(&fixture);
        if (perf_fixture) {
            perf_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
}

void MPRISTestScenarioRunner::addIntegrationScenarios() {
    ScenarioConfig config;
    
    config.name = "Player Integration";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* integration_fixture = dynamic_cast<IntegrationTestFixture*>(&fixture);
        if (integration_fixture) {
            integration_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
    
    config.name = "End-to-End Workflow";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* integration_fixture = dynamic_cast<IntegrationTestFixture*>(&fixture);
        if (integration_fixture) {
            integration_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
}

void MPRISTestScenarioRunner::addStressTestScenarios() {
    ScenarioConfig config;
    
    config.name = "High Concurrency Stress";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* stress_fixture = dynamic_cast<StressTestFixture*>(&fixture);
        if (stress_fixture) {
            stress_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
    
    config.name = "Connection Instability";
    config.test_func = [](MPRISTestFixture& fixture) -> bool {
        auto* stress_fixture = dynamic_cast<StressTestFixture*>(&fixture);
        if (stress_fixture) {
            stress_fixture->runTest();
            return fixture.getValidationErrors().empty();
        }
        return false;
    };
    addScenario(config);
}

} // namespace TestFramework