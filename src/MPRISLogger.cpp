#include "psymp3.h"

#ifdef HAVE_DBUS

#include <dbus/dbus.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace MPRIS {

// PerformanceMetrics implementation
std::string PerformanceMetrics::toString() const {
    std::ostringstream oss;
    oss << "PerformanceMetrics {\n"
        << "  lock_acquisitions: " << lock_acquisitions << "\n"
        << "  lock_contention_time_us: " << lock_contention_time_us << "\n"
        << "  dbus_messages_sent: " << dbus_messages_sent << "\n"
        << "  dbus_messages_received: " << dbus_messages_received << "\n"
        << "  property_updates: " << property_updates << "\n"
        << "  signal_emissions: " << signal_emissions << "\n"
        << "  connection_attempts: " << connection_attempts << "\n"
        << "  connection_failures: " << connection_failures << "\n"
        << "}";
    return oss.str();
}

// AtomicPerformanceMetrics implementation
void AtomicPerformanceMetrics::reset() {
    lock_acquisitions = 0;
    lock_contention_time_us = 0;
    dbus_messages_sent = 0;
    dbus_messages_received = 0;
    property_updates = 0;
    signal_emissions = 0;
    connection_attempts = 0;
    connection_failures = 0;
}

PerformanceMetrics AtomicPerformanceMetrics::snapshot() const {
    PerformanceMetrics result;
    result.lock_acquisitions = lock_acquisitions.load();
    result.lock_contention_time_us = lock_contention_time_us.load();
    result.dbus_messages_sent = dbus_messages_sent.load();
    result.dbus_messages_received = dbus_messages_received.load();
    result.property_updates = property_updates.load();
    result.signal_emissions = signal_emissions.load();
    result.connection_attempts = connection_attempts.load();
    result.connection_failures = connection_failures.load();
    return result;
}

// ConnectionState implementation
std::string ConnectionState::toString() const {
    std::ostringstream oss;
    const char* status_str = "UNKNOWN";
    switch (status) {
        case DISCONNECTED: status_str = "DISCONNECTED"; break;
        case CONNECTING: status_str = "CONNECTING"; break;
        case CONNECTED: status_str = "CONNECTED"; break;
        case RECONNECTING: status_str = "RECONNECTING"; break;
        case FAILED: status_str = "FAILED"; break;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto state_duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_state_change).count();
    auto activity_duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity).count();
    
    oss << "ConnectionState {\n"
        << "  status: " << status_str << "\n"
        << "  state_duration_seconds: " << state_duration << "\n"
        << "  last_activity_seconds_ago: " << activity_duration << "\n"
        << "  reconnect_attempts: " << reconnect_attempts << "\n"
        << "  last_error: \"" << last_error << "\"\n"
        << "}";
    return oss.str();
}

// MPRISLogger implementation
MPRISLogger& MPRISLogger::getInstance() {
    static MPRISLogger instance;
    return instance;
}

void MPRISLogger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_log_level = level;
    writeLog_unlocked(LogLevel::INFO, "Logger", "Log level changed to " + levelToString(level));
}

LogLevel MPRISLogger::getLogLevel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_log_level;
}

void MPRISLogger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_log_file) {
        m_log_file->close();
        m_log_file.reset();
    }
    
    if (!filename.empty()) {
        m_log_file = std::make_unique<std::ofstream>(filename, std::ios::app);
        if (!m_log_file->is_open()) {
            m_log_file.reset();
            writeLog_unlocked(LogLevel::ERROR, "Logger", "Failed to open log file: " + filename);
        } else {
            writeLog_unlocked(LogLevel::INFO, "Logger", "Log file opened: " + filename);
        }
    }
}

void MPRISLogger::enableConsoleOutput(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_console_output = enable;
    writeLog_unlocked(LogLevel::INFO, "Logger", 
                     std::string("Console output ") + (enable ? "enabled" : "disabled"));
}

void MPRISLogger::enableDebugMode(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_debug_mode = enable;
    writeLog_unlocked(LogLevel::INFO, "Logger", 
                     std::string("Debug mode ") + (enable ? "enabled" : "disabled"));
}

void MPRISLogger::enableMessageTracing(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_message_tracing = enable;
    writeLog_unlocked(LogLevel::INFO, "Logger", 
                     std::string("Message tracing ") + (enable ? "enabled" : "disabled"));
}

void MPRISLogger::enablePerformanceMetrics(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_performance_metrics = enable;
    if (enable) {
        m_metrics.reset();
    }
    writeLog_unlocked(LogLevel::INFO, "Logger", 
                     std::string("Performance metrics ") + (enable ? "enabled" : "disabled"));
}

void MPRISLogger::log(LogLevel level, const std::string& component, const std::string& message) {
    if (!isLevelEnabled(level)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    writeLog_unlocked(level, component, message);
}

void MPRISLogger::trace(const std::string& component, const std::string& message) {
    log(LogLevel::TRACE, component, message);
}

void MPRISLogger::debug(const std::string& component, const std::string& message) {
    log(LogLevel::DEBUG, component, message);
}

void MPRISLogger::info(const std::string& component, const std::string& message) {
    log(LogLevel::INFO, component, message);
}

void MPRISLogger::warn(const std::string& component, const std::string& message) {
    log(LogLevel::WARN, component, message);
}

void MPRISLogger::error(const std::string& component, const std::string& message) {
    log(LogLevel::ERROR, component, message);
}

void MPRISLogger::fatal(const std::string& component, const std::string& message) {
    log(LogLevel::FATAL, component, message);
}

void MPRISLogger::traceDBusMessage(const std::string& direction, DBusMessage* message, const std::string& context) {
    if (!isLevelEnabled(LogLevel::TRACE) || !m_message_tracing || !message) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string formatted_message = formatDBusMessage_unlocked(message);
    std::string full_message = direction + " D-Bus message";
    if (!context.empty()) {
        full_message += " (" + context + ")";
    }
    full_message += ": " + formatted_message;
    
    writeLog_unlocked(LogLevel::TRACE, "DBus", full_message);
}

void MPRISLogger::traceDBusConnection(const std::string& event, DBusConnection* connection, const std::string& details) {
    if (!isLevelEnabled(LogLevel::TRACE) || !m_message_tracing) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string message = "Connection " + event;
    if (connection) {
        message += " [" + std::to_string(reinterpret_cast<uintptr_t>(connection)) + "]";
    }
    if (!details.empty()) {
        message += ": " + details;
    }
    
    writeLog_unlocked(LogLevel::TRACE, "DBus", message);
}

void MPRISLogger::recordLockAcquisition(const std::string& lock_name, uint64_t wait_time_us) {
    if (!m_performance_metrics) {
        return;
    }
    
    m_metrics.lock_acquisitions++;
    m_metrics.lock_contention_time_us += wait_time_us;
    
    if (m_debug_mode && wait_time_us > 1000) { // Log if wait time > 1ms
        debug("Performance", "Lock contention on " + lock_name + ": " + std::to_string(wait_time_us) + "us");
    }
}

void MPRISLogger::recordDBusMessage(bool sent) {
    if (!m_performance_metrics) {
        return;
    }
    
    if (sent) {
        m_metrics.dbus_messages_sent++;
    } else {
        m_metrics.dbus_messages_received++;
    }
}

void MPRISLogger::recordPropertyUpdate() {
    if (m_performance_metrics) {
        m_metrics.property_updates++;
    }
}

void MPRISLogger::recordSignalEmission() {
    if (m_performance_metrics) {
        m_metrics.signal_emissions++;
    }
}

void MPRISLogger::recordConnectionAttempt(bool success) {
    if (!m_performance_metrics) {
        return;
    }
    
    m_metrics.connection_attempts++;
    if (!success) {
        m_metrics.connection_failures++;
    }
}

PerformanceMetrics MPRISLogger::getMetrics() const {
    return m_metrics.snapshot();
}

void MPRISLogger::resetMetrics() {
    if (m_performance_metrics) {
        m_metrics.reset();
        info("Performance", "Metrics reset");
    }
}

void MPRISLogger::updateConnectionState(ConnectionState::Status status, const std::string& details) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    bool state_changed = (m_connection_state.status != status);
    
    if (state_changed) {
        m_connection_state.last_state_change = now;
        if (status == ConnectionState::RECONNECTING) {
            m_connection_state.reconnect_attempts++;
        } else if (status == ConnectionState::CONNECTED) {
            m_connection_state.reconnect_attempts = 0;
        }
    }
    
    m_connection_state.status = status;
    m_connection_state.last_activity = now;
    
    if (!details.empty()) {
        m_connection_state.last_error = details;
    }
    
    if (state_changed || m_debug_mode) {
        std::string message = "Connection state: " + m_connection_state.toString();
        if (!details.empty()) {
            message += " - " + details;
        }
        writeLog_unlocked(LogLevel::INFO, "Connection", message);
    }
}

ConnectionState MPRISLogger::getConnectionState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connection_state;
}

void MPRISLogger::dumpState(const std::string& component, const std::unordered_map<std::string, std::string>& state) {
    if (!m_debug_mode) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "State dump for " << component << " {\n";
    for (const auto& [key, value] : state) {
        oss << "  " << key << ": " << value << "\n";
    }
    oss << "}";
    
    writeLog_unlocked(LogLevel::DEBUG, component, oss.str());
}

void MPRISLogger::dumpFullSystemState() {
    if (!m_debug_mode) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Dump performance metrics
    writeLog_unlocked(LogLevel::DEBUG, "System", "=== FULL SYSTEM STATE DUMP ===");
    writeLog_unlocked(LogLevel::DEBUG, "Performance", m_metrics.snapshot().toString());
    writeLog_unlocked(LogLevel::DEBUG, "Connection", m_connection_state.toString());
    
    // Call registered state dumpers
    for (const auto& [component, dumper] : m_state_dumpers) {
        try {
            auto state = dumper();
            std::ostringstream oss;
            oss << "Component state for " << component << " {\n";
            for (const auto& [key, value] : state) {
                oss << "  " << key << ": " << value << "\n";
            }
            oss << "}";
            writeLog_unlocked(LogLevel::DEBUG, component, oss.str());
        } catch (const std::exception& e) {
            writeLog_unlocked(LogLevel::ERROR, "System", 
                            "Failed to dump state for " + component + ": " + e.what());
        }
    }
    
    writeLog_unlocked(LogLevel::DEBUG, "System", "=== END SYSTEM STATE DUMP ===");
}

bool MPRISLogger::isLevelEnabled(LogLevel level) const {
    return level >= m_log_level;
}

std::string MPRISLogger::formatTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void MPRISLogger::writeLog_unlocked(LogLevel level, const std::string& component, const std::string& message) {
    std::string timestamp = formatTimestamp();
    std::string level_str = levelToString(level);
    
    std::ostringstream oss;
    oss << "[" << timestamp << "] [" << level_str << "] [" << component << "] " << message;
    std::string formatted_message = oss.str();
    
    // Write to console if enabled
    if (m_console_output) {
        if (level >= LogLevel::ERROR) {
            std::cerr << formatted_message << std::endl;
        } else {
            std::cout << formatted_message << std::endl;
        }
    }
    
    // Write to file if available
    if (m_log_file && m_log_file->is_open()) {
        *m_log_file << formatted_message << std::endl;
        m_log_file->flush();
    }
}

std::string MPRISLogger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        case LogLevel::OFF:   return "OFF  ";
        default: return "UNKNOWN";
    }
}

std::string MPRISLogger::formatDBusMessage_unlocked(DBusMessage* message) const {
    if (!message) {
        return "NULL message";
    }
    
    std::ostringstream oss;
    
    // Message type
    int type = dbus_message_get_type(message);
    const char* type_str = "UNKNOWN";
    switch (type) {
        case DBUS_MESSAGE_TYPE_METHOD_CALL: type_str = "METHOD_CALL"; break;
        case DBUS_MESSAGE_TYPE_METHOD_RETURN: type_str = "METHOD_RETURN"; break;
        case DBUS_MESSAGE_TYPE_ERROR: type_str = "ERROR"; break;
        case DBUS_MESSAGE_TYPE_SIGNAL: type_str = "SIGNAL"; break;
    }
    oss << "type=" << type_str;
    
    // Interface, member, path
    const char* interface = dbus_message_get_interface(message);
    const char* member = dbus_message_get_member(message);
    const char* path = dbus_message_get_path(message);
    const char* destination = dbus_message_get_destination(message);
    const char* sender = dbus_message_get_sender(message);
    
    if (interface) oss << " interface=" << interface;
    if (member) oss << " member=" << member;
    if (path) oss << " path=" << path;
    if (destination) oss << " dest=" << destination;
    if (sender) oss << " sender=" << sender;
    
    // Serial number
    oss << " serial=" << dbus_message_get_serial(message);
    
    return oss.str();
}

// LockTimer implementation
LockTimer::LockTimer(const std::string& lock_name) 
    : m_lock_name(lock_name), m_start_time(std::chrono::steady_clock::now()) {
}

LockTimer::~LockTimer() {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - m_start_time);
    MPRISLogger::getInstance().recordLockAcquisition(m_lock_name, duration.count());
}

} // namespace MPRIS

#endif // HAVE_DBUS