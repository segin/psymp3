/*
 * test_mpris_reconnection_behavior.cpp - Test MPRIS D-Bus reconnection behavior
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS

#include <dbus/dbus.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * @brief Test MPRIS reconnection behavior when D-Bus connection is lost
 */
class MPRISReconnectionTester {
public:
    MPRISReconnectionTester() : m_test_session_pid(-1) {}
    
    ~MPRISReconnectionTester() {
        cleanupTestSession();
    }
    
    bool runAllTests() {
        std::cout << "Running MPRIS reconnection behavior tests..." << std::endl;
        std::cout << "============================================" << std::endl;
        
        bool all_passed = true;
        
        // Test 1: Basic reconnection after connection loss
        if (!testBasicReconnection()) {
            std::cout << "✗ Basic reconnection test FAILED" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✓ Basic reconnection test PASSED" << std::endl;
        }
        
        // Test 2: Service restart simulation
        if (!testServiceRestart()) {
            std::cout << "✗ Service restart test FAILED" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✓ Service restart test PASSED" << std::endl;
        }
        
        // Test 3: Multiple reconnection cycles
        if (!testMultipleReconnections()) {
            std::cout << "✗ Multiple reconnections test FAILED" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✓ Multiple reconnections test PASSED" << std::endl;
        }
        
        // Test 4: Reconnection under load
        if (!testReconnectionUnderLoad()) {
            std::cout << "✗ Reconnection under load test FAILED" << std::endl;
            all_passed = false;
        } else {
            std::cout << "✓ Reconnection under load test PASSED" << std::endl;
        }
        
        return all_passed;
    }

private:
    pid_t m_test_session_pid;
    std::string m_test_session_address;
    
    bool startTestDBusSession() {
        // Create a temporary D-Bus session for testing
        std::string config_file = "/tmp/test_dbus_session_$$.conf";
        
        // Write D-Bus configuration
        std::ofstream config(config_file);
        config << "<!DOCTYPE busconfig PUBLIC \"-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN\"\n";
        config << " \"http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd\">\n";
        config << "<busconfig>\n";
        config << "  <type>session</type>\n";
        config << "  <listen>unix:tmpdir=/tmp</listen>\n";
        config << "  <standard_session_servicedirs />\n";
        config << "  <policy context=\"default\">\n";
        config << "    <allow send_destination=\"*\" eavesdrop=\"true\"/>\n";
        config << "    <allow eavesdrop=\"true\"/>\n";
        config << "    <allow own=\"*\"/>\n";
        config << "  </policy>\n";
        config << "</busconfig>\n";
        config.close();
        
        // Start D-Bus daemon
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            std::cerr << "Failed to create pipe" << std::endl;
            return false;
        }
        
        m_test_session_pid = fork();
        if (m_test_session_pid == -1) {
            std::cerr << "Failed to fork D-Bus daemon" << std::endl;
            close(pipefd[0]);
            close(pipefd[1]);
            return false;
        }
        
        if (m_test_session_pid == 0) {
            // Child process - run D-Bus daemon
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            
            execlp("dbus-daemon", "dbus-daemon", 
                   "--config-file", config_file.c_str(),
                   "--print-address", "--nofork", nullptr);
            
            // If we get here, exec failed
            std::cerr << "Failed to exec dbus-daemon" << std::endl;
            exit(1);
        }
        
        // Parent process - read address
        close(pipefd[1]);
        
        char address_buffer[256];
        ssize_t bytes_read = read(pipefd[0], address_buffer, sizeof(address_buffer) - 1);
        close(pipefd[0]);
        
        if (bytes_read > 0) {
            address_buffer[bytes_read] = '\0';
            // Remove newline if present
            if (address_buffer[bytes_read - 1] == '\n') {
                address_buffer[bytes_read - 1] = '\0';
            }
            m_test_session_address = address_buffer;
            
            // Set environment variable
            setenv("DBUS_SESSION_BUS_ADDRESS", m_test_session_address.c_str(), 1);
            
            std::cout << "Started test D-Bus session: " << m_test_session_address << std::endl;
            
            // Give daemon time to start
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Clean up config file
            unlink(config_file.c_str());
            
            return true;
        }
        
        std::cerr << "Failed to read D-Bus address" << std::endl;
        cleanupTestSession();
        return false;
    }
    
    void cleanupTestSession() {
        if (m_test_session_pid > 0) {
            kill(m_test_session_pid, SIGTERM);
            
            // Wait for process to exit
            int status;
            waitpid(m_test_session_pid, &status, 0);
            
            m_test_session_pid = -1;
        }
    }
    
    void killTestSession() {
        if (m_test_session_pid > 0) {
            kill(m_test_session_pid, SIGKILL);
            
            // Wait for process to exit
            int status;
            waitpid(m_test_session_pid, &status, 0);
            
            m_test_session_pid = -1;
        }
    }
    
    bool isServiceAvailable() {
        DBusError error;
        dbus_error_init(&error);
        
        DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
        if (dbus_error_is_set(&error)) {
            dbus_error_free(&error);
            return false;
        }
        
        if (!conn) return false;
        
        // Try to call a simple method
        DBusMessage* msg = dbus_message_new_method_call(
            "org.mpris.MediaPlayer2.psymp3",
            "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties",
            "Get"
        );
        
        if (!msg) {
            dbus_connection_unref(conn);
            return false;
        }
        
        const char* interface = "org.mpris.MediaPlayer2.Player";
        const char* property = "PlaybackStatus";
        
        dbus_message_append_args(msg, 
                                DBUS_TYPE_STRING, &interface,
                                DBUS_TYPE_STRING, &property,
                                DBUS_TYPE_INVALID);
        
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(
            conn, msg, 1000, nullptr
        );
        
        dbus_message_unref(msg);
        dbus_connection_unref(conn);
        
        if (reply) {
            dbus_message_unref(reply);
            return true;
        }
        
        return false;
    }
    
    bool testBasicReconnection() {
        std::cout << std::endl << "Testing basic reconnection..." << std::endl;
        
        // Start test D-Bus session
        if (!startTestDBusSession()) {
            std::cerr << "Failed to start test D-Bus session" << std::endl;
            return false;
        }
        
        // Create mock player and MPRIS manager
        MockPlayer mock_player;
        MPRISManager mpris_manager(reinterpret_cast<Player*>(&mock_player));
        
        // Initialize MPRIS
        auto init_result = mpris_manager.initialize();
        if (!init_result.isSuccess()) {
            std::cerr << "Failed to initialize MPRIS: " << init_result.getError() << std::endl;
            return false;
        }
        
        std::cout << "MPRIS initialized successfully" << std::endl;
        
        // Verify service is available
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (!isServiceAvailable()) {
            std::cerr << "MPRIS service not available after initialization" << std::endl;
            return false;
        }
        
        std::cout << "MPRIS service confirmed available" << std::endl;
        
        // Kill D-Bus session to simulate connection loss
        std::cout << "Simulating D-Bus connection loss..." << std::endl;
        killTestSession();
        
        // Wait a bit for connection loss to be detected
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Restart D-Bus session
        std::cout << "Restarting D-Bus session..." << std::endl;
        if (!startTestDBusSession()) {
            std::cerr << "Failed to restart test D-Bus session" << std::endl;
            return false;
        }
        
        // Wait for MPRIS to reconnect
        std::cout << "Waiting for MPRIS reconnection..." << std::endl;
        bool reconnected = false;
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (isServiceAvailable()) {
                reconnected = true;
                break;
            }
        }
        
        mpris_manager.shutdown();
        
        if (reconnected) {
            std::cout << "MPRIS successfully reconnected" << std::endl;
            return true;
        } else {
            std::cerr << "MPRIS failed to reconnect within timeout" << std::endl;
            return false;
        }
    }
    
    bool testServiceRestart() {
        std::cout << std::endl << "Testing service restart..." << std::endl;
        
        // Start test D-Bus session
        if (!startTestDBusSession()) {
            return false;
        }
        
        MockPlayer mock_player;
        MPRISManager mpris_manager(reinterpret_cast<Player*>(&mock_player));
        
        // Initialize and verify
        auto init_result = mpris_manager.initialize();
        if (!init_result.isSuccess()) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Simulate service restart by shutting down and reinitializing
        std::cout << "Simulating service restart..." << std::endl;
        mpris_manager.shutdown();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Reinitialize
        init_result = mpris_manager.initialize();
        if (!init_result.isSuccess()) {
            std::cerr << "Failed to reinitialize MPRIS after restart" << std::endl;
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        bool service_available = isServiceAvailable();
        mpris_manager.shutdown();
        
        if (service_available) {
            std::cout << "Service restart successful" << std::endl;
            return true;
        } else {
            std::cerr << "Service not available after restart" << std::endl;
            return false;
        }
    }
    
    bool testMultipleReconnections() {
        std::cout << std::endl << "Testing multiple reconnections..." << std::endl;
        
        if (!startTestDBusSession()) {
            return false;
        }
        
        MockPlayer mock_player;
        MPRISManager mpris_manager(reinterpret_cast<Player*>(&mock_player));
        
        auto init_result = mpris_manager.initialize();
        if (!init_result.isSuccess()) {
            return false;
        }
        
        const int num_cycles = 3;
        bool all_cycles_passed = true;
        
        for (int cycle = 0; cycle < num_cycles; ++cycle) {
            std::cout << "Reconnection cycle " << (cycle + 1) << "/" << num_cycles << std::endl;
            
            // Kill and restart D-Bus session
            killTestSession();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            if (!startTestDBusSession()) {
                all_cycles_passed = false;
                break;
            }
            
            // Wait for reconnection
            bool reconnected = false;
            for (int i = 0; i < 5; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                if (isServiceAvailable()) {
                    reconnected = true;
                    break;
                }
            }
            
            if (!reconnected) {
                std::cerr << "Failed to reconnect in cycle " << (cycle + 1) << std::endl;
                all_cycles_passed = false;
                break;
            }
            
            std::cout << "Cycle " << (cycle + 1) << " successful" << std::endl;
        }
        
        mpris_manager.shutdown();
        
        if (all_cycles_passed) {
            std::cout << "All reconnection cycles successful" << std::endl;
            return true;
        } else {
            std::cerr << "Some reconnection cycles failed" << std::endl;
            return false;
        }
    }
    
    bool testReconnectionUnderLoad() {
        std::cout << std::endl << "Testing reconnection under load..." << std::endl;
        
        if (!startTestDBusSession()) {
            return false;
        }
        
        MockPlayer mock_player;
        MPRISManager mpris_manager(reinterpret_cast<Player*>(&mock_player));
        
        auto init_result = mpris_manager.initialize();
        if (!init_result.isSuccess()) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Start load generation thread
        std::atomic<bool> stop_load{false};
        std::thread load_thread([&mpris_manager, &stop_load]() {
            while (!stop_load.load()) {
                // Generate load by updating properties
                mpris_manager.updatePlaybackStatus(MPRISTypes::PlaybackStatus::Playing);
                mpris_manager.updateMetadata("Test Artist", "Test Title", "Test Album");
                mpris_manager.updatePosition(1000000); // 1 second
                
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
        
        // Simulate connection loss under load
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "Simulating connection loss under load..." << std::endl;
        
        killTestSession();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        if (!startTestDBusSession()) {
            stop_load.store(true);
            load_thread.join();
            return false;
        }
        
        // Wait for reconnection under load
        bool reconnected = false;
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (isServiceAvailable()) {
                reconnected = true;
                break;
            }
        }
        
        // Stop load generation
        stop_load.store(true);
        load_thread.join();
        
        mpris_manager.shutdown();
        
        if (reconnected) {
            std::cout << "Reconnection under load successful" << std::endl;
            return true;
        } else {
            std::cerr << "Failed to reconnect under load" << std::endl;
            return false;
        }
    }
};

// Mock Player class for testing
class MockPlayer {
public:
    MockPlayer() : state(PlayerState::Stopped) {}
    
    bool play() { 
        std::lock_guard<std::mutex> lock(m_mutex);
        state = PlayerState::Playing; 
        return true; 
    }
    
    bool pause() { 
        std::lock_guard<std::mutex> lock(m_mutex);
        state = PlayerState::Paused; 
        return true; 
    }
    
    bool stop() { 
        std::lock_guard<std::mutex> lock(m_mutex);
        state = PlayerState::Stopped; 
        return true; 
    }
    
    void nextTrack() {
        std::lock_guard<std::mutex> lock(m_mutex);
    }
    
    void prevTrack() {
        std::lock_guard<std::mutex> lock(m_mutex);
    }
    
    void seekTo(unsigned long pos) {
        std::lock_guard<std::mutex> lock(m_mutex);
    }
    
    PlayerState getState() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return state; 
    }
    
private:
    mutable std::mutex m_mutex;
    PlayerState state;
};

int main() {
    std::cout << "MPRIS Reconnection Behavior Test" << std::endl;
    std::cout << "================================" << std::endl;
    
    // Check if we can run D-Bus daemon
    if (system("which dbus-daemon > /dev/null 2>&1") != 0) {
        std::cerr << "dbus-daemon not found. Cannot run reconnection tests." << std::endl;
        return 1;
    }
    
    MPRISReconnectionTester tester;
    bool all_passed = tester.runAllTests();
    
    if (all_passed) {
        std::cout << std::endl << "✓ All MPRIS reconnection tests PASSED!" << std::endl;
        std::cout << "MPRIS system handles connection loss and recovery correctly." << std::endl;
        return 0;
    } else {
        std::cout << std::endl << "✗ Some MPRIS reconnection tests FAILED!" << std::endl;
        std::cout << "MPRIS system has issues with connection recovery." << std::endl;
        return 1;
    }
}

#else // !HAVE_DBUS

int main() {
    std::cout << "MPRIS reconnection behavior test skipped (D-Bus not available)" << std::endl;
    return 0;
}

#endif // HAVE_DBUS