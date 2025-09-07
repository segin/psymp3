#include "psymp3.h"

#ifdef HAVE_DBUS
#include <dbus/dbus.h>
#endif

namespace MPRISTypes {

// DBusVariant string conversion
std::string DBusVariant::toString() const {
    switch (type) {
        case String:
            return "\"" + std::get<std::string>(value) + "\"";
        case StringArray: {
            const auto& arr = std::get<std::vector<std::string>>(value);
            std::string result = "[";
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) result += ", ";
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
        default:
            return "<unknown>";
    }
}

// MPRISMetadata implementation
std::map<std::string, DBusVariant> MPRISMetadata::toDBusDict() const {
    std::map<std::string, DBusVariant> dict;
    
    if (!artist.empty()) {
        dict["xesam:artist"] = DBusVariant(std::vector<std::string>{artist});
    }
    if (!title.empty()) {
        dict["xesam:title"] = DBusVariant(std::string(title));
    }
    if (!album.empty()) {
        dict["xesam:album"] = DBusVariant(std::string(album));
    }
    if (!track_id.empty()) {
        dict["mpris:trackid"] = DBusVariant(std::string(track_id));
    }
    if (length_us > 0) {
        dict["mpris:length"] = DBusVariant(static_cast<int64_t>(length_us));
    }
    if (!art_url.empty()) {
        dict["mpris:artUrl"] = DBusVariant(std::string(art_url));
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
    return artist.empty() && title.empty() && album.empty() && 
           track_id.empty() && length_us == 0 && art_url.empty();
}

// RAII deleters implementation
void DBusConnectionDeleter::operator()(DBusConnection* conn) {
#ifdef HAVE_DBUS
    if (conn) {
        dbus_connection_unref(conn);
    }
#else
    (void)conn; // Suppress unused parameter warning
#endif
}

void DBusMessageDeleter::operator()(DBusMessage* msg) {
#ifdef HAVE_DBUS
    if (msg) {
        dbus_message_unref(msg);
    }
#else
    (void)msg; // Suppress unused parameter warning
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

PlaybackStatus stringToPlaybackStatus(const std::string& str) {
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

LoopStatus stringToLoopStatus(const std::string& str) {
    if (str == "Track") {
        return LoopStatus::Track;
    } else if (str == "Playlist") {
        return LoopStatus::Playlist;
    } else {
        return LoopStatus::None; // Default fallback
    }
}

} // namespace MPRISTypes