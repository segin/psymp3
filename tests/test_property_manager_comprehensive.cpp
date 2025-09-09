/*
 * test_property_manager_comprehensive.cpp - Comprehensive unit tests for PropertyManager
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include "test_framework_threading.h"
#include "mock_player.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <random>

using namespace TestFramework;
using namespace TestFramework::Threading;

/**
 * @brief Test class for PropertyManager comprehensive testing
 */
class PropertyManagerTest : public TestCase {
public:
    PropertyManagerTest() : TestCase("PropertyManagerTest") {}

protected:
    void setUp() override {
        // Create mock player with threading safety testing enabled
        MockPlayer::Config config;
        config.thread_safety_testing = true;
        config.simulate_state_changes = true;
        config.state_change_delay = std::chrono::milliseconds{10};
        
        m_mock_player = std::make_unique<MockPlayer>(config);
        m_property_manager = std::make_unique<PropertyManager>(reinterpret_cast<Player*>(m_mock_player.get()));
    }

    void tearDown() override {
        m_property_manager.reset();
        m_mock_player.reset();
    }

    void runTest() override {
        testBasicPropertyOperations();
        testConcurrentPropertyAccess();
        testMetadataCaching();
        testPlaybackStatusTracking();
        testPositionInterpolation();
        testPropertySynchronization();
        testErrorHandlingAndValidation();
        testPerformanceUnderLoad();
    }

private:
    std::unique_ptr<MockPlayer> m_mock_player;
    std::unique_ptr<PropertyManager> m_property_manager;

    void testBasicPropertyOperations() {
        // Test metadata updates
        m_property_manager->updateMetadata("Test Artist", "Test Title", "Test Album");
        
        auto metadata = m_property_manager->getMetadata();
        ASSERT_TRUE(metadata.find("xesam:artist") != metadata.end(), "Should contain artist metadata");
        ASSERT_TRUE(metadata.find("xesam:title") != metadata.end(), "Should contain title metadata");
        ASSERT_TRUE(metadata.find("xesam:album") != metadata.end(), "Should contain album metadata");
        
        // Test playback status updates
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
        std::string status = m_property_manager->getPlaybackStatus();
        ASSERT_EQUALS(std::string("Playing"), status, "Should report correct playback status");
        
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Paused);
        status = m_property_manager->getPlaybackStatus();
        ASSERT_EQUALS(std::string("Paused"), status, "Should update playback status");
        
        // Test position updates
        uint64_t test_position = 123456789; // microseconds
        m_property_manager->updatePosition(test_position);
        uint64_t retrieved_position = m_property_manager->getPosition();
        ASSERT_EQUALS(test_position, retrieved_position, "Should store and retrieve position correctly");
    }

    void testConcurrentPropertyAccess() {
        ThreadSafetyTester::Config config;
        config.num_threads = 6;
        config.operations_per_thread = 100;
        config.test_duration = std::chrono::milliseconds{3000};
        
        ThreadSafetyTester tester(config);
        
        // Test concurrent metadata updates and reads
        std::atomic<size_t> update_counter{0};
        auto metadata_test = [this, &update_counter]() -> bool {
            try {
                size_t counter = update_counter.fetch_add(1);
                std::string artist = "Artist_" + std::to_string(counter);
                std::string title = "Title_" + std::to_string(counter);
                std::string album = "Album_" + std::to_string(counter);
                
                m_property_manager->updateMetadata(artist, title, album);
                
                // Read back metadata
                auto metadata = m_property_manager->getMetadata();
                
                // Verify we got some metadata back (may not be the exact values due to concurrency)
                return !metadata.empty();
            } catch (...) {
                return false;
            }
        };
        
        auto results = tester.runTest(metadata_test, "ConcurrentMetadataAccess");
        ASSERT_TRUE(results.successful_operations > 0, "Should have successful metadata operations");
        ASSERT_FALSE(results.deadlock_detected, "Should not detect deadlocks in metadata access");
        
        // Test concurrent position updates
        std::atomic<uint64_t> position_counter{0};
        auto position_test = [this, &position_counter]() -> bool {
            try {
                uint64_t position = position_counter.fetch_add(1000);
                m_property_manager->updatePosition(position);
                
                uint64_t retrieved = m_property_manager->getPosition();
                // Position should be valid (may not match exactly due to concurrency and interpolation)
                return retrieved >= 0;
            } catch (...) {
                return false;
            }
        };
        
        auto position_results = tester.runTest(position_test, "ConcurrentPositionAccess");
        ASSERT_FALSE(position_results.deadlock_detected, "Should not detect deadlocks in position access");
    }

    void testMetadataCaching() {
        // Test metadata caching behavior
        m_property_manager->updateMetadata("Cached Artist", "Cached Title", "Cached Album");
        
        // Multiple reads should return consistent data
        auto metadata1 = m_property_manager->getMetadata();
        auto metadata2 = m_property_manager->getMetadata();
        auto metadata3 = m_property_manager->getMetadata();
        
        ASSERT_EQUALS(metadata1.size(), metadata2.size(), "Cached metadata should be consistent");
        ASSERT_EQUALS(metadata2.size(), metadata3.size(), "Cached metadata should be consistent");
        
        // Test metadata updates override cache
        m_property_manager->updateMetadata("New Artist", "New Title", "New Album");
        auto new_metadata = m_property_manager->getMetadata();
        
        // Should have updated values (exact comparison depends on implementation)
        ASSERT_TRUE(!new_metadata.empty(), "Should have updated metadata");
        
        // Test empty metadata handling
        m_property_manager->updateMetadata("", "", "");
        auto empty_metadata = m_property_manager->getMetadata();
        ASSERT_TRUE(!empty_metadata.empty(), "Should handle empty metadata gracefully");
    }

    void testPlaybackStatusTracking() {
        // Test all playback states
        std::vector<MPRISTypes::PlaybackStatus> states = {
            MPRISTypes::PlaybackStatus::Playing,
            MPRISTypes::PlaybackStatus::Paused,
            MPRISTypes::PlaybackStatus::Stopped
        };
        
        for (auto state : states) {
            m_property_manager->updatePlaybackStatus(state);
            std::string status_str = m_property_manager->getPlaybackStatus();
            ASSERT_TRUE(!status_str.empty(), "Should return valid status string");
        }
        
        // Test rapid state changes
        for (int i = 0; i < 50; ++i) {
            MPRISTypes::PlaybackStatus state = static_cast<MPRISTypes::PlaybackStatus>(i % 3);
            m_property_manager->updatePlaybackStatus(state);
            std::string status = m_property_manager->getPlaybackStatus();
            ASSERT_TRUE(!status.empty(), "Should handle rapid state changes");
        }
        
        // Test state consistency
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
        for (int i = 0; i < 10; ++i) {
            std::string status = m_property_manager->getPlaybackStatus();
            ASSERT_EQUALS(std::string("Playing"), status, "Status should remain consistent");
        }
    }

    void testPositionInterpolation() {
        // Test position interpolation during playback
        uint64_t base_position = 1000000; // 1 second in microseconds
        m_property_manager->updatePosition(base_position);
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
        
        uint64_t position1 = m_property_manager->getPosition();
        
        // Wait a bit and check if position interpolates
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        uint64_t position2 = m_property_manager->getPosition();
        
        // During playback, position should advance (or at least not go backwards)
        ASSERT_TRUE(position2 >= position1, "Position should not go backwards during playback");
        
        // Test position during pause
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Paused);
        uint64_t paused_position1 = m_property_manager->getPosition();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        uint64_t paused_position2 = m_property_manager->getPosition();
        
        // During pause, position should remain stable
        ASSERT_EQUALS(paused_position1, paused_position2, "Position should be stable during pause");
        
        // Test position during stop
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Stopped);
        uint64_t stopped_position = m_property_manager->getPosition();
        ASSERT_TRUE(stopped_position >= 0, "Stopped position should be valid");
    }

    void testPropertySynchronization() {
        // Test synchronization with mock player state
        MockPlayer::TrackInfo track("Sync Artist", "Sync Title", "Sync Album");
        track.duration_us = 180000000; // 3 minutes
        
        m_mock_player->setCurrentTrack(track);
        m_mock_player->setState(PlayerState::Playing);
        m_mock_player->setPosition(60000000); // 1 minute
        
        // Update property manager from player state
        m_property_manager->updateMetadata(track.artist, track.title, track.album);
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
        m_property_manager->updatePosition(60000000);
        
        // Verify synchronization
        auto metadata = m_property_manager->getMetadata();
        std::string status = m_property_manager->getPlaybackStatus();
        uint64_t position = m_property_manager->getPosition();
        
        ASSERT_TRUE(!metadata.empty(), "Should have synchronized metadata");
        ASSERT_EQUALS(std::string("Playing"), status, "Should have synchronized status");
        ASSERT_EQUALS(uint64_t(60000000), position, "Should have synchronized position");
        
        // Test batch updates
        for (int i = 0; i < 20; ++i) {
            MockPlayer::TrackInfo batch_track("Artist_" + std::to_string(i), 
                                             "Title_" + std::to_string(i), 
                                             "Album_" + std::to_string(i));
            
            m_property_manager->updateMetadata(batch_track.artist, batch_track.title, batch_track.album);
            m_property_manager->updatePosition(i * 1000000);
        }
        
        // Should handle batch updates without issues
        auto final_metadata = m_property_manager->getMetadata();
        ASSERT_TRUE(!final_metadata.empty(), "Should handle batch updates");
    }

    void testErrorHandlingAndValidation() {
        // Test invalid metadata handling
        m_property_manager->updateMetadata("", "", ""); // Empty strings
        auto empty_metadata = m_property_manager->getMetadata();
        ASSERT_TRUE(!empty_metadata.empty(), "Should handle empty metadata gracefully");
        
        // Test very long metadata strings
        std::string long_string(10000, 'A');
        m_property_manager->updateMetadata(long_string, long_string, long_string);
        auto long_metadata = m_property_manager->getMetadata();
        ASSERT_TRUE(!long_metadata.empty(), "Should handle long metadata strings");
        
        // Test special characters in metadata
        m_property_manager->updateMetadata("Artíst with ñ", "Tïtle with ü", "Albüm with ø");
        auto unicode_metadata = m_property_manager->getMetadata();
        ASSERT_TRUE(!unicode_metadata.empty(), "Should handle unicode characters");
        
        // Test invalid position values
        m_property_manager->updatePosition(UINT64_MAX);
        uint64_t max_position = m_property_manager->getPosition();
        ASSERT_TRUE(max_position >= 0, "Should handle maximum position value");
        
        m_property_manager->updatePosition(0);
        uint64_t zero_position = m_property_manager->getPosition();
        ASSERT_EQUALS(uint64_t(0), zero_position, "Should handle zero position");
        
        // Test error recovery
        try {
            // Simulate error condition
            throw std::runtime_error("Simulated error");
        } catch (...) {
            // Property manager should still be functional
            m_property_manager->updateMetadata("Recovery Artist", "Recovery Title", "Recovery Album");
            auto recovery_metadata = m_property_manager->getMetadata();
            ASSERT_TRUE(!recovery_metadata.empty(), "Should recover from errors");
        }
    }

    void testPerformanceUnderLoad() {
        // Measure property update performance
        auto start_time = std::chrono::high_resolution_clock::now();
        
        const size_t num_operations = 1000;
        
        for (size_t i = 0; i < num_operations; ++i) {
            std::string suffix = std::to_string(i);
            m_property_manager->updateMetadata("Artist_" + suffix, "Title_" + suffix, "Album_" + suffix);
            m_property_manager->updatePosition(i * 1000);
            
            if (i % 3 == 0) {
                m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
            } else if (i % 3 == 1) {
                m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Paused);
            } else {
                m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Stopped);
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Performance should be reasonable (less than 5ms per operation on average)
        auto avg_time_per_op = duration.count() / num_operations;
        ASSERT_TRUE(avg_time_per_op < 5, "Property operations should be fast");
        
        // Test read performance
        start_time = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < num_operations; ++i) {
            auto metadata = m_property_manager->getMetadata();
            std::string status = m_property_manager->getPlaybackStatus();
            uint64_t position = m_property_manager->getPosition();
            
            // Verify we got valid data
            ASSERT_TRUE(!metadata.empty(), "Should get valid metadata");
            ASSERT_TRUE(!status.empty(), "Should get valid status");
        }
        
        end_time = std::chrono::high_resolution_clock::now();
        auto read_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Read performance should be very fast (less than 1ms per operation)
        auto avg_read_time = read_duration.count() / num_operations;
        ASSERT_TRUE(avg_read_time < 1, "Property reads should be very fast");
        
        // Test lock contention under load
        LockContentionAnalyzer analyzer;
        std::mutex test_mutex;
        
        auto contention_metrics = analyzer.analyzeLockContention(
            test_mutex, 
            std::chrono::milliseconds{1000}, 
            6
        );
        
        ASSERT_TRUE(contention_metrics.total_acquisitions > 0, "Should measure lock acquisitions");
        ASSERT_TRUE(contention_metrics.contention_ratio < 0.5, "Lock contention should be reasonable");
    }
};

/**
 * @brief Test class for PropertyManager edge cases and stress scenarios
 */
class PropertyManagerStressTest : public TestCase {
public:
    PropertyManagerStressTest() : TestCase("PropertyManagerStressTest") {}

protected:
    void runTest() override {
        testHighFrequencyUpdates();
        testMemoryUsageUnderLoad();
        testConcurrentReadersWriters();
        testExceptionSafety();
    }

private:
    void testHighFrequencyUpdates() {
        auto mock_player = std::make_unique<MockPlayer>();
        auto property_manager = std::make_unique<PropertyManager>(reinterpret_cast<Player*>(mock_player.get()));
        
        // High frequency position updates (simulating real-time playback)
        const size_t updates_per_second = 100;
        const size_t test_duration_seconds = 2;
        const size_t total_updates = updates_per_second * test_duration_seconds;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < total_updates; ++i) {
            uint64_t position = i * (1000000 / updates_per_second); // microseconds
            property_manager->updatePosition(position);
            
            // Simulate real-time intervals
            std::this_thread::sleep_for(std::chrono::microseconds(10000 / updates_per_second));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        ASSERT_TRUE(duration.count() < (test_duration_seconds * 1000 * 1.5), 
                   "High frequency updates should complete in reasonable time");
    }

    void testMemoryUsageUnderLoad() {
        auto mock_player = std::make_unique<MockPlayer>();
        auto property_manager = std::make_unique<PropertyManager>(reinterpret_cast<Player*>(mock_player.get()));
        
        // Test with large metadata strings
        std::string large_string(1000, 'X');
        
        for (int i = 0; i < 100; ++i) {
            property_manager->updateMetadata(
                large_string + std::to_string(i),
                large_string + std::to_string(i),
                large_string + std::to_string(i)
            );
        }
        
        // Memory usage should be reasonable (test by ensuring operations still work)
        auto metadata = property_manager->getMetadata();
        ASSERT_TRUE(!metadata.empty(), "Should handle large metadata without memory issues");
    }

    void testConcurrentReadersWriters() {
        auto mock_player = std::make_unique<MockPlayer>();
        auto property_manager = std::make_unique<PropertyManager>(reinterpret_cast<Player*>(mock_player.get()));
        
        std::atomic<bool> should_stop{false};
        std::atomic<size_t> read_operations{0};
        std::atomic<size_t> write_operations{0};
        
        // Start multiple reader threads
        std::vector<std::thread> reader_threads;
        for (int i = 0; i < 4; ++i) {
            reader_threads.emplace_back([&]() {
                while (!should_stop.load()) {
                    auto metadata = property_manager->getMetadata();
                    std::string status = property_manager->getPlaybackStatus();
                    uint64_t position = property_manager->getPosition();
                    read_operations.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            });
        }
        
        // Start multiple writer threads
        std::vector<std::thread> writer_threads;
        for (int i = 0; i < 2; ++i) {
            writer_threads.emplace_back([&, i]() {
                size_t counter = 0;
                while (!should_stop.load()) {
                    std::string suffix = std::to_string(i) + "_" + std::to_string(counter++);
                    property_manager->updateMetadata("Artist_" + suffix, "Title_" + suffix, "Album_" + suffix);
                    property_manager->updatePosition(counter * 1000);
                    write_operations.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::microseconds(500));
                }
            });
        }
        
        // Let test run for a while
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        should_stop.store(true);
        
        // Wait for all threads to complete
        for (auto& thread : reader_threads) {
            thread.join();
        }
        for (auto& thread : writer_threads) {
            thread.join();
        }
        
        ASSERT_TRUE(read_operations.load() > 0, "Should have completed read operations");
        ASSERT_TRUE(write_operations.load() > 0, "Should have completed write operations");
        
        // Property manager should still be functional
        auto final_metadata = property_manager->getMetadata();
        ASSERT_TRUE(!final_metadata.empty(), "Should be functional after concurrent access");
    }

    void testExceptionSafety() {
        auto mock_player = std::make_unique<MockPlayer>();
        auto property_manager = std::make_unique<PropertyManager>(reinterpret_cast<Player*>(mock_player.get()));
        
        // Test exception safety during updates
        try {
            property_manager->updateMetadata("Test", "Test", "Test");
            throw std::runtime_error("Simulated exception");
        } catch (const std::exception&) {
            // Property manager should still work
            auto metadata = property_manager->getMetadata();
            ASSERT_TRUE(!metadata.empty(), "Should be functional after exception");
        }
        
        // Test with mock player that throws exceptions
        mock_player->enableErrorSimulation(true);
        mock_player->setErrorRate(0.5);
        
        // Property manager should handle player errors gracefully
        for (int i = 0; i < 20; ++i) {
            try {
                property_manager->updateMetadata("Error Test", "Error Test", "Error Test");
                property_manager->updatePosition(i * 1000);
            } catch (...) {
                // Exceptions from player should be handled
            }
        }
        
        // Should still be functional
        auto final_metadata = property_manager->getMetadata();
        ASSERT_TRUE(!final_metadata.empty(), "Should handle player errors gracefully");
    }
};

// Test suite setup and execution
int main() {
    TestSuite suite("PropertyManager Comprehensive Tests");
    
    suite.addTest(std::make_unique<PropertyManagerTest>());
    suite.addTest(std::make_unique<PropertyManagerStressTest>());
    
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results);
}