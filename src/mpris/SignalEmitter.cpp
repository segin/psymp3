/*
 * SignalEmitter.cpp - MPRIS D-Bus signal emission
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

SignalEmitter::SignalEmitter(DBusConnectionManager *connection)
    : m_connection(connection),
      m_last_batch_flush(std::chrono::steady_clock::now()) {
  if (!m_connection) {
    throw std::invalid_argument(
        "SignalEmitter: connection manager cannot be null");
  }
}

SignalEmitter::~SignalEmitter() {
  stop(false); // Don't wait for completion in destructor
}

Result<void> SignalEmitter::emitPropertiesChanged(
    const std::string &interface_name,
    const std::map<std::string, DBusVariant> &changed_properties) {
  std::lock_guard<std::mutex> lock(m_mutex);
  return emitPropertiesChanged_unlocked(interface_name, changed_properties);
}

Result<void> SignalEmitter::emitSeeked(uint64_t position_us) {
  std::lock_guard<std::mutex> lock(m_mutex);
  return emitSeeked_unlocked(position_us);
}

Result<void> SignalEmitter::start() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return start_unlocked();
}

void SignalEmitter::stop(bool wait_for_completion) {
  std::lock_guard<std::mutex> lock(m_mutex);
  stop_unlocked(wait_for_completion);
}

bool SignalEmitter::isRunning() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return isRunning_unlocked();
}

size_t SignalEmitter::getQueueSize() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return getQueueSize_unlocked();
}

bool SignalEmitter::isQueueFull() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return isQueueFull_unlocked();
}

SignalEmitter::Statistics SignalEmitter::getStatistics() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return getStatistics_unlocked();
}

void SignalEmitter::resetStatistics() {
  std::lock_guard<std::mutex> lock(m_mutex);
  resetStatistics_unlocked();
}

// Private implementations - assume locks are already held

Result<void> SignalEmitter::emitPropertiesChanged_unlocked(
    [[maybe_unused]] const std::string &interface_name,
    [[maybe_unused]] const std::map<std::string, DBusVariant> &changed_properties) {
#ifndef HAVE_DBUS
  return Result<void>::error("D-Bus support not compiled in");
#else
  if (changed_properties.empty()) {
    return Result<void>::error(
        "Cannot emit PropertiesChanged with empty properties");
  }

  if (!isRunning_unlocked()) {
    return Result<void>::error("SignalEmitter is not running");
  }

  // Add to batch for efficient emission
  addToBatch_unlocked(interface_name, changed_properties);

  // Check if we should flush the batch immediately
  if (shouldFlushBatch_unlocked()) {
    flushBatch_unlocked();
  }

  m_statistics.signals_queued++;
  return Result<void>::success();
#endif
}

Result<void> SignalEmitter::emitSeeked_unlocked([[maybe_unused]] uint64_t position_us) {
#ifndef HAVE_DBUS
  return Result<void>::error("D-Bus support not compiled in");
#else
  if (!isRunning_unlocked()) {
    return Result<void>::error("SignalEmitter is not running");
  }

  // Seeked signals are not batched - emit immediately
  auto signal_func = [this, position_us]() {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto message_result = createSeekedMessage_unlocked(position_us);
    if (message_result.isSuccess()) {
      auto send_result =
          sendSignalMessage_unlocked(message_result.getValue().get());
      if (send_result.isSuccess()) {
        m_statistics.signals_sent++;
      } else {
        m_statistics.signals_failed++;
      }
    } else {
      m_statistics.signals_failed++;
    }
  };

  if (!enqueueSignal_unlocked(signal_func)) {
    m_statistics.signals_dropped++;
    return Result<void>::error("Signal queue is full, Seeked signal dropped");
  }

  // Notify worker thread
  m_signal_cv.notify_one();

  m_statistics.signals_queued++;
  return Result<void>::success();
#endif
}

Result<void> SignalEmitter::start_unlocked() {
#ifndef HAVE_DBUS
  return Result<void>::error("D-Bus support not compiled in");
#else
  if (isRunning_unlocked()) {
    return Result<void>::success(); // Already running
  }

  // Reset shutdown flag
  m_shutdown_requested = false;

  try {
    // Start worker thread
    m_signal_thread = std::thread(&SignalEmitter::signalWorkerLoop, this);
    m_signal_thread_active = true;

    return Result<void>::success();
  } catch (const std::exception &e) {
    std::string error_msg = "Failed to start signal emitter thread: ";
    error_msg += e.what();
    return Result<void>::error(error_msg);
  }
#endif
}

void SignalEmitter::stop_unlocked(bool wait_for_completion) {
  if (!isRunning_unlocked()) {
    return; // Already stopped
  }

  // Signal shutdown
  m_shutdown_requested = true;
  m_signal_cv.notify_all();

  // Wait for thread to finish if requested and thread is joinable
  if (wait_for_completion && m_signal_thread.joinable()) {
    // Temporarily release lock to avoid deadlock during join
    m_mutex.unlock();
    try {
      m_signal_thread.join();
    } catch (const std::exception &) {
      // Ignore join errors
    }
    m_mutex.lock();
  } else if (m_signal_thread.joinable()) {
    // Detach thread if not waiting
    m_signal_thread.detach();
  }

  m_signal_thread_active = false;
}

bool SignalEmitter::isRunning_unlocked() const {
  return m_signal_thread_active.load();
}

size_t SignalEmitter::getQueueSize_unlocked() const {
  return m_signal_queue.size();
}

bool SignalEmitter::isQueueFull_unlocked() const {
  return m_signal_queue.size() >= MAX_QUEUE_SIZE;
}

SignalEmitter::Statistics SignalEmitter::getStatistics_unlocked() const {
  return m_statistics;
}

void SignalEmitter::resetStatistics_unlocked() { m_statistics = Statistics{}; }

// Internal signal processing

void SignalEmitter::signalWorkerLoop() {
  while (!m_shutdown_requested.load()) {
    std::unique_lock<std::mutex> lock(m_mutex);

    // Wait for signals or timeout
    m_signal_cv.wait_for(lock, WORKER_TIMEOUT, [this] {
      return !m_signal_queue.empty() || m_shutdown_requested.load() ||
             shouldFlushBatch_unlocked();
    });

    if (m_shutdown_requested.load()) {
      // Process remaining signals before shutdown
      processSignalQueue_unlocked();
      flushBatch_unlocked();
      break;
    }

    // Process queued signals
    processSignalQueue_unlocked();

    // Check if we need to flush batched signals
    if (shouldFlushBatch_unlocked()) {
      flushBatch_unlocked();
    }
  }
}

void SignalEmitter::processSignalQueue_unlocked() {
  // Process all queued signals
  while (!m_signal_queue.empty() && !m_shutdown_requested.load()) {
    if (!processNextSignal_unlocked()) {
      break; // Error processing signal
    }
  }
}

bool SignalEmitter::processNextSignal_unlocked() {
  if (m_signal_queue.empty()) {
    return false;
  }

  auto signal_func = std::move(m_signal_queue.front());
  m_signal_queue.pop();

  try {
    // Execute signal function (this may temporarily release and reacquire the
    // lock)
    signal_func();
    return true;
  } catch (const std::exception &) {
    m_statistics.signals_failed++;
    return false;
  }
}

void SignalEmitter::processBatchedSignals_unlocked() { flushBatch_unlocked(); }

// Signal creation helpers

Result<DBusMessagePtr> SignalEmitter::createPropertiesChangedMessage_unlocked(
    [[maybe_unused]] const std::string &interface_name,
    [[maybe_unused]] const std::map<std::string, DBusVariant> &changed_properties) {
#ifndef HAVE_DBUS
  return Result<DBusMessagePtr>::error("D-Bus support not compiled in");
#else
  // Create PropertiesChanged signal message
  DBusMessage *raw_message = dbus_message_new_signal(
      DBUS_OBJECT_PATH, DBUS_PROPERTIES_INTERFACE, "PropertiesChanged");

  if (!raw_message) {
    return Result<DBusMessagePtr>::error(
        "Failed to create PropertiesChanged signal message");
  }

  DBusMessagePtr message(raw_message);

  // Add arguments: interface name, changed properties dict, invalidated
  // properties array
  DBusMessageIter iter, dict_iter, array_iter;
  dbus_message_iter_init_append(message.get(), &iter);

  // Argument 1: Interface name (string)
  const char *interface_cstr = interface_name.c_str();
  if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING,
                                      &interface_cstr)) {
    return Result<DBusMessagePtr>::error(
        "Failed to append interface name to PropertiesChanged signal");
  }

  // Argument 2: Changed properties (dict)
  if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}",
                                        &dict_iter)) {
    return Result<DBusMessagePtr>::error(
        "Failed to open properties dictionary in PropertiesChanged signal");
  }

  for (const auto &[key, variant] : changed_properties) {
    DBusMessageIter entry_iter, variant_iter;

    // Open dict entry
    if (!dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY,
                                          nullptr, &entry_iter)) {
      return Result<DBusMessagePtr>::error(
          "Failed to open dict entry in PropertiesChanged signal");
    }

    // Add key (string)
    const char *key_cstr = key.c_str();
    if (!dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING,
                                        &key_cstr)) {
      return Result<DBusMessagePtr>::error(
          "Failed to append property key in PropertiesChanged signal");
    }

    // Add value (variant)
    const char *variant_signature;
    switch (variant.type) {
    case DBusVariant::String:
      variant_signature = "s";
      break;
    case DBusVariant::StringArray:
      variant_signature = "as";
      break;
    case DBusVariant::Int64:
      variant_signature = "x";
      break;
    case DBusVariant::UInt64:
      variant_signature = "t";
      break;
    case DBusVariant::Double:
      variant_signature = "d";
      break;
    case DBusVariant::Boolean:
      variant_signature = "b";
      break;
    default:
      return Result<DBusMessagePtr>::error(
          "Unsupported variant type in PropertiesChanged signal");
    }

    if (!dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT,
                                          variant_signature, &variant_iter)) {
      return Result<DBusMessagePtr>::error(
          "Failed to open variant in PropertiesChanged signal");
    }

    // Append variant value based on type
    bool append_success = false;
    switch (variant.type) {
    case DBusVariant::String: {
      const auto &str_val = variant.get<std::string>();
      const char *str_cstr = str_val.c_str();
      append_success = dbus_message_iter_append_basic(
          &variant_iter, DBUS_TYPE_STRING, &str_cstr);
      break;
    }
    case DBusVariant::StringArray: {
      const auto &arr_val = variant.get<std::vector<std::string>>();
      DBusMessageIter array_iter_inner;
      if (dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY, "s",
                                           &array_iter_inner)) {
        append_success = true;
        for (const auto &str : arr_val) {
          const char *str_cstr = str.c_str();
          if (!dbus_message_iter_append_basic(&array_iter_inner,
                                              DBUS_TYPE_STRING, &str_cstr)) {
            append_success = false;
            break;
          }
        }
        if (append_success) {
          append_success = dbus_message_iter_close_container(&variant_iter,
                                                             &array_iter_inner);
        }
      }
      break;
    }
    case DBusVariant::Int64: {
      auto int_val = variant.get<int64_t>();
      append_success = dbus_message_iter_append_basic(
          &variant_iter, DBUS_TYPE_INT64, &int_val);
      break;
    }
    case DBusVariant::UInt64: {
      auto uint_val = variant.get<uint64_t>();
      append_success = dbus_message_iter_append_basic(
          &variant_iter, DBUS_TYPE_UINT64, &uint_val);
      break;
    }
    case DBusVariant::Double: {
      auto double_val = variant.get<double>();
      append_success = dbus_message_iter_append_basic(
          &variant_iter, DBUS_TYPE_DOUBLE, &double_val);
      break;
    }
    case DBusVariant::Boolean: {
      auto bool_val = variant.get<bool>();
      dbus_bool_t dbus_bool = bool_val ? TRUE : FALSE;
      append_success = dbus_message_iter_append_basic(
          &variant_iter, DBUS_TYPE_BOOLEAN, &dbus_bool);
      break;
    }
    }

    if (!append_success) {
      return Result<DBusMessagePtr>::error(
          "Failed to append variant value in PropertiesChanged signal");
    }

    // Close variant and dict entry
    if (!dbus_message_iter_close_container(&entry_iter, &variant_iter) ||
        !dbus_message_iter_close_container(&dict_iter, &entry_iter)) {
      return Result<DBusMessagePtr>::error(
          "Failed to close containers in PropertiesChanged signal");
    }
  }

  // Close properties dictionary
  if (!dbus_message_iter_close_container(&iter, &dict_iter)) {
    return Result<DBusMessagePtr>::error(
        "Failed to close properties dictionary in PropertiesChanged signal");
  }

  // Argument 3: Invalidated properties (empty array)
  if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s",
                                        &array_iter) ||
      !dbus_message_iter_close_container(&iter, &array_iter)) {
    return Result<DBusMessagePtr>::error("Failed to add invalidated properties "
                                         "array in PropertiesChanged signal");
  }

  return Result<DBusMessagePtr>::success(std::move(message));
#endif
}

Result<DBusMessagePtr>
SignalEmitter::createSeekedMessage_unlocked([[maybe_unused]] uint64_t position_us) {
#ifndef HAVE_DBUS
  return Result<DBusMessagePtr>::error("D-Bus support not compiled in");
#else
  // Create Seeked signal message
  DBusMessage *raw_message = dbus_message_new_signal(
      DBUS_OBJECT_PATH, MPRIS_PLAYER_INTERFACE, "Seeked");

  if (!raw_message) {
    return Result<DBusMessagePtr>::error(
        "Failed to create Seeked signal message");
  }

  DBusMessagePtr message(raw_message);

  // Add position argument (int64)
  int64_t position_signed = static_cast<int64_t>(position_us);
  if (!dbus_message_append_args(message.get(), DBUS_TYPE_INT64,
                                &position_signed, DBUS_TYPE_INVALID)) {
    return Result<DBusMessagePtr>::error(
        "Failed to append position to Seeked signal");
  }

  return Result<DBusMessagePtr>::success(std::move(message));
#endif
}

// D-Bus message sending

Result<void> SignalEmitter::sendSignalMessage_unlocked([[maybe_unused]] DBusMessage *message) {
#ifndef HAVE_DBUS
  return Result<void>::error("D-Bus support not compiled in");
#else
  if (!message) {
    return Result<void>::error("Cannot send null message");
  }

  DBusConnection *connection = m_connection->getConnection();
  if (!connection) {
    return Result<void>::error("No D-Bus connection available");
  }

  // Send the signal (non-blocking)
  dbus_uint32_t serial = 0;
  if (!dbus_connection_send(connection, message, &serial)) {
    return Result<void>::error("Failed to send D-Bus signal message");
  }

  // Flush the connection to ensure message is sent
  dbus_connection_flush(connection);

  return Result<void>::success();
#endif
}

// Queue management

bool SignalEmitter::enqueueSignal_unlocked(std::function<void()> signal_func) {
  if (isQueueFull_unlocked()) {
    // Drop oldest signals to make room
    dropOldestSignals_unlocked(QUEUE_DROP_COUNT);
  }

  if (isQueueFull_unlocked()) {
    return false; // Still full after dropping
  }

  m_signal_queue.push(std::move(signal_func));
  return true;
}

void SignalEmitter::dropOldestSignals_unlocked(size_t count) {
  size_t dropped = 0;
  while (!m_signal_queue.empty() && dropped < count) {
    m_signal_queue.pop();
    dropped++;
    m_statistics.signals_dropped++;
  }
}

// Batching support

void SignalEmitter::addToBatch_unlocked(
    const std::string &interface_name,
    const std::map<std::string, DBusVariant> &properties) {
  auto &batch = m_batched_properties[interface_name];
  batch.m_interface = interface_name;
  batch.timestamp = std::chrono::steady_clock::now();

  // Merge properties into batch
  for (const auto &[key, value] : properties) {
    batch.properties[key] = value;
  }

  // Limit batch size
  if (batch.properties.size() > MAX_BATCH_SIZE) {
    flushBatch_unlocked();
  }
}

bool SignalEmitter::shouldFlushBatch_unlocked() const {
  if (m_batched_properties.empty()) {
    return false;
  }

  auto now = std::chrono::steady_clock::now();
  auto time_since_last_flush = now - m_last_batch_flush;

  return time_since_last_flush >= BATCH_TIMEOUT;
}

void SignalEmitter::flushBatch_unlocked() {
  if (m_batched_properties.empty()) {
    return;
  }

  // Create signal functions for each batched interface
  for (const auto &[interface_name, batch] : m_batched_properties) {
    auto signal_func = [this, interface_name = batch.m_interface,
                        properties = batch.properties]() {
      std::lock_guard<std::mutex> lock(m_mutex);
      auto message_result =
          createPropertiesChangedMessage_unlocked(interface_name, properties);
      if (message_result.isSuccess()) {
        auto send_result =
            sendSignalMessage_unlocked(message_result.getValue().get());
        if (send_result.isSuccess()) {
          m_statistics.signals_sent++;
          m_statistics.batches_sent++;
        } else {
          m_statistics.signals_failed++;
        }
      } else {
        m_statistics.signals_failed++;
      }
    };

    if (!enqueueSignal_unlocked(signal_func)) {
      m_statistics.signals_dropped++;
    }
  }

  // Clear batched properties and update flush time
  m_batched_properties.clear();
  m_last_batch_flush = std::chrono::steady_clock::now();

  // Notify worker thread
  m_signal_cv.notify_one();
}

} // namespace MPRIS
} // namespace PsyMP3
