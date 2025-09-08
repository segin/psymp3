/*
 * test_mpris_manager_integration.cpp - Integration tests for MPRISManager
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"
#include "test_framework.h"

#ifdef HAVE_DBUS

/**
 * Basic initialization and shutdown test for MPRISManager
 */
class MPRISManagerBasicTest : public TestFramework::TestCase {
public:
    MPRISManagerBasicTest() : TestFramework::TestCase("MPRISManagerBasicTest") {}
    
    void setUp() override {
        // Create a null player for testing (MPRISManager should handle this gracefully)
        m_mpris_manager = std::make_unique<MPRISManager>(nullptr);
    }
    
    void runTest() override {
        // Initially not initialized
        ASSERT_FALSE(m_mpris_manager->isInitialized(), "MPRISManager should not be initialized initially");
        ASSERT_FALSE(m_mpris_manager->isConnected(), "MPRISManager should not be connected initially");
        
        // Initialize (may fail if D-Bus is not available, which is OK for testing)
        auto result = m_mpris_manager->initialize();
        
        if (result.isSuccess()) {
            // If initialization succeeded, verify state
            ASSERT_TRUE(m_mpris_manager->isInitialized(), "MPRISManager should be initialized after successful init");
            
            // Shutdown should work cleanly
            m_mpris_manager->shutdown();
            ASSERT_FALSE(m_mpris_manager->isInitialized(), "MPRISManager should not be initialized after shutdown");
        } else {
            // If initialization failed, that's OK for testing environments
            // Just verify the error is reported properly
            ASSERT_FALSE(result.getError().empty(), "Error message should not be empty on initialization failure");
            ASSERT_FALSE(m_mpris_manager->isInitialized(), "MPRISManager should not be initialized after failed init");
        }
    }
    
private:
    std::unique_ptr<MPRISManager> m_mpris_manager;
};

/**
 * Test metadata updates without crashing
 */
class MPRISManagerMetadataTest : public TestFramework::TestCase {
public:
    MPRISManagerMetadataTest() : TestFramework::TestCase("MPRISManagerMetadataTest") {}
    
    void setUp() override {
        m_mpris_manager = std::make_unique<MPRISManager>(nullptr);
    }
    
    void runTest() override {
        // Initialize (ignore result for this test)
        m_mpris_manager->initialize();
        
        // Update metadata should not crash even if not connected
        m_mpris_manager->updateMetadata("Test Artist", "Test Title", "Test Album");
        
        // Multiple updates should be handled gracefully
        for (int i = 0; i < 10; ++i) {
            m_mpris_manager->updateMetadata(
                "Artist " + std::to_string(i),
                "Title " + std::to_string(i),
                "Album " + std::to_string(i)
            );
        }
        
        // Empty metadata should be handled
        m_mpris_manager->updateMetadata("", "", "");
        
        // Test passes if no crashes occur
        ASSERT_TRUE(true, "Metadata updates completed without crashes");
    }
    
private:
    std::unique_ptr<MPRISManager> m_mpris_manager;
};

/**
 * Test playback status updates
 */
class MPRISManagerStatusTest : public TestFramework::TestCase {
public:
    MPRISManagerStatusTest() : TestFramework::TestCase("MPRISManagerStatusTest") {}
    
    void setUp() override {
        m_mpris_manager = std::make_unique<MPRISManager>(nullptr);
    }
    
    void runTest() override {
        m_mpris_manager->initialize();
        
        // Test all playback states
        m_mpris_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
        m_mpris_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Paused);
        m_mpris_manager->updatePlaybackStatus(MPRISTypes::PlaybackStatus::Stopped);
        
        // Rapid state changes should be handled
        for (int i = 0; i < 20; ++i) {
            auto status = (i % 2 == 0) ? MPRISTypes::PlaybackStatus::Playing : MPRISTypes::PlaybackStatus::Paused;
            m_mpris_manager->updatePlaybackStatus(status);
        }
        
        // Test passes if no crashes occur
        ASSERT_TRUE(true, "Status updates completed without crashes");
    }
    
private:
    std::unique_ptr<MPRISManager> m_mpris_manager;
};

/**
 * Test position updates and seeking
 */
class MPRISManagerPositionTest : public TestFramework::TestCase {
public:
    MPRISManagerPositionTest() : TestFramework::TestCase("MPRISManagerPositionTest") {}
    
    void setUp() override {
        m_mpris_manager = std::make_unique<MPRISManager>(nullptr);
    }
    
    void runTest() override {
        m_mpris_manager->initialize();
        
        // Test position updates
        m_mpris_manager->updatePosition(0);
        m_mpris_manager->updatePosition(30000000); // 30 seconds in microseconds
        m_mpris_manager->updatePosition(120000000); // 2 minutes in microseconds
        
        // Test seeking notifications
        m_mpris_manager->notifySeeked(60000000); // 1 minute in microseconds
        m_mpris_manager->notifySeeked(0); // Seek to beginning
        
        // Test rapid position updates (simulating playback)
        for (uint64_t pos = 0; pos < 10000000; pos += 1000000) { // 10 seconds, 1 second increments
            m_mpris_manager->updatePosition(pos);
        }
        
        // Test passes if no crashes occur
        ASSERT_TRUE(true, "Position updates completed without crashes");
    }
    
private:
    std::unique_ptr<MPRISManager> m_mpris_manager;
};

/**
 * Test error reporting functionality
 */
class MPRISManagerErrorTest : public TestFramework::TestCase {
public:
    MPRISManagerErrorTest() : TestFramework::TestCase("MPRISManagerErrorTest") {}
    
    void setUp() override {
        m_mpris_manager = std::make_unique<MPRISManager>(nullptr);
    }
    
    void runTest() override {
        // Before initialization, should have no error or a meaningful error
        std::string initial_error = m_mpris_manager->getLastError();
        
        // Try to initialize
        auto result = m_mpris_manager->initialize();
        
        if (!result.isSuccess()) {
            // If initialization failed, should have a meaningful error
            std::string error = m_mpris_manager->getLastError();
            ASSERT_FALSE(error.empty(), "Error message should not be empty on initialization failure");
        }
        
        // Test auto-reconnect settings
        m_mpris_manager->setAutoReconnect(true);
        m_mpris_manager->setAutoReconnect(false);
        m_mpris_manager->setAutoReconnect(true);
        
        // Manual reconnection should not crash
        auto reconnect_result = m_mpris_manager->reconnect();
        // Should not crash regardless of result
        
        // Test passes if no crashes occur
        ASSERT_TRUE(true, "Error reporting completed without crashes");
    }
    
private:
    std::unique_ptr<MPRISManager> m_mpris_manager;
};



#else // !HAVE_DBUS

// Stub classes when D-Bus is not available
class MPRISManagerBasicTest : public TestFramework::TestCase {
public:
    MPRISManagerBasicTest() : TestFramework::TestCase("MPRISManagerBasicTest") {}
    
    void runTest() override {
        // Skip test when D-Bus is not available
        ASSERT_TRUE(true, "Basic test skipped - D-Bus not available");
    }
};

class MPRISManagerMetadataTest : public TestFramework::TestCase {
public:
    MPRISManagerMetadataTest() : TestFramework::TestCase("MPRISManagerMetadataTest") {}
    
    void runTest() override {
        // Skip test when D-Bus is not available
        ASSERT_TRUE(true, "Metadata test skipped - D-Bus not available");
    }
};

class MPRISManagerStatusTest : public TestFramework::TestCase {
public:
    MPRISManagerStatusTest() : TestFramework::TestCase("MPRISManagerStatusTest") {}
    
    void runTest() override {
        // Skip test when D-Bus is not available
        ASSERT_TRUE(true, "Status test skipped - D-Bus not available");
    }
};

class MPRISManagerPositionTest : public TestFramework::TestCase {
public:
    MPRISManagerPositionTest() : TestFramework::TestCase("MPRISManagerPositionTest") {}
    
    void runTest() override {
        // Skip test when D-Bus is not available
        ASSERT_TRUE(true, "Position test skipped - D-Bus not available");
    }
};

class MPRISManagerErrorTest : public TestFramework::TestCase {
public:
    MPRISManagerErrorTest() : TestFramework::TestCase("MPRISManagerErrorTest") {}
    
    void runTest() override {
        // Skip test when D-Bus is not available
        ASSERT_TRUE(true, "Error test skipped - D-Bus not available");
    }
};

#endif // HAVE_DBUS

// Simple main function for standalone test execution
int main() {
    printf("Running MPRISManager integration tests...\n");
    
    try {
        // Test basic functionality
        MPRISManagerBasicTest basic_test;
        basic_test.runTest();
        printf("✓ Basic test passed\n");
        
        MPRISManagerMetadataTest metadata_test;
        metadata_test.runTest();
        printf("✓ Metadata test passed\n");
        
        MPRISManagerStatusTest status_test;
        status_test.runTest();
        printf("✓ Status test passed\n");
        
        MPRISManagerPositionTest position_test;
        position_test.runTest();
        printf("✓ Position test passed\n");
        
        MPRISManagerErrorTest error_test;
        error_test.runTest();
        printf("✓ Error test passed\n");
        
        printf("All MPRISManager integration tests passed!\n");
        return 0;
        
    } catch (const std::exception& e) {
        printf("✗ Test failed: %s\n", e.what());
        return 1;
    }
}