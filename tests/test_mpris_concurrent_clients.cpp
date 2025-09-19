/*
 * test_mpris_concurrent_clients.cpp - Test MPRIS with multiple concurrent D-Bus clients
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
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <future>

/**
 * @brief Simulates a D-Bus client that interacts with MPRIS
 */
class MPRISClient {
public:
    MPRISClient(int client_id) 
        : m_client_id(client_id), m_connection(nullptr), m_operations_completed(0), m_errors_encountered(0) {
        
        // Initialize D-Bus connection
        DBusError error;
        dbus_error_init(&error);
        
        m_connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
        if (dbus_error_is_set(&error)) {
            std::cerr << "Client " << client_id << ": Failed to connect to D-Bus: " << error.message << std::endl;
            dbus_error_free(&error);
            m_connection = nullptr;
        }
    }
    
    ~MPRISClient() {
        if (m_connection) {
            dbus_connection_unref(m_connection);
        }
    }
    
    bool isConnected() const {
        return m_connection != nullptr;
    }
    
    /**
     * @brief Run client operations for specified duration
     */
    void runOperations(std::chrono::milliseconds duration) {
        if (!m_connection) {
            std::cerr << "Client " << m_client_id << ": No D-Bus connection" << std::endl;
            return;
        }
        
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + duration;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> operation_dist(0, 6);
        std::uniform_int_distribution<> delay_dist(10, 100); // 10-100ms delays
        
        while (std::chrono::steady_clock::now() < end_time) {
            // Perform random MPRIS operation
            int operation = operation_dist(gen);
            
            switch (operation) {
                case 0: getPlaybackStatus(); break;
                case 1: getMetadata(); break;
                case 2: getPosition(); break;
                case 3: callPlay(); break;
                case 4: callPause(); break;
                case 5: callStop(); break;
                case 6: callSeek(); break;
            }
            
            m_operations_completed++;
            
            // Random delay between operations
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
        }
    }
    
    int getOperationsCompleted() const { return m_operations_completed; }
    int getErrorsEncountered() const { return m_errors_encountered; }

private:
    int m_client_id;
    DBusConnection* m_connection;
    std::atomic<int> m_operations_completed;
    std::atomic<int> m_errors_encountered;
    
    static constexpr const char* MPRIS_SERVICE_NAME = "org.mpris.MediaPlayer2.psymp3";
    static constexpr const char* MPRIS_OBJECT_PATH = "/org/mpris/MediaPlayer2";
    static constexpr const char* MPRIS_PLAYER_INTERFACE = "org.mpris.MediaPlayer2.Player";
    static constexpr const char* DBUS_PROPERTIES_INTERFACE = "org.freedesktop.DBus.Properties";
    
    bool getPlaybackStatus() {
        return getProperty("PlaybackStatus");
    }
    
    bool getMetadata() {
        return getProperty("Metadata");
    }
    
    bool getPosition() {
        return getProperty("Position");
    }
    
    bool getProperty(const char* property_name) {
        DBusMessage* msg = dbus_message_new_method_call(
            MPRIS_SERVICE_NAME,
            MPRIS_OBJECT_PATH,
            DBUS_PROPERTIES_INTERFACE,
            "Get"
        );
        
        if (!msg) {
            m_errors_encountered++;
            return false;
        }
        
        const char* interface = MPRIS_PLAYER_INTERFACE;
        if (!dbus_message_append_args(msg, 
                                     DBUS_TYPE_STRING, &interface,
                                     DBUS_TYPE_STRING, &property_name,
                                     DBUS_TYPE_INVALID)) {
            dbus_message_unref(msg);
            m_errors_encountered++;
            return false;
        }
        
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(
            m_connection, msg, 2000, nullptr // 2 second timeout
        );
        
        dbus_message_unref(msg);
        
        if (!reply) {
            m_errors_encountered++;
            return false;
        }
        
        dbus_message_unref(reply);
        return true;
    }
    
    bool callPlay() {
        return callMethod("Play");
    }
    
    bool callPause() {
        return callMethod("Pause");
    }
    
    bool callStop() {
        return callMethod("Stop");
    }
    
    bool callSeek() {
        DBusMessage* msg = dbus_message_new_method_call(
            MPRIS_SERVICE_NAME,
            MPRIS_OBJECT_PATH,
            MPRIS_PLAYER_INTERFACE,
            "Seek"
        );
        
        if (!msg) {
            m_errors_encountered++;
            return false;
        }
        
        // Seek by 5 seconds (5,000,000 microseconds)
        dbus_int64_t offset = 5000000;
        if (!dbus_message_append_args(msg, DBUS_TYPE_INT64, &offset, DBUS_TYPE_INVALID)) {
            dbus_message_unref(msg);
            m_errors_encountered++;
            return false;
        }
        
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(
            m_connection, msg, 2000, nullptr
        );
        
        dbus_message_unref(msg);
        
        if (!reply) {
            m_errors_encountered++;
            return false;
        }
        
        dbus_message_unref(reply);
        return true;
    }
    
    bool callMethod(const char* method_name) {
        DBusMessage* msg = dbus_message_new_method_call(
            MPRIS_SERVICE_NAME,
            MPRIS_OBJECT_PATH,
            MPRIS_PLAYER_INTERFACE,
            method_name
        );
        
        if (!msg) {
            m_errors_encountered++;
            return false;
        }
        
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(
            m_connection, msg, 2000, nullptr
        );
        
        dbus_message_unref(msg);
        
        if (!reply) {
            m_errors_encountered++;
            return false;
        }
        
        dbus_message_unref(reply);
        return true;
    }
};

/**
 * @brief Test coordinator for concurrent client testing
 */
class ConcurrentClientTester {
public:
    ConcurrentClientTester() : m_total_operations(0), m_total_errors(0) {}
    
    bool runTest(int num_clients, std::chrono::milliseconds test_duration) {
        std::cout << "Running concurrent client test with " << num_clients 
                  << " clients for " << test_duration.count() << "ms..." << std::endl;
        
        // Create clients
        std::vector<std::unique_ptr<MPRISClient>> clients;
        for (int i = 0; i < num_clients; ++i) {
            auto client = std::make_unique<MPRISClient>(i);
            if (!client->isConnected()) {
                std::cerr << "Failed to create client " << i << std::endl;
                return false;
            }
            clients.push_back(std::move(client));
        }
        
        std::cout << "Created " << clients.size() << " D-Bus clients" << std::endl;
        
        // Start all clients concurrently
        std::vector<std::future<void>> futures;
        auto start_time = std::chrono::steady_clock::now();
        
        for (auto& client : clients) {
            futures.push_back(std::async(std::launch::async, [&client, test_duration]() {
                client->runOperations(test_duration);
            }));
        }
        
        // Wait for all clients to complete
        for (auto& future : futures) {
            future.wait();
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Collect results
        for (const auto& client : clients) {
            m_total_operations += client->getOperationsCompleted();
            m_total_errors += client->getErrorsEncountered();
        }
        
        // Print results
        std::cout << std::endl << "Concurrent Client Test Results:" << std::endl;
        std::cout << "==============================" << std::endl;
        std::cout << "Test duration: " << actual_duration.count() << "ms" << std::endl;
        std::cout << "Number of clients: " << num_clients << std::endl;
        std::cout << "Total operations: " << m_total_operations << std::endl;
        std::cout << "Total errors: " << m_total_errors << std::endl;
        std::cout << "Operations per second: " << (m_total_operations * 1000.0 / actual_duration.count()) << std::endl;
        std::cout << "Error rate: " << (m_total_errors * 100.0 / m_total_operations) << "%" << std::endl;
        
        // Individual client results
        std::cout << std::endl << "Per-client results:" << std::endl;
        for (size_t i = 0; i < clients.size(); ++i) {
            std::cout << "Client " << i << ": " 
                      << clients[i]->getOperationsCompleted() << " operations, "
                      << clients[i]->getErrorsEncountered() << " errors" << std::endl;
        }
        
        // Test passes if error rate is below 5%
        double error_rate = (m_total_operations > 0) ? (m_total_errors * 100.0 / m_total_operations) : 0.0;
        return error_rate < 5.0;
    }

private:
    int m_total_operations;
    int m_total_errors;
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
        // Mock implementation
    }
    
    void prevTrack() {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Mock implementation
    }
    
    void seekTo(unsigned long pos) {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Mock implementation
    }
    
    PlayerState getState() const { 
        std::lock_guard<std::mutex> lock(m_mutex);
        return state; 
    }
    
private:
    mutable std::mutex m_mutex;
    PlayerState state;
};

int main(int argc, char* argv[]) {
    std::cout << "MPRIS Concurrent Clients Test" << std::endl;
    std::cout << "=============================" << std::endl;
    
    // Parse command line arguments
    int num_clients = 8;
    int test_duration_ms = 10000; // 10 seconds
    
    if (argc > 1) {
        num_clients = std::atoi(argv[1]);
        if (num_clients <= 0 || num_clients > 50) {
            std::cerr << "Invalid number of clients. Using default: 8" << std::endl;
            num_clients = 8;
        }
    }
    
    if (argc > 2) {
        test_duration_ms = std::atoi(argv[2]);
        if (test_duration_ms <= 0 || test_duration_ms > 60000) {
            std::cerr << "Invalid test duration. Using default: 10000ms" << std::endl;
            test_duration_ms = 10000;
        }
    }
    
    // Start MPRIS service
    MockPlayer mock_player;
    MPRISManager mpris_manager(reinterpret_cast<Player*>(&mock_player));
    
    auto init_result = mpris_manager.initialize();
    if (!init_result.isSuccess()) {
        std::cerr << "Failed to initialize MPRIS: " << init_result.getError() << std::endl;
        return 1;
    }
    
    std::cout << "MPRIS service initialized successfully" << std::endl;
    
    // Give service time to register
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Run concurrent client test
    ConcurrentClientTester tester;
    bool test_passed = tester.runTest(num_clients, std::chrono::milliseconds(test_duration_ms));
    
    // Shutdown MPRIS service
    mpris_manager.shutdown();
    
    if (test_passed) {
        std::cout << std::endl << "✓ Concurrent clients test PASSED!" << std::endl;
        std::cout << "MPRIS system handled multiple concurrent clients successfully." << std::endl;
        return 0;
    } else {
        std::cout << std::endl << "✗ Concurrent clients test FAILED!" << std::endl;
        std::cout << "MPRIS system had issues with concurrent client access." << std::endl;
        return 1;
    }
}

#else // !HAVE_DBUS

int main() {
    std::cout << "MPRIS concurrent clients test skipped (D-Bus not available)" << std::endl;
    return 0;
}

#endif // HAVE_DBUS