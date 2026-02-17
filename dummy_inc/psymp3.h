#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <variant>
#include <functional>
#include <cstdint>
#include <dbus/dbus.h>
#define HAVE_DBUS 1
namespace PsyMP3 {
  namespace MPRIS {
    struct DBusVariant {
      enum Type { String, StringArray, Int64, UInt64, Double, Boolean, Dictionary };
      Type type;
      template<typename T> T get() const { return T(); }
      DBusVariant(const std::string&) : type(String) {}
      DBusVariant(const std::vector<std::string>&) : type(StringArray) {}
      DBusVariant(int64_t) : type(Int64) {}
      DBusVariant(uint64_t) : type(UInt64) {}
      DBusVariant(double) : type(Double) {}
      DBusVariant(bool) : type(Boolean) {}
    };
    using DBusDictionary = std::map<std::string, DBusVariant>;
    template<typename T> struct Result {
      static Result success(const T&) { return Result(); }
      static Result error(const std::string&) { return Result(); }
      bool isSuccess() { return true; }
      T getValue() { return T(); }
      std::string getError() { return ""; }
    };
    std::string loopStatusToString(int) { return ""; }
  }
  enum LoopMode { None, One, All };
}
class Player { public: void seekTo(unsigned long) {} void nextTrack() {} void prevTrack() {} bool play() { return true; } bool pause() { return true; } bool stop() { return true; } bool playPause() { return true; } void setVolume(double) {} double getVolume() { return 1.0; } void setLoopMode(PsyMP3::LoopMode) {} static void synthesizeUserEvent(int, void*, void*) {} };
class PropertyManager { public:
  uint64_t getPosition() { return 0; }
  uint64_t getLength() { return 0; }
  bool canGoNext() { return true; }
  bool canGoPrevious() { return true; }
  bool canSeek() { return true; }
  bool canControl() { return true; }
  std::string getPlaybackStatus() { return ""; }
  std::map<std::string, PsyMP3::MPRIS::DBusVariant> getMetadata() { return {}; }
  PsyMP3::LoopMode getLoopStatus() { return PsyMP3::LoopMode::None; }
  std::map<std::string, PsyMP3::MPRIS::DBusVariant> getAllProperties() { return {}; }
};
#define MPRIS_MEDIAPLAYER2_INTERFACE "org.mpris.MediaPlayer2"
#define MPRIS_PLAYER_INTERFACE "org.mpris.MediaPlayer2.Player"
#define DBUS_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"
#define QUIT_APPLICATION 1
#define MAX_SEEK_OFFSET_US 1000000
#define MAX_POSITION_US 1000000
#include "mpris/MethodHandler.h"
