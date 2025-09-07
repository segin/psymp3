#ifndef DBUSCONNECTIONMANAGER_H
#define DBUSCONNECTIONMANAGER_H

#include "MPRISTypes.h"
#include <mutex>
#include <chrono>
#include <atomic>

// Forward declarations for D-Bus types
struct DBusConnection;
struct DBusError;

namespace MPRISTypes {

/**
 * @brief Manages D-Bus connection lifecycle with automatic error recovery
 * 
 * This class handles D-Bus connection establishment, monitoring, and automatic
 * reconnection following the project's threading safety guidelines with the
 * public/private lock pattern.
 * 
 * Lock acquisition order (to prevent deadlocks):
 * 1. DBusConnectionManager::m_mutex (this class only uses one mutex)
 */
class DBusConnectionManager {
public:
    /**
     * @brief Constructor - initializes connection manager
     */
    DBusConnectionManager();
    
    /**
     * @brief Destructor - ensures clean disconnection
     */
    ~DBusConnectionManager();
    
    // Non-copyable and non-movable for thread safety
    DBusConnectionManager(const DBusConnectionManager&) = delete;
    DBusConnectionManager& operator=(const DBusConnectionManager&) = delete;
    DBusConnectionManager(DBusConnectionManager&&) = delete;
    DBusConnectionManager& operator=(DBusConnectionManager&&) = delete;
    
    /**
     * @brief Establish D-Bus connection
     * @return Result indicating success or error message
     */
    Result<void> connect();
    
    /**
     * @brief Disconnect from D-Bus
     */
    void disconnect();
    
    /**
     * @brief Check if currently connected to D-Bus
     * @return true if connected, false otherwise
     */
    bool isConnected() const;
    
    /**
     * @brief Get the raw D-Bus connection pointer
     * @return DBusConnection pointer or nullptr if not connected
     * @note Caller must not store this pointer - it may become invalid
     */
    DBusConnection* getConnection();
    
    /**
     * @brief Enable or disable automatic reconnection
     * @param enable true to enable auto-reconnect, false to disable
     */
    void enableAutoReconnect(bool enable);
    
    /**
     * @brief Attempt manual reconnection
     * @return Result indicating success or error message
     */
    Result<void> attemptReconnection();
    
    /**
     * @brief Check if auto-reconnect is enabled
     * @return true if auto-reconnect is enabled
     */
    bool isAutoReconnectEnabled() const;
    
    /**
     * @brief Get time since last reconnection attempt
     * @return Duration since last attempt, or zero if never attempted
     */
    std::chrono::seconds getTimeSinceLastReconnectAttempt() const;

private:
    // Private implementations - assume locks are already held
    Result<void> connect_unlocked();
    void disconnect_unlocked();
    bool isConnected_unlocked() const;
    DBusConnection* getConnection_unlocked();
    void enableAutoReconnect_unlocked(bool enable);
    Result<void> attemptReconnection_unlocked();
    bool isAutoReconnectEnabled_unlocked() const;
    std::chrono::seconds getTimeSinceLastReconnectAttempt_unlocked() const;
    
    // Internal helper methods
    void cleanupConnection_unlocked();
    Result<void> establishConnection_unlocked();
    bool shouldAttemptReconnect_unlocked() const;
    std::chrono::seconds calculateBackoffDelay_unlocked() const;
    void updateReconnectAttemptTime_unlocked();
    
    // Threading safety
    mutable std::mutex m_mutex;
    
    // Connection state
    DBusConnectionPtr m_connection;
    std::atomic<bool> m_connected{false};
    
    // Reconnection management
    bool m_auto_reconnect{true};
    std::chrono::steady_clock::time_point m_last_reconnect_attempt;
    int m_reconnect_attempt_count{0};
    
    // Configuration constants
    static constexpr std::chrono::seconds MIN_RECONNECT_INTERVAL{1};
    static constexpr std::chrono::seconds MAX_RECONNECT_INTERVAL{60};
    static constexpr int MAX_RECONNECT_ATTEMPTS{10};
    static constexpr const char* DBUS_SERVICE_NAME = "org.mpris.MediaPlayer2.psymp3";
    static constexpr const char* DBUS_OBJECT_PATH = "/org/mpris/MediaPlayer2";
};

} // namespace MPRISTypes

#endif // DBUSCONNECTIONMANAGER_H