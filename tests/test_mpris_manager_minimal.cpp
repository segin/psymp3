/*
 * test_mpris_manager_minimal.cpp - Minimal test for MPRISManager core functionality
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS

using namespace PsyMP3::MPRIS;

/**
 * Test MPRISManager construction and basic state
 */
void test_mpris_manager_construction() {
    printf("Testing MPRISManager construction...\n");
    
    // Test construction with null player
    MPRISManager manager(nullptr);
    
    // Should not be initialized initially
    if (manager.isInitialized()) {
        printf("✗ MPRISManager should not be initialized on construction\n");
        exit(1);
    }
    
    if (manager.isConnected()) {
        printf("✗ MPRISManager should not be connected on construction\n");
        exit(1);
    }
    
    // Error message should be empty initially or contain a meaningful message
    std::string error = manager.getLastError();
    // Both empty and non-empty are valid here
    
    printf("✓ MPRISManager construction test passed\n");
}

/**
 * Test MPRISManager operations before initialization
 */
void test_mpris_manager_pre_init_operations() {
    printf("Testing MPRISManager operations before initialization...\n");
    
    MPRISManager manager(nullptr);
    
    // These operations should not crash even when not initialized
    manager.updateMetadata("Test Artist", "Test Title", "Test Album");
    manager.updatePlaybackStatus(PlaybackStatus::Playing);
    manager.updatePosition(30000000); // 30 seconds
    manager.notifySeeked(60000000); // 1 minute
    
    // Settings should work
    manager.setAutoReconnect(true);
    manager.setAutoReconnect(false);
    
    // Manual reconnection should not crash (but will likely fail)
    auto result = manager.reconnect();
    // Result can be success or failure, both are valid
    
    printf("✓ Pre-initialization operations test passed\n");
}

/**
 * Test MPRISManager shutdown without initialization
 */
void test_mpris_manager_shutdown_without_init() {
    printf("Testing MPRISManager shutdown without initialization...\n");
    
    MPRISManager manager(nullptr);
    
    // Shutdown should be safe even if never initialized
    manager.shutdown();
    
    // Should still not be initialized
    if (manager.isInitialized()) {
        printf("✗ MPRISManager should not be initialized after shutdown without init\n");
        exit(1);
    }
    
    printf("✓ Shutdown without initialization test passed\n");
}

/**
 * Test MPRISManager initialization attempt
 */
void test_mpris_manager_initialization() {
    printf("Testing MPRISManager initialization...\n");
    
    MPRISManager manager(nullptr);
    
    // Try to initialize
    auto result = manager.initialize();
    
    if (result.isSuccess()) {
        printf("✓ MPRISManager initialization succeeded\n");
        
        // Should be initialized now
        if (!manager.isInitialized()) {
            printf("✗ MPRISManager should be initialized after successful init\n");
            exit(1);
        }
        
        // Test operations after initialization
        manager.updateMetadata("Init Test Artist", "Init Test Title", "Init Test Album");
        manager.updatePlaybackStatus(PlaybackStatus::Paused);
        manager.updatePosition(120000000); // 2 minutes
        
        // Test shutdown after successful initialization
        manager.shutdown();
        
        if (manager.isInitialized()) {
            printf("✗ MPRISManager should not be initialized after shutdown\n");
            exit(1);
        }
        
        printf("✓ Initialization and shutdown cycle completed successfully\n");
        
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

#else // !HAVE_DBUS

void test_mpris_manager_construction() {
    printf("ℹ MPRISManager construction test skipped - D-Bus not available\n");
}

void test_mpris_manager_pre_init_operations() {
    printf("ℹ MPRISManager pre-init operations test skipped - D-Bus not available\n");
}

void test_mpris_manager_shutdown_without_init() {
    printf("ℹ MPRISManager shutdown test skipped - D-Bus not available\n");
}

void test_mpris_manager_initialization() {
    printf("ℹ MPRISManager initialization test skipped - D-Bus not available\n");
}

#endif // HAVE_DBUS

/**
 * Main function for minimal MPRISManager tests
 */
int main() {
    printf("Running minimal MPRISManager tests...\n\n");
    
    try {
        test_mpris_manager_construction();
        test_mpris_manager_pre_init_operations();
        test_mpris_manager_shutdown_without_init();
        test_mpris_manager_initialization();
        
        printf("\n✓ All minimal MPRISManager tests passed!\n");
        return 0;
        
    } catch (const std::exception& e) {
        printf("\n✗ Test failed with exception: %s\n", e.what());
        return 1;
    } catch (...) {
        printf("\n✗ Test failed with unknown exception\n");
        return 1;
    }
}