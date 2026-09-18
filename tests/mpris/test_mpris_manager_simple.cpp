/*
 * test_mpris_manager_simple.cpp - Simple standalone test for MPRISManager
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS

using namespace PsyMP3::MPRIS;

/**
 * Simple test for MPRISManager basic functionality
 */
void test_mpris_manager_basic() {
    printf("Testing MPRISManager basic functionality...\n");
    
    // Create MPRISManager with null player (should handle gracefully)
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
    
    // Try to initialize (may fail if D-Bus is not available)
    auto result = manager.initialize();
    
    if (result.isSuccess()) {
        printf("✓ MPRISManager initialized successfully\n");
        
        if (!manager.isInitialized()) {
            printf("✗ MPRISManager should be initialized after successful init\n");
            exit(1);
        }
        
        // Test metadata updates (should not crash)
        manager.updateMetadata("Test Artist", "Test Title", "Test Album");
        manager.updatePlaybackStatus(PlaybackStatus::Playing);
        manager.updatePosition(30000000); // 30 seconds
        manager.notifySeeked(60000000); // 1 minute
        
        // Test shutdown
        manager.shutdown();
        
        if (manager.isInitialized()) {
            printf("✗ MPRISManager should not be initialized after shutdown\n");
            exit(1);
        }
        
        printf("✓ MPRISManager shutdown successfully\n");
        
    } else {
        printf("ℹ MPRISManager initialization failed (expected in test environment): %s\n", 
               result.getError().c_str());
        
        if (result.getError().empty()) {
            printf("✗ Error message should not be empty on initialization failure\n");
            exit(1);
        }
        
        if (manager.isInitialized()) {
            printf("✗ MPRISManager should not be initialized after failed init\n");
            exit(1);
        }
    }
    
    printf("✓ Basic MPRISManager test passed\n");
}

/**
 * Test error handling and edge cases
 */
void test_mpris_manager_error_handling() {
    printf("Testing MPRISManager error handling...\n");
    
    MPRISManager manager(nullptr);
    
    // Test operations before initialization (should not crash)
    manager.updateMetadata("", "", "");
    manager.updatePlaybackStatus(PlaybackStatus::Stopped);
    manager.updatePosition(0);
    manager.notifySeeked(0);
    
    // Test auto-reconnect settings
    manager.setAutoReconnect(true);
    manager.setAutoReconnect(false);
    manager.setAutoReconnect(true);
    
    // Test manual reconnection (should not crash)
    auto reconnect_result = manager.reconnect();
    
    // Test error reporting
    std::string error = manager.getLastError();
    // Error may be empty or contain a message, both are valid
    
    printf("✓ Error handling test passed\n");
}

#else // !HAVE_DBUS

void test_mpris_manager_basic() {
    printf("ℹ MPRISManager basic test skipped - D-Bus not available\n");
}

void test_mpris_manager_error_handling() {
    printf("ℹ MPRISManager error handling test skipped - D-Bus not available\n");
}

#endif // HAVE_DBUS

/**
 * Simple main function for standalone test execution
 */
int main() {
    printf("Running simple MPRISManager tests...\n\n");
    
    try {
        test_mpris_manager_basic();
        test_mpris_manager_error_handling();
        
        printf("\n✓ All MPRISManager tests passed!\n");
        return 0;
        
    } catch (const std::exception& e) {
        printf("\n✗ Test failed with exception: %s\n", e.what());
        return 1;
    } catch (...) {
        printf("\n✗ Test failed with unknown exception\n");
        return 1;
    }
}