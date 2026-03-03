#ifndef __PSYMP3_H__
#define __PSYMP3_H__

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <condition_variable>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

// Mock TagLib
#include <taglib/tstring.h>

// Mock Debug
#include "debug.h"

// Types
typedef off_t filesize_t;

enum {
    QUIT_APPLICATION = 100
};

enum class LoopMode {
    None,
    One,
    All
};

enum class PlayerState {
    Stopped,
    Playing,
    Paused
};

enum class FFTMode {
    Original
};

class LastFM {};

typedef uint32_t Uint32;
typedef uint16_t Uint16;
typedef int SDLKey;
typedef uint32_t SDL_TimerID;

struct SDL_keysym {
    int sym;
};

struct SDL_MouseButtonEvent {
    int button;
};

struct SDL_MouseMotionEvent {
    int x;
    int y;
};

struct SDL_UserEvent {
    int type;
};

union SDL_Event {
    uint32_t type;
    SDL_UserEvent user;
};

class Display {};
class Surface {};
class Playlist {
public:
    bool canGoNext() const { return true; }
    bool canGoPrevious() const { return true; }
};
class Font {};
class FastFourier {};
class Audio {};
class System {};
class Stream {};
class Widget {};
class Label {};
class SpectrumAnalyzerWidget {};
class PlayerProgressBarWidget {};
class LyricsWidget {};
class FadingWidget {};
class WindowFrameWidget {};

// Forward declarations
namespace PsyMP3 {
    namespace Core {
        class InvalidMediaException : public std::exception {
        public:
            InvalidMediaException(const std::string& msg) : m_msg(msg) {}
            const char* what() const noexcept override { return m_msg.c_str(); }
        private:
            std::string m_msg;
        };
    }
}
using PsyMP3::Core::InvalidMediaException;

// Mock Memory stuff
namespace PsyMP3 {
namespace IO {

class IOBufferPool {
public:
    typedef std::vector<uint8_t> Buffer;
    static IOBufferPool& getInstance() {
        static IOBufferPool instance;
        return instance;
    }
    Buffer acquire(size_t size) { return Buffer(size); }
    void setMaxPoolSize(size_t) {}
    void setMaxBuffersPerSize(size_t) {}
    void clear() {}
    void optimizeAllocationPatterns() {}
    void compactMemory() {}
    void defragmentPools() {}

    struct Stats { size_t current_pool_size = 0; size_t max_pool_size = 1; size_t total_pool_hits = 0; size_t total_pool_misses = 0; };
    std::map<std::string, size_t> getStats() {
        std::map<std::string, size_t> stats;
        stats["total_pool_hits"] = 0;
        stats["total_pool_misses"] = 0;
        stats["current_pool_size"] = 0;
        stats["max_pool_size"] = 1;
        return stats;
    }
};

class MemoryOptimizer {
public:
    enum class MemoryPressureLevel { Low, Medium, High, Critical };
    static MemoryOptimizer& getInstance() {
        static MemoryOptimizer instance;
        return instance;
    }
    void getRecommendedBufferPoolParams(size_t& size, size_t& count) { size = 1000; count = 1; }
    MemoryPressureLevel getMemoryPressureLevel() { return MemoryPressureLevel::Low; }
    bool shouldEnableReadAhead() { return true; }
    size_t getOptimalBufferSize(size_t current, const std::string&, bool) { return current; }
    size_t getRecommendedReadAheadSize(size_t current) { return current; }
    void registerAllocation(size_t, const std::string&) {}
    void registerDeallocation(size_t, const std::string&) {}
};

class MemoryPoolManager {
public:
    static MemoryPoolManager& getInstance() {
        static MemoryPoolManager instance;
        return instance;
    }
    void initializePools() {}
    std::map<std::string, size_t> getMemoryStats() { return {}; }
    void setMemoryLimits(size_t, size_t) {}
    bool isSafeToAllocate(size_t, const std::string&) { return true; }
    void optimizeMemoryUsage() {}
};

class MemoryTracker {
public:
    static MemoryTracker& getInstance() {
        static MemoryTracker instance;
        return instance;
    }
    struct Stats { size_t total_physical_memory = 0; size_t available_physical_memory = 0; size_t process_memory_usage = 0; size_t peak_memory_usage = 0; };
    Stats getStats() { return Stats(); }
    void requestMemoryCleanup(int) {}
};

}
}

// RAIIFileHandle mock
namespace PsyMP3 {
namespace IO {
class RAIIFileHandle {
public:
    RAIIFileHandle() : m_fp(nullptr), m_owned(false) {}
    ~RAIIFileHandle() { close(); }

    bool open(const char* path, const char* mode) {
        close();
        m_fp = fopen(path, mode);
        m_owned = true;
        return m_fp != nullptr;
    }

    bool open(const wchar_t* path, const wchar_t* mode) {
        return false; // Not supported in mock
    }

    int close() {
        int result = 0;
        if (m_fp && m_owned) {
            result = fclose(m_fp);
        }
        m_fp = nullptr;
        m_owned = false;
        return result;
    }

    FILE* get() const { return m_fp; }
    operator FILE*() const { return m_fp; }
    bool is_valid() const { return m_fp != nullptr; }
    explicit operator bool() const { return is_valid(); }

    void reset(FILE* fp, bool owned) {
        close();
        m_fp = fp;
        m_owned = owned;
    }

private:
    FILE* m_fp;
    bool m_owned;
};
}
}

// Include IOHandler header
#include "io/IOHandler.h"
#include "io/file/FileIOHandler.h"

#endif
