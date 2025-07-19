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

#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <vector>
#include <unordered_set>

class Debug {
public:
    static void init(const std::string& logfile, const std::vector<std::string>& channels);
    static void shutdown();

    template<typename... Args>
    static inline void log(const std::string& channel, Args&&... args) {
        if (isChannelEnabled(channel)) {
            std::stringstream ss;
            (ss << ... << args);
            write(channel, ss.str());
        }
    }

private:
    static bool isChannelEnabled(const std::string& channel);
    static void write(const std::string& channel, const std::string& message);

    static std::ofstream m_logfile;
    static std::mutex m_mutex;
    static std::unordered_set<std::string> m_enabled_channels;
    static bool m_log_to_file;
};

#endif // DEBUG_H