/*
 * MPRISManager.cpp - MPRIS D-Bus integration coordinator implementation
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

#ifdef HAVE_DBUS

MPRISManager::MPRISManager(Player* player)
    : m_player(player)
    , m_connection(nullptr)
    , m_properties(nullptr)
    , m_methods(nullptr)
    , m_signals(nullptr)
{
    if (!m_player) {
        setLastError_unlocked("Player instance cannot be null");
        return;
    }
    
    logInfo_unlocked("MPRISManager created");
}

MPRISManager::~MPRISManager() {
    shutdown();
    logInfo_unlocked("MPRISManager destroyed");
}

MPRISTypes::Result<void> MPRISManager::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return initialize_unlocked();
}

void MPRISManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    shutdown_unlocked();
}

void MPRISManager::updateMetadata(const std::string& artist, const std::string& title, const std::string& album) {
    std::lock_guard<std::mutex> lock(m_mutex);
    updateMetadata_unlocked(artist, title, album);
}

void MPRISManager::updatePlaybackStatus(MPRISTypes::PlaybackStatus status) {
    std::lock_guard<std::mutex> lock(m_mutex);
    updatePlaybackStatus_unlocked(status);
}

void MPRISManager::updatePosition(uint64_t position_us) {
    std::lock_guard<std::mutex> lock(m_mutex);
    updatePosition_unlocked(position_us);
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

MPRISTypes::Result<void> MPRISManager::reconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return reconnect_unlocked();
}

// Private implementations

MPRISTypes::Result<void> MPRISManager::initialize_unlocked() {
    if (m_initialized.load()) {
        return MPRISTypes::Result<void>::success();
    }
    
    if (m_shutdown_requested.load()) {
        return MPRISTypes::Result<void>::error("Cannot initialize after shutdown requested");
    }
    
    logInfo_unlocked("Initializing MPRIS system");
    
    // Initialize components in dependency order
    auto result = initializeComponents_unlocked();
    if (!result.isSuccess()) {
        setLastError_unlocked("Component initialization failed: " + result.getError());
        shutdownComponents_unlocked(); // Clean up partial initialization
        return result;
    }
    
    // Establish D-Bus connection
    result = establishDBusConnection_unlocked();
    if (!result.isSuccess()) {
        setLastError_unlocked("D-Bus connection failed: " + result.getError());
        shutdownComponents_unlocked();
        return result;
    }
    
    // Register D-Bus service
    result = registerDBusService_unlocked();
    if (!result.isSuccess()) {
        setLastError_unlocked("D-Bus service registration failed: " + result.getError());
        shutdownComponents_unlocked();
        return result;
    }
    
    m_initialization_phase = InitializationPhase::Complete;
    m_initialized.store(true);
    
    logInfo_unlocked("MPRIS system initialized successfully");
    return MPRISTypes::Result<void>::success();
}

void MPRISManager::shutdown_unlocked() {
    if (m_shutdown_requested.load()) {
        return; // Already shutting down
    }
    
    m_shutdown_requested.store(true);
    logInfo_unlocked("Shutting down MPRIS system");
    
    // Unregister D-Bus service first
    unregisterDBusService_unlocked();
    
    // Shutdown components in reverse order
    shutdownComponents_unlocked();
    
    m_initialized.store(false);
    m_initialization_phase = InitializationPhase::None;
    
    logInfo_unlocked("MPRIS system shutdown complete");
}

void MPRISManager::updateMetadata_unlocked(const std::string& artist, const std::string& title, const std::string& album) {
    if (!isInitialized_unlocked() || !m_properties) {
        return;
    }
    
    try {
        m_properties->updateMetadata(artist, title, album);
        emitPropertyChanges_unlocked();
    } catch (const std::exception& e) {
        logError_unlocked("updateMetadata", e.what());
        handleConnectionLoss_unlocked();
    }
}

void MPRISManager::updatePlaybackStatus_unlocked(MPRISTypes::PlaybackStatus status) {
    if (!isInitialized_unlocked() || !m_properties) {
        return;
    }
    
    try {
        m_properties->updatePlaybackStatus(status);
        emitPropertyChanges_unlocked();
    } catch (const std::exception& e) {
        logError_unlocked("updatePlaybackStatus", e.what());
        handleConnectionLoss_unlocked();
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
    
    try {
        // Update position first
        if (m_properties) {
            m_properties->updatePosition(position_us);
        }
        
        // Emit Seeked signal
        auto result = m_signals->emitSeeked(position_us);
        if (!result.isSuccess()) {
            logError_unlocked("notifySeeked", "Failed to emit Seeked signal: " + result.getError());
        }
    } catch (const std::exception& e) {
        logError_unlocked("notifySeeked", e.what());
        handleConnectionLoss_unlocked();
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

MPRISTypes::Result<void> MPRISManager::reconnect_unlocked() {
    if (!m_connection) {
        return MPRISTypes::Result<void>::error("No connection manager available");
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

MPRISTypes::Result<void> MPRISManager::initializeComponents_unlocked() {
    try {
        // Initialize connection manager
        m_initialization_phase = InitializationPhase::Connection;
        m_connection = std::make_unique<MPRISTypes::DBusConnectionManager>();
        if (!m_connection) {
            return MPRISTypes::Result<void>::error("Failed to create DBusConnectionManager");
        }
        
        // Initialize property manager
        m_initialization_phase = InitializationPhase::Properties;
        m_properties = std::make_unique<PropertyManager>(m_player);
        if (!m_properties) {
            return MPRISTypes::Result<void>::error("Failed to create PropertyManager");
        }
        
        // Initialize method handler (skip if player is null for testing)
        m_initialization_phase = InitializationPhase::Methods;
        if (m_player) {
            try {
                m_methods = std::make_unique<MethodHandler>(m_player, m_properties.get());
                if (!m_methods) {
                    return MPRISTypes::Result<void>::error("Failed to create MethodHandler");
                }
            } catch (const std::exception& e) {
                return MPRISTypes::Result<void>::error("MethodHandler creation failed: " + std::string(e.what()));
            }
        } else {
            logInfo_unlocked("Skipping MethodHandler creation - no Player instance");
        }
        
        // Initialize signal emitter
        m_initialization_phase = InitializationPhase::Signals;
        m_signals = std::make_unique<MPRISTypes::SignalEmitter>(m_connection.get());
        if (!m_signals) {
            return MPRISTypes::Result<void>::error("Failed to create SignalEmitter");
        }
        
        logInfo_unlocked("All components initialized successfully");
        return MPRISTypes::Result<void>::success();
        
    } catch (const std::exception& e) {
        return MPRISTypes::Result<void>::error("Exception during component initialization: " + std::string(e.what()));
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

MPRISTypes::Result<void> MPRISManager::establishDBusConnection_unlocked() {
    if (!m_connection) {
        return MPRISTypes::Result<void>::error("Connection manager not initialized");
    }
    
    auto result = m_connection->connect();
    if (!result.isSuccess()) {
        return result;
    }
    
    // Start signal emitter after connection is established
    if (m_signals) {
        auto signal_result = m_signals->start();
        if (!signal_result.isSuccess()) {
            return MPRISTypes::Result<void>::error("Failed to start signal emitter: " + signal_result.getError());
        }
    }
    
    return MPRISTypes::Result<void>::success();
}

MPRISTypes::Result<void> MPRISManager::registerDBusService_unlocked() {
    if (!m_connection || !m_connection->isConnected()) {
        return MPRISTypes::Result<void>::error("Not connected to D-Bus");
    }
    
    DBusConnection* conn = m_connection->getConnection();
    if (!conn) {
        return MPRISTypes::Result<void>::error("Invalid D-Bus connection");
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
        return MPRISTypes::Result<void>::error(error_msg);
    }
    
    if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        return MPRISTypes::Result<void>::error("Failed to become primary owner of D-Bus name");
    }
    
    // Register object path with method handler
    if (!dbus_connection_register_object_path(conn, DBUS_OBJECT_PATH, nullptr, nullptr)) {
        return MPRISTypes::Result<void>::error("Failed to register D-Bus object path");
    }
    
    logInfo_unlocked("D-Bus service registered successfully");
    return MPRISTypes::Result<void>::success();
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
    std::string message = "MPRISManager::" + context + ": " + error;
    // In a real implementation, this would use the project's logging system
    fprintf(stderr, "[ERROR] %s\n", message.c_str());
}

void MPRISManager::logInfo_unlocked(const std::string& message) {
    std::string full_message = "MPRISManager: " + message;
    // In a real implementation, this would use the project's logging system
    fprintf(stdout, "[INFO] %s\n", full_message.c_str());
}

void MPRISManager::emitPropertyChanges_unlocked() {
    if (!m_signals || !m_properties) {
        return;
    }
    
    try {
        // Get all current properties
        auto properties = m_properties->getAllProperties();
        
        // Emit PropertiesChanged signal for Player interface
        auto result = m_signals->emitPropertiesChanged("org.mpris.MediaPlayer2.Player", properties);
        if (!result.isSuccess()) {
            logError_unlocked("emitPropertyChanges", "Failed to emit PropertiesChanged: " + result.getError());
        }
    } catch (const std::exception& e) {
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

#else // !HAVE_DBUS

// Stub implementation when D-Bus is not available
MPRISManager::MPRISManager(Player* player) : m_player(player) {}
MPRISManager::~MPRISManager() {}

MPRISTypes::Result<void> MPRISManager::initialize() {
    return MPRISTypes::Result<void>::error("D-Bus support not compiled in");
}

void MPRISManager::shutdown() {}
void MPRISManager::updateMetadata(const std::string&, const std::string&, const std::string&) {}
void MPRISManager::updatePlaybackStatus(MPRISTypes::PlaybackStatus) {}
void MPRISManager::updatePosition(uint64_t) {}
void MPRISManager::notifySeeked(uint64_t) {}
bool MPRISManager::isInitialized() const { return false; }
bool MPRISManager::isConnected() const { return false; }
std::string MPRISManager::getLastError() const { return "D-Bus support not compiled in"; }
void MPRISManager::setAutoReconnect(bool) {}
MPRISTypes::Result<void> MPRISManager::reconnect() { return MPRISTypes::Result<void>::error("D-Bus support not compiled in"); }

#endif // HAVE_DBUS