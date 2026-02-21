#ifndef MPRISMANAGER_H
#define MPRISMANAGER_H

// Forward declarations
class Player;

namespace PsyMP3 {
namespace MPRIS {
    class DBusConnectionManager;
    class SignalEmitter;
    class PropertyManager;
    class MethodHandler;
    enum class PlaybackStatus;
    template<typename T> class Result;
}
}

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

namespace PsyMP3 {
namespace MPRIS {

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
    PsyMP3::MPRIS::Result<void> initialize();
    
    /**
     * Shutdown MPRIS system and clean up all resources
     */
    void shutdown();
    
    /**
     * Update metadata for current track
     * @param artist Artist name
     * @param title Track title
     * @param album Album name
     * @param length_us Track length in microseconds (0 if unknown)
     */
    void updateMetadata(const std::string& artist, const std::string& title, const std::string& album, uint64_t length_us = 0);
    
    /**
     * Update playback status
     * @param status New playback status
     */
    void updatePlaybackStatus(PsyMP3::MPRIS::PlaybackStatus status);
    
    /**
     * Update current position
     * @param position_us Position in microseconds
     */
    void updatePosition(uint64_t position_us);

    /**
     * Update loop status
     * @param status New loop status
     */
    void updateLoopStatus(PsyMP3::MPRIS::LoopStatus status);
    
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
    PsyMP3::MPRIS::Result<void> reconnect();
    
    /**
     * Get current degradation level
     * @return Current degradation level
     */
    PsyMP3::MPRIS::GracefulDegradationManager::DegradationLevel getDegradationLevel() const;
    
    /**
     * Set degradation level manually
     * @param level New degradation level
     */
    void setDegradationLevel(PsyMP3::MPRIS::GracefulDegradationManager::DegradationLevel level);
    
    /**
     * Check if a specific feature is available at current degradation level
     * @param feature Feature name to check
     * @return true if feature is available
     */
    bool isFeatureAvailable(const std::string& feature) const;
    
    /**
     * Get error statistics
     * @return Current error statistics
     */
    PsyMP3::MPRIS::ErrorLogger::ErrorStats getErrorStats() const;
    
    /**
     * Get recovery statistics
     * @return Current recovery statistics
     */
    PsyMP3::MPRIS::ErrorRecoveryManager::RecoveryStats getRecoveryStats() const;
    
    /**
     * Reset error and recovery statistics
     */
    void resetStats();
    
    /**
     * Configure error logging level
     * @param level New logging level
     */
    void setLogLevel(PsyMP3::MPRIS::ErrorLogger::LogLevel level);
    
    /**
     * Report error to Player for user notification
     * @param error Error to report
     */
    void reportErrorToPlayer(const PsyMP3::MPRIS::MPRISError& error);

private:
    // Private implementations - assume locks are already held
    
    PsyMP3::MPRIS::Result<void> initialize_unlocked();
    void shutdown_unlocked();
    void updateMetadata_unlocked(const std::string& artist, const std::string& title, const std::string& album, uint64_t length_us);
    void updatePlaybackStatus_unlocked(PsyMP3::MPRIS::PlaybackStatus status);
    void updatePosition_unlocked(uint64_t position_us);
    void updateLoopStatus_unlocked(PsyMP3::MPRIS::LoopStatus status);
    void notifySeeked_unlocked(uint64_t position_us);
    bool isInitialized_unlocked() const;
    bool isConnected_unlocked() const;
    std::string getLastError_unlocked() const;
    void setAutoReconnect_unlocked(bool enable);
    PsyMP3::MPRIS::Result<void> reconnect_unlocked();
    
    // Error handling and recovery methods
    PsyMP3::MPRIS::GracefulDegradationManager::DegradationLevel getDegradationLevel_unlocked() const;
    void setDegradationLevel_unlocked(PsyMP3::MPRIS::GracefulDegradationManager::DegradationLevel level);
    bool isFeatureAvailable_unlocked(const std::string& feature) const;
    PsyMP3::MPRIS::ErrorLogger::ErrorStats getErrorStats_unlocked() const;
    PsyMP3::MPRIS::ErrorRecoveryManager::RecoveryStats getRecoveryStats_unlocked() const;
    void resetStats_unlocked();
    void setLogLevel_unlocked(PsyMP3::MPRIS::ErrorLogger::LogLevel level);
    void reportErrorToPlayer_unlocked(const PsyMP3::MPRIS::MPRISError& error);
    
    // Error handling utilities
    void handleError_unlocked(const PsyMP3::MPRIS::MPRISError& error);
    bool attemptErrorRecovery_unlocked(const PsyMP3::MPRIS::MPRISError& error);
    void configureErrorRecovery_unlocked();
    
    // Internal component management
    PsyMP3::MPRIS::Result<void> initializeComponents_unlocked();
    void shutdownComponents_unlocked();
    PsyMP3::MPRIS::Result<void> establishDBusConnection_unlocked();
    PsyMP3::MPRIS::Result<void> registerDBusService_unlocked();
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
    std::unique_ptr<PsyMP3::MPRIS::DBusConnectionManager> m_connection;
    std::unique_ptr<PsyMP3::MPRIS::PropertyManager> m_properties;
    std::unique_ptr<PsyMP3::MPRIS::MethodHandler> m_methods;
    std::unique_ptr<PsyMP3::MPRIS::SignalEmitter> m_signals;
    
    // Error handling and recovery systems
    PsyMP3::MPRIS::ErrorRecoveryManager m_recovery_manager;
    PsyMP3::MPRIS::GracefulDegradationManager m_degradation_manager;
    
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

} // namespace MPRIS
} // namespace PsyMP3
#endif // MPRISMANAGER_H