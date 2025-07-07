/*
 * debug.cpp - Debug output system implementation
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

// Initialize debug flags (disabled by default)
bool Debug::widget_blitting_enabled = false;
bool Debug::runtime_debug_enabled = false;