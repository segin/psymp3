#ifndef MPRISTYPES_H
#define MPRISTYPES_H

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <exception>
#include <chrono>
#include <functional>

// Forward declarations for D-Bus types
struct DBusConnection;
struct DBusMessage;

namespace MPRISTypes {

// Error handling system

/**
 * MPRISError - Comprehensive exception hierarchy for MPRIS operations
 * 
 * Provides categorized error handling with context information and recovery hints.
 * All MPRIS operations should throw MPRISError or its derived classes for
 * consistent error handling throughout the system.
 */
class MPRISError : public std::exception {
public:
    /**
     * Error categories for different types of failures
     */
    enum class Category {
        Connection,     // D-Bus connection issues, network problems
        Message,        // Malformed messages, invalid parameters
        PlayerState,    // Invalid state transitions, resource conflicts
        Threading,      // Deadlocks, race conditions, synchronization issues
        Resource,       // Memory allocation, file access, system resources
        Protocol,       // MPRIS protocol violations, specification compliance
        Configuration,  // Invalid settings, missing dependencies
        Internal        // Internal logic errors, programming bugs
    };
    
    /**
     * Severity levels for error classification
     */
    enum class Severity {
        Info,           // Informational, operation can continue
        Warning,        // Warning, operation may be degraded
        Error,          // Error, operation failed but system stable
        Critical,       // Critical, system stability may be compromised
        Fatal           // Fatal, immediate shutdown required
    };
    
    /**
     * Recovery strategies for different error types
     */
    enum class RecoveryStrategy {
        None,           // No recovery possible
        Retry,          // Simple retry may succeed
        Reconnect,      // Reconnection required
        Reset,          // Component reset required
        Restart,        // Full restart required
        Degrade,        // Graceful degradation possible
        UserAction      // User intervention required
    };
    
    /**
     * Constructor with full error context
     */
    MPRISError(Category category, 
               Severity severity,
               const std::string& message,
               const std::string& context = "",
               RecoveryStrategy recovery = RecoveryStrategy::None,
               const std::string& details = "");
    
    /**
     * Simplified constructor for common cases
     */
    MPRISError(Category category, const std::string& message);
    
    // Exception interface
    const char* what() const noexcept override;
    
    // Error information accessors
    Category getCategory() const { return m_category; }
    Severity getSeverity() const { return m_severity; }
    const std::string& getMessage() const { return m_message; }
    const std::string& getContext() const { return m_context; }
    const std::string& getDetails() const { return m_details; }
    RecoveryStrategy getRecoveryStrategy() const { return m_recovery; }
    
    // Timestamp and error ID
    std::chrono::system_clock::time_point getTimestamp() const { return m_timestamp; }
    uint64_t getErrorId() const { return m_error_id; }
    
    // Utility methods
    std::string getCategoryString() const;
    std::string getSeverityString() const;
    std::string getRecoveryStrategyString() const;
    std::string getFullDescription() const;
    
    // Error chaining support
    void setCause(std::exception_ptr cause) { m_cause = cause; }
    std::exception_ptr getCause() const { return m_cause; }
    bool hasCause() const { return m_cause != nullptr; }
    
private:
    Category m_category;
    Severity m_severity;
    std::string m_message;
    std::string m_context;
    std::string m_details;
    RecoveryStrategy m_recovery;
    std::chrono::system_clock::time_point m_timestamp;
    uint64_t m_error_id;
    std::exception_ptr m_cause;
    
    mutable std::string m_what_cache; // Cache for what() method
    
    static uint64_t generateErrorId();
};

/**
 * Specialized exception classes for specific error categories
 */

class ConnectionError : public MPRISError {
public:
    ConnectionError(const std::string& message, const std::string& context = "")
        : MPRISError(Category::Connection, Severity::Error, message, context, RecoveryStrategy::Reconnect) {}
};

class MessageError : public MPRISError {
public:
    MessageError(const std::string& message, const std::string& context = "")
        : MPRISError(Category::Message, Severity::Warning, message, context, RecoveryStrategy::None) {}
};

class PlayerStateError : public MPRISError {
public:
    PlayerStateError(const std::string& message, const std::string& context = "")
        : MPRISError(Category::PlayerState, Severity::Error, message, context, RecoveryStrategy::Reset) {}
};

class ThreadingError : public MPRISError {
public:
    ThreadingError(const std::string& message, const std::string& context = "")
        : MPRISError(Category::Threading, Severity::Critical, message, context, RecoveryStrategy::Restart) {}
};

class ResourceError : public MPRISError {
public:
    ResourceError(const std::string& message, const std::string& context = "")
        : MPRISError(Category::Resource, Severity::Error, message, context, RecoveryStrategy::Retry) {}
};

class ProtocolError : public MPRISError {
public:
    ProtocolError(const std::string& message, const std::string& context = "")
        : MPRISError(Category::Protocol, Severity::Warning, message, context, RecoveryStrategy::Degrade) {}
};

/**
 * Error logging system with configurable detail levels
 */
class ErrorLogger {
public:
    enum class LogLevel {
        None = 0,       // No logging
        Fatal = 1,      // Only fatal errors
        Critical = 2,   // Critical and fatal
        Error = 3,      // Error, critical, and fatal
        Warning = 4,    // Warning and above
        Info = 5,       // Info and above
        Debug = 6,      // All messages including debug
        Trace = 7       // Maximum verbosity
    };
    
    /**
     * Log handler function type
     * Parameters: level, category, message, context, timestamp
     */
    using LogHandler = std::function<void(LogLevel, MPRISError::Category, 
                                         const std::string&, const std::string&,
                                         std::chrono::system_clock::time_point)>;
    
    // Singleton access
    static ErrorLogger& getInstance();
    
    // Configuration
    void setLogLevel(LogLevel level) { m_log_level = level; }
    LogLevel getLogLevel() const { return m_log_level; }
    
    void setLogHandler(LogHandler handler) { m_log_handler = handler; }
    void setDefaultLogHandler(); // Sets stderr-based handler
    
    // Logging methods
    void logError(const MPRISError& error);
    void logMessage(LogLevel level, MPRISError::Category category, 
                   const std::string& message, const std::string& context = "");
    
    // Convenience methods
    void logFatal(const std::string& message, const std::string& context = "");
    void logCritical(const std::string& message, const std::string& context = "");
    void logError(const std::string& message, const std::string& context = "");
    void logWarning(const std::string& message, const std::string& context = "");
    void logInfo(const std::string& message, const std::string& context = "");
    void logDebug(const std::string& message, const std::string& context = "");
    void logTrace(const std::string& message, const std::string& context = "");
    
    // Statistics
    struct ErrorStats {
        uint64_t total_errors = 0;
        uint64_t connection_errors = 0;
        uint64_t message_errors = 0;
        uint64_t player_state_errors = 0;
        uint64_t threading_errors = 0;
        uint64_t resource_errors = 0;
        uint64_t protocol_errors = 0;
        uint64_t configuration_errors = 0;
        uint64_t internal_errors = 0;
        std::chrono::system_clock::time_point last_error_time;
    };
    
    ErrorStats getErrorStats() const;
    void resetErrorStats();
    
private:
    ErrorLogger() = default;
    
    LogLevel m_log_level = LogLevel::Warning;
    LogHandler m_log_handler;
    mutable std::mutex m_mutex;
    ErrorStats m_stats;
    
    void updateStats(const MPRISError& error);
    LogLevel severityToLogLevel(MPRISError::Severity severity) const;
};

/**
 * Error recovery system for handling different failure types
 */
class ErrorRecoveryManager {
public:
    /**
     * Recovery action function type
     * Returns true if recovery was successful, false otherwise
     */
    using RecoveryAction = std::function<bool()>;
    
    /**
     * Recovery configuration for different error types
     */
    struct RecoveryConfig {
        int max_attempts = 3;
        std::chrono::milliseconds initial_delay{100};
        std::chrono::milliseconds max_delay{5000};
        double backoff_multiplier = 2.0;
        bool exponential_backoff = true;
    };
    
    ErrorRecoveryManager();
    
    // Configuration
    void setRecoveryConfig(MPRISError::Category category, const RecoveryConfig& config);
    RecoveryConfig getRecoveryConfig(MPRISError::Category category) const;
    
    void setRecoveryAction(MPRISError::RecoveryStrategy strategy, RecoveryAction action);
    
    // Recovery execution
    bool attemptRecovery(const MPRISError& error);
    bool attemptRecovery(MPRISError::RecoveryStrategy strategy, MPRISError::Category category);
    
    // Statistics
    struct RecoveryStats {
        uint64_t total_attempts = 0;
        uint64_t successful_recoveries = 0;
        uint64_t failed_recoveries = 0;
        std::map<MPRISError::Category, uint64_t> attempts_by_category;
        std::map<MPRISError::RecoveryStrategy, uint64_t> attempts_by_strategy;
    };
    
    RecoveryStats getRecoveryStats() const;
    void resetRecoveryStats();
    
private:
    std::map<MPRISError::Category, RecoveryConfig> m_recovery_configs;
    std::map<MPRISError::RecoveryStrategy, RecoveryAction> m_recovery_actions;
    std::map<MPRISError::Category, std::chrono::system_clock::time_point> m_last_attempt_times;
    std::map<MPRISError::Category, int> m_attempt_counts;
    
    mutable std::mutex m_mutex;
    RecoveryStats m_stats;
    
    bool shouldAttemptRecovery(MPRISError::Category category) const;
    std::chrono::milliseconds calculateDelay(MPRISError::Category category, int attempt) const;
    void updateStats(MPRISError::Category category, MPRISError::RecoveryStrategy strategy, bool success);
};

/**
 * Graceful degradation manager for handling service unavailability
 */
class GracefulDegradationManager {
public:
    enum class DegradationLevel {
        None,           // Full functionality
        Limited,        // Some features disabled
        Minimal,        // Only basic functionality
        Disabled        // Service completely disabled
    };
    
    GracefulDegradationManager();
    
    // Degradation control
    void setDegradationLevel(DegradationLevel level);
    DegradationLevel getDegradationLevel() const;
    
    // Feature availability checks
    bool isFeatureAvailable(const std::string& feature) const;
    void disableFeature(const std::string& feature);
    void enableFeature(const std::string& feature);
    
    // Automatic degradation based on error patterns
    void reportError(const MPRISError& error);
    void checkAutoDegradation();
    
    // Configuration
    void setErrorThreshold(MPRISError::Category category, int threshold);
    void setTimeWindow(std::chrono::seconds window);
    
private:
    DegradationLevel m_current_level = DegradationLevel::None;
    std::set<std::string> m_disabled_features;
    
    // Auto-degradation tracking
    std::map<MPRISError::Category, int> m_error_thresholds;
    std::map<MPRISError::Category, std::vector<std::chrono::system_clock::time_point>> m_recent_errors;
    std::chrono::seconds m_time_window{60}; // 1 minute window
    
    mutable std::mutex m_mutex;
    
    void updateDegradationLevel();
    void cleanupOldErrors();
};

// Enumerations for MPRIS protocol
enum class PlaybackStatus {
    Playing,
    Paused,
    Stopped
};

enum class LoopStatus {
    None,
    Track,
    Playlist
};

// DBus variant type for property values
struct DBusVariant {
    enum Type { 
        String = 0, 
        StringArray = 1, 
        Int64 = 2, 
        UInt64 = 3, 
        Double = 4, 
        Boolean = 5 
    } type;
    
    std::variant<std::string, std::vector<std::string>, int64_t, uint64_t, double, bool> value;
    
    // Default constructor
    DBusVariant() : type(String), value(std::string{}) {}
    
    // Constructors for different types
    explicit DBusVariant(const std::string& str) : type(String), value(str) {}
    explicit DBusVariant(const std::vector<std::string>& arr) : type(StringArray), value(arr) {}
    explicit DBusVariant(int64_t i) : type(Int64), value(i) {}
    explicit DBusVariant(uint64_t u) : type(UInt64), value(u) {}
    explicit DBusVariant(double d) : type(Double), value(d) {}
    explicit DBusVariant(bool b) : type(Boolean), value(b) {}
    
    // Type-safe getters
    template<typename T>
    const T& get() const {
        return std::get<T>(value);
    }
    
    // String conversion for debugging
    std::string toString() const;
};

// MPRIS metadata structure
struct MPRISMetadata {
    std::string artist;
    std::string title;
    std::string album;
    std::string track_id;
    uint64_t length_us = 0;
    std::string art_url;
    
    // Convert to D-Bus dictionary format
    std::map<std::string, DBusVariant> toDBusDict() const;
    
    // Clear all metadata
    void clear();
    
    // Check if metadata is empty
    bool isEmpty() const;
};

// RAII deleters for D-Bus resources
struct DBusConnectionDeleter {
    void operator()(DBusConnection* conn);
};

struct DBusMessageDeleter {
    void operator()(DBusMessage* msg);
};

// RAII wrappers for D-Bus resources
using DBusConnectionPtr = std::unique_ptr<DBusConnection, DBusConnectionDeleter>;
using DBusMessagePtr = std::unique_ptr<DBusMessage, DBusMessageDeleter>;

// Result template class for error handling
template<typename T>
class Result {
public:
    // Factory methods
    static Result success(T value) { 
        return Result(std::move(value)); 
    }
    
    static Result error(const std::string& message) { 
        return Result(ErrorTag{}, message); 
    }
    
    // State checking
    bool isSuccess() const { return m_success; }
    bool isError() const { return !m_success; }
    
    // Value access (throws if error)
    const T& getValue() const {
        if (!m_success) {
            throw std::runtime_error("Attempted to get value from error result: " + m_error);
        }
        return m_value;
    }
    
    T& getValue() {
        if (!m_success) {
            throw std::runtime_error("Attempted to get value from error result: " + m_error);
        }
        return m_value;
    }
    
    // Error access
    const std::string& getError() const { return m_error; }
    
    // Move value out (for efficiency)
    T moveValue() {
        if (!m_success) {
            throw std::runtime_error("Attempted to move value from error result: " + m_error);
        }
        return std::move(m_value);
    }
    
    // Conversion operators for convenience
    explicit operator bool() const { return m_success; }
    
private:
    // Private constructors to enforce factory pattern
    explicit Result(T value) : m_success(true), m_value(std::move(value)) {}
    
    // Use a tag to distinguish error constructor
    struct ErrorTag {};
    explicit Result(ErrorTag, const std::string& error) : m_success(false), m_error(error) {}
    
    bool m_success;
    T m_value;
    std::string m_error;
};

// Specialization for void results
template<>
class Result<void> {
public:
    static Result success() { 
        return Result(true); 
    }
    
    static Result error(const std::string& message) { 
        return Result(false, message); 
    }
    
    bool isSuccess() const { return m_success; }
    bool isError() const { return !m_success; }
    
    const std::string& getError() const { return m_error; }
    
    explicit operator bool() const { return m_success; }
    
private:
    explicit Result(bool success) : m_success(success) {}
    Result(bool success, const std::string& error) : m_success(success), m_error(error) {}
    
    bool m_success;
    std::string m_error;
};

// Utility functions for type conversions
std::string playbackStatusToString(PlaybackStatus status);
PlaybackStatus stringToPlaybackStatus(const std::string& str);

std::string loopStatusToString(LoopStatus status);
LoopStatus stringToLoopStatus(const std::string& str);

} // namespace MPRISTypes

#endif // MPRISTYPES_H