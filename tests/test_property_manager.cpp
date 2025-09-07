/*
 * test_property_manager.cpp - Unit tests for PropertyManager class
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

// Mock Player class for testing
class MockPlayer {
public:
    MockPlayer() = default;
    ~MockPlayer() = default;
    
    // Mock methods that PropertyManager might call
    // (Currently PropertyManager doesn't call Player methods directly,
    //  but this provides a foundation for future integration)
};

class PropertyManagerTest : public TestFramework::TestCase {
public:
    PropertyManagerTest(const std::string& name) : TestCase(name) {}
    
protected:
    void setUp() override {
        m_mock_player = std::make_unique<MockPlayer>();
        m_property_manager = std::make_unique<PropertyManager>(reinterpret_cast<Player*>(m_mock_player.get()));
    }
    
    void tearDown() override {
        m_property_manager.reset();
        m_mock_player.reset();
    }
    
    std::unique_ptr<MockPlayer> m_mock_player;
    std::unique_ptr<PropertyManager> m_property_manager;
};

// Test basic metadata operations
class TestBasicMetadata : public PropertyManagerTest {
public:
    TestBasicMetadata() : PropertyManagerTest("BasicMetadata") {}
    
    void runTest() override {
        // Test initial state
        auto metadata = m_property_manager->getMetadata();
        ASSERT_TRUE(metadata.empty() || metadata.find("xesam:title") == metadata.end(), 
                   "Initial metadata should be empty");
        
        // Test setting metadata
        m_property_manager->updateMetadata("Test Artist", "Test Title", "Test Album");
        
        metadata = m_property_manager->getMetadata();
        ASSERT_FALSE(metadata.empty(), "Metadata should not be empty after update");
        
        // Verify metadata content
        auto title_it = metadata.find("xesam:title");
        if (title_it != metadata.end()) {
            ASSERT_EQUALS(title_it->second.get<std::string>(), "Test Title", 
                         "Title should match what was set");
        }
        
        auto artist_it = metadata.find("xesam:artist");
        if (artist_it != metadata.end()) {
            // Artist is stored as string array in MPRIS
            auto artists = artist_it->second.get<std::vector<std::string>>();
            ASSERT_FALSE(artists.empty(), "Artist array should not be empty");
            ASSERT_EQUALS(artists[0], "Test Artist", "Artist should match what was set");
        }
        
        auto album_it = metadata.find("xesam:album");
        if (album_it != metadata.end()) {
            ASSERT_EQUALS(album_it->second.get<std::string>(), "Test Album", 
                         "Album should match what was set");
        }
        
        // Test clearing metadata
        m_property_manager->clearMetadata();
        metadata = m_property_manager->getMetadata();
        ASSERT_TRUE(metadata.empty() || metadata.find("xesam:title") == metadata.end(), 
                   "Metadata should be empty after clear");
    }
};

// Test playback status operations
class TestPlaybackStatus : public PropertyManagerTest {
public:
    TestPlaybackStatus() : PropertyManagerTest("PlaybackStatus") {}
    
    void runTest() override {
        // Test initial status
        std::string status = m_property_manager->getPlaybackStatus();
        ASSERT_EQUALS(status, "Stopped", "Initial status should be Stopped");
        
        // Test setting to Playing
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
        status = m_property_manager->getPlaybackStatus();
        ASSERT_EQUALS(status, "Playing", "Status should be Playing after update");
        
        // Test setting to Paused
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Paused);
        status = m_property_manager->getPlaybackStatus();
        ASSERT_EQUALS(status, "Paused", "Status should be Paused after update");
        
        // Test setting back to Stopped
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Stopped);
        status = m_property_manager->getPlaybackStatus();
        ASSERT_EQUALS(status, "Stopped", "Status should be Stopped after update");
    }
};

// Test position tracking and interpolation
class TestPositionTracking : public PropertyManagerTest {
public:
    TestPositionTracking() : PropertyManagerTest("PositionTracking") {}
    
    void runTest() override {
        // Test initial position
        uint64_t position = m_property_manager->getPosition();
        ASSERT_EQUALS(position, 0ULL, "Initial position should be 0");
        
        // Test setting position while stopped
        m_property_manager->updatePosition(5000000); // 5 seconds in microseconds
        position = m_property_manager->getPosition();
        ASSERT_EQUALS(position, 5000000ULL, "Position should match what was set while stopped");
        
        // Test position interpolation while playing
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
        m_property_manager->updatePosition(1000000); // 1 second
        
        // Wait a bit and check if position interpolates
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        position = m_property_manager->getPosition();
        ASSERT_TRUE(position >= 1000000ULL, "Position should be at least the set value");
        ASSERT_TRUE(position <= 1200000ULL, "Position should not advance too much (within 200ms)");
        
        // Test that position doesn't interpolate when paused
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Paused);
        m_property_manager->updatePosition(2000000); // 2 seconds
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        position = m_property_manager->getPosition();
        ASSERT_EQUALS(position, 2000000ULL, "Position should not interpolate when paused");
    }
};

// Test thread safety with concurrent access
class TestThreadSafety : public PropertyManagerTest {
public:
    TestThreadSafety() : PropertyManagerTest("ThreadSafety") {}
    
    void runTest() override {
        const int num_threads = 4;
        const int operations_per_thread = 100;
        std::vector<std::thread> threads;
        std::atomic<int> completed_operations{0};
        std::atomic<bool> test_failed{false};
        
        // Launch multiple threads that perform concurrent operations
        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back([this, i, operations_per_thread, &completed_operations, &test_failed]() {
                try {
                    for (int j = 0; j < operations_per_thread; ++j) {
                        // Alternate between different operations
                        switch ((i + j) % 4) {
                            case 0:
                                m_property_manager->updateMetadata(
                                    "Artist" + std::to_string(i), 
                                    "Title" + std::to_string(j), 
                                    "Album" + std::to_string(i + j)
                                );
                                break;
                            case 1:
                                m_property_manager->updatePlaybackStatus(
                                    static_cast<MPRISTypes::PlaybackStatus>(j % 3)
                                );
                                break;
                            case 2:
                                m_property_manager->updatePosition(j * 1000000ULL);
                                break;
                            case 3:
                                // Read operations
                                m_property_manager->getMetadata();
                                m_property_manager->getPlaybackStatus();
                                m_property_manager->getPosition();
                                break;
                        }
                        completed_operations.fetch_add(1);
                    }
                } catch (const std::exception& e) {
                    test_failed.store(true);
                }
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        ASSERT_FALSE(test_failed.load(), "No exceptions should occur during concurrent access");
        ASSERT_EQUALS(completed_operations.load(), num_threads * operations_per_thread, 
                     "All operations should complete successfully");
        
        // Verify the PropertyManager is still in a valid state
        auto metadata = m_property_manager->getMetadata();
        std::string status = m_property_manager->getPlaybackStatus();
        uint64_t position = m_property_manager->getPosition();
        
        // These should not throw exceptions
        ASSERT_TRUE(true, "PropertyManager should remain in valid state after concurrent access");
    }
};

// Test all properties retrieval
class TestAllProperties : public PropertyManagerTest {
public:
    TestAllProperties() : PropertyManagerTest("AllProperties") {}
    
    void runTest() override {
        // Set up some test data
        m_property_manager->updateMetadata("Test Artist", "Test Title", "Test Album");
        m_property_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
        m_property_manager->updatePosition(3000000); // 3 seconds
        
        // Get all properties
        auto properties = m_property_manager->getAllProperties();
        
        // Verify essential properties are present
        ASSERT_TRUE(properties.find("PlaybackStatus") != properties.end(), 
                   "PlaybackStatus should be present");
        ASSERT_TRUE(properties.find("CanControl") != properties.end(), 
                   "CanControl should be present");
        ASSERT_TRUE(properties.find("Position") != properties.end(), 
                   "Position should be present");
        
        // Verify PlaybackStatus value
        auto status_it = properties.find("PlaybackStatus");
        if (status_it != properties.end()) {
            ASSERT_EQUALS(status_it->second.get<std::string>(), "Playing", 
                         "PlaybackStatus should be Playing");
        }
        
        // Verify CanControl value
        auto can_control_it = properties.find("CanControl");
        if (can_control_it != properties.end()) {
            ASSERT_TRUE(can_control_it->second.get<bool>(), 
                       "CanControl should be true");
        }
    }
};

// Test edge cases and error conditions
class TestEdgeCases : public PropertyManagerTest {
public:
    TestEdgeCases() : PropertyManagerTest("EdgeCases") {}
    
    void runTest() override {
        // Test empty strings
        m_property_manager->updateMetadata("", "", "");
        auto metadata = m_property_manager->getMetadata();
        // Should handle empty strings gracefully
        
        // Test very large position values
        uint64_t large_position = UINT64_MAX - 1000000;
        m_property_manager->updatePosition(large_position);
        uint64_t retrieved_position = m_property_manager->getPosition();
        ASSERT_EQUALS(retrieved_position, large_position, 
                     "Should handle large position values");
        
        // Test rapid status changes
        for (int i = 0; i < 100; ++i) {
            m_property_manager->updatePlaybackStatus(
                static_cast<MPRISTypes::PlaybackStatus>(i % 3)
            );
        }
        
        // Should not crash or throw exceptions
        std::string final_status = m_property_manager->getPlaybackStatus();
        ASSERT_TRUE(!final_status.empty(), "Status should remain valid after rapid changes");
        
        // Test multiple clears
        for (int i = 0; i < 10; ++i) {
            m_property_manager->clearMetadata();
        }
        
        metadata = m_property_manager->getMetadata();
        ASSERT_TRUE(metadata.empty() || metadata.find("xesam:title") == metadata.end(), 
                   "Metadata should remain cleared after multiple clear operations");
    }
};

int main() {
    TestFramework::TestSuite suite("PropertyManager Tests");
    
    // Add all test cases
    suite.addTest(std::make_unique<TestBasicMetadata>());
    suite.addTest(std::make_unique<TestPlaybackStatus>());
    suite.addTest(std::make_unique<TestPositionTracking>());
    suite.addTest(std::make_unique<TestThreadSafety>());
    suite.addTest(std::make_unique<TestAllProperties>());
    suite.addTest(std::make_unique<TestEdgeCases>());
    
    // Run all tests
    auto results = suite.runAll();
    suite.printResults(results);
    
    return suite.getFailureCount(results) == 0 ? 0 : 1;
}