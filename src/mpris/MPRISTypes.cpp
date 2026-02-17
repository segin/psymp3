/*
 * MPRISTypes.cpp - MPRIS D-Bus type definitions and helpers
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace MPRIS {

// DBusVariant string conversion
std::string DBusVariant::toString() const {
  switch (type) {
  case String:
    return "\"" + std::get<std::string>(value) + "\"";
  case StringArray: {
    const auto &arr = std::get<std::vector<std::string>>(value);
    std::string result = "[";
    for (size_t i = 0; i < arr.size(); ++i) {
      if (i > 0)
        result += ", ";
      result += "\"" + arr[i] + "\"";
    }
    result += "]";
    return result;
  }
  case Int64:
    return std::to_string(std::get<int64_t>(value));
  case UInt64:
    return std::to_string(std::get<uint64_t>(value));
  case Double:
    return std::to_string(std::get<double>(value));
  case Boolean:
    return std::get<bool>(value) ? "true" : "false";
  case Dictionary: {
    const auto &dict = *std::get<std::shared_ptr<DBusDictionary>>(value);
    std::string result = "{";
    bool first = true;
    for (const auto &[key, val] : dict) {
      if (!first)
        result += ", ";
      result += "\"" + key + "\": " + val.toString();
      first = false;
    }
    result += "}";
    return result;
  }
  default:
    return "<unknown>";
  }
}

// MPRISMetadata implementation
std::map<std::string, DBusVariant> MPRISMetadata::toDBusDict() const {
  std::map<std::string, DBusVariant> dict;

  // Use insert() instead of operator[] to avoid ARM ABI warning about
  // parameter passing changes in GCC 7.1 for std::map with complex value types
  if (!artist.empty()) {
    dict.insert(std::make_pair(std::string("xesam:artist"),
                               DBusVariant(std::vector<std::string>{artist})));
  }
  if (!title.empty()) {
    dict.insert(std::make_pair(std::string("xesam:title"),
                               DBusVariant(std::string(title))));
  }
  if (!album.empty()) {
    dict.insert(std::make_pair(std::string("xesam:album"),
                               DBusVariant(std::string(album))));
  }
  if (!track_id.empty()) {
    dict.insert(std::make_pair(std::string("mpris:trackid"),
                               DBusVariant(std::string(track_id))));
  }
  if (length_us > 0) {
    dict.insert(std::make_pair(std::string("mpris:length"),
                               DBusVariant(static_cast<int64_t>(length_us))));
  }
  if (!art_url.empty()) {
    dict.insert(std::make_pair(std::string("mpris:artUrl"),
                               DBusVariant(std::string(art_url))));
  }

  return dict;
}

void MPRISMetadata::clear() {
  artist.clear();
  title.clear();
  album.clear();
  track_id.clear();
  length_us = 0;
  art_url.clear();
}

bool MPRISMetadata::isEmpty() const {
  return artist.empty() && title.empty() && album.empty() && track_id.empty() &&
         length_us == 0 && art_url.empty();
}

// RAII deleters implementation
void DBusConnectionDeleter::operator()([[maybe_unused]] DBusConnection *conn) {
#ifdef HAVE_DBUS
  if (conn) {
    dbus_connection_unref(conn);
  }
#endif
}

void DBusMessageDeleter::operator()([[maybe_unused]] DBusMessage *msg) {
#ifdef HAVE_DBUS
  if (msg) {
    dbus_message_unref(msg);
  }
#endif
}

// Utility functions for type conversions
std::string playbackStatusToString(PlaybackStatus status) {
  switch (status) {
  case PlaybackStatus::Playing:
    return "Playing";
  case PlaybackStatus::Paused:
    return "Paused";
  case PlaybackStatus::Stopped:
    return "Stopped";
  default:
    return "Stopped"; // Default fallback
  }
}

PlaybackStatus stringToPlaybackStatus(const std::string &str) {
  if (str == "Playing") {
    return PlaybackStatus::Playing;
  } else if (str == "Paused") {
    return PlaybackStatus::Paused;
  } else {
    return PlaybackStatus::Stopped; // Default fallback
  }
}

std::string loopStatusToString(LoopStatus status) {
  switch (status) {
  case LoopStatus::None:
    return "None";
  case LoopStatus::Track:
    return "Track";
  case LoopStatus::Playlist:
    return "Playlist";
  default:
    return "None"; // Default fallback
  }
}

LoopStatus stringToLoopStatus(const std::string &str) {
  if (str == "Track") {
    return LoopStatus::Track;
  } else if (str == "Playlist") {
    return LoopStatus::Playlist;
  } else {
    return LoopStatus::None; // Default fallback
  }
}

#ifdef HAVE_DBUS
// Append a DBusVariant to a DBusMessageIter recursively
void appendVariantToDBusIter(struct DBusMessageIter* iter, const DBusVariant& variant) {
  DBusMessageIter variant_iter;

  switch (variant.type) {
  case DBusVariant::String: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "s",
                                     &variant_iter);
    const std::string &str_val = variant.get<std::string>();
    const char *str_cstr = str_val.c_str();
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &str_cstr);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  case DBusVariant::StringArray: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "as",
                                     &variant_iter);
    DBusMessageIter array_iter;
    dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY, "s",
                                     &array_iter);

    const auto &str_array = variant.get<std::vector<std::string>>();
    for (const auto &str : str_array) {
      const char *str_cstr = str.c_str();
      dbus_message_iter_append_basic(&array_iter, DBUS_TYPE_STRING, &str_cstr);
    }

    dbus_message_iter_close_container(&variant_iter, &array_iter);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  case DBusVariant::Int64: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "x",
                                     &variant_iter);
    dbus_int64_t int_val = static_cast<dbus_int64_t>(variant.get<int64_t>());
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_INT64, &int_val);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  case DBusVariant::UInt64: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "t",
                                     &variant_iter);
    dbus_uint64_t uint_val =
        static_cast<dbus_uint64_t>(variant.get<uint64_t>());
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_UINT64, &uint_val);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  case DBusVariant::Double: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "d",
                                     &variant_iter);
    double double_val = variant.get<double>();
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_DOUBLE,
                                   &double_val);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  case DBusVariant::Boolean: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "b",
                                     &variant_iter);
    dbus_bool_t bool_val = variant.get<bool>() ? TRUE : FALSE;
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_val);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  case DBusVariant::Dictionary: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "a{sv}",
                                     &variant_iter);
    DBusMessageIter dict_iter;
    dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY, "{sv}",
                                     &dict_iter);

    const auto &dict =
        *variant.get<std::shared_ptr<PsyMP3::MPRIS::DBusDictionary>>();
    for (const auto &[key, value] : dict) {
      DBusMessageIter entry_iter;
      dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY,
                                       nullptr, &entry_iter);

      const char *key_cstr = key.c_str();
      dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING, &key_cstr);

      // Recursive call for the value variant
      appendVariantToDBusIter(&entry_iter, value);

      dbus_message_iter_close_container(&dict_iter, &entry_iter);
    }

    dbus_message_iter_close_container(&variant_iter, &dict_iter);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  default:
    throw std::runtime_error("Unknown variant type");
  }
}
#endif

// Error handling system implementation

// MPRISError implementation
uint64_t MPRISError::generateErrorId() {
  static std::atomic<uint64_t> counter{1};
  return counter.fetch_add(1);
}

MPRISError::MPRISError(Category category, Severity severity,
                       const std::string &message, const std::string &context,
                       RecoveryStrategy recovery, const std::string &details)
    : m_category(category), m_severity(severity), m_message(message),
      m_context(context), m_details(details), m_recovery(recovery),
      m_timestamp(std::chrono::system_clock::now()),
      m_error_id(generateErrorId()), m_cause(nullptr) {}

MPRISError::MPRISError(Category category, const std::string &message)
    : MPRISError(category, Severity::Error, message, "", RecoveryStrategy::None,
                 "") {}

const char *MPRISError::what() const noexcept {
  if (m_what_cache.empty()) {
    try {
      m_what_cache = "[" + getCategoryString() + "] " + m_message;
      if (!m_context.empty()) {
        m_what_cache += " (Context: " + m_context + ")";
      }
    } catch (...) {
      // Fallback if string operations fail
      return "MPRISError: Failed to format error message";
    }
  }
  return m_what_cache.c_str();
}

std::string MPRISError::getCategoryString() const {
  switch (m_category) {
  case Category::Connection:
    return "Connection";
  case Category::Message:
    return "Message";
  case Category::PlayerState:
    return "PlayerState";
  case Category::Threading:
    return "Threading";
  case Category::Resource:
    return "Resource";
  case Category::Protocol:
    return "Protocol";
  case Category::Configuration:
    return "Configuration";
  case Category::Internal:
    return "Internal";
  default:
    return "Unknown";
  }
}

std::string MPRISError::getSeverityString() const {
  switch (m_severity) {
  case Severity::Info:
    return "Info";
  case Severity::Warning:
    return "Warning";
  case Severity::Error:
    return "Error";
  case Severity::Critical:
    return "Critical";
  case Severity::Fatal:
    return "Fatal";
  default:
    return "Unknown";
  }
}

std::string MPRISError::getRecoveryStrategyString() const {
  switch (m_recovery) {
  case RecoveryStrategy::None:
    return "None";
  case RecoveryStrategy::Retry:
    return "Retry";
  case RecoveryStrategy::Reconnect:
    return "Reconnect";
  case RecoveryStrategy::Reset:
    return "Reset";
  case RecoveryStrategy::Restart:
    return "Restart";
  case RecoveryStrategy::Degrade:
    return "Degrade";
  case RecoveryStrategy::UserAction:
    return "UserAction";
  default:
    return "Unknown";
  }
}

std::string MPRISError::getFullDescription() const {
  std::string desc =
      "[" + getSeverityString() + "] " + getCategoryString() + ": " + m_message;

  if (!m_context.empty()) {
    desc += "\nContext: " + m_context;
  }

  if (!m_details.empty()) {
    desc += "\nDetails: " + m_details;
  }

  desc += "\nRecovery Strategy: " + getRecoveryStrategyString();
  desc += "\nError ID: " + std::to_string(m_error_id);

  // Format timestamp
  auto time_t = std::chrono::system_clock::to_time_t(m_timestamp);
  char time_str[100];
  if (std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S",
                    std::localtime(&time_t))) {
    desc += "\nTimestamp: " + std::string(time_str);
  }

  return desc;
}

// ErrorLogger implementation
ErrorLogger &ErrorLogger::getInstance() {
  static ErrorLogger instance;
  return instance;
}

void ErrorLogger::setDefaultLogHandler() {
  m_log_handler = [](LogLevel level, MPRISError::Category category,
                     const std::string &message, const std::string &context,
                     std::chrono::system_clock::time_point timestamp) {
    // Format timestamp
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    char time_str[100];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S",
                  std::localtime(&time_t));

    // Format log level
    const char *level_str = "UNKNOWN";
    switch (level) {
    case LogLevel::Fatal:
      level_str = "FATAL";
      break;
    case LogLevel::Critical:
      level_str = "CRITICAL";
      break;
    case LogLevel::Error:
      level_str = "ERROR";
      break;
    case LogLevel::Warning:
      level_str = "WARNING";
      break;
    case LogLevel::Info:
      level_str = "INFO";
      break;
    case LogLevel::Debug:
      level_str = "DEBUG";
      break;
    case LogLevel::Trace:
      level_str = "TRACE";
      break;
    default:
      break;
    }

    // Format category
    const char *category_str = "Unknown";
    switch (category) {
    case MPRISError::Category::Connection:
      category_str = "Connection";
      break;
    case MPRISError::Category::Message:
      category_str = "Message";
      break;
    case MPRISError::Category::PlayerState:
      category_str = "PlayerState";
      break;
    case MPRISError::Category::Threading:
      category_str = "Threading";
      break;
    case MPRISError::Category::Resource:
      category_str = "Resource";
      break;
    case MPRISError::Category::Protocol:
      category_str = "Protocol";
      break;
    case MPRISError::Category::Configuration:
      category_str = "Configuration";
      break;
    case MPRISError::Category::Internal:
      category_str = "Internal";
      break;
    }

    // Output to stderr
    fprintf(stderr, "[%s] %s [%s] %s", time_str, level_str, category_str,
            message.c_str());
    if (!context.empty()) {
      fprintf(stderr, " (Context: %s)", context.c_str());
    }
    fprintf(stderr, "\n");
    fflush(stderr);
  };
}

void ErrorLogger::logError(const MPRISError &error) {
  LogLevel level = severityToLogLevel(error.getSeverity());

  std::lock_guard<std::mutex> lock(m_mutex);

  if (static_cast<int>(level) <= static_cast<int>(m_log_level) &&
      m_log_handler) {
    m_log_handler(level, error.getCategory(), error.getMessage(),
                  error.getContext(), error.getTimestamp());
  }

  updateStats(error);
}

void ErrorLogger::logMessage(LogLevel level, MPRISError::Category category,
                             const std::string &message,
                             const std::string &context) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (static_cast<int>(level) <= static_cast<int>(m_log_level) &&
      m_log_handler) {
    m_log_handler(level, category, message, context,
                  std::chrono::system_clock::now());
  }
}

void ErrorLogger::logFatal(const std::string &message,
                           const std::string &context) {
  logMessage(LogLevel::Fatal, MPRISError::Category::Internal, message, context);
}

void ErrorLogger::logCritical(const std::string &message,
                              const std::string &context) {
  logMessage(LogLevel::Critical, MPRISError::Category::Internal, message,
             context);
}

void ErrorLogger::logError(const std::string &message,
                           const std::string &context) {
  logMessage(LogLevel::Error, MPRISError::Category::Internal, message, context);
}

void ErrorLogger::logWarning(const std::string &message,
                             const std::string &context) {
  logMessage(LogLevel::Warning, MPRISError::Category::Internal, message,
             context);
}

void ErrorLogger::logInfo(const std::string &message,
                          const std::string &context) {
  logMessage(LogLevel::Info, MPRISError::Category::Internal, message, context);
}

void ErrorLogger::logDebug(const std::string &message,
                           const std::string &context) {
  logMessage(LogLevel::Debug, MPRISError::Category::Internal, message, context);
}

void ErrorLogger::logTrace(const std::string &message,
                           const std::string &context) {
  logMessage(LogLevel::Trace, MPRISError::Category::Internal, message, context);
}

ErrorLogger::ErrorStats ErrorLogger::getErrorStats() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_stats;
}

void ErrorLogger::resetErrorStats() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_stats = ErrorStats{};
}

void ErrorLogger::updateStats(const MPRISError &error) {
  m_stats.total_errors++;
  m_stats.last_error_time = error.getTimestamp();

  switch (error.getCategory()) {
  case MPRISError::Category::Connection:
    m_stats.connection_errors++;
    break;
  case MPRISError::Category::Message:
    m_stats.message_errors++;
    break;
  case MPRISError::Category::PlayerState:
    m_stats.player_state_errors++;
    break;
  case MPRISError::Category::Threading:
    m_stats.threading_errors++;
    break;
  case MPRISError::Category::Resource:
    m_stats.resource_errors++;
    break;
  case MPRISError::Category::Protocol:
    m_stats.protocol_errors++;
    break;
  case MPRISError::Category::Configuration:
    m_stats.configuration_errors++;
    break;
  case MPRISError::Category::Internal:
    m_stats.internal_errors++;
    break;
  }
}

ErrorLogger::LogLevel
ErrorLogger::severityToLogLevel(MPRISError::Severity severity) const {
  switch (severity) {
  case MPRISError::Severity::Info:
    return LogLevel::Info;
  case MPRISError::Severity::Warning:
    return LogLevel::Warning;
  case MPRISError::Severity::Error:
    return LogLevel::Error;
  case MPRISError::Severity::Critical:
    return LogLevel::Critical;
  case MPRISError::Severity::Fatal:
    return LogLevel::Fatal;
  default:
    return LogLevel::Error;
  }
}

// ErrorRecoveryManager implementation
ErrorRecoveryManager::ErrorRecoveryManager() {
  // Set default recovery configurations for each category
  RecoveryConfig default_config;

  // Connection errors: more aggressive retry with longer delays
  RecoveryConfig connection_config;
  connection_config.max_attempts = 5;
  connection_config.initial_delay = std::chrono::milliseconds(500);
  connection_config.max_delay = std::chrono::milliseconds(10000);
  connection_config.backoff_multiplier = 2.0;
  m_recovery_configs[MPRISError::Category::Connection] = connection_config;

  // Message errors: quick retry with short delays
  RecoveryConfig message_config;
  message_config.max_attempts = 2;
  message_config.initial_delay = std::chrono::milliseconds(50);
  message_config.max_delay = std::chrono::milliseconds(200);
  message_config.backoff_multiplier = 1.5;
  m_recovery_configs[MPRISError::Category::Message] = message_config;

  // Player state errors: moderate retry
  RecoveryConfig player_config;
  player_config.max_attempts = 3;
  player_config.initial_delay = std::chrono::milliseconds(100);
  player_config.max_delay = std::chrono::milliseconds(1000);
  m_recovery_configs[MPRISError::Category::PlayerState] = player_config;

  // Threading errors: minimal retry (dangerous to retry)
  RecoveryConfig threading_config;
  threading_config.max_attempts = 1;
  threading_config.initial_delay = std::chrono::milliseconds(1000);
  threading_config.max_delay = std::chrono::milliseconds(1000);
  threading_config.exponential_backoff = false;
  m_recovery_configs[MPRISError::Category::Threading] = threading_config;

  // Resource errors: moderate retry with exponential backoff
  m_recovery_configs[MPRISError::Category::Resource] = default_config;

  // Protocol errors: quick retry
  m_recovery_configs[MPRISError::Category::Protocol] = message_config;

  // Configuration errors: no retry (requires user intervention)
  RecoveryConfig config_config;
  config_config.max_attempts = 0;
  m_recovery_configs[MPRISError::Category::Configuration] = config_config;

  // Internal errors: minimal retry
  m_recovery_configs[MPRISError::Category::Internal] = threading_config;
}

void ErrorRecoveryManager::setRecoveryConfig(MPRISError::Category category,
                                             const RecoveryConfig &config) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_recovery_configs[category] = config;
}

ErrorRecoveryManager::RecoveryConfig
ErrorRecoveryManager::getRecoveryConfig(MPRISError::Category category) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  auto it = m_recovery_configs.find(category);
  if (it != m_recovery_configs.end()) {
    return it->second;
  }
  return RecoveryConfig{}; // Default config
}

void ErrorRecoveryManager::setRecoveryAction(
    MPRISError::RecoveryStrategy strategy, RecoveryAction action) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_recovery_actions[strategy] = action;
}

bool ErrorRecoveryManager::attemptRecovery(const MPRISError &error) {
  return attemptRecovery(error.getRecoveryStrategy(), error.getCategory());
}

bool ErrorRecoveryManager::attemptRecovery(
    MPRISError::RecoveryStrategy strategy, MPRISError::Category category) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!shouldAttemptRecovery(category)) {
    updateStats(category, strategy, false);
    return false;
  }

  auto action_it = m_recovery_actions.find(strategy);
  if (action_it == m_recovery_actions.end()) {
    updateStats(category, strategy, false);
    return false;
  }

  // Calculate delay before retry
  int attempt = m_attempt_counts[category];
  auto delay = calculateDelay(category, attempt);

  // In a real implementation, you might want to sleep here
  // std::this_thread::sleep_for(delay);

  m_attempt_counts[category]++;
  m_last_attempt_times[category] = std::chrono::system_clock::now();

  bool success = false;
  try {
    success = action_it->second();
  } catch (const std::exception &e) {
    ErrorLogger::getInstance().logError(
        "Recovery action failed: " + std::string(e.what()),
        "ErrorRecoveryManager::attemptRecovery");
    success = false;
  }

  if (success) {
    // Reset attempt count on successful recovery
    m_attempt_counts[category] = 0;
  }

  updateStats(category, strategy, success);
  return success;
}

ErrorRecoveryManager::RecoveryStats
ErrorRecoveryManager::getRecoveryStats() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_stats;
}

void ErrorRecoveryManager::resetRecoveryStats() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_stats = RecoveryStats{};
}

bool ErrorRecoveryManager::shouldAttemptRecovery(
    MPRISError::Category category) const {
  auto config_it = m_recovery_configs.find(category);
  if (config_it == m_recovery_configs.end()) {
    return false;
  }

  const auto &config = config_it->second;
  if (config.max_attempts <= 0) {
    return false;
  }

  int current_attempts = 0;
  auto attempt_it = m_attempt_counts.find(category);
  if (attempt_it != m_attempt_counts.end()) {
    current_attempts = attempt_it->second;
  }

  return current_attempts < config.max_attempts;
}

std::chrono::milliseconds
ErrorRecoveryManager::calculateDelay(MPRISError::Category category,
                                     int attempt) const {
  auto config_it = m_recovery_configs.find(category);
  if (config_it == m_recovery_configs.end()) {
    return std::chrono::milliseconds(100);
  }

  const auto &config = config_it->second;

  if (!config.exponential_backoff || attempt <= 0) {
    return config.initial_delay;
  }

  double delay_ms = config.initial_delay.count();
  for (int i = 0; i < attempt; ++i) {
    delay_ms *= config.backoff_multiplier;
  }

  auto result = std::chrono::milliseconds(static_cast<long long>(delay_ms));
  return std::min(result, config.max_delay);
}

void ErrorRecoveryManager::updateStats(MPRISError::Category category,
                                       MPRISError::RecoveryStrategy strategy,
                                       bool success) {
  m_stats.total_attempts++;
  m_stats.attempts_by_category[category]++;
  m_stats.attempts_by_strategy[strategy]++;

  if (success) {
    m_stats.successful_recoveries++;
  } else {
    m_stats.failed_recoveries++;
  }
}

// GracefulDegradationManager implementation
GracefulDegradationManager::GracefulDegradationManager() {
  // Set default error thresholds
  m_error_thresholds[MPRISError::Category::Connection] = 5;
  m_error_thresholds[MPRISError::Category::Message] = 10;
  m_error_thresholds[MPRISError::Category::PlayerState] = 3;
  m_error_thresholds[MPRISError::Category::Threading] = 1; // Very low tolerance
  m_error_thresholds[MPRISError::Category::Resource] = 5;
  m_error_thresholds[MPRISError::Category::Protocol] = 8;
  m_error_thresholds[MPRISError::Category::Configuration] = 1;
  m_error_thresholds[MPRISError::Category::Internal] = 2;
}

void GracefulDegradationManager::setDegradationLevel(DegradationLevel level) {
  std::lock_guard<std::mutex> lock(m_mutex);
  setDegradationLevel_unlocked(level);
}

void GracefulDegradationManager::setDegradationLevel_unlocked(
    DegradationLevel level) {
  m_current_level = level;

  // Update feature availability based on degradation level
  switch (level) {
  case DegradationLevel::None:
    // All features enabled
    m_disabled_features.clear();
    break;

  case DegradationLevel::Limited:
    // Disable non-essential features
    disableFeature("metadata_updates");
    disableFeature("position_tracking");
    break;

  case DegradationLevel::Minimal:
    // Only basic playback control
    disableFeature("metadata_updates");
    disableFeature("position_tracking");
    disableFeature("seeking");
    disableFeature("property_queries");
    break;

  case DegradationLevel::Disabled:
    // All features disabled
    disableFeature("playback_control");
    disableFeature("metadata_updates");
    disableFeature("position_tracking");
    disableFeature("seeking");
    disableFeature("property_queries");
    disableFeature("signal_emission");
    break;
  }
}

GracefulDegradationManager::DegradationLevel
GracefulDegradationManager::getDegradationLevel() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_current_level;
}

bool GracefulDegradationManager::isFeatureAvailable(
    const std::string &feature) const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_disabled_features.find(feature) == m_disabled_features.end();
}

void GracefulDegradationManager::disableFeature(const std::string &feature) {
  m_disabled_features.insert(feature);
}

void GracefulDegradationManager::enableFeature(const std::string &feature) {
  m_disabled_features.erase(feature);
}

void GracefulDegradationManager::reportError(const MPRISError &error) {
  std::lock_guard<std::mutex> lock(m_mutex);

  auto category = error.getCategory();
  auto now = std::chrono::system_clock::now();

  m_recent_errors[category].push_back(now);

  // Clean up old errors outside the time window
  cleanupOldErrors();

  // Check if we need to update degradation level
  updateDegradationLevel();
}

void GracefulDegradationManager::checkAutoDegradation() {
  std::lock_guard<std::mutex> lock(m_mutex);
  cleanupOldErrors();
  updateDegradationLevel();
}

void GracefulDegradationManager::setErrorThreshold(
    MPRISError::Category category, int threshold) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_error_thresholds[category] = threshold;
}

void GracefulDegradationManager::setTimeWindow(std::chrono::seconds window) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_time_window = window;
}

void GracefulDegradationManager::updateDegradationLevel() {
  // Count errors by category in the current time window
  int total_critical_errors = 0;
  int total_errors = 0;

  for (const auto &[category, errors] : m_recent_errors) {
    int error_count = errors.size();
    total_errors += error_count;

    auto threshold_it = m_error_thresholds.find(category);
    int threshold =
        (threshold_it != m_error_thresholds.end()) ? threshold_it->second : 5;

    // Critical categories that trigger immediate degradation
    if (category == MPRISError::Category::Threading ||
        category == MPRISError::Category::Internal) {
      if (error_count > 0) {
        total_critical_errors += error_count;
      }
    }

    // Check if this category exceeds its threshold
    if (error_count >= threshold) {
      total_critical_errors += error_count;
    }
  }

  // Determine new degradation level
  DegradationLevel new_level = DegradationLevel::None;

  if (total_critical_errors > 0) {
    if (total_critical_errors >= 10 || total_errors >= 50) {
      new_level = DegradationLevel::Disabled;
    } else if (total_critical_errors >= 5 || total_errors >= 25) {
      new_level = DegradationLevel::Minimal;
    } else if (total_critical_errors >= 2 || total_errors >= 10) {
      new_level = DegradationLevel::Limited;
    }
  }

  // Only increase degradation level automatically (manual intervention required
  // to decrease)
  if (static_cast<int>(new_level) > static_cast<int>(m_current_level)) {
    setDegradationLevel_unlocked(new_level);

    ErrorLogger::getInstance().logWarning(
        "Auto-degradation triggered: level changed to " +
            std::to_string(static_cast<int>(new_level)),
        "GracefulDegradationManager");
  }
}

void GracefulDegradationManager::cleanupOldErrors() {
  auto now = std::chrono::system_clock::now();
  auto cutoff = now - m_time_window;

  for (auto &[category, errors] : m_recent_errors) {
    errors.erase(
        std::remove_if(
            errors.begin(), errors.end(),
            [cutoff](const std::chrono::system_clock::time_point &timestamp) {
              return timestamp < cutoff;
            }),
        errors.end());
  }
}

} // namespace MPRIS
} // namespace PsyMP3
