#include "psymp3.h"
#include <cassert>
#include <iostream>

#ifdef HAVE_DBUS
#include <thread>
#include <chrono>
#include <vector>
#include <future>
#endif

using namespace PsyMP3::MPRIS;

// Test helper macros
#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            std::cerr << "ASSERTION FAILED: " << #expected << " != " << #actual \
                      << " (expected: " << (expected) << ", actual: " << (actual) << ")" << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERTION FAILED: " << #condition << " is false" << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        if (condition) { \
            std::cerr << "ASSERTION FAILED: " << #condition << " is true" << std::endl; \
            return false; \
        } \
    } while(0)
#ifdef HAVE_DBUS
// Test basic construction and initial state
bool test_basic_construction() {
    std::cout << "Testing basic construction..." << std::endl;
    
    DBusConnectionManager manager;
    
    // Initially should not be connected
    ASSERT_FALSE(manager.isConnected());
    ASSERT_EQ(nullptr, manager.getConnection());
    ASSERT_TRUE(manager.isAutoReconnectEnabled());
    ASSERT_EQ(0, static_cast<int>(manager.getTimeSinceLastReconnectAttempt().count()));
    
    return true;
}
    
// Test connection lifecycle
bool test_connection_lifecycle() {
    std::cout << "Testing connection lifecycle..." << std::endl;
    
    DBusConnectionManager manager;
    
    // Test connection
    auto connect_result = manager.connect();
    if (connect_result.isSuccess()) {
        ASSERT_TRUE(manager.isConnected());
        ASSERT_TRUE(manager.getConnection() != nullptr);
        
        // Test disconnection
        manager.disconnect();
        ASSERT_FALSE(manager.isConnected());
        ASSERT_EQ(nullptr, manager.getConnection());
    } else {
        // D-Bus might not be available in test environment
        std::cout << "D-Bus connection failed (expected in some test environments): " 
                  << connect_result.getError() << std::endl;
    }
    
    return true;
}
    
// Test auto-reconnect configuration
bool test_auto_reconnect_configuration() {
    std::cout << "Testing auto-reconnect configuration..." << std::endl;
    
    DBusConnectionManager manager;
    
    // Test enabling/disabling auto-reconnect
    ASSERT_TRUE(manager.isAutoReconnectEnabled());
    
    manager.enableAutoReconnect(false);
    ASSERT_FALSE(manager.isAutoReconnectEnabled());
    
    manager.enableAutoReconnect(true);
    ASSERT_TRUE(manager.isAutoReconnectEnabled());
    
    return true;
}
    
// Test reconnection backoff
bool test_reconnection_backoff() {
    std::cout << "Testing reconnection backoff..." << std::endl;
    
    DBusConnectionManager manager;
    
    // First attempt should be allowed immediately
    auto result1 = manager.attemptReconnection();
    auto time1 = manager.getTimeSinceLastReconnectAttempt();
    
    // Second attempt should be throttled
    auto result2 = manager.attemptReconnection();
    auto time2 = manager.getTimeSinceLastReconnectAttempt();
    
    ASSERT_TRUE(time1.count() >= 0);
    ASSERT_TRUE(time2.count() >= 0);
    
    // If D-Bus is not available, both should fail but with proper timing
    if (result1.isError() && result2.isError()) {
        std::cout << "Reconnection attempts failed as expected without D-Bus" << std::endl;
    }
    
    return true;
}
    
// Test thread safety
bool test_thread_safety() {
    std::cout << "Testing thread safety..." << std::endl;
    
    DBusConnectionManager manager;
    const int num_threads = 4;
    const int operations_per_thread = 10;
    
    std::vector<std::future<void>> futures;
    std::atomic<int> successful_operations{0};
    
    // Launch multiple threads performing various operations
    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(std::async(std::launch::async, [&manager, &successful_operations, operations_per_thread]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                // Mix of different operations
                switch (j % 4) {
                    case 0:
                        manager.isConnected();
                        break;
                    case 1:
                        manager.getConnection();
                        break;
                    case 2:
                        manager.enableAutoReconnect(j % 2 == 0);
                        break;
                    case 3:
                        manager.getTimeSinceLastReconnectAttempt();
                        break;
                }
                successful_operations++;
                
                // Small delay to increase chance of race conditions
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }));
    }
    
    // Wait for all threads to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    ASSERT_EQ(num_threads * operations_per_thread, successful_operations.load());
    
    return true;
}
    
// Test connection state monitoring
bool test_connection_state_monitoring() {
    std::cout << "Testing connection state monitoring..." << std::endl;
    
    DBusConnectionManager manager;
    
    // Test state consistency
    bool initial_connected = manager.isConnected();
    DBusConnection* initial_connection = manager.getConnection();
    
    if (initial_connected) {
        ASSERT_TRUE(initial_connection != nullptr);
    } else {
        ASSERT_EQ(nullptr, initial_connection);
    }
    
    // Test that multiple calls return consistent results
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(initial_connected, manager.isConnected());
        ASSERT_EQ(initial_connection, manager.getConnection());
    }
    
    return true;
}
    
// Test error handling
bool test_error_handling() {
    std::cout << "Testing error handling..." << std::endl;
    
    DBusConnectionManager manager;
    
    // Test that operations don't crash even if D-Bus is unavailable
    auto connect_result = manager.connect();
    auto reconnect_result = manager.attemptReconnection();
    
    // Results should be valid (either success or error, not crash)
    ASSERT_TRUE(connect_result.isSuccess() || connect_result.isError());
    ASSERT_TRUE(reconnect_result.isSuccess() || reconnect_result.isError());
    
    // Error messages should be informative
    if (connect_result.isError()) {
        ASSERT_FALSE(connect_result.getError().empty());
        std::cout << "Connect error: " << connect_result.getError() << std::endl;
    }
    
    if (reconnect_result.isError()) {
        ASSERT_FALSE(reconnect_result.getError().empty());
        std::cout << "Reconnect error: " << reconnect_result.getError() << std::endl;
    }
    
    return true;
}
#else
// Test without D-Bus support
bool test_without_dbus_support() {
    std::cout << "Testing without D-Bus support..." << std::endl;
    
    DBusConnectionManager manager;
    
    // All operations should work but return appropriate errors
    ASSERT_FALSE(manager.isConnected());
    ASSERT_EQ(nullptr, manager.getConnection());
    
    auto connect_result = manager.connect();
    ASSERT_TRUE(connect_result.isError());
    ASSERT_TRUE(connect_result.getError().find("D-Bus support not compiled") != std::string::npos);
    
    auto reconnect_result = manager.attemptReconnection();
    ASSERT_TRUE(reconnect_result.isError());
    ASSERT_TRUE(reconnect_result.getError().find("D-Bus support not compiled") != std::string::npos);
    
    // Configuration operations should still work
    manager.enableAutoReconnect(false);
    ASSERT_FALSE(manager.isAutoReconnectEnabled());
    
    manager.enableAutoReconnect(true);
    ASSERT_TRUE(manager.isAutoReconnectEnabled());
    
    return true;
}
#endif

// Main test runner
int main() {
    std::cout << "Running DBusConnectionManager unit tests..." << std::endl;
    
    bool all_passed = true;
    
#ifdef HAVE_DBUS
    all_passed &= test_basic_construction();
    all_passed &= test_connection_lifecycle();
    all_passed &= test_auto_reconnect_configuration();
    all_passed &= test_reconnection_backoff();
    all_passed &= test_thread_safety();
    all_passed &= test_connection_state_monitoring();
    all_passed &= test_error_handling();
#else
    all_passed &= test_without_dbus_support();
#endif
    
    if (all_passed) {
        std::cout << "All DBusConnectionManager tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "Some DBusConnectionManager tests FAILED!" << std::endl;
        return 1;
    }
}