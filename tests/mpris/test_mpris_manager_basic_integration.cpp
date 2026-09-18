/*
 * test_mpris_manager_basic_integration.cpp - Basic integration test for MPRISManager
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS

using namespace PsyMP3::MPRIS;

/**
 * Basic integration test for MPRISManager without Player dependencies
 */
void test_mpris_manager_basic_integration() {
    printf("Testing MPRISManager basic integration...\n");
    
    // Test construction with null player (should work for testing)
    MPRISManager manager(nullptr);
    
    // Initially not initialized
    if (manager.isInitialized()) {
        printf("✗ MPRISManager should not be initialized initially\n");
        exit(1);
    }
    
    if (manager.isConnected()) {
        printf("✗ MPRISManager should not be connected initially\n");
        exit(1);
    }
    
    // Test initialization (may fail if D-Bus is not available, which is OK)
    auto result = manager.initialize();
    
    if (result.isSuccess()) {
        printf("✓ MPRISManager initialized successfully\n");
        
        // Should be initialized now
        if (!manager.isInitialized()) {
            printf("✗ MPRISManager should be initialized after successful init\n");
            exit(1);
        }
        
        // Test metadata updates (should not crash)
        manager.updateMetadata("Test Artist", "Test Title", "Test Album");
        manager.updatePlaybackStatus(PlaybackStatus::Playing);
        manager.updatePosition(30000000); // 30 seconds
        manager.notifySeeked(60000000); // 1 minute
        
        // Test auto-reconnect settings
        manager.setAutoReconnect(true);
        manager.setAutoReconnect(false);
        manager.setAutoReconnect(true);
        
        // Test manual reconnection (should not crash)
        auto reconnect_result = manager.reconnect();
        
        // Test shutdown
        manager.shutdown();
        
        if (manager.isInitialized()) {
            printf("✗ MPRISManager should not be initialized after shutdown\n");
            exit(1);
        }
        
        printf("✓ MPRISManager integration test completed successfully\n");
        
    } else {
        printf("ℹ MPRISManager initialization failed (expected in test environment): %s\n", 
               result.getError().c_str());
        
        // Should have a meaningful error message
        if (result.getError().empty()) {
            printf("✗ Error message should not be empty on initialization failure\n");
            exit(1);
        }
        
        // Should not be initialized
        if (manager.isInitialized()) {
            printf("✗ MPRISManager should not be initialized after failed init\n");
            exit(1);
        }
        
        printf("✓ Initialization failure handled correctly\n");
    }
}

/**
 * Test component coordination without D-Bus connection
 */
void test_mpris_manager_component_coordination() {
    printf("Testing MPRISManager component coordination...\n");
    
    MPRISManager manager(nullptr);
    
    // Test operations before initialization (should not crash)
    manager.updateMetadata("", "", "");
    manager.updatePlaybackStatus(PlaybackStatus::Stopped);
    manager.updatePosition(0);
    manager.notifySeeked(0);
    
    // Test error reporting
    std::string error = manager.getLastError();
    // Error may be empty or contain a message, both are valid
    
    // Test multiple rapid updates (should not crash)
    for (int i = 0; i < 10; ++i) {
        manager.updateMetadata(
            "Artist " + std::to_string(i),
            "Title " + std::to_string(i),
            "Album " + std::to_string(i)
        );
        manager.updatePlaybackStatus(
            (i % 2 == 0) ? PlaybackStatus::Playing : PlaybackStatus::Paused
        );
        manager.updatePosition(i * 1000000); // i seconds
    }
    
    printf("✓ Component coordination test completed\n");
}

#else // !HAVE_DBUS

void test_mpris_manager_basic_integration() {
    printf("ℹ MPRISManager basic integration test skipped - D-Bus not available\n");
}

void test_mpris_manager_component_coordination() {
    printf("ℹ MPRISManager component coordination test skipped - D-Bus not available\n");
}

#endif // HAVE_DBUS

/**
 * Main function for basic integration test
 */
int main() {
    printf("Running MPRISManager basic integration tests...\n\n");
    
    try {
        test_mpris_manager_basic_integration();
        test_mpris_manager_component_coordination();
        
        printf("\n✓ All MPRISManager basic integration tests passed!\n");
        return 0;
        
    } catch (const std::exception& e) {
        printf("\n✗ Test failed with exception: %s\n", e.what());
        return 1;
    } catch (...) {
        printf("\n✗ Test failed with unknown exception\n");
        return 1;
    }
}