/*
 * debug.cpp - Debug output system implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include <chrono>
#include <iomanip>
#include <iostream>

// Initialize debug flags (disabled by default)
bool Debug::widget_blitting_enabled = false;
bool Debug::runtime_debug_enabled = false;

// Definition of the runtime debug function
void Debug::runtime(const std::string& message)
{
    if (runtime_debug_enabled) {
        auto now = std::chrono::system_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
        auto timer = std::chrono::system_clock::to_time_t(now);
        std::tm bt = *std::localtime(&timer);

        std::cout << std::put_time(&bt, "%H:%M:%S") << '.' << std::dec << std::setfill('0') << std::setw(6) << us.count()
                  << ": " << message << std::endl;
    }
}
