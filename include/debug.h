/*
 * debug.h - Debug output system header
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef DEBUG_H
#define DEBUG_H

// No direct #include <mutex>
#include <iostream>
#include <unordered_set>
#include <sstream>
// should be in psymp3.h

class Debug {
public:
    static void init(const std::string& logfile, const std::vector<std::string>& channels);
    static void shutdown();

    // Check if a debug channel is enabled (O(1) lookup)
    static bool isChannelEnabled(const std::string& channel);

    // Basic logging without location info
    template<typename... Args>
    static inline void log(const std::string& channel, Args&&... args) {
        if (isChannelEnabled(channel)) {
            std::stringstream ss;
            if constexpr (sizeof...(args) > 0) {
                (ss << ... << args);
            }
            write(channel, "", 0, ss.str());
        }
    }

    // Logging with function and line number
    template<typename... Args>
    static inline void log(const std::string& channel, const char* function, int line, Args&&... args) {
        if (isChannelEnabled(channel)) {
            std::stringstream ss;
            if constexpr (sizeof...(args) > 0) {
                (ss << ... << args);
            }
            write(channel, function, line, ss.str());
        }
    }

private:
    static void write(const std::string& channel, const std::string& function, int line, const std::string& message);

    static std::ofstream m_logfile;
    static std::mutex m_mutex;
    static std::unordered_set<std::string> m_enabled_channels;
    static bool m_log_to_file;
};

// Convenience macro for logging with location info
#define DEBUG_LOG(channel, ...) Debug::log(channel, __FUNCTION__, __LINE__, __VA_ARGS__)

// Lazy evaluation macro - checks if channel is enabled before evaluating arguments
// This prevents string formatting overhead when logging is disabled (Requirements 3.1, 3.3)
#define DEBUG_LOG_LAZY(channel, ...) \
    do { \
        if (Debug::isChannelEnabled(channel)) { \
            Debug::log(channel, __VA_ARGS__); \
        } \
    } while(0)

#endif // DEBUG_H