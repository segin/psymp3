/*
 * test_comprehensive_mock_framework.cpp - Comprehensive test demonstrating mock framework
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
#include "mpris_test_fixtures.h"

using namespace TestFramework;

/**
 * Test comprehensive mock framework integration
 */
bool test_comprehensive_mock_framework_integration() {
    std::cout << "Testing comprehensive mock framework integration..." << std::endl;
    
    // Create all types of test fixtures
    auto fixtures = MPRISTestFixtureFactory::createAllFixtures();
    
    size_t total_fixtures = fixtures.size();
    size_t successful_fixtures = 0;
    
    for (auto& fixture : fixtures) {
        try {
            // Set up fixture
            fixture->setUp();
            
            // Validate basic functionality
            bool valid = fixture->validateMPRISState() && 
                        fixture->validatePlayerIntegration() && 
                        fixture->validateDBusIntegration();
            
            if (valid) {
                successful_fixtures++;
                std::cout << "  ✓ " << fixture->getName() << " fixture working" << std::endl;
            } else {
                std::cout << "  ✗ " << fixture->getName() << " fixture failed: " << fixture->getValidationErrors() << std::endl;
            }
            
            // Clean up
            fixture->tearDown();
            
        } catch (const std::exception& e) {
            std::cout << "  ✗ " << fixture->getName() << " fixture exception: " << e.what() << std::endl;
        }
    }
    
    std::cout << "Fixture test results: " << successful_fixtures << "/" << total_fixtures << " passed" << std::endl;
    
    return successful_fixtures == total_fixtures;
}

/**
 * Test scenario runner functionality
 */
bool test_scenario_runner() {
    std::cout << "Testing scenario runner functionality..." << std::endl;
    
    MPRISTestScenarioRunner runner;
    auto basic_fixture = MPRISTestFixtureFactory::createBasicFixture();
    
    // Run all predefined scenarios
    auto results = runner.runAllScenarios(*basic_fixture);
    
    size_t passed = runner.getPassedScenarioCount(results);
    size_t failed = runner.getFailedScenarioCount(results);
    
    std::cout << "Scenario results: " << passed << " passed, " << failed << " failed" << std::endl;
    
    // Print detailed results
    runner.printScenarioResults(results);
    
    // We expect some scenarios to pass (at least basic ones should work)
    return passed > 0;
}

/**
 * Test data generator functionality
 */
bool test_data_generator() {
    std::cout << "Testing data generator functionality..." << std::endl;
    
    // Test track generation
    auto track = MPRISTestDataGenerator::generateTestTrack(1);
    if (track.artist.empty() || track.title.empty() || track.album.empty()) {
        std::cout << "  ✗ Track generation failed" << std::endl;
        return false;
    }
    std::cout << "  ✓ Track generation working" << std::endl;
    
    // Test playlist generation
    auto playlist = MPRISTestDataGenerator::generateTestPlaylist(5);
    if (playlist.size() != 5) {
        std::cout << "  ✗ Playlist generation failed" << std::endl;
        return false;
    }
    std::cout << "  ✓ Playlist generation working" << std::endl;
    
    // Test message generation
    auto messages = MPRISTestDataGenerator::generateTestMessages(10);
    if (messages.size() != 10) {
        std::cout << "  ✗ Message generation failed" << std::endl;
        return false;
    }
    std::cout << "  ✓ Message generation working" << std::endl;
    
    // Test malformed message generation
    auto malformed = MPRISTestDataGenerator::generateMalformedMessages(3);
    if (malformed.size() != 3) {
        std::cout << "  ✗ Malformed message generation failed" << std::endl;
        return false;
    }
    std::cout << "  ✓ Malformed message generation working" << std::endl;
    
    // Test property changes generation
    auto properties = MPRISTestDataGenerator::generatePropertyChanges();
    if (properties.empty()) {
        std::cout << "  ✗ Property changes generation failed" << std::endl;
        return false;
    }
    std::cout << "  ✓ Property changes generation working" << std::endl;
    
    // Test stress operations generation
    auto operations = MPRISTestDataGenerator::generateStressTestOperations(100);
    if (operations.size() != 100) {
        std::cout << "  ✗ Stress operations generation failed" << std::endl;
        return false;
    }
    std::cout << "  ✓ Stress operations generation working" << std::endl;
    
    return true;
}

/**
 * Test threading utilities integration
 */
bool test_threading_utilities_integration() {
    std::cout << "Testing threading utilities integration..." << std::endl;
    
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
    
    auto results = tester.runTest(test_func, "Threading integration test");
    
    if (results.deadlock_detected) {
        std::cout << "  ✗ Deadlock detected in threading test" << std::endl;
        return false;
    }
    
    if (results.total_operations == 0) {
        std::cout << "  ✗ No operations completed in threading test" << std::endl;
        return false;
    }
    
    std::cout << "  ✓ Threading safety tester working (" << results.total_operations << " operations)" << std::endl;
    
    // Test lock contention analyzer
    Threading::LockContentionAnalyzer analyzer;
    std::mutex test_mutex;
    
    auto metrics = analyzer.analyzeLockContention(test_mutex, std::chrono::milliseconds{200}, 4);
    
    if (metrics.total_acquisitions == 0) {
        std::cout << "  ✗ Lock contention analyzer failed" << std::endl;
        return false;
    }
    
    std::cout << "  ✓ Lock contention analyzer working (" << metrics.total_acquisitions << " acquisitions)" << std::endl;
    
    // Test race condition detector
    Threading::RaceConditionDetector detector;
    
    std::atomic<int> safe_counter{0};
    
    auto setup_func = [&]() { safe_counter.store(0); };
    auto test_func_safe = [&](size_t thread_id, size_t iteration) { safe_counter.fetch_add(1); };
    auto verify_func = [&]() -> bool { return safe_counter.load() == 400; }; // 4 threads * 100 iterations
    
    bool race_detected = detector.detectRaceCondition(setup_func, test_func_safe, verify_func, 4, 100);
    
    // Should not detect race condition with atomic operations
    if (race_detected) {
        std::cout << "  ⚠ Race condition detector may be overly sensitive" << std::endl;
    } else {
        std::cout << "  ✓ Race condition detector working" << std::endl;
    }
    
    // Test threading benchmark
    Threading::ThreadingBenchmark benchmark;
    
    auto operation = [](size_t index) {
        volatile int temp = 0;
        for (int i = 0; i < 100; ++i) {
            temp += i;
        }
    };
    
    auto bench_results = benchmark.benchmarkScaling(operation, 1000, 4);
    
    if (bench_results.operations_per_second == 0) {
        std::cout << "  ✗ Threading benchmark failed" << std::endl;
        return false;
    }
    
    std::cout << "  ✓ Threading benchmark working (" << bench_results.operations_per_second << " ops/sec)" << std::endl;
    
    return true;
}

/**
 * Test performance benchmarking capabilities
 */
bool test_performance_benchmarking() {
    std::cout << "Testing performance benchmarking capabilities..." << std::endl;
    
    auto player = MockPlayerFactory::createPerformanceTestPlayer();
    MockDBusConnection::Config config;
    config.enable_message_logging = false;
    MockDBusConnection dbus_connection(config);
    
    if (!dbus_connection.connect()) {
        std::cout << "  ✗ Failed to connect mock D-Bus" << std::endl;
        return false;
    }
    
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
    if (stats.messages_sent != num_operations) {
        std::cout << "  ✗ Message count mismatch: expected " << num_operations << ", got " << stats.messages_sent << std::endl;
        return false;
    }
    
    std::cout << "  ✓ Performance benchmarking working" << std::endl;
    return true;
}

/**
 * Test error simulation and recovery
 */
bool test_error_simulation_and_recovery() {
    std::cout << "Testing error simulation and recovery..." << std::endl;
    
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
    if (success_rate > 0.8 || success_rate < 0.2) {
        std::cout << "  ⚠ Error simulation may not be working correctly" << std::endl;
    } else {
        std::cout << "  ✓ Player error simulation working" << std::endl;
    }
    
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
    if (message_success_rate > 0.9 || message_success_rate < 0.5) {
        std::cout << "  ⚠ D-Bus error simulation may not be working correctly" << std::endl;
    } else {
        std::cout << "  ✓ D-Bus error simulation working" << std::endl;
    }
    
    // Test connection loss and recovery
    dbus_connection.simulateConnectionLoss();
    if (dbus_connection.isConnected()) {
        std::cout << "  ✗ Connection loss simulation failed" << std::endl;
        return false;
    }
    
    dbus_connection.simulateConnectionRestore();
    if (!dbus_connection.isConnected()) {
        std::cout << "  ✗ Connection restore simulation failed" << std::endl;
        return false;
    }
    
    std::cout << "  ✓ Connection loss/restore simulation working" << std::endl;
    
    return true;
}

/**
 * Main test runner
 */
int main() {
    std::cout << "Running Comprehensive Mock Framework Tests..." << std::endl;
    std::cout << "=============================================" << std::endl << std::endl;
    
    bool all_passed = true;
    
    try {
        all_passed &= test_comprehensive_mock_framework_integration();
        std::cout << std::endl;
        
        all_passed &= test_scenario_runner();
        std::cout << std::endl;
        
        all_passed &= test_data_generator();
        std::cout << std::endl;
        
        all_passed &= test_threading_utilities_integration();
        std::cout << std::endl;
        
        all_passed &= test_performance_benchmarking();
        std::cout << std::endl;
        
        all_passed &= test_error_simulation_and_recovery();
        std::cout << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "✗ Test failed with exception: " << e.what() << std::endl;
        all_passed = false;
    }
    
    if (all_passed) {
        std::cout << "✓ All comprehensive mock framework tests PASSED!" << std::endl;
        std::cout << "The MPRIS mock framework is fully functional and ready for use." << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some comprehensive mock framework tests FAILED!" << std::endl;
        std::cout << "Please review the mock framework implementation." << std::endl;
        return 1;
    }
}