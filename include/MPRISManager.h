#ifndef MPRISMANAGER_H
#define MPRISMANAGER_H

// Forward declarations
class Player;

namespace MPRISTypes {
    class DBusConnectionManager;
    class SignalEmitter;
    enum class PlaybackStatus;
    template<typename T> class Result;
}

class PropertyManager;
class MethodHandler;

/**
 * MPRISManager - Central coordinator for MPRIS D-Bus integration
 * 
 * This class serves as the main interface that replaces the monolithic MPRIS class.
 * It coordinates all MPRIS components and follows the project's threading safety
 * guidelines with the public/private lock pattern.
 * 
 * Lock acquisition order (to prevent deadlocks):
 * 1. MPRISManager::m_mutex (this class)
 * 2. Component locks (DBusConnectionManager, PropertyManager, etc.)
 * 3. Player locks (when calling Player methods)
 */
class MPRISManager {
public:
    /**
     * Constructor
     * @param player Pointer to Player instance for integration (non-owning)
     */
    explicit MPRISManager(Player* player);
    
    /**
     * Destructor - ensures clean shutdown of all components
     */
    ~MPRISManager();
    
    // Non-copyable and non-movable for thread safety
    MPRISManager(const MPRISManager&) = delete;
    MPRISManager& operator=(const MPRISManager&) = delete;
    MPRISManager(MPRISManager&&) = delete;
    MPRISManager& operator=(MPRISManager&&) = delete;
    
    // Public API - acquires locks and calls private implementations
    
    /**
     * Initialize MPRIS system and establish D-Bus connection
     * @return Result indicating success or error message
     */
    MPRISTypes::Result<void> initialize();
    
    /**
     * Shutdown MPRIS system and clean up all resources
     */
    void shutdown();
    
    /**
     * Update metadata for current track
     * @param artist Artist name
     * @param title Track title
     * @param album Album name
     */
    void updateMetadata(const std::string& artist, const std::string& title, const std::string& album);
    
    /**
     * Update playback status
     * @param status New playback status
     */
    void updatePlaybackStatus(MPRISTypes::PlaybackStatus status);
    
    /**
     * Update current position
     * @param position_us Position in microseconds
     */
    void updatePosition(uint64_t position_us);
    
    /**
     * Notify that seeking occurred (emits Seeked signal)
     * @param position_us New position in microseconds
     */
    void notifySeeked(uint64_t position_us);
    
    /**
     * Check if MPRIS is initialized and ready
     * @return true if initialized and connected
     */
    bool isInitialized() const;
    
    /**
     * Check if D-Bus connection is active
     * @return true if connected to D-Bus
     */
    bool isConnected() const;
    
    /**
     * Get initialization error message if initialization failed
     * @return Error message or empty string if no error
     */
    std::string getLastError() const;
    
    /**
     * Enable or disable automatic reconnection on connection loss
     * @param enable true to enable auto-reconnect
     */
    void setAutoReconnect(bool enable);
    
    /**
     * Manually attempt reconnection to D-Bus
     * @return Result indicating success or error message
     */
    MPRISTypes::Result<void> reconnect();

private:
    // Private implementations - assume locks are already held
    
    MPRISTypes::Result<void> initialize_unlocked();
    void shutdown_unlocked();
    void updateMetadata_unlocked(const std::string& artist, const std::string& title, const std::string& album);
    void updatePlaybackStatus_unlocked(MPRISTypes::PlaybackStatus status);
    void updatePosition_unlocked(uint64_t position_us);
    void notifySeeked_unlocked(uint64_t position_us);
    bool isInitialized_unlocked() const;
    bool isConnected_unlocked() const;
    std::string getLastError_unlocked() const;
    void setAutoReconnect_unlocked(bool enable);
    MPRISTypes::Result<void> reconnect_unlocked();
    
    // Internal component management
    MPRISTypes::Result<void> initializeComponents_unlocked();
    void shutdownComponents_unlocked();
    MPRISTypes::Result<void> establishDBusConnection_unlocked();
    MPRISTypes::Result<void> registerDBusService_unlocked();
    void unregisterDBusService_unlocked();
    
    // Connection monitoring and recovery
    void handleConnectionLoss_unlocked();
    void scheduleReconnection_unlocked();
    bool shouldAttemptReconnection_unlocked() const;
    
    // Error handling and logging
    void setLastError_unlocked(const std::string& error);
    void logError_unlocked(const std::string& context, const std::string& error);
    void logInfo_unlocked(const std::string& message);
    
    // Component coordination
    void emitPropertyChanges_unlocked();
    void updateComponentStates_unlocked();
    
    // Threading safety
    mutable std::mutex m_mutex;
    
    // Player reference (non-owning)
    Player* m_player;
    
    // MPRIS components (owned)
    std::unique_ptr<MPRISTypes::DBusConnectionManager> m_connection;
    std::unique_ptr<PropertyManager> m_properties;
    std::unique_ptr<MethodHandler> m_methods;
    std::unique_ptr<MPRISTypes::SignalEmitter> m_signals;
    
    // State management
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_shutdown_requested{false};
    std::string m_last_error;
    
    // Reconnection management
    bool m_auto_reconnect{true};
    std::chrono::steady_clock::time_point m_last_reconnect_attempt;
    int m_reconnect_attempt_count{0};
    
    // Configuration constants
    static constexpr std::chrono::seconds RECONNECT_INTERVAL{5};
    static constexpr int MAX_RECONNECT_ATTEMPTS{10};
    static constexpr const char* DBUS_SERVICE_NAME = "org.mpris.MediaPlayer2.psymp3";
    static constexpr const char* DBUS_OBJECT_PATH = "/org/mpris/MediaPlayer2";
    
    // Component initialization order (for proper dependency management)
    enum class InitializationPhase {
        None,
        Connection,
        Properties,
        Methods,
        Signals,
        Registration,
        Complete
    };
    
    InitializationPhase m_initialization_phase{InitializationPhase::None};
};

#endif // MPRISMANAGER_H