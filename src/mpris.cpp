#include "psymp3.h"

#ifdef HAVE_DBUS

// DBus vtable for MPRIS
DBusObjectPathVTable MPRIS::vtable = {
    nullptr,  // unregister_function
    MPRIS::staticHandleMessage,  // message_function
    nullptr,  // dbus_internal_pad1
    nullptr,  // dbus_internal_pad2
    nullptr,  // dbus_internal_pad3
    nullptr   // dbus_internal_pad4
};

MPRIS::MPRIS(Player* player)
    : m_player(player),
      m_conn(nullptr),
      m_initialized(false)
{
}

MPRIS::~MPRIS() {
    shutdown();
}

void MPRIS::init() {
    DBusError err;
    dbus_error_init(&err);

    std::cout << "MPRIS: Initializing..." << std::endl;

    m_conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        std::cerr << "MPRIS: DBus connection error: " << err.message << std::endl;
        dbus_error_free(&err);
        return;
    }
    if (m_conn == nullptr) {
        std::cerr << "MPRIS: DBus connection is null." << std::endl;
        return;
    }

    std::cout << "MPRIS: DBus connection successful." << std::endl;

    int ret = dbus_bus_request_name(m_conn, "org.mpris.MediaPlayer2.psymp3",
                                    DBUS_NAME_FLAG_REPLACE_EXISTING | DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (dbus_error_is_set(&err)) {
        std::cerr << "MPRIS: DBus request name error: " << err.message << std::endl;
        dbus_error_free(&err);
        return;
    }
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        std::cerr << "MPRIS: Not primary owner of name. Reply: " << ret << std::endl;
        return;
    }

    std::cout << "MPRIS: DBus name requested successfully." << std::endl;

    // Register the path and interface for MPRIS
    if (!dbus_connection_register_object_path(m_conn, "/org/mpris/MediaPlayer2", &MPRIS::vtable, this)) {
        std::cerr << "MPRIS: Failed to register object path." << std::endl;
        return;
    }

    std::cout << "MPRIS: Object path registered." << std::endl;

    m_initialized = true;
    std::cout << "MPRIS initialized successfully." << std::endl;
}

void MPRIS::shutdown() {
    if (m_initialized) {
        dbus_bus_release_name(m_conn, "org.mpris.MediaPlayer2.psymp3", nullptr);
        dbus_connection_unref(m_conn);
        m_initialized = false;
        std::cout << "MPRIS shut down." << std::endl;
    }
}

void MPRIS::updateMetadata(const std::string& artist, const std::string& title, const std::string& album) {
    if (!m_initialized) return;

    DBusMessage* msg;
    DBusMessageIter args;

    msg = dbus_message_new_signal("/org/mpris/MediaPlayer2",
                                  "org.mpris.MediaPlayer2.Player",
                                  "PropertiesChanged");
    if (msg == nullptr) {
        std::cerr << "Failed to create DBus signal." << std::endl;
        return;
    }

    dbus_message_iter_init_append(msg, &args);
    const char* interface_name = "org.mpris.MediaPlayer2.Player";
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &interface_name);

    DBusMessageIter array_iter;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &array_iter);

    // Add metadata properties
    DBusMessageIter dict_entry_iter;
    dbus_message_iter_open_container(&array_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &dict_entry_iter);
    const char* metadata_key = "Metadata";
    dbus_message_iter_append_basic(&dict_entry_iter, DBUS_TYPE_STRING, &metadata_key);
    
    DBusMessageIter variant_iter;
    dbus_message_iter_open_container(&dict_entry_iter, DBUS_TYPE_VARIANT, "a{sv}", &variant_iter);
    DBusMessageIter metadata_dict_iter;
    dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY, "{sv}", &metadata_dict_iter);

    // Add artist
    DBusMessageIter artist_entry_iter;
    dbus_message_iter_open_container(&metadata_dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &artist_entry_iter);
    const char* artist_key = "xesam:artist";
    dbus_message_iter_append_basic(&artist_entry_iter, DBUS_TYPE_STRING, &artist_key);
    DBusMessageIter artist_array_iter;
    dbus_message_iter_open_container(&artist_entry_iter, DBUS_TYPE_ARRAY, "s", &artist_array_iter);
    const char* artist_cstr = artist.c_str();
    dbus_message_iter_append_basic(&artist_array_iter, DBUS_TYPE_STRING, &artist_cstr);
    dbus_message_iter_close_container(&artist_entry_iter, &artist_array_iter);
    dbus_message_iter_close_container(&metadata_dict_iter, &artist_entry_iter);

    // Add title
    DBusMessageIter title_entry_iter;
    dbus_message_iter_open_container(&metadata_dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &title_entry_iter);
    const char* title_key = "xesam:title";
    dbus_message_iter_append_basic(&title_entry_iter, DBUS_TYPE_STRING, &title_key);
    const char* title_cstr = title.c_str();
    dbus_message_iter_append_basic(&title_entry_iter, DBUS_TYPE_STRING, &title_cstr);
    dbus_message_iter_close_container(&metadata_dict_iter, &title_entry_iter);

    // Add album
    DBusMessageIter album_entry_iter;
    dbus_message_iter_open_container(&metadata_dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &album_entry_iter);
    const char* album_key = "xesam:album";
    dbus_message_iter_append_basic(&album_entry_iter, DBUS_TYPE_STRING, &album_key);
    const char* album_cstr = album.c_str();
    dbus_message_iter_append_basic(&album_entry_iter, DBUS_TYPE_STRING, &album_cstr);
    dbus_message_iter_close_container(&metadata_dict_iter, &album_entry_iter);

    dbus_message_iter_close_container(&variant_iter, &metadata_dict_iter);
    dbus_message_iter_close_container(&array_iter, &dict_entry_iter);

    // Empty array of invalidated properties
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "s", &array_iter);
    dbus_message_iter_close_container(&args, &array_iter);

    dbus_connection_send(m_conn, msg, nullptr);
    dbus_message_unref(msg);
}

void MPRIS::updatePlaybackStatus(const std::string& status) {
    if (!m_initialized) return;

    DBusMessage* msg;
    DBusMessageIter args;

    msg = dbus_message_new_signal("/org/mpris/MediaPlayer2",
                                  "org.mpris.MediaPlayer2.Player",
                                  "PropertiesChanged");
    if (msg == nullptr) {
        std::cerr << "Failed to create DBus signal." << std::endl;
        return;
    }

    dbus_message_iter_init_append(msg, &args);
    const char* interface_name = "org.mpris.MediaPlayer2.Player";
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &interface_name);

    DBusMessageIter array_iter;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &array_iter);

    // Add PlaybackStatus property
    DBusMessageIter dict_entry_iter;
    dbus_message_iter_open_container(&array_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &dict_entry_iter);
    const char* property_name = "PlaybackStatus";
    dbus_message_iter_append_basic(&dict_entry_iter, DBUS_TYPE_STRING, &property_name);
    
    DBusMessageIter variant_iter;
    dbus_message_iter_open_container(&dict_entry_iter, DBUS_TYPE_VARIANT, "s", &variant_iter);
    const char* status_cstr = status.c_str();
    dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &status_cstr);
    dbus_message_iter_close_container(&dict_entry_iter, &variant_iter);
    dbus_message_iter_close_container(&array_iter, &dict_entry_iter);

    // Empty array of invalidated properties
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "s", &array_iter);
    dbus_message_iter_close_container(&args, &array_iter);

    dbus_connection_send(m_conn, msg, nullptr);
    dbus_message_unref(msg);
}

DBusHandlerResult MPRIS::handleMessage(DBusConnection* connection, DBusMessage* message) {
    const char* interface_name = dbus_message_get_interface(message);
    const char* member_name = dbus_message_get_member(message);
    
    (void)interface_name; // Suppress unused variable warning
    (void)member_name;    // Suppress unused variable warning

    if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2", "Raise")) {
        // Bring the media player to the foreground
        // For a CLI app, this might mean nothing or just printing a message
        std::cout << "MPRIS: Received Raise command." << std::endl;
        DBusMessage* reply = dbus_message_new_method_return(message);
        dbus_connection_send(connection, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2", "Quit")) {
        // Quit the media player
        std::cout << "MPRIS: Received Quit command." << std::endl;
        DBusMessage* reply = dbus_message_new_method_return(message);
        dbus_connection_send(connection, reply, nullptr);
        dbus_message_unref(reply);
        // Signal the player to exit
        m_player->synthesizeUserEvent(QUIT_APPLICATION, nullptr, nullptr);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "Play")) {
        std::cout << "MPRIS: Received Play command." << std::endl;
        m_player->play();
        DBusMessage* reply = dbus_message_new_method_return(message);
        dbus_connection_send(connection, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "Pause")) {
        std::cout << "MPRIS: Received Pause command." << std::endl;
        m_player->pause();
        DBusMessage* reply = dbus_message_new_method_return(message);
        dbus_connection_send(connection, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "PlayPause")) {
        std::cout << "MPRIS: Received PlayPause command." << std::endl;
        m_player->playPause();
        DBusMessage* reply = dbus_message_new_method_return(message);
        dbus_connection_send(connection, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "Stop")) {
        std::cout << "MPRIS: Received Stop command." << std::endl;
        m_player->stop();
        DBusMessage* reply = dbus_message_new_method_return(message);
        dbus_connection_send(connection, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "Next")) {
        std::cout << "MPRIS: Received Next command." << std::endl;
        m_player->nextTrack();
        DBusMessage* reply = dbus_message_new_method_return(message);
        dbus_connection_send(connection, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "Previous")) {
        std::cout << "MPRIS: Received Previous command." << std::endl;
        m_player->prevTrack();
        DBusMessage* reply = dbus_message_new_method_return(message);
        dbus_connection_send(connection, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    } else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "Seek")) {
        dbus_int64_t offset;
        if (dbus_message_get_args(message, nullptr, DBUS_TYPE_INT64, &offset, DBUS_TYPE_INVALID)) {
            std::cout << "MPRIS: Received Seek command with offset: " << offset << std::endl;
            // The Seek method in MPRIS is relative to the current position.
            // Our seekTo is absolute, so we need to calculate the new absolute position.
            // This requires knowing the current position, which is tricky with mutexes.
            // For simplicity, let's assume a direct seek for now, or implement a relative seek in Player.
            // For now, we'll just add the offset to the current position.
            // This needs to be done carefully under mutex protection.
            {
                std::lock_guard<std::mutex> lock(*m_player->mutex);
                if (m_player->stream) {
                    unsigned long current_pos = m_player->stream->getPosition();
                    long new_pos = static_cast<long>(current_pos) + static_cast<long>(offset / 1000); // Convert microseconds to milliseconds
                    if (new_pos < 0) new_pos = 0;
                    m_player->seekTo(static_cast<unsigned long>(new_pos));
                }
            }
            DBusMessage* reply = dbus_message_new_method_return(message);
            dbus_connection_send(connection, reply, nullptr);
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    } else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "SetPosition")) {
        dbus_uint64_t track_id;
        dbus_int64_t position;
        if (dbus_message_get_args(message, nullptr, DBUS_TYPE_OBJECT_PATH, &track_id, DBUS_TYPE_INT64, &position, DBUS_TYPE_INVALID)) {
            std::cout << "MPRIS: Received SetPosition command for track " << track_id << " to position " << position << std::endl;
            // Position is in microseconds, convert to milliseconds
            m_player->seekTo(static_cast<unsigned long>(position / 1000));
            DBusMessage* reply = dbus_message_new_method_return(message);
            dbus_connection_send(connection, reply, nullptr);
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    } else if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Properties", "Get")) {
        const char* iface_name;
        const char* prop_name;
        if (dbus_message_get_args(message, nullptr, DBUS_TYPE_STRING, &iface_name, DBUS_TYPE_STRING, &prop_name, DBUS_TYPE_INVALID)) {
            DBusMessage* reply = dbus_message_new_method_return(message);
            DBusMessageIter args;
            dbus_message_iter_init_append(reply, &args);

            if (std::string(prop_name) == "PlaybackStatus") {
                std::string status_str;
                if (m_player->state == PlayerState::Playing) {
                    status_str = "Playing";
                } else if (m_player->state == PlayerState::Paused) {
                    status_str = "Paused";
                } else {
                    status_str = "Stopped";
                }
                // Create a variant containing the string
                DBusMessageIter variant_iter;
                dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "s", &variant_iter);
                const char* status_cstr = status_str.c_str();
                dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &status_cstr);
                dbus_message_iter_close_container(&args, &variant_iter);
            } else if (std::string(prop_name) == "Metadata") {
                DBusMessageIter variant_iter;
                dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "a{sv}", &variant_iter);
                DBusMessageIter metadata_dict_iter;
                dbus_message_iter_open_container(&variant_iter, DBUS_TYPE_ARRAY, "{sv}", &metadata_dict_iter);

                // Add metadata properties
                if (m_player->stream) {
                    std::string artist_str = m_player->stream->getArtist().to8Bit(true);
                    std::string title_str = m_player->stream->getTitle().to8Bit(true);
                    std::string album_str = m_player->stream->getAlbum().to8Bit(true);

                    // Artist
                    DBusMessageIter artist_entry_iter;
                    dbus_message_iter_open_container(&metadata_dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &artist_entry_iter);
                    const char* artist_key = "xesam:artist";
                    dbus_message_iter_append_basic(&artist_entry_iter, DBUS_TYPE_STRING, &artist_key);
                    DBusMessageIter artist_array_iter;
                    dbus_message_iter_open_container(&artist_entry_iter, DBUS_TYPE_ARRAY, "s", &artist_array_iter);
                    const char* artist_str_cstr = artist_str.c_str();
                    dbus_message_iter_append_basic(&artist_array_iter, DBUS_TYPE_STRING, &artist_str_cstr);
                    dbus_message_iter_close_container(&artist_entry_iter, &artist_array_iter);
                    dbus_message_iter_close_container(&metadata_dict_iter, &artist_entry_iter);

                    // Title
                    DBusMessageIter title_entry_iter;
                    dbus_message_iter_open_container(&metadata_dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &title_entry_iter);
                    const char* title_key = "xesam:title";
                    dbus_message_iter_append_basic(&title_entry_iter, DBUS_TYPE_STRING, &title_key);
                    const char* title_str_cstr = title_str.c_str();
                    dbus_message_iter_append_basic(&title_entry_iter, DBUS_TYPE_STRING, &title_str_cstr);
                    dbus_message_iter_close_container(&metadata_dict_iter, &title_entry_iter);

                    // Album
                    DBusMessageIter album_entry_iter;
                    dbus_message_iter_open_container(&metadata_dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &album_entry_iter);
                    const char* album_key = "xesam:album";
                    dbus_message_iter_append_basic(&album_entry_iter, DBUS_TYPE_STRING, &album_key);
                    const char* album_str_cstr = album_str.c_str();
                    dbus_message_iter_append_basic(&album_entry_iter, DBUS_TYPE_STRING, &album_str_cstr);
                    dbus_message_iter_close_container(&metadata_dict_iter, &album_entry_iter);

                    // Duration (in microseconds)
                    DBusMessageIter length_entry_iter;
                    dbus_message_iter_open_container(&metadata_dict_iter, DBUS_TYPE_DICT_ENTRY, nullptr, &length_entry_iter);
                    const char* length_key = "mpris:length";
                    dbus_message_iter_append_basic(&length_entry_iter, DBUS_TYPE_STRING, &length_key);
                    dbus_int64_t length_us = static_cast<dbus_int64_t>(m_player->stream->getLength()) * 1000;
                    dbus_message_iter_append_basic(&length_entry_iter, DBUS_TYPE_UINT64, &length_us);
                    dbus_message_iter_close_container(&metadata_dict_iter, &length_entry_iter);
                }

                dbus_message_iter_close_container(&variant_iter, &metadata_dict_iter);
            }

            dbus_connection_send(connection, reply, nullptr);
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    } else if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Properties", "Set")) {
        const char* iface_name;
        const char* prop_name;
        DBusMessageIter variant_iter;
        if (dbus_message_get_args(message, nullptr, DBUS_TYPE_STRING, &iface_name, DBUS_TYPE_STRING, &prop_name, DBUS_TYPE_VARIANT, &variant_iter, DBUS_TYPE_INVALID)) {
            if (std::string(prop_name) == "PlaybackStatus") {
                const char* status_str;
                dbus_message_iter_get_basic(&variant_iter, &status_str);
                std::string status = status_str;
                std::cout << "MPRIS: Received Set PlaybackStatus to " << status << std::endl;
                if (status == "Playing") {
                    m_player->play();
                } else if (status == "Paused") {
                    m_player->pause();
                } else if (status == "Stopped") {
                    m_player->stop();
                }
            } else if (std::string(prop_name) == "Volume") {
                double volume;
                dbus_message_iter_get_basic(&variant_iter, &volume);
                std::cout << "MPRIS: Received Set Volume to " << volume << std::endl;
                // Implement volume control if available in Player
            } else if (std::string(prop_name) == "Position") {
                dbus_int64_t position_us;
                dbus_message_iter_get_basic(&variant_iter, &position_us);
                std::cout << "MPRIS: Received Set Position to " << position_us << " us" << std::endl;
                m_player->seekTo(static_cast<unsigned long>(position_us / 1000));
            }
            DBusMessage* reply = dbus_message_new_method_return(message);
            dbus_connection_send(connection, reply, nullptr);
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult MPRIS::staticHandleMessage(DBusConnection* connection, DBusMessage* message, void* user_data) {
    return static_cast<MPRIS*>(user_data)->handleMessage(connection, message);
}

#endif // HAVE_DBUS
