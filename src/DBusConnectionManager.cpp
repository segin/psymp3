#include "psymp3.h"

#ifdef HAVE_DBUS
#include <dbus/dbus.h>
#endif

#include <algorithm>
#include <sstream>

namespace MPRISTypes {

DBusConnectionManager::DBusConnectionManager() 
    : m_connection(nullptr)
    , m_last_reconnect_attempt(std::chrono::steady_clock::time_point{})
    , m_reconnect_attempt_count(0) {
}

DBusConnectionManager::~DBusConnectionManager() {
    disconnect();
}

Result<void> DBusConnectionManager::connect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return connect_unlocked();
}

void DBusConnectionManager::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    disconnect_unlocked();
}

bool DBusConnectionManager::isConnected() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return isConnected_unlocked();
}

DBusConnection* DBusConnectionManager::getConnection() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getConnection_unlocked();
}

void DBusConnectionManager::enableAutoReconnect(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    enableAutoReconnect_unlocked(enable);
}

Result<void> DBusConnectionManager::attemptReconnection() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return attemptReconnection_unlocked();
}

bool DBusConnectionManager::isAutoReconnectEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return isAutoReconnectEnabled_unlocked();
}

std::chrono::seconds DBusConnectionManager::getTimeSinceLastReconnectAttempt() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getTimeSinceLastReconnectAttempt_unlocked();
}

// Private implementations - assume locks are already held

Result<void> DBusConnectionManager::connect_unlocked() {
#ifndef HAVE_DBUS
    return Result<void>::error("D-Bus support not compiled in");
#else
    // If already connected, return success
    if (isConnected_unlocked()) {
        return Result<void>::success();
    }
    
    // Clean up any existing connection
    cleanupConnection_unlocked();
    
    // Attempt to establish new connection
    auto result = establishConnection_unlocked();
    if (result.isSuccess()) {
        m_connected = true;
        m_reconnect_attempt_count = 0; // Reset attempt count on success
    }
    
    return result;
#endif
}

void DBusConnectionManager::disconnect_unlocked() {
    cleanupConnection_unlocked();
    m_connected = false;
}

bool DBusConnectionManager::isConnected_unlocked() const {
#ifndef HAVE_DBUS
    return false;
#else
    return m_connected && m_connection && dbus_connection_get_is_connected(m_connection.get());
#endif
}

DBusConnection* DBusConnectionManager::getConnection_unlocked() {
    if (!isConnected_unlocked()) {
        return nullptr;
    }
    return m_connection.get();
}

void DBusConnectionManager::enableAutoReconnect_unlocked(bool enable) {
    m_auto_reconnect = enable;
}

Result<void> DBusConnectionManager::attemptReconnection_unlocked() {
#ifndef HAVE_DBUS
    return Result<void>::error("D-Bus support not compiled in");
#else
    // Check if we should attempt reconnection
    if (!shouldAttemptReconnect_unlocked()) {
        std::ostringstream oss;
        oss << "Reconnection not allowed: too many attempts (" 
            << m_reconnect_attempt_count << "/" << MAX_RECONNECT_ATTEMPTS 
            << ") or too soon since last attempt";
        return Result<void>::error(oss.str());
    }
    
    // Update attempt tracking
    updateReconnectAttemptTime_unlocked();
    m_reconnect_attempt_count++;
    
    // Disconnect existing connection
    disconnect_unlocked();
    
    // Attempt new connection
    return connect_unlocked();
#endif
}

bool DBusConnectionManager::isAutoReconnectEnabled_unlocked() const {
    return m_auto_reconnect;
}

std::chrono::seconds DBusConnectionManager::getTimeSinceLastReconnectAttempt_unlocked() const {
    if (m_last_reconnect_attempt == std::chrono::steady_clock::time_point{}) {
        return std::chrono::seconds{0};
    }
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_reconnect_attempt);
    return duration;
}

// Internal helper methods

void DBusConnectionManager::cleanupConnection_unlocked() {
#ifdef HAVE_DBUS
    if (m_connection) {
        // Unregister from D-Bus if we were registered
        if (dbus_connection_get_is_connected(m_connection.get())) {
            // Release the service name if we own it
            DBusError error;
            dbus_error_init(&error);
            
            int result = dbus_bus_release_name(m_connection.get(), DBUS_SERVICE_NAME, &error);
            if (dbus_error_is_set(&error)) {
                // Log error but continue cleanup
                dbus_error_free(&error);
            }
            (void)result; // Suppress unused variable warning
        }
        
        // Reset the connection pointer (RAII will handle cleanup)
        m_connection.reset();
    }
#endif
}

Result<void> DBusConnectionManager::establishConnection_unlocked() {
#ifndef HAVE_DBUS
    return Result<void>::error("D-Bus support not compiled in");
#else
    DBusError error;
    dbus_error_init(&error);
    
    // Connect to session bus
    DBusConnection* raw_connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set(&error)) {
        std::string error_msg = "Failed to connect to D-Bus session bus: ";
        error_msg += error.message;
        dbus_error_free(&error);
        return Result<void>::error(error_msg);
    }
    
    if (!raw_connection) {
        return Result<void>::error("Failed to connect to D-Bus session bus: null connection");
    }
    
    // Wrap in RAII pointer
    m_connection = DBusConnectionPtr(raw_connection);
    
    // Request service name
    int name_result = dbus_bus_request_name(m_connection.get(), DBUS_SERVICE_NAME, 
                                           DBUS_NAME_FLAG_REPLACE_EXISTING, &error);
    
    if (dbus_error_is_set(&error)) {
        std::string error_msg = "Failed to request D-Bus service name: ";
        error_msg += error.message;
        dbus_error_free(&error);
        cleanupConnection_unlocked();
        return Result<void>::error(error_msg);
    }
    
    // Check if we got the name
    if (name_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER && 
        name_result != DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER) {
        std::ostringstream oss;
        oss << "Failed to acquire D-Bus service name '" << DBUS_SERVICE_NAME 
            << "': result code " << name_result;
        cleanupConnection_unlocked();
        return Result<void>::error(oss.str());
    }
    
    // Set up connection for threading
    dbus_connection_set_exit_on_disconnect(m_connection.get(), FALSE);
    
    return Result<void>::success();
#endif
}

bool DBusConnectionManager::shouldAttemptReconnect_unlocked() const {
    // Check if we've exceeded maximum attempts
    if (m_reconnect_attempt_count >= MAX_RECONNECT_ATTEMPTS) {
        return false;
    }
    
    // Check if enough time has passed since last attempt
    auto time_since_last = getTimeSinceLastReconnectAttempt_unlocked();
    auto required_delay = calculateBackoffDelay_unlocked();
    
    return time_since_last >= required_delay;
}

std::chrono::seconds DBusConnectionManager::calculateBackoffDelay_unlocked() const {
    if (m_reconnect_attempt_count == 0) {
        return MIN_RECONNECT_INTERVAL;
    }
    
    // Exponential backoff: 2^attempt_count seconds, capped at MAX_RECONNECT_INTERVAL
    int delay_seconds = 1 << std::min(m_reconnect_attempt_count, 6); // Cap at 2^6 = 64 seconds
    auto delay = std::chrono::seconds(delay_seconds);
    
    return std::min(delay, MAX_RECONNECT_INTERVAL);
}

void DBusConnectionManager::updateReconnectAttemptTime_unlocked() {
    m_last_reconnect_attempt = std::chrono::steady_clock::now();
}

} // namespace MPRISTypes