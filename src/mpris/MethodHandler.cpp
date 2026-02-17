/*
 * MethodHandler.cpp - MPRIS D-Bus method handling
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

MethodHandler::MethodHandler(Player *player, PropertyManager *properties)
    : m_player(player), m_properties(properties), m_initialized(false) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_properties) {
    throw std::invalid_argument(
        "MethodHandler: PropertyManager pointer cannot be null");
  }

  // Note: Player can be null for testing purposes
  if (!m_player) {
    logError_unlocked("MethodHandler",
                      "Player is null - MPRIS commands will be ignored");
  }

  initializeMethodHandlers_unlocked();
  m_initialized = true;
}

MethodHandler::~MethodHandler() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_initialized = false;
  m_method_handlers.clear();
}

// Public API implementations

DBusHandlerResult MethodHandler::handleMessage(DBusConnection *connection,
                                               DBusMessage *message) {
  std::lock_guard<std::mutex> lock(m_mutex);
  return handleMessage_unlocked(connection, message);
}

bool MethodHandler::isReady() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return isReady_unlocked();
}

// Private implementations

DBusHandlerResult
MethodHandler::handleMessage_unlocked(DBusConnection *connection,
                                      DBusMessage *message) {
  if (!m_initialized || !connection || !message) {
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  const char *interface_name = dbus_message_get_interface(message);
  const char *member_name = dbus_message_get_member(message);

  if (!interface_name || !member_name) {
    logError_unlocked("handleMessage",
                      "Missing interface or member name in D-Bus message");
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
  }

  logMethodCall_unlocked(interface_name, member_name);

  // Create method key for dispatch table lookup
  std::string method_key =
      std::string(interface_name) + "." + std::string(member_name);

  auto handler_it = m_method_handlers.find(method_key);
  if (handler_it != m_method_handlers.end()) {
    try {
      return handler_it->second(connection, message);
    } catch (const std::exception &e) {
      logError_unlocked("handleMessage", "Exception in method handler: " +
                                             std::string(e.what()));
      sendErrorReply_unlocked(connection, message,
                              "org.mpris.MediaPlayer2.Error.Failed",
                              "Internal error processing method call");
      return DBUS_HANDLER_RESULT_HANDLED;
    }
  }

  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

bool MethodHandler::isReady_unlocked() const {
  return m_initialized && m_player && m_properties;
}

void MethodHandler::initializeMethodHandlers_unlocked() {
  // MPRIS MediaPlayer2 interface methods
  m_method_handlers[std::string(MPRIS_MEDIAPLAYER2_INTERFACE) + ".Raise"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handleRaise_unlocked(conn, msg);
      };

  m_method_handlers[std::string(MPRIS_MEDIAPLAYER2_INTERFACE) + ".Quit"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handleQuit_unlocked(conn, msg);
      };

  // MPRIS MediaPlayer2.Player interface methods
  m_method_handlers[std::string(MPRIS_PLAYER_INTERFACE) + ".Play"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handlePlay_unlocked(conn, msg);
      };

  m_method_handlers[std::string(MPRIS_PLAYER_INTERFACE) + ".Pause"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handlePause_unlocked(conn, msg);
      };

  m_method_handlers[std::string(MPRIS_PLAYER_INTERFACE) + ".Stop"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handleStop_unlocked(conn, msg);
      };

  m_method_handlers[std::string(MPRIS_PLAYER_INTERFACE) + ".PlayPause"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handlePlayPause_unlocked(conn, msg);
      };

  m_method_handlers[std::string(MPRIS_PLAYER_INTERFACE) + ".Next"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handleNext_unlocked(conn, msg);
      };

  m_method_handlers[std::string(MPRIS_PLAYER_INTERFACE) + ".Previous"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handlePrevious_unlocked(conn, msg);
      };

  m_method_handlers[std::string(MPRIS_PLAYER_INTERFACE) + ".Seek"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handleSeek_unlocked(conn, msg);
      };

  m_method_handlers[std::string(MPRIS_PLAYER_INTERFACE) + ".SetPosition"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handleSetPosition_unlocked(conn, msg);
      };

  // D-Bus Properties interface methods
  m_method_handlers[std::string(DBUS_PROPERTIES_INTERFACE) + ".Get"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handleGetProperty_unlocked(conn, msg);
      };

  m_method_handlers[std::string(DBUS_PROPERTIES_INTERFACE) + ".Set"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handleSetProperty_unlocked(conn, msg);
      };

  m_method_handlers[std::string(DBUS_PROPERTIES_INTERFACE) + ".GetAll"] =
      [this](DBusConnection *conn, DBusMessage *msg) {
        return handleGetAllProperties_unlocked(conn, msg);
      };
}

// MPRIS MediaPlayer2 interface handlers

DBusHandlerResult
MethodHandler::handleRaise_unlocked(DBusConnection *connection,
                                    DBusMessage *message) {
  // Bring the media player to the foreground
  // For PsyMP3, this might mean focusing the window or just logging
  std::cout << "MPRIS: Received Raise command - bringing player to foreground"
            << std::endl;

  sendMethodReturn_unlocked(connection, message);
  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult MethodHandler::handleQuit_unlocked(DBusConnection *connection,
                                                     DBusMessage *message) {
  std::cout << "MPRIS: Received Quit command - shutting down player"
            << std::endl;

  // Send reply before triggering quit to ensure client gets response
  sendMethodReturn_unlocked(connection, message);

  // Signal the player to exit using the established pattern
  // Note: This is a static method, so it doesn't require m_player to be
  // non-null
#ifndef TESTING
  if (m_player) {
    Player::synthesizeUserEvent(QUIT_APPLICATION, nullptr, nullptr);
  }
#endif

  return DBUS_HANDLER_RESULT_HANDLED;
}

// MPRIS MediaPlayer2.Player interface handlers

DBusHandlerResult MethodHandler::handlePlay_unlocked(DBusConnection *connection,
                                                     DBusMessage *message) {
  std::cout << "MPRIS: Received Play command" << std::endl;

  if (!m_player) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Player not available");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

#ifdef TESTING
  // In testing mode, just send success response
  sendMethodReturn_unlocked(connection, message);
#else
  bool success = m_player->play();
  if (success) {
    sendMethodReturn_unlocked(connection, message);
  } else {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Failed to start playback");
  }
#endif

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult
MethodHandler::handlePause_unlocked(DBusConnection *connection,
                                    DBusMessage *message) {
  std::cout << "MPRIS: Received Pause command" << std::endl;

  if (!m_player) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Player not available");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

#ifdef TESTING
  // In testing mode, just send success response
  sendMethodReturn_unlocked(connection, message);
#else
  bool success = m_player->pause();
  if (success) {
    sendMethodReturn_unlocked(connection, message);
  } else {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Failed to pause playback");
  }
#endif

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult MethodHandler::handleStop_unlocked(DBusConnection *connection,
                                                     DBusMessage *message) {
  std::cout << "MPRIS: Received Stop command" << std::endl;

  if (!m_player) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Player not available");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

#ifdef TESTING
  // In testing mode, just send success response
  sendMethodReturn_unlocked(connection, message);
#else
  bool success = m_player->stop();
  if (success) {
    sendMethodReturn_unlocked(connection, message);
  } else {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Failed to stop playback");
  }
#endif

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult
MethodHandler::handlePlayPause_unlocked(DBusConnection *connection,
                                        DBusMessage *message) {
  std::cout << "MPRIS: Received PlayPause command" << std::endl;

  if (!m_player) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Player not available");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

#ifdef TESTING
  // In testing mode, just send success response
  sendMethodReturn_unlocked(connection, message);
#else
  bool success = m_player->playPause();
  if (success) {
    sendMethodReturn_unlocked(connection, message);
  } else {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Failed to toggle playback");
  }
#endif

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult MethodHandler::handleNext_unlocked(DBusConnection *connection,
                                                     DBusMessage *message) {
  std::cout << "MPRIS: Received Next command" << std::endl;

  if (!m_player) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Player not available");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  if (!m_properties->canGoNext()) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Cannot go to next track");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

#ifdef TESTING
  // In testing mode, just send success response
  sendMethodReturn_unlocked(connection, message);
#else
  m_player->nextTrack();
  sendMethodReturn_unlocked(connection, message);
#endif

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult
MethodHandler::handlePrevious_unlocked(DBusConnection *connection,
                                       DBusMessage *message) {
  std::cout << "MPRIS: Received Previous command" << std::endl;

  if (!m_player) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Player not available");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  if (!m_properties->canGoPrevious()) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Cannot go to previous track");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

#ifdef TESTING
  // In testing mode, just send success response
  sendMethodReturn_unlocked(connection, message);
#else
  m_player->prevTrack();
  sendMethodReturn_unlocked(connection, message);
#endif

  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult MethodHandler::handleSeek_unlocked(DBusConnection *connection,
                                                     DBusMessage *message) {
  auto offset_result = parseSeekArguments_unlocked(message);
  if (!offset_result.isSuccess()) {
    logValidationError_unlocked("Seek", "offset", offset_result.getError());
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            offset_result.getError());
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  int64_t offset_us = offset_result.getValue();
  auto validation_result = validateSeekOffset_unlocked(offset_us);
  if (!validation_result.isSuccess()) {
    logValidationError_unlocked("Seek", "offset", validation_result.getError());
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            validation_result.getError());
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  if (!m_properties->canSeek()) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Seeking is not supported");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  std::cout << "MPRIS: Received Seek command with offset: " << offset_us
            << " microseconds" << std::endl;

  // Calculate new absolute position (current position + offset)
  uint64_t current_position_us = m_properties->getPosition();
  int64_t new_position_us =
      static_cast<int64_t>(current_position_us) + offset_us;

  // Clamp to valid range
  if (new_position_us < 0) {
    new_position_us = 0;
  }

  uint64_t track_length_us = m_properties->getLength();
  if (track_length_us > 0 &&
      static_cast<uint64_t>(new_position_us) > track_length_us) {
    new_position_us = static_cast<int64_t>(track_length_us);
  }

  if (!m_player) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Player not available");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  // Convert to milliseconds for Player::seekTo
#ifndef TESTING
  unsigned long new_position_ms =
      static_cast<unsigned long>(new_position_us / 1000);
  m_player->seekTo(new_position_ms);
#endif

  sendMethodReturn_unlocked(connection, message);
  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult
MethodHandler::handleSetPosition_unlocked(DBusConnection *connection,
                                          DBusMessage *message) {
  auto args_result = parseSetPositionArguments_unlocked(message);
  if (!args_result.isSuccess()) {
    logValidationError_unlocked("SetPosition", "arguments",
                                args_result.getError());
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            args_result.getError());
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  auto [track_id, position_us] = args_result.getValue();

  auto track_validation = validateTrackId_unlocked(track_id);
  if (!track_validation.isSuccess()) {
    logValidationError_unlocked("SetPosition", "track_id",
                                track_validation.getError());
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            track_validation.getError());
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  auto position_validation = validatePosition_unlocked(position_us);
  if (!position_validation.isSuccess()) {
    logValidationError_unlocked("SetPosition", "position",
                                position_validation.getError());
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            position_validation.getError());
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  if (!m_properties->canSeek()) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Seeking is not supported");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  std::cout << "MPRIS: Received SetPosition command for track " << track_id
            << " to position " << position_us << " microseconds" << std::endl;

  if (!m_player) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Player not available");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  // Convert to milliseconds for Player::seekTo
#ifndef TESTING
  unsigned long position_ms = static_cast<unsigned long>(position_us / 1000);
  m_player->seekTo(position_ms);
#endif

  sendMethodReturn_unlocked(connection, message);
  return DBUS_HANDLER_RESULT_HANDLED;
}

#else // !HAVE_DBUS

// Stub implementations when D-Bus is not available

MethodHandler::MethodHandler([[maybe_unused]] Player *player,
                             [[maybe_unused]] PropertyManager *properties)
    : m_player(player), m_properties(properties), m_initialized(false) {}

MethodHandler::~MethodHandler() {}

DBusHandlerResult
MethodHandler::handleMessage([[maybe_unused]] DBusConnection *connection,
                             [[maybe_unused]] DBusMessage *message) {
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

bool MethodHandler::isReady() const { return false; }

#endif // HAVE_DBUS

#ifdef HAVE_DBUS

// D-Bus Properties interface handlers

DBusHandlerResult
MethodHandler::handleGetProperty_unlocked(DBusConnection *connection,
                                          DBusMessage *message) {
  auto args_result = parsePropertyArguments_unlocked(message);
  if (!args_result.isSuccess()) {
    logValidationError_unlocked("Get", "arguments", args_result.getError());
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            args_result.getError());
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  auto [interface_name, property_name] = args_result.getValue();

  std::cout << "MPRIS: Received Get property " << property_name
            << " on interface " << interface_name << std::endl;

  // Create reply message
  DBusMessage *reply = dbus_message_new_method_return(message);
  if (!reply) {
    logError_unlocked("handleGetProperty", "Failed to create reply message");
    return DBUS_HANDLER_RESULT_NEED_MEMORY;
  }

  try {
    appendPropertyToMessage_unlocked(reply, property_name);
    dbus_connection_send(connection, reply, nullptr);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
  } catch (const std::exception &e) {
    dbus_message_unref(reply);
    logError_unlocked("handleGetProperty",
                      "Failed to append property: " + std::string(e.what()));
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Failed to get property value");
    return DBUS_HANDLER_RESULT_HANDLED;
  }
}

DBusHandlerResult
MethodHandler::handleSetProperty_unlocked(DBusConnection *connection,
                                          DBusMessage *message) {
  // Parse arguments: interface_name, property_name, variant_value
  DBusMessageIter args;
  if (!dbus_message_iter_init(message, &args)) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            "Missing arguments for Set method");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  const char *interface_name;
  const char *property_name;

  if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            "First argument must be interface name string");
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  dbus_message_iter_get_basic(&args, &interface_name);

  if (!dbus_message_iter_next(&args) ||
      dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            "Second argument must be property name string");
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  dbus_message_iter_get_basic(&args, &property_name);

  if (!dbus_message_iter_next(&args) ||
      dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_VARIANT) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            "Third argument must be variant value");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  DBusMessageIter variant_iter;
  dbus_message_iter_recurse(&args, &variant_iter);

  std::cout << "MPRIS: Received Set property " << property_name
            << " on interface " << interface_name << std::endl;

  // Handle writable properties
  std::string prop_name_str(property_name);

  if (prop_name_str == "Volume") {
    if (dbus_message_iter_get_arg_type(&variant_iter) != DBUS_TYPE_DOUBLE) {
      sendErrorReply_unlocked(connection, message,
                              "org.mpris.MediaPlayer2.Error.InvalidArgs",
                              "Volume must be a double value");
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    double volume;
    dbus_message_iter_get_basic(&variant_iter, &volume);

    if (volume < 0.0 || volume > 1.0) {
      sendErrorReply_unlocked(connection, message,
                              "org.mpris.MediaPlayer2.Error.InvalidArgs",
                              "Volume must be between 0.0 and 1.0");
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    std::cout << "MPRIS: Setting volume to " << volume << std::endl;
    if (m_player) {
      m_player->setVolume(volume);
    }

  } else if (prop_name_str == "LoopStatus") {
    if (dbus_message_iter_get_arg_type(&variant_iter) != DBUS_TYPE_STRING) {
      sendErrorReply_unlocked(connection, message,
                              "org.mpris.MediaPlayer2.Error.InvalidArgs",
                              "LoopStatus must be a string value");
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    const char *loop_status;
    dbus_message_iter_get_basic(&variant_iter, &loop_status);

    std::string loop_str(loop_status);
    if (loop_str != "None" && loop_str != "Track" && loop_str != "Playlist") {
      sendErrorReply_unlocked(
          connection, message, "org.mpris.MediaPlayer2.Error.InvalidArgs",
          "LoopStatus must be 'None', 'Track', or 'Playlist'");
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    std::cout << "MPRIS: Setting loop status to " << loop_str << std::endl;

    // Map MPRIS string to LoopMode
    LoopMode mode = LoopMode::None;
    if (loop_str == "Track") {
        mode = LoopMode::One;
    } else if (loop_str == "Playlist") {
        mode = LoopMode::All;
    }

    if (m_player) {
        m_player->setLoopMode(mode);
    }

  } else if (prop_name_str == "Shuffle") {
    if (dbus_message_iter_get_arg_type(&variant_iter) != DBUS_TYPE_BOOLEAN) {
      sendErrorReply_unlocked(connection, message,
                              "org.mpris.MediaPlayer2.Error.InvalidArgs",
                              "Shuffle must be a boolean value");
      return DBUS_HANDLER_RESULT_HANDLED;
    }

    dbus_bool_t shuffle;
    dbus_message_iter_get_basic(&variant_iter, &shuffle);

    std::cout << "MPRIS: Setting shuffle to " << (shuffle ? "true" : "false")
              << std::endl;
    // TODO: Implement shuffle control in Player if available

  } else {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.NotSupported",
                            "Property '" + prop_name_str + "' is not writable");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  sendMethodReturn_unlocked(connection, message);
  return DBUS_HANDLER_RESULT_HANDLED;
}

DBusHandlerResult
MethodHandler::handleGetAllProperties_unlocked(DBusConnection *connection,
                                               DBusMessage *message) {
  DBusMessageIter args;
  if (!dbus_message_iter_init(message, &args)) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            "Missing interface name argument");
    return DBUS_HANDLER_RESULT_HANDLED;
  }

  const char *interface_name;
  if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.InvalidArgs",
                            "Interface name must be a string");
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  dbus_message_iter_get_basic(&args, &interface_name);

  std::cout << "MPRIS: Received GetAll properties for interface "
            << interface_name << std::endl;

  // Create reply message
  DBusMessage *reply = dbus_message_new_method_return(message);
  if (!reply) {
    logError_unlocked("handleGetAllProperties",
                      "Failed to create reply message");
    return DBUS_HANDLER_RESULT_NEED_MEMORY;
  }

  try {
    appendAllPropertiesToMessage_unlocked(reply, interface_name);
    dbus_connection_send(connection, reply, nullptr);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
  } catch (const std::exception &e) {
    dbus_message_unref(reply);
    logError_unlocked("handleGetAllProperties",
                      "Failed to append properties: " + std::string(e.what()));
    sendErrorReply_unlocked(connection, message,
                            "org.mpris.MediaPlayer2.Error.Failed",
                            "Failed to get all properties");
    return DBUS_HANDLER_RESULT_HANDLED;
  }
}

// Utility methods for D-Bus message handling

void MethodHandler::sendMethodReturn_unlocked(DBusConnection *connection,
                                              DBusMessage *message) {
  DBusMessage *reply = dbus_message_new_method_return(message);
  if (reply) {
    dbus_connection_send(connection, reply, nullptr);
    dbus_message_unref(reply);
  } else {
    logError_unlocked("sendMethodReturn",
                      "Failed to create method return message");
  }
}

void MethodHandler::sendErrorReply_unlocked(DBusConnection *connection,
                                            DBusMessage *message,
                                            const std::string &error_name,
                                            const std::string &error_message) {
  DBusMessage *reply = dbus_message_new_error(message, error_name.c_str(),
                                              error_message.c_str());
  if (reply) {
    dbus_connection_send(connection, reply, nullptr);
    dbus_message_unref(reply);
  } else {
    logError_unlocked("sendErrorReply", "Failed to create error reply message");
  }
}

// Input validation helpers

PsyMP3::MPRIS::Result<int64_t>
MethodHandler::validateSeekOffset_unlocked(int64_t offset) {
  if (std::abs(offset) > MAX_SEEK_OFFSET_US) {
    return PsyMP3::MPRIS::Result<int64_t>::error(
        "Seek offset too large (max 1 hour)");
  }
  return PsyMP3::MPRIS::Result<int64_t>::success(offset);
}

PsyMP3::MPRIS::Result<uint64_t>
MethodHandler::validatePosition_unlocked(uint64_t position_us) {
  if (position_us > MAX_POSITION_US) {
    return PsyMP3::MPRIS::Result<uint64_t>::error(
        "Position too large (max 24 hours)");
  }
  return PsyMP3::MPRIS::Result<uint64_t>::success(position_us);
}

PsyMP3::MPRIS::Result<std::string>
MethodHandler::validateTrackId_unlocked(const std::string &track_id) {
  if (track_id.empty()) {
    return PsyMP3::MPRIS::Result<std::string>::error(
        "Track ID cannot be empty");
  }

  // Basic validation - track ID should be a valid object path or URI
  if (track_id.find_first_of(" \t\n\r") != std::string::npos) {
    return PsyMP3::MPRIS::Result<std::string>::error(
        "Track ID contains invalid characters");
  }

  return PsyMP3::MPRIS::Result<std::string>::success(track_id);
}

// D-Bus message parsing helpers

PsyMP3::MPRIS::Result<int64_t>
MethodHandler::parseSeekArguments_unlocked(DBusMessage *message) {
  DBusMessageIter args;
  if (!dbus_message_iter_init(message, &args)) {
    return PsyMP3::MPRIS::Result<int64_t>::error(
        "Missing arguments for Seek method");
  }

  if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT64) {
    return PsyMP3::MPRIS::Result<int64_t>::error(
        "Seek offset must be an int64 value");
  }

  dbus_int64_t offset;
  dbus_message_iter_get_basic(&args, &offset);

  return PsyMP3::MPRIS::Result<int64_t>::success(static_cast<int64_t>(offset));
}

PsyMP3::MPRIS::Result<std::pair<std::string, uint64_t>>
MethodHandler::parseSetPositionArguments_unlocked(DBusMessage *message) {
  DBusMessageIter args;
  if (!dbus_message_iter_init(message, &args)) {
    return PsyMP3::MPRIS::Result<std::pair<std::string, uint64_t>>::error(
        "Missing arguments for SetPosition method");
  }

  // First argument: track ID (object path)
  if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_OBJECT_PATH) {
    return PsyMP3::MPRIS::Result<std::pair<std::string, uint64_t>>::error(
        "First argument must be track ID object path");
  }

  const char *track_id_cstr;
  dbus_message_iter_get_basic(&args, &track_id_cstr);
  std::string track_id(track_id_cstr);

  // Second argument: position in microseconds
  if (!dbus_message_iter_next(&args) ||
      dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_INT64) {
    return PsyMP3::MPRIS::Result<std::pair<std::string, uint64_t>>::error(
        "Second argument must be position int64 value");
  }

  dbus_int64_t position;
  dbus_message_iter_get_basic(&args, &position);

  if (position < 0) {
    return PsyMP3::MPRIS::Result<std::pair<std::string, uint64_t>>::error(
        "Position cannot be negative");
  }

  return PsyMP3::MPRIS::Result<std::pair<std::string, uint64_t>>::success(
      std::make_pair(track_id, static_cast<uint64_t>(position)));
}

PsyMP3::MPRIS::Result<std::pair<std::string, std::string>>
MethodHandler::parsePropertyArguments_unlocked(DBusMessage *message) {
  DBusMessageIter args;
  if (!dbus_message_iter_init(message, &args)) {
    return PsyMP3::MPRIS::Result<std::pair<std::string, std::string>>::error(
        "Missing arguments for property method");
  }

  // First argument: interface name
  if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
    return PsyMP3::MPRIS::Result<std::pair<std::string, std::string>>::error(
        "First argument must be interface name string");
  }

  const char *interface_name;
  dbus_message_iter_get_basic(&args, &interface_name);

  // Second argument: property name
  if (!dbus_message_iter_next(&args) ||
      dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
    return PsyMP3::MPRIS::Result<std::pair<std::string, std::string>>::error(
        "Second argument must be property name string");
  }

  const char *property_name;
  dbus_message_iter_get_basic(&args, &property_name);

  return PsyMP3::MPRIS::Result<std::pair<std::string, std::string>>::success(
      std::make_pair(std::string(interface_name), std::string(property_name)));
}

#endif // HAVE_DBUS

#ifdef HAVE_DBUS

// Property value serialization for D-Bus responses

void MethodHandler::appendVariantToMessage_unlocked(
    DBusMessage *reply, const PsyMP3::MPRIS::DBusVariant &variant) {
  DBusMessageIter args;
  dbus_message_iter_init_append(reply, &args);
  appendVariantToIter_unlocked(&args, variant);
}

void MethodHandler::appendVariantToIter_unlocked(
    DBusMessageIter *iter, const PsyMP3::MPRIS::DBusVariant &variant) {
  DBusMessageIter variant_iter;

  switch (variant.type) {
  case PsyMP3::MPRIS::DBusVariant::String: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "s",
                                     &variant_iter);
    const std::string &str_val = variant.get<std::string>();
    const char *str_cstr = str_val.c_str();
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &str_cstr);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  case PsyMP3::MPRIS::DBusVariant::StringArray: {
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
  case PsyMP3::MPRIS::DBusVariant::Int64: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "x",
                                     &variant_iter);
    dbus_int64_t int_val = static_cast<dbus_int64_t>(variant.get<int64_t>());
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_INT64, &int_val);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  case PsyMP3::MPRIS::DBusVariant::UInt64: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "t",
                                     &variant_iter);
    dbus_uint64_t uint_val =
        static_cast<dbus_uint64_t>(variant.get<uint64_t>());
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_UINT64, &uint_val);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  case PsyMP3::MPRIS::DBusVariant::Double: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "d",
                                     &variant_iter);
    double double_val = variant.get<double>();
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_DOUBLE,
                                   &double_val);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  case PsyMP3::MPRIS::DBusVariant::Boolean: {
    dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "b",
                                     &variant_iter);
    dbus_bool_t bool_val = variant.get<bool>() ? TRUE : FALSE;
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_val);
    dbus_message_iter_close_container(iter, &variant_iter);
    break;
  }
  case PsyMP3::MPRIS::DBusVariant::Dictionary: {
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
      appendVariantToIter_unlocked(&entry_iter, value);

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

void MethodHandler::appendPropertyToMessage_unlocked(
    DBusMessage *reply, const std::string &property_name) {
  DBusMessageIter args, variant_iter;
  dbus_message_iter_init_append(reply, &args);

  if (property_name == "PlaybackStatus") {
    appendVariantToIter_unlocked(
        &args, PsyMP3::MPRIS::DBusVariant(m_properties->getPlaybackStatus()));

  } else if (property_name == "Metadata") {
    appendVariantToIter_unlocked(
        &args, PsyMP3::MPRIS::DBusVariant(m_properties->getMetadata()));

  } else if (property_name == "Position") {
    uint64_t position = m_properties->getPosition();
    dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "x",
                                     &variant_iter);
    dbus_int64_t position_val = static_cast<dbus_int64_t>(position);
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_INT64,
                                   &position_val);
    dbus_message_iter_close_container(&args, &variant_iter);

  } else if (property_name == "CanGoNext") {
    bool can_go_next = m_properties->canGoNext();
    dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "b",
                                     &variant_iter);
    dbus_bool_t bool_val = can_go_next ? TRUE : FALSE;
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_val);
    dbus_message_iter_close_container(&args, &variant_iter);

  } else if (property_name == "CanGoPrevious") {
    bool can_go_previous = m_properties->canGoPrevious();
    dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "b",
                                     &variant_iter);
    dbus_bool_t bool_val = can_go_previous ? TRUE : FALSE;
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_val);
    dbus_message_iter_close_container(&args, &variant_iter);

  } else if (property_name == "CanSeek") {
    bool can_seek = m_properties->canSeek();
    dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "b",
                                     &variant_iter);
    dbus_bool_t bool_val = can_seek ? TRUE : FALSE;
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_val);
    dbus_message_iter_close_container(&args, &variant_iter);

  } else if (property_name == "CanControl") {
    bool can_control = m_properties->canControl();
    dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "b",
                                     &variant_iter);
    dbus_bool_t bool_val = can_control ? TRUE : FALSE;
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_val);
    dbus_message_iter_close_container(&args, &variant_iter);

  } else if (property_name == "Volume") {
    double volume = m_player ? m_player->getVolume() : 1.0;
    dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "d",
                                     &variant_iter);
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_DOUBLE, &volume);
    dbus_message_iter_close_container(&args, &variant_iter);

  } else if (property_name == "LoopStatus") {
    std::string loop_status = PsyMP3::MPRIS::loopStatusToString(m_properties->getLoopStatus());
    dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "s",
                                     &variant_iter);
    const char *loop_status_cstr = loop_status.c_str();
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING,
                                   &loop_status_cstr);
    dbus_message_iter_close_container(&args, &variant_iter);

  } else if (property_name == "Shuffle") {
    // Default to false since Player doesn't have shuffle control yet
    dbus_bool_t shuffle = FALSE;
    dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "b",
                                     &variant_iter);
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &shuffle);
    dbus_message_iter_close_container(&args, &variant_iter);

  } else {
    throw std::runtime_error("Unknown property: " + property_name);
  }
}

void MethodHandler::appendAllPropertiesToMessage_unlocked(
    DBusMessage *reply, const std::string &interface_name) {
  DBusMessageIter args, dict_iter;
  dbus_message_iter_init_append(reply, &args);
  dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict_iter);

  if (interface_name == MPRIS_PLAYER_INTERFACE) {
    // Add all Player interface properties
    std::vector<std::string> properties = {
        "PlaybackStatus", "Metadata", "Position",   "CanGoNext",
        "CanGoPrevious",  "CanSeek",  "CanControl", "Volume",
        "LoopStatus",     "Shuffle"};

    auto all_properties = m_properties->getAllProperties();
    for (const auto &prop_name : properties) {
      DBusMessageIter entry_iter, variant_iter;
      dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY,
                                       nullptr, &entry_iter);

      const char *prop_name_cstr = prop_name.c_str();
      dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING,
                                     &prop_name_cstr);

      try {
        auto it = all_properties.find(prop_name);
        if (it != all_properties.end()) {
          appendVariantToIter_unlocked(&entry_iter, it->second);
        } else {
          // Fallback - this should not happen if all properties are in the map
          dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s",
                                           &variant_iter);
          const char *empty_str = "";
          dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING,
                                         &empty_str);
          dbus_message_iter_close_container(&entry_iter, &variant_iter);
        }
      } catch (const std::exception &e) {
          logError_unlocked("appendAllPropertiesToMessage",
                            "Failed to get property " + prop_name + ": " +
                                e.what());
          // Add empty variant as fallback
          dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s",
                                           &variant_iter);
          const char *empty_str = "";
          dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING,
                                         &empty_str);
          dbus_message_iter_close_container(&entry_iter, &variant_iter);
        }
      }

      dbus_message_iter_close_container(&dict_iter, &entry_iter);
    }
  } else if (interface_name == MPRIS_MEDIAPLAYER2_INTERFACE) {
    // Add MediaPlayer2 interface properties
    DBusMessageIter entry_iter, variant_iter;

    // Identity property
    dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                     &entry_iter);
    const char *identity_key = "Identity";
    dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING,
                                   &identity_key);
    dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "s",
                                     &variant_iter);
    const char *identity_val = "PsyMP3";
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING,
                                   &identity_val);
    dbus_message_iter_close_container(&entry_iter, &variant_iter);
    dbus_message_iter_close_container(&dict_iter, &entry_iter);

    // CanQuit property
    dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                     &entry_iter);
    const char *can_quit_key = "CanQuit";
    dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING,
                                   &can_quit_key);
    dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "b",
                                     &variant_iter);
    dbus_bool_t can_quit_val = TRUE;
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN,
                                   &can_quit_val);
    dbus_message_iter_close_container(&entry_iter, &variant_iter);
    dbus_message_iter_close_container(&dict_iter, &entry_iter);

    // CanRaise property
    dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr,
                                     &entry_iter);
    const char *can_raise_key = "CanRaise";
    dbus_message_iter_append_basic(&entry_iter, DBUS_TYPE_STRING,
                                   &can_raise_key);
    dbus_message_iter_open_container(&entry_iter, DBUS_TYPE_VARIANT, "b",
                                     &variant_iter);
    dbus_bool_t can_raise_val = TRUE;
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN,
                                   &can_raise_val);
    dbus_message_iter_close_container(&entry_iter, &variant_iter);
    dbus_message_iter_close_container(&dict_iter, &entry_iter);
  }

  dbus_message_iter_close_container(&args, &dict_iter);
}

// Error handling and logging

void MethodHandler::logMethodCall_unlocked(const std::string &interface_name,
                                           const std::string &method_name) {
  std::cout << "MPRIS MethodHandler: " << interface_name << "." << method_name
            << std::endl;
}

void MethodHandler::logError_unlocked(const std::string &context,
                                      const std::string &error_message) {
  std::cerr << "MPRIS MethodHandler Error [" << context
            << "]: " << error_message << std::endl;
}

void MethodHandler::logValidationError_unlocked(
    const std::string &method_name, const std::string &parameter,
    const std::string &error_message) {
  std::cerr << "MPRIS MethodHandler Validation Error [" << method_name << "."
            << parameter << "]: " << error_message << std::endl;
}

#endif // HAVE_DBUS

} // namespace MPRIS
} // namespace PsyMP3
