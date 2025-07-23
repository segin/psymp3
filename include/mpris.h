#ifndef MPRIS_H
#define MPRIS_H

#ifdef HAVE_DBUS

// No direct includes - all includes should be in psymp3.h

class Player; // Forward declaration

class MPRIS {
public:
    MPRIS(Player* player);
    ~MPRIS();

    void init();
    void shutdown();
    void updateMetadata(const std::string& artist, const std::string& title, const std::string& album);
    void updatePlaybackStatus(const std::string& status); // "Playing", "Paused", "Stopped"

private:
    Player* m_player;
    DBusConnection* m_conn;
    bool m_initialized;

    static DBusHandlerResult staticHandleMessage(DBusConnection* connection, DBusMessage* message, void* user_data);
    DBusHandlerResult handleMessage(DBusConnection* connection, DBusMessage* message);
};

#endif // HAVE_DBUS

#endif // MPRIS_H
