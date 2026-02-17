/*
 * MPRISManager.cpp - MPRIS D-Bus integration coordinator implementation
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace MPRIS {

#ifdef HAVE_DBUS

MPRISManager::MPRISManager(Player* player)
    : m_player(player)
    , m_connection(nullptr)
    , m_properties(nullptr)
    , m_methods(nullptr)
    , m_signals(nullptr)
{
    // Logging system now integrated with PsyMP3 debug system
    
    // Initialize error logging system with default handler
    ErrorLogger::getInstance().setDefaultLogHandler();
    ErrorLogger::getInstance().setLogLevel(PsyMP3::MPRIS::ErrorLogger::LogLevel::Warning);
    
    // Configure error recovery system
    configureErrorRecovery_unlocked();
    
    if (!m_player) {
        setLastError_unlocked("Player instance cannot be null");
        MPRIS_LOG_ERROR("MPRISManager", "Player instance cannot be null");
        return;
    }
    
    MPRIS_LOG_INFO("MPRISManager", "MPRISManager created with comprehensive error handling and logging");
    logInfo_unlocked("MPRISManager created with comprehensive error handling");
}

MPRISManager::~MPRISManager() {
    shutdown();
    logInfo_unlocked("MPRISManager destroyed");
}

Result<void> MPRISManager::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return initialize_unlocked();
}

void MPRISManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    shutdown_unlocked();
}

void MPRISManager::updateMetadata(const std::string& artist, const std::string& title, const std::string& album, uint64_t length_us) {
    std::lock_guard<std::mutex> lock(m_mutex);
    updateMetadata_unlocked(artist, title, album, length_us);
}

void MPRISManager::updatePlaybackStatus(PlaybackStatus status) {
    std::lock_guard<std::mutex> lock(m_mutex);
    updatePlaybackStatus_unlocked(status);
}

void MPRISManager::updatePosition(uint64_t position_us) {
    std::lock_guard<std::mutex> lock(m_mutex);
    updatePosition_unlocked(position_us);
}

void MPRISManager::updateLoopStatus(PsyMP3::MPRIS::LoopStatus status) {
    std::lock_guard<std::mutex> lock(m_mutex);
    updateLoopStatus_unlocked(status);
}

void MPRISManager::notifySeeked(uint64_t position_us) {
    std::lock_guard<std::mutex> lock(m_mutex);
    notifySeeked_unlocked(position_us);
}

bool MPRISManager::isInitialized() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return isInitialized_unlocked();
}

bool MPRISManager::isConnected() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return isConnected_unlocked();
}

std::string MPRISManager::getLastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getLastError_unlocked();
}

void MPRISManager::setAutoReconnect(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    setAutoReconnect_unlocked(enable);
}

Result<void> MPRISManager::reconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return reconnect_unlocked();
}

GracefulDegradationManager::DegradationLevel MPRISManager::getDegradationLevel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getDegradationLevel_unlocked();
}

void MPRISManager::setDegradationLevel(GracefulDegradationManager::DegradationLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    setDegradationLevel_unlocked(level);
}

bool MPRISManager::isFeatureAvailable(const std::string& feature) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return isFeatureAvailable_unlocked(feature);
}

ErrorLogger::ErrorStats MPRISManager::getErrorStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getErrorStats_unlocked();
}

ErrorRecoveryManager::RecoveryStats MPRISManager::getRecoveryStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return getRecoveryStats_unlocked();
}

void MPRISManager::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    resetStats_unlocked();
}

void MPRISManager::setLogLevel(ErrorLogger::LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    setLogLevel_unlocked(level);
}

void MPRISManager::reportErrorToPlayer(const MPRISError& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    reportErrorToPlayer_unlocked(error);
}

// Private implementations

Result<void> MPRISManager::initialize_unlocked() {
    MPRIS_MEASURE_LOCK("MPRISManager::initialize");
    
    if (m_initialized.load()) {
        MPRIS_LOG_DEBUG("MPRISManager", "Already initialized, returning success");
        return Result<void>::success();
    }
    
    if (m_shutdown_requested.load()) {
        MPRIS_LOG_ERROR("MPRISManager", "Cannot initialize after shutdown requested");
        return Result<void>::error("Cannot initialize after shutdown requested");
    }
    
    MPRIS_LOG_INFO("MPRISManager", "Initializing MPRIS system");
    logInfo_unlocked("Initializing MPRIS system");
    
    // Initialize components in dependency order
    auto result = initializeComponents_unlocked();
    if (!result.isSuccess()) {
        MPRIS_LOG_ERROR("MPRISManager", "Component initialization failed: " + result.getError());
        setLastError_unlocked("Component initialization failed: " + result.getError());
        shutdownComponents_unlocked(); // Clean up partial initialization
        return result;
    }
    
    // Establish D-Bus connection
    result = establishDBusConnection_unlocked();
    if (!result.isSuccess()) {
        MPRIS_LOG_ERROR("MPRISManager", "D-Bus connection failed: " + result.getError());
        setLastError_unlocked("D-Bus connection failed: " + result.getError());
        shutdownComponents_unlocked();
        return result;
    }
    
    // Register D-Bus service
    result = registerDBusService_unlocked();
    if (!result.isSuccess()) {
        MPRIS_LOG_ERROR("MPRISManager", "D-Bus service registration failed: " + result.getError());
        setLastError_unlocked("D-Bus service registration failed: " + result.getError());
        shutdownComponents_unlocked();
        return result;
    }
    
    m_initialization_phase = InitializationPhase::Complete;
    m_initialized.store(true);
    
    MPRIS_LOG_INFO("MPRISManager", "MPRIS system initialized successfully");
    logInfo_unlocked("MPRIS system initialized successfully");
    return Result<void>::success();
}

void MPRISManager::shutdown_unlocked() {
    MPRIS_MEASURE_LOCK("MPRISManager::shutdown");
    
    if (m_shutdown_requested.load()) {
        MPRIS_LOG_DEBUG("MPRISManager", "Already shutting down");
        return; // Already shutting down
    }
    
    m_shutdown_requested.store(true);
    MPRIS_LOG_INFO("MPRISManager", "Shutting down MPRIS system");
    logInfo_unlocked("Shutting down MPRIS system");
    
    // Unregister D-Bus service first
    unregisterDBusService_unlocked();
    
    // Shutdown components in reverse order
    shutdownComponents_unlocked();
    
    m_initialized.store(false);
    m_initialization_phase = InitializationPhase::None;
    
    MPRIS_LOG_INFO("MPRISManager", "MPRIS system shutdown complete");
    logInfo_unlocked("MPRIS system shutdown complete");
}

void MPRISManager::updateMetadata_unlocked(const std::string& artist, const std::string& title, const std::string& album, uint64_t length_us) {
    MPRIS_MEASURE_LOCK("MPRISManager::updateMetadata");
    
    if (!isInitialized_unlocked() || !m_properties) {
        MPRIS_LOG_DEBUG("MPRISManager", "Cannot update metadata - not initialized or no properties manager");
        return;
    }
    
    // Check if metadata updates are available at current degradation level
    if (!isFeatureAvailable_unlocked("metadata_updates")) {
        MPRIS_LOG_DEBUG("MPRISManager", "Metadata updates disabled due to degradation level");
        return; // Feature disabled due to degradation
    }
    
    MPRIS_LOG_TRACE("MPRISManager", "Updating metadata: artist='" + artist + "', title='" + title + "', album='" + album + "', length='" + std::to_string(length_us) + "'");
    
    try {
        m_properties->updateMetadata(artist, title, album, length_us);
        emitPropertyChanges_unlocked();
        MPRIS_LOG_DEBUG("MPRISManager", "Metadata updated successfully");
    } catch (const std::exception& e) {
        MPRIS_LOG_ERROR("MPRISManager", "Failed to update metadata: " + std::string(e.what()));
        MPRISError error(
            MPRISError::Category::PlayerState,
            MPRISError::Severity::Warning,
            "Failed to update metadata: " + std::string(e.what()),
            "updateMetadata",
            MPRISError::RecoveryStrategy::Retry
        );
        handleError_unlocked(error);
    }
}

void MPRISManager::updatePlaybackStatus_unlocked(PlaybackStatus status) {
    if (!isInitialized_unlocked() || !m_properties) {
        return;
    }
    
    // Playback status updates are always allowed (core functionality)
    
    try {
        m_properties->updatePlaybackStatus(status);
        emitPropertyChanges_unlocked();
    } catch (const std::exception& e) {
        MPRISError error(
            MPRISError::Category::PlayerState,
            MPRISError::Severity::Error,
            "Failed to update playback status: " + std::string(e.what()),
            "updatePlaybackStatus",
            MPRISError::RecoveryStrategy::Retry
        );
        handleError_unlocked(error);
    }
}

void MPRISManager::updateLoopStatus_unlocked(PsyMP3::MPRIS::LoopStatus status) {
    if (!isInitialized_unlocked() || !m_properties) {
        return;
    }

    try {
        m_properties->updateLoopStatus(status);
        emitPropertyChanges_unlocked();
    } catch (const std::exception& e) {
        MPRISError error(
            MPRISError::Category::PlayerState,
            MPRISError::Severity::Error,
            "Failed to update loop status: " + std::string(e.what()),
            "updateLoopStatus",
            MPRISError::RecoveryStrategy::Retry
        );
        handleError_unlocked(error);
    }
}

void MPRISManager::updatePosition_unlocked(uint64_t position_us) {
    if (!isInitialized_unlocked() || !m_properties) {
        return;
    }
    
    try {
        m_properties->updatePosition(position_us);
        // Note: Position updates don't emit PropertyChanged signals by design
        // Only Seeked signals are emitted for position changes
    } catch (const std::exception& e) {
        logError_unlocked("updatePosition", e.what());
        handleConnectionLoss_unlocked();
    }
}

void MPRISManager::notifySeeked_unlocked(uint64_t position_us) {
    if (!isInitialized_unlocked() || !m_signals) {
        return;
    }
    
    // Check if seeking is available at current degradation level
    if (!isFeatureAvailable_unlocked("seeking")) {
        return; // Feature disabled due to degradation
    }
    
    try {
        // Update position first
        if (m_properties && isFeatureAvailable_unlocked("position_tracking")) {
            m_properties->updatePosition(position_us);
        }
        
        // Emit Seeked signal
        if (isFeatureAvailable_unlocked("signal_emission")) {
            auto result = m_signals->emitSeeked(position_us);
            if (!result.isSuccess()) {
                MPRISError error(
                    MPRISError::Category::Protocol,
                    MPRISError::Severity::Warning,
                    "Failed to emit Seeked signal: " + result.getError(),
                    "notifySeeked",
                    MPRISError::RecoveryStrategy::Retry
                );
                handleError_unlocked(error);
            }
        }
    } catch (const std::exception& e) {
        MPRISError error(
            MPRISError::Category::Protocol,
            MPRISError::Severity::Error,
            "Failed to notify seek: " + std::string(e.what()),
            "notifySeeked",
            MPRISError::RecoveryStrategy::Reconnect
        );
        handleError_unlocked(error);
    }
}

bool MPRISManager::isInitialized_unlocked() const {
    return m_initialized.load() && m_initialization_phase == InitializationPhase::Complete;
}

bool MPRISManager::isConnected_unlocked() const {
    return m_connection && m_connection->isConnected();
}

std::string MPRISManager::getLastError_unlocked() const {
    return m_last_error;
}

void MPRISManager::setAutoReconnect_unlocked(bool enable) {
    m_auto_reconnect = enable;
    if (m_connection) {
        m_connection->enableAutoReconnect(enable);
    }
}

Result<void> MPRISManager::reconnect_unlocked() {
    if (!m_connection) {
        return Result<void>::error("No connection manager available");
    }
    
    logInfo_unlocked("Attempting manual reconnection");
    
    auto result = m_connection->attemptReconnection();
    if (result.isSuccess()) {
        logInfo_unlocked("Manual reconnection successful");
        updateComponentStates_unlocked();
    } else {
        logError_unlocked("reconnect", "Manual reconnection failed: " + result.getError());
    }
    
    return result;
}

Result<void> MPRISManager::initializeComponents_unlocked() {
    try {
        // Initialize connection manager
        m_initialization_phase = InitializationPhase::Connection;
        m_connection = std::make_unique<DBusConnectionManager>();
        if (!m_connection) {
            return Result<void>::error("Failed to create DBusConnectionManager");
        }
        
        // Initialize property manager
        m_initialization_phase = InitializationPhase::Properties;
        m_properties = std::make_unique<PropertyManager>(m_player);
        if (!m_properties) {
            return Result<void>::error("Failed to create PropertyManager");
        }
        
        // Initialize method handler (skip if player is null for testing)
        m_initialization_phase = InitializationPhase::Methods;
        if (m_player) {
            try {
                m_methods = std::make_unique<MethodHandler>(m_player, m_properties.get());
                if (!m_methods) {
                    return Result<void>::error("Failed to create MethodHandler");
                }
            } catch (const std::exception& e) {
                return Result<void>::error("MethodHandler creation failed: " + std::string(e.what()));
            }
        } else {
            logInfo_unlocked("Skipping MethodHandler creation - no Player instance");
        }
        
        // Initialize signal emitter
        m_initialization_phase = InitializationPhase::Signals;
        m_signals = std::make_unique<SignalEmitter>(m_connection.get());
        if (!m_signals) {
            return Result<void>::error("Failed to create SignalEmitter");
        }
        
        logInfo_unlocked("All components initialized successfully");
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error("Exception during component initialization: " + std::string(e.what()));
    }
}

void MPRISManager::shutdownComponents_unlocked() {
    // Shutdown in reverse order of initialization
    
    if (m_signals) {
        try {
            m_signals->stop(true); // Wait for completion
            m_signals.reset();
        } catch (const std::exception& e) {
            logError_unlocked("shutdown", "SignalEmitter shutdown error: " + std::string(e.what()));
        }
    }
    
    if (m_methods) {
        try {
            m_methods.reset();
        } catch (const std::exception& e) {
            logError_unlocked("shutdown", "MethodHandler shutdown error: " + std::string(e.what()));
        }
    }
    
    if (m_properties) {
        try {
            m_properties.reset();
        } catch (const std::exception& e) {
            logError_unlocked("shutdown", "PropertyManager shutdown error: " + std::string(e.what()));
        }
    }
    
    if (m_connection) {
        try {
            m_connection->disconnect();
            m_connection.reset();
        } catch (const std::exception& e) {
            logError_unlocked("shutdown", "DBusConnectionManager shutdown error: " + std::string(e.what()));
        }
    }
    
    m_initialization_phase = InitializationPhase::None;
}

Result<void> MPRISManager::establishDBusConnection_unlocked() {
    if (!m_connection) {
        return Result<void>::error("Connection manager not initialized");
    }
    
    auto result = m_connection->connect();
    if (!result.isSuccess()) {
        return result;
    }
    
    // Start signal emitter after connection is established
    if (m_signals) {
        auto signal_result = m_signals->start();
        if (!signal_result.isSuccess()) {
            return Result<void>::error("Failed to start signal emitter: " + signal_result.getError());
        }
    }
    
    return Result<void>::success();
}

Result<void> MPRISManager::registerDBusService_unlocked() {
    if (!m_connection || !m_connection->isConnected()) {
        return Result<void>::error("Not connected to D-Bus");
    }
    
    DBusConnection* conn = m_connection->getConnection();
    if (!conn) {
        return Result<void>::error("Invalid D-Bus connection");
    }
    
    m_initialization_phase = InitializationPhase::Registration;
    
    // Request service name
    DBusError error;
    dbus_error_init(&error);
    
    int result = dbus_bus_request_name(conn, DBUS_SERVICE_NAME, 
                                      DBUS_NAME_FLAG_REPLACE_EXISTING, &error);
    
    if (dbus_error_is_set(&error)) {
        std::string error_msg = "Failed to request D-Bus name: " + std::string(error.message);
        dbus_error_free(&error);
        return Result<void>::error(error_msg);
    }
    
    if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        return Result<void>::error("Failed to become primary owner of D-Bus name");
    }
    
    // Register object path with method handler
    if (!dbus_connection_register_object_path(conn, DBUS_OBJECT_PATH, nullptr, nullptr)) {
        return Result<void>::error("Failed to register D-Bus object path");
    }
    
    logInfo_unlocked("D-Bus service registered successfully");
    return Result<void>::success();
}

void MPRISManager::unregisterDBusService_unlocked() {
    if (!m_connection || !m_connection->isConnected()) {
        return;
    }
    
    DBusConnection* conn = m_connection->getConnection();
    if (!conn) {
        return;
    }
    
    try {
        // Unregister object path
        dbus_connection_unregister_object_path(conn, DBUS_OBJECT_PATH);
        
        // Release service name
        DBusError error;
        dbus_error_init(&error);
        
        dbus_bus_release_name(conn, DBUS_SERVICE_NAME, &error);
        
        if (dbus_error_is_set(&error)) {
            logError_unlocked("unregisterDBusService", "Error releasing D-Bus name: " + std::string(error.message));
            dbus_error_free(&error);
        }
        
        logInfo_unlocked("D-Bus service unregistered");
        
    } catch (const std::exception& e) {
        logError_unlocked("unregisterDBusService", e.what());
    }
}

void MPRISManager::handleConnectionLoss_unlocked() {
    if (!m_auto_reconnect || m_shutdown_requested.load()) {
        return;
    }
    
    logInfo_unlocked("Handling D-Bus connection loss");
    
    if (shouldAttemptReconnection_unlocked()) {
        scheduleReconnection_unlocked();
    }
}

void MPRISManager::scheduleReconnection_unlocked() {
    m_last_reconnect_attempt = std::chrono::steady_clock::now();
    m_reconnect_attempt_count++;
    
    logInfo_unlocked("Scheduling reconnection attempt " + std::to_string(m_reconnect_attempt_count));
    
    // In a real implementation, you might want to use a timer or separate thread
    // For now, we'll just attempt immediate reconnection
    auto result = reconnect_unlocked();
    if (!result.isSuccess()) {
        logError_unlocked("scheduleReconnection", "Reconnection failed: " + result.getError());
    }
}

bool MPRISManager::shouldAttemptReconnection_unlocked() const {
    if (m_reconnect_attempt_count >= MAX_RECONNECT_ATTEMPTS) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_reconnect_attempt);
    
    return time_since_last >= RECONNECT_INTERVAL;
}

void MPRISManager::setLastError_unlocked(const std::string& error) {
    m_last_error = error;
}

void MPRISManager::logError_unlocked(const std::string& context, const std::string& error) {
    // Use the comprehensive error logging system
    ErrorLogger::getInstance().logError(error, "MPRISManager::" + context);
    
    // Also report to degradation manager for auto-degradation
    MPRISError mpris_error(PsyMP3::MPRIS::MPRISError::Category::Internal, error);
    m_degradation_manager.reportError(mpris_error);
}

void MPRISManager::logInfo_unlocked(const std::string& message) {
    // Use the comprehensive error logging system
    ErrorLogger::getInstance().logInfo(message, "MPRISManager");
}

void MPRISManager::emitPropertyChanges_unlocked() {
    if (!m_signals || !m_properties) {
        MPRIS_LOG_DEBUG("MPRISManager", "Cannot emit property changes - missing signals or properties manager");
        return;
    }
    
    try {
        // Get all current properties
        auto properties = m_properties->getAllProperties();
        
        MPRIS_LOG_TRACE("MPRISManager", "Emitting PropertiesChanged signal with " + std::to_string(properties.size()) + " properties");
        
        // Emit PropertiesChanged signal for Player interface
        auto result = m_signals->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
        if (!result.isSuccess()) {
            MPRIS_LOG_ERROR("MPRISManager", "Failed to emit PropertiesChanged: " + result.getError());
            logError_unlocked("emitPropertyChanges", "Failed to emit PropertiesChanged: " + result.getError());
        } else {
            MPRIS_LOG_TRACE("MPRISManager", "PropertiesChanged signal emitted successfully");
        }
    } catch (const std::exception& e) {
        MPRIS_LOG_ERROR("MPRISManager", "Exception in emitPropertyChanges: " + std::string(e.what()));
        logError_unlocked("emitPropertyChanges", e.what());
    }
}

void MPRISManager::updateComponentStates_unlocked() {
    // Update component states after reconnection
    if (m_connection && m_connection->isConnected()) {
        logInfo_unlocked("Connection restored, updating component states");
        
        // Reset reconnection counter on successful connection
        m_reconnect_attempt_count = 0;
        
        // Restart signal emitter if needed
        if (m_signals && !m_signals->isRunning()) {
            auto result = m_signals->start();
            if (!result.isSuccess()) {
                logError_unlocked("updateComponentStates", "Failed to restart signal emitter: " + result.getError());
            }
        }
    }
}

// Error handling and recovery method implementations

GracefulDegradationManager::DegradationLevel MPRISManager::getDegradationLevel_unlocked() const {
    return m_degradation_manager.getDegradationLevel();
}

void MPRISManager::setDegradationLevel_unlocked(GracefulDegradationManager::DegradationLevel level) {
    m_degradation_manager.setDegradationLevel(level);
    logInfo_unlocked("Degradation level set to " + std::to_string(static_cast<int>(level)));
}

bool MPRISManager::isFeatureAvailable_unlocked(const std::string& feature) const {
    return m_degradation_manager.isFeatureAvailable(feature);
}

ErrorLogger::ErrorStats MPRISManager::getErrorStats_unlocked() const {
    return ErrorLogger::getInstance().getErrorStats();
}

ErrorRecoveryManager::RecoveryStats MPRISManager::getRecoveryStats_unlocked() const {
    return m_recovery_manager.getRecoveryStats();
}

void MPRISManager::resetStats_unlocked() {
    ErrorLogger::getInstance().resetErrorStats();
    m_recovery_manager.resetRecoveryStats();
    logInfo_unlocked("Error and recovery statistics reset");
}

void MPRISManager::setLogLevel_unlocked(ErrorLogger::LogLevel level) {
    ErrorLogger::getInstance().setLogLevel(level);
    logInfo_unlocked("Log level set to " + std::to_string(static_cast<int>(level)));
}

void MPRISManager::reportErrorToPlayer_unlocked(const MPRISError& error) {
    if (!m_player) {
        return; // No player to report to
    }
    
    // Create a user-friendly error message based on error category and severity
    std::string user_message;
    
    switch (error.getCategory()) {
        case MPRISError::Category::Connection:
            user_message = "Media control integration temporarily unavailable";
            break;
        case MPRISError::Category::Message:
            user_message = "Media control communication error";
            break;
        case MPRISError::Category::PlayerState:
            user_message = "Media control state synchronization issue";
            break;
        case MPRISError::Category::Threading:
            user_message = "Media control system error - restarting";
            break;
        case MPRISError::Category::Resource:
            user_message = "Media control resource limitation";
            break;
        case MPRISError::Category::Protocol:
            user_message = "Media control protocol error";
            break;
        case MPRISError::Category::Configuration:
            user_message = "Media control configuration issue";
            break;
        case MPRISError::Category::Internal:
            user_message = "Internal media control error";
            break;
        default:
            user_message = "Media control system error";
            break;
    }
    
    // Only report critical and fatal errors to user to avoid spam
    if (error.getSeverity() >= MPRISError::Severity::Critical) {
        // In a real implementation, this would call a Player method to show user notification
        // For now, we'll log it as a user-facing message
        logError_unlocked("reportErrorToPlayer", "User notification: " + user_message);
        
        if (m_player) {
            m_player->showMPRISError(user_message);
        }
    }
}

void MPRISManager::handleError_unlocked(const MPRISError& error) {
    // Log the error
    ErrorLogger::getInstance().logError(error);
    
    // Report to degradation manager
    m_degradation_manager.reportError(error);
    
    // Attempt recovery if appropriate
    bool recovery_attempted = attemptErrorRecovery_unlocked(error);
    
    // Report to player if necessary
    if (!recovery_attempted || error.getSeverity() >= MPRISError::Severity::Critical) {
        reportErrorToPlayer_unlocked(error);
    }
    
    // Handle specific error categories
    switch (error.getCategory()) {
        case MPRISError::Category::Connection:
            handleConnectionLoss_unlocked();
            break;
            
        case MPRISError::Category::Threading:
            // Threading errors are serious - consider shutdown
            if (error.getSeverity() >= MPRISError::Severity::Critical) {
                logError_unlocked("handleError", "Critical threading error - initiating shutdown");
                shutdown_unlocked();
            }
            break;
            
        case MPRISError::Category::Internal:
            // Internal errors may require component reset
            if (error.getSeverity() >= MPRISError::Severity::Critical) {
                logError_unlocked("handleError", "Critical internal error - resetting components");
                shutdownComponents_unlocked();
                // Attempt to reinitialize after a delay
                scheduleReconnection_unlocked();
            }
            break;
            
        default:
            // Other errors are handled by the recovery system
            break;
    }
}

bool MPRISManager::attemptErrorRecovery_unlocked(const MPRISError& error) {
    if (error.getRecoveryStrategy() == MPRISError::RecoveryStrategy::None) {
        return false; // No recovery possible
    }
    
    logInfo_unlocked("Attempting error recovery for " + error.getCategoryString() + " error");
    
    bool success = m_recovery_manager.attemptRecovery(error);
    
    if (success) {
        logInfo_unlocked("Error recovery successful");
    } else {
        logError_unlocked("attemptErrorRecovery", "Error recovery failed");
    }
    
    return success;
}

void MPRISManager::configureErrorRecovery_unlocked() {
    // Configure recovery actions for different strategies
    
    // Reconnect strategy - attempt D-Bus reconnection
    m_recovery_manager.setRecoveryAction(
        MPRISError::RecoveryStrategy::Reconnect,
        [this]() -> bool {
            auto result = reconnect_unlocked();
            return result.isSuccess();
        }
    );
    
    // Reset strategy - reset components
    m_recovery_manager.setRecoveryAction(
        MPRISError::RecoveryStrategy::Reset,
        [this]() -> bool {
            try {
                shutdownComponents_unlocked();
                auto result = initializeComponents_unlocked();
                if (result.isSuccess()) {
                    result = establishDBusConnection_unlocked();
                    if (result.isSuccess()) {
                        result = registerDBusService_unlocked();
                    }
                }
                return result.isSuccess();
            } catch (const std::exception& e) {
                logError_unlocked("configureErrorRecovery", "Reset recovery failed: " + std::string(e.what()));
                return false;
            }
        }
    );
    
    // Restart strategy - full restart
    m_recovery_manager.setRecoveryAction(
        MPRISError::RecoveryStrategy::Restart,
        [this]() -> bool {
            try {
                shutdown_unlocked();
                auto result = initialize_unlocked();
                return result.isSuccess();
            } catch (const std::exception& e) {
                logError_unlocked("configureErrorRecovery", "Restart recovery failed: " + std::string(e.what()));
                return false;
            }
        }
    );
    
    // Degrade strategy - enable graceful degradation
    m_recovery_manager.setRecoveryAction(
        MPRISError::RecoveryStrategy::Degrade,
        [this]() -> bool {
            auto current_level = m_degradation_manager.getDegradationLevel();
            auto new_level = static_cast<GracefulDegradationManager::DegradationLevel>(
                std::min(static_cast<int>(current_level) + 1, 
                        static_cast<int>(GracefulDegradationManager::DegradationLevel::Disabled))
            );
            
            if (new_level != current_level) {
                m_degradation_manager.setDegradationLevel(new_level);
                logInfo_unlocked("Graceful degradation activated: level " + std::to_string(static_cast<int>(new_level)));
                return true;
            }
            
            return false; // Already at maximum degradation
        }
    );
    
    // Retry strategy - simple retry (handled by recovery manager automatically)
    m_recovery_manager.setRecoveryAction(
        MPRISError::RecoveryStrategy::Retry,
        [this]() -> bool {
            // For retry strategy, we don't do anything special here
            // The recovery manager handles the retry logic
            return true;
        }
    );
}

#else // !HAVE_DBUS

// Stub implementation when D-Bus is not available
MPRISManager::MPRISManager(Player* player) : m_player(player) {}
MPRISManager::~MPRISManager() {}

Result<void> MPRISManager::initialize() {
    return Result<void>::error("D-Bus support not compiled in");
}

void MPRISManager::shutdown() {}
void MPRISManager::updateMetadata(const std::string&, const std::string&, const std::string&, uint64_t) {}
void MPRISManager::updatePlaybackStatus(PlaybackStatus) {}
void MPRISManager::updateLoopStatus(PsyMP3::MPRIS::LoopStatus) {}
void MPRISManager::updatePosition(uint64_t) {}
void MPRISManager::notifySeeked(uint64_t) {}
bool MPRISManager::isInitialized() const { return false; }
bool MPRISManager::isConnected() const { return false; }
std::string MPRISManager::getLastError() const { return "D-Bus support not compiled in"; }
void MPRISManager::setAutoReconnect(bool) {}
Result<void> MPRISManager::reconnect() { return Result<void>::error("D-Bus support not compiled in"); }

#endif // HAVE_DBUS

} // namespace MPRIS
} // namespace PsyMP3
