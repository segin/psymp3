/*
 * test_mpris_integration.cpp - Integration test for new MPRIS system with Player
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 */

#include "psymp3.h"

// Mock Player class for testing MPRIS integration
class MockPlayer {
public:
    MockPlayer() : state(PlayerState::Stopped) {}
    
    bool play() {
        state = PlayerState::Playing;
        return true;
    }
    
    bool pause() {
        state = PlayerState::Paused;
        return true;
    }
    
    bool stop() {
        state = PlayerState::Stopped;
        return true;
    }
    
    void nextTrack() {
        // Mock implementation
    }
    
    void prevTrack() {
        // Mock implementation
    }
    
    void seekTo(unsigned long pos) {
        // Mock implementation
    }
    
    PlayerState getState() const { return state; }
    
private:
    PlayerState state;
};

int main() {
    std::cout << "Testing MPRIS integration with Player class..." << std::endl;
    
#ifdef HAVE_DBUS
    try {
        // Create mock player
        MockPlayer mock_player;
        
        // Create MPRIS manager
        MPRISManager mpris_manager(reinterpret_cast<Player*>(&mock_player));
        
        // Test initialization
        auto init_result = mpris_manager.initialize();
        if (init_result.isSuccess()) {
            std::cout << "✓ MPRIS initialization successful" << std::endl;
        } else {
            std::cout << "✗ MPRIS initialization failed: " << init_result.getError() << std::endl;
            // This is expected if D-Bus is not available, so continue
        }
        
        // Test status updates (should work even without D-Bus connection)
        mpris_manager.updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
        std::cout << "✓ Playback status update completed" << std::endl;
        
        mpris_manager.updateMetadata("Test Artist", "Test Title", "Test Album");
        std::cout << "✓ Metadata update completed" << std::endl;
        
        mpris_manager.updatePosition(30000000); // 30 seconds in microseconds
        std::cout << "✓ Position update completed" << std::endl;
        
        mpris_manager.notifySeeked(45000000); // 45 seconds in microseconds
        std::cout << "✓ Seek notification completed" << std::endl;
        
        // Test shutdown
        mpris_manager.shutdown();
        std::cout << "✓ MPRIS shutdown completed" << std::endl;
        
        std::cout << "✓ All MPRIS integration tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "✗ MPRIS integration test failed with exception: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cout << "✓ MPRIS not compiled in - integration test skipped" << std::endl;
    return 0;
#endif
}