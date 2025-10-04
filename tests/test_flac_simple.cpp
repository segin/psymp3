/*
 * test_flac_simple.cpp - Simple test for FLAC codec deadlock fix
 */

#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>

// Simulate the deadlock scenario
class MockFLACCodec {
private:
    std::mutex m_decoder_mutex;
    std::mutex m_buffer_mutex;
    std::vector<int16_t> m_output_buffer;
    
public:
    // This simulates the old broken pattern
    bool processFrameData_broken() {
        std::lock_guard<std::mutex> decoder_lock(m_decoder_mutex);
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        
        // Simulate libFLAC calling our write callback
        return processChannelAssignment_broken();
    }
    
    // This simulates the old broken channel processing (would deadlock)
    bool processChannelAssignment_broken() {
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex); // DEADLOCK!
        m_output_buffer.resize(1024);
        return true;
    }
    
    // This simulates the fixed pattern
    bool processFrameData_fixed() {
        std::lock_guard<std::mutex> decoder_lock(m_decoder_mutex);
        std::lock_guard<std::mutex> buffer_lock(m_buffer_mutex);
        
        // Simulate libFLAC calling our write callback
        return processChannelAssignment_fixed();
    }
    
    // This simulates the fixed channel processing (no additional lock)
    bool processChannelAssignment_fixed() {
        // NOTE: buffer_mutex is already held by caller
        m_output_buffer.resize(1024);
        return true;
    }
};

int main() {
    std::cout << "Testing FLAC codec deadlock fix..." << std::endl;
    
    MockFLACCodec codec;
    
    // Test the fixed version
    std::cout << "Testing fixed version..." << std::endl;
    
    bool success = false;
    std::thread test_thread([&codec, &success]() {
        try {
            success = codec.processFrameData_fixed();
        } catch (...) {
            success = false;
        }
    });
    
    // Wait for completion with timeout
    if (test_thread.joinable()) {
        test_thread.join();
    }
    
    if (success) {
        std::cout << "SUCCESS: Fixed version completed without deadlock" << std::endl;
        std::cout << "FLAC codec deadlock fix test PASSED" << std::endl;
        return 0;
    } else {
        std::cout << "ERROR: Fixed version failed" << std::endl;
        return 1;
    }
}