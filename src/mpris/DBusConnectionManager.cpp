/*
 * DBusConnectionManager.cpp - D-Bus connection management for MPRIS
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

#ifdef HAVE_DBUS
DBusAPIWrapper DBusConnectionManager::s_dbus_api = {
    // Private, not shared: dbus_bus_get() caches the connection per bus type
    // and keeps returning the DEAD cached one after a bus restart (the cache
    // only purges once the Disconnected message is dispatched, which nothing
    // pumps on an abandoned connection) - reconnection could never succeed.
    // A private connection is created fresh from the current bus address.
    [](int type, DBusError* error) -> DBusConnection* {
        // libdbus caches the session-bus ADDRESS globally on first use, so
        // after a bus restart (new address in the environment) every
        // dbus_bus_get* call keeps dialing the dead socket forever. Read
        // the address from the CURRENT environment and open it directly;
        // fall back to libdbus's resolution when the variable is unset.
        if (type == (int)DBUS_BUS_SESSION) {
            const char* addr = getenv("DBUS_SESSION_BUS_ADDRESS");
            if (addr && *addr) {
                DBusConnection* conn = dbus_connection_open_private(addr, error);
                if (!conn) return nullptr;
                if (!dbus_bus_register(conn, error)) {
                    dbus_connection_close(conn);
                    dbus_connection_unref(conn);
                    return nullptr;
                }
                return conn;
            }
        }
        return dbus_bus_get_private((DBusBusType)type, error);
    },
    [](DBusConnection* conn, const char* name, unsigned int flags, DBusError* error) { return dbus_bus_request_name(conn, name, flags, error); },
    [](DBusConnection* conn, const char* name, DBusError* error) { return dbus_bus_release_name(conn, name, error); },
    [](DBusConnection* conn) { return (int)dbus_connection_get_is_connected(conn); },
    [](DBusError* error) { dbus_error_init(error); },
    [](const DBusError* error) { return (int)dbus_error_is_set(error); },
    [](DBusError* error) { dbus_error_free(error); },
    [](DBusConnection* conn, int exit_on_disconnect) { dbus_connection_set_exit_on_disconnect(conn, (dbus_bool_t)exit_on_disconnect); },
    [](DBusConnection* conn) {
        // Private connections must be closed before the final unref.
        dbus_connection_close(conn);
        dbus_connection_unref(conn);
    }
};

// libdbus must be told to use threads before the first connection is created:
// the SignalEmitter worker thread sends signals concurrently with the main
// thread's read_write_dispatch pump, and that is only safe once libdbus has
// installed its internal locks.
static std::once_flag s_dbus_threads_init_flag;
#else
DBusAPIWrapper DBusConnectionManager::s_dbus_api = {
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};
#endif

void DBusConnectionManager::setDBusAPI(const DBusAPIWrapper& api) {
    s_dbus_api = api;
}

void DBusConnectionManager::unrefConnection(DBusConnection* conn) {
#ifdef HAVE_DBUS
    if (conn && s_dbus_api.connection_unref) {
        s_dbus_api.connection_unref(conn);
    }
#endif
}

DBusConnectionManager::DBusConnectionManager()
    : m_connection(nullptr),
      m_last_reconnect_attempt(std::chrono::steady_clock::time_point{}),
      m_reconnect_attempt_count(0) {}

DBusConnectionManager::~DBusConnectionManager() { disconnect(); }

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

DBusConnection *DBusConnectionManager::getConnection() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return getConnection_unlocked();
}

void DBusConnectionManager::enableAutoReconnect(bool enable) {
  std::lock_guard<std::mutex> lock(m_mutex);
  enableAutoReconnect_unlocked(enable);
}

Result<void> DBusConnectionManager::attemptReconnection(bool force) {
  std::lock_guard<std::mutex> lock(m_mutex);
  return attemptReconnection_unlocked(force);
}

std::string DBusConnectionManager::getAcquiredServiceName() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_acquired_service_name;
}

bool DBusConnectionManager::isAutoReconnectEnabled() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return isAutoReconnectEnabled_unlocked();
}

std::chrono::seconds
DBusConnectionManager::getTimeSinceLastReconnectAttempt() const {
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
  return m_connected && m_connection &&
         s_dbus_api.connection_get_is_connected(m_connection.get());
#endif
}

DBusConnection *DBusConnectionManager::getConnection_unlocked() {
  if (!isConnected_unlocked()) {
    return nullptr;
  }
  return m_connection.get();
}

void DBusConnectionManager::enableAutoReconnect_unlocked(bool enable) {
  m_auto_reconnect = enable;
}

Result<void> DBusConnectionManager::attemptReconnection_unlocked(bool force) {
#ifndef HAVE_DBUS
  return Result<void>::error("D-Bus support not compiled in");
#else
  // Check if we should attempt reconnection. An explicit (forced) request
  // bypasses the budget/backoff gate, which exists to throttle automatic
  // retry loops, not deliberate caller action.
  if (!force && !shouldAttemptReconnect_unlocked()) {
    std::ostringstream oss;
    oss << "Reconnection not allowed: too many attempts ("
        << m_reconnect_attempt_count << "/" << MAX_RECONNECT_ATTEMPTS
        << ") or too soon since last attempt";
    MPRIS_LOG_WARN("DBusConnectionManager", oss.str());
    return Result<void>::error(oss.str());
  }

  MPRIS_LOG_INFO("DBusConnectionManager",
                 "Attempting D-Bus reconnection (attempt " +
                     std::to_string(m_reconnect_attempt_count + 1) + "/" +
                     std::to_string(MAX_RECONNECT_ATTEMPTS) + ")");

  // Update attempt tracking
  updateReconnectAttemptTime_unlocked();
  m_reconnect_attempt_count++;

  // Disconnect existing connection
  disconnect_unlocked();

  // Attempt new connection
  auto result = connect_unlocked();
  if (result.isSuccess()) {
    MPRIS_LOG_INFO("DBusConnectionManager", "D-Bus reconnection successful");
    // A successful reconnection proves the bus is healthy again: clear the
    // attempt budget and backoff clock. Without this, transient blips
    // permanently consume MAX_RECONNECT_ATTEMPTS over the process lifetime
    // and reconnection is eventually denied forever.
    m_reconnect_attempt_count = 0;
    m_last_reconnect_attempt = std::chrono::steady_clock::time_point{};
  } else {
    MPRIS_LOG_ERROR("DBusConnectionManager",
                    "D-Bus reconnection failed: " + result.getError());
  }

  return result;
#endif
}

bool DBusConnectionManager::isAutoReconnectEnabled_unlocked() const {
  return m_auto_reconnect;
}

std::chrono::seconds
DBusConnectionManager::getTimeSinceLastReconnectAttempt_unlocked() const {
  if (m_last_reconnect_attempt == std::chrono::steady_clock::time_point{}) {
    return std::chrono::seconds{0};
  }

  auto now = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::seconds>(
      now - m_last_reconnect_attempt);
  return duration;
}

// Internal helper methods

void DBusConnectionManager::cleanupConnection_unlocked() {
#ifdef HAVE_DBUS
  if (m_connection) {
    MPRIS_LOG_DEBUG("DBusConnectionManager", "Cleaning up D-Bus connection");
    MPRIS_TRACE_DBUS_MESSAGE("cleanup", m_connection.get(),
                             "Starting connection cleanup");

    // Unregister from D-Bus if we were registered
    if (s_dbus_api.connection_get_is_connected(m_connection.get())) {
      MPRIS_LOG_DEBUG("DBusConnectionManager", "Releasing D-Bus service name");
      // Release the service name if we own it
      DBusError error;
      s_dbus_api.error_init(&error);

      const std::string name_to_release = m_acquired_service_name.empty()
                                              ? std::string(DBUS_SERVICE_NAME)
                                              : m_acquired_service_name;
      [[maybe_unused]] int result = s_dbus_api.bus_release_name(
          m_connection.get(), name_to_release.c_str(), &error);
      if (s_dbus_api.error_is_set(&error)) {
        MPRIS_LOG_WARN("DBusConnectionManager",
                       "Error releasing D-Bus service name: " +
                           std::string(error.message));
        s_dbus_api.error_free(&error);
      } else {
        MPRIS_LOG_DEBUG("DBusConnectionManager",
                        "D-Bus service name released successfully");
      }
    }

    // Reset the connection pointer (RAII will handle cleanup)
    MPRIS_TRACE_DBUS_MESSAGE("destroyed", m_connection.get(),
                             "Connection being destroyed");
    m_connection.reset();
    m_acquired_service_name.clear();
    MPRIS_LOG_DEBUG("DBusConnectionManager",
                    "D-Bus connection cleanup complete");
  }
#endif
}

Result<void> DBusConnectionManager::establishConnection_unlocked() {
#ifndef HAVE_DBUS
  return Result<void>::error("D-Bus support not compiled in");
#else
  MPRIS_LOG_INFO("DBusConnectionManager", "Establishing D-Bus connection");

  // Enable libdbus thread support before the first connection is created.
  std::call_once(s_dbus_threads_init_flag, []() { dbus_threads_init_default(); });

  DBusError error;
  s_dbus_api.error_init(&error);

  // Connect to session bus
  MPRIS_LOG_DEBUG("DBusConnectionManager", "Connecting to D-Bus session bus");
  DBusConnection *raw_connection = s_dbus_api.bus_get(DBUS_BUS_SESSION, &error);
  if (s_dbus_api.error_is_set(&error)) {
    std::string error_msg = "Failed to connect to D-Bus session bus: ";
    error_msg += error.message;
    MPRIS_LOG_ERROR("DBusConnectionManager", error_msg);
    s_dbus_api.error_free(&error);
    return Result<void>::error(error_msg);
  }

  if (!raw_connection) {
    MPRIS_LOG_ERROR("DBusConnectionManager",
                    "Failed to connect to D-Bus session bus: null connection");
    return Result<void>::error(
        "Failed to connect to D-Bus session bus: null connection");
  }

  // Wrap in RAII pointer
  m_connection = DBusConnectionPtr(raw_connection);
  MPRIS_LOG_DEBUG("DBusConnectionManager", "D-Bus connection established");
  MPRIS_TRACE_DBUS_MESSAGE("established", raw_connection,
                           "Session bus connection");

  // Request the well-known service name. DO_NOT_QUEUE, not
  // REPLACE_EXISTING: stealing the name from a live instance is hostile,
  // and waiting in the queue helps nobody. Per the MPRIS2 spec, when
  // another instance owns the name, fall back to the unique
  // org.mpris.MediaPlayer2.psymp3.instance<pid> form instead.
  MPRIS_LOG_DEBUG("DBusConnectionManager", "Requesting D-Bus service name: " +
                                               std::string(DBUS_SERVICE_NAME));
  int name_result =
      s_dbus_api.bus_request_name(m_connection.get(), DBUS_SERVICE_NAME,
                            DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);

  if (s_dbus_api.error_is_set(&error)) {
    std::string error_msg = "Failed to request D-Bus service name: ";
    error_msg += error.message;
    MPRIS_LOG_ERROR("DBusConnectionManager", error_msg);
    s_dbus_api.error_free(&error);
    cleanupConnection_unlocked();
    return Result<void>::error(error_msg);
  }

  if (name_result == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ||
      name_result == DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER) {
    m_acquired_service_name = DBUS_SERVICE_NAME;
  } else {
    std::string instance_name =
        std::string(DBUS_SERVICE_NAME) + ".instance" + std::to_string(getpid());
    MPRIS_LOG_INFO("DBusConnectionManager",
                   "Well-known name taken; requesting instance name: " +
                       instance_name);
    name_result = s_dbus_api.bus_request_name(
        m_connection.get(), instance_name.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE,
        &error);

    if (s_dbus_api.error_is_set(&error)) {
      std::string error_msg = "Failed to request instance service name: ";
      error_msg += error.message;
      MPRIS_LOG_ERROR("DBusConnectionManager", error_msg);
      s_dbus_api.error_free(&error);
      cleanupConnection_unlocked();
      return Result<void>::error(error_msg);
    }

    if (name_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER &&
        name_result != DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER) {
      std::ostringstream oss;
      oss << "Failed to acquire D-Bus service name '" << instance_name
          << "': result code " << name_result;
      MPRIS_LOG_ERROR("DBusConnectionManager", oss.str());
      cleanupConnection_unlocked();
      return Result<void>::error(oss.str());
    }
    m_acquired_service_name = instance_name;
  }

  MPRIS_LOG_INFO("DBusConnectionManager",
                 "D-Bus service name acquired successfully: " +
                     m_acquired_service_name);

  // Set up connection for threading
  s_dbus_api.connection_set_exit_on_disconnect(m_connection.get(), FALSE);

  MPRIS_LOG_INFO("DBusConnectionManager", "D-Bus connection fully established");
  return Result<void>::success();
#endif
}

bool DBusConnectionManager::shouldAttemptReconnect_unlocked() const {
  // Check if we've exceeded maximum attempts
  if (m_reconnect_attempt_count >= MAX_RECONNECT_ATTEMPTS) {
    return false;
  }

  // Backoff throttles retries against a bus that just failed; with no prior
  // attempt on record there is nothing to back off from, so allow
  // immediately. (The epoch sentinel makes getTimeSince... report 0, which
  // would otherwise wrongly deny the very first reconnection attempt.)
  if (m_last_reconnect_attempt == std::chrono::steady_clock::time_point{}) {
    return true;
  }

  // Check if enough time has passed since last attempt
  auto time_since_last = getTimeSinceLastReconnectAttempt_unlocked();
  auto required_delay = calculateBackoffDelay_unlocked();

  return time_since_last >= required_delay;
}

std::chrono::seconds
DBusConnectionManager::calculateBackoffDelay_unlocked() const {
  if (m_reconnect_attempt_count == 0) {
    return MIN_RECONNECT_INTERVAL;
  }

  // Exponential backoff: 2^attempt_count seconds, capped at
  // MAX_RECONNECT_INTERVAL
  int delay_seconds =
      1 << std::min(m_reconnect_attempt_count, 6); // Cap at 2^6 = 64 seconds
  auto delay = std::chrono::seconds(delay_seconds);

  return std::min(delay, MAX_RECONNECT_INTERVAL);
}

void DBusConnectionManager::updateReconnectAttemptTime_unlocked() {
  m_last_reconnect_attempt = std::chrono::steady_clock::now();
}

} // namespace MPRIS
} // namespace PsyMP3
