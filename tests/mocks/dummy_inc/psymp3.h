#ifndef PSYM3_H
#define PSYM3_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <iostream>
#include <variant>

enum class LoopMode {
    None,
    One,
    All
};

class Player {
public:
    void setVolume(double);
    double getVolume() const;
    void setLoopMode(LoopMode);
    bool play();
    bool pause();
    bool stop();
    bool playPause();
    void nextTrack();
    void prevTrack();
    void seekTo(unsigned long);
    static void synthesizeUserEvent(int, void*, void*);
};

#define QUIT_APPLICATION 100

namespace PsyMP3 {
namespace MPRIS {
    class PropertyManager;
    enum class LoopStatus { None, Track, Playlist };
    std::string loopStatusToString(LoopStatus);

    struct DBusVariant {
        enum Type { String, StringArray, Int64, UInt64, Double, Boolean, Dictionary };
        Type type;
        DBusVariant(const std::string&) {}
        DBusVariant(bool) {}
        DBusVariant(int64_t) {}
        DBusVariant(uint64_t) {}
        DBusVariant(double) {}
        // ... templates
        template<typename T> T get() const { return T(); }
    };

    using DBusDictionary = std::map<std::string, DBusVariant>;

    template<typename T>
    struct Result {
        static Result<T> success(T) { return Result<T>(); }
        static Result<T> error(std::string) { return Result<T>(); }
        bool isSuccess() { return true; }
        std::string getError() { return ""; }
        T getValue() { return T(); }
    };
}
}

#include <dbus/dbus.h>

#endif
