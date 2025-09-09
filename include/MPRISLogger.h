#ifndef MPRIS_LOGGER_H
#define MPRIS_LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <memory>
#include <functional>
#include <unordered_map>
#include <atomic>

// Forward declarations
struct DBusMessage;
struct DBusConnection;

namespace MPRIS {

enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5,
    OFF = 6
};

struct PerformanceMetrics {
    uint64_t lock_acquisitions{0};
    uint64_t lock_contention_time_us{0};
    uint64_t dbus_messages_sent{0};
    uint64_t dbus_messages_received{0};
    uint64_t property_updates{0};
    uint64_t signal_emissions{0};
    uint64_t connection_attempts{0};
    uint64_t connection_failures{0};
    
    std::string toString() const;
};

struct AtomicPerformanceMetrics {
    std::atomic<uint64_t> lock_acquisitions{0};
    std::atomic<uint64_t> lock_contention_time_us{0};
    std::atomic<uint64_t> dbus_messages_sent{0};
    std::atomic<uint64_t> dbus_messages_received{0};
    std::atomic<uint64_t> property_updates{0};
    std::atomic<uint64_t> signal_emissions{0};
    std::atomic<uint64_t> connection_attempts{0};
    std::atomic<uint64_t> connection_failures{0};
    
    void reset();
    PerformanceMetrics snapshot() const;
};

struct ConnectionState {
    enum Status {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        RECONNECTING,
        FAILED
    } status;
    
    std::chrono::steady_clock::time_point last_state_change;
    std::chrono::steady_clock::time_point last_activity;
    uint32_t reconnect_attempts;
    std::string last_error;
    
    ConnectionState() : status(DISCONNECTED), reconnect_attempts(0) {}
    std::string toString() const;
};

class MPRISLogger {
public:
    static MPRISLogger& getInstance();
    
    // Configuration
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;
    void setLogFile(const std::string& filename);
    void enableConsoleOutput(bool enable);
    void enableDebugMode(bool enable);
    void enableMessageTracing(bool enable);
    void enablePerformanceMetrics(bool enable);
    
    // Logging methods
    void log(LogLevel level, const std::string& component, const std::string& message);
    void trace(const std::string& component, const std::string& message);
    void debug(const std::string& component, const std::string& message);
    void info(const std::string& component, const std::string& message);
    void warn(const std::string& component, const std::string& message);
    void error(const std::string& component, const std::string& message);
    void fatal(const std::string& component, const std::string& message);
    
    // D-Bus message tracing
    void traceDBusMessage(const std::string& direction, DBusMessage* message, const std::string& context = "");
    void traceDBusConnection(const std::string& event, DBusConnection* connection, const std::string& details = "");
    
    // Performance metrics
    void recordLockAcquisition(const std::string& lock_name, uint64_t wait_time_us);
    void recordDBusMessage(bool sent);
    void recordPropertyUpdate();
    void recordSignalEmission();
    void recordConnectionAttempt(bool success);
    PerformanceMetrics getMetrics() const;
    void resetMetrics();
    
    // Connection state monitoring
    void updateConnectionState(ConnectionState::Status status, const std::string& details = "");
    ConnectionState getConnectionState() const;
    
    // Debug state dumping
    void dumpState(const std::string& component, const std::unordered_map<std::string, std::string>& state);
    void dumpFullSystemState();
    
    // Utility methods
    bool isLevelEnabled(LogLevel level) const;
    std::string formatTimestamp() const;
    
private:
    MPRISLogger() = default;
    ~MPRISLogger() = default;
    MPRISLogger(const MPRISLogger&) = delete;
    MPRISLogger& operator=(const MPRISLogger&) = delete;
    
    void writeLog_unlocked(LogLevel level, const std::string& component, const std::string& message);
    std::string levelToString(LogLevel level) const;
    std::string formatDBusMessage_unlocked(DBusMessage* message) const;
    
    mutable std::mutex m_mutex;
    LogLevel m_log_level{LogLevel::INFO};
    std::unique_ptr<std::ofstream> m_log_file;
    bool m_console_output{true};
    bool m_debug_mode{false};
    bool m_message_tracing{false};
    bool m_performance_metrics{false};
    
    AtomicPerformanceMetrics m_metrics;
    ConnectionState m_connection_state;
    
    // State dump callbacks
    std::unordered_map<std::string, std::function<std::unordered_map<std::string, std::string>()>> m_state_dumpers;
};

// RAII class for measuring lock contention
class LockTimer {
public:
    LockTimer(const std::string& lock_name);
    ~LockTimer();
    
private:
    std::string m_lock_name;
    std::chrono::steady_clock::time_point m_start_time;
};

// Convenience macros for logging
#define MPRIS_LOG_TRACE(component, message) \
    do { if (MPRIS::MPRISLogger::getInstance().isLevelEnabled(MPRIS::LogLevel::TRACE)) \
         MPRIS::MPRISLogger::getInstance().trace(component, message); } while(0)

#define MPRIS_LOG_DEBUG(component, message) \
    do { if (MPRIS::MPRISLogger::getInstance().isLevelEnabled(MPRIS::LogLevel::DEBUG)) \
         MPRIS::MPRISLogger::getInstance().debug(component, message); } while(0)

#define MPRIS_LOG_INFO(component, message) \
    MPRIS::MPRISLogger::getInstance().info(component, message)

#define MPRIS_LOG_WARN(component, message) \
    MPRIS::MPRISLogger::getInstance().warn(component, message)

#define MPRIS_LOG_ERROR(component, message) \
    MPRIS::MPRISLogger::getInstance().error(component, message)

#define MPRIS_LOG_FATAL(component, message) \
    MPRIS::MPRISLogger::getInstance().fatal(component, message)

#define MPRIS_TRACE_DBUS_MESSAGE(direction, message, context) \
    do { if (MPRIS::MPRISLogger::getInstance().isLevelEnabled(MPRIS::LogLevel::TRACE)) \
         MPRIS::MPRISLogger::getInstance().traceDBusMessage(direction, message, context); } while(0)

#define MPRIS_MEASURE_LOCK(lock_name) \
    MPRIS::LockTimer _lock_timer(lock_name)

} // namespace MPRIS

#endif // MPRIS_LOGGER_H