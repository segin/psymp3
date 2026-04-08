/*
 * truetype.cpp - FreeType2 TrueType font initialization
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

FT_Library PsyMP3::Core::TrueType::m_library;

void PsyMP3::Core::TrueType::Init() {
  Debug::log("font", "TrueType::Init() called.");
  FT_Error error = FT_Init_FreeType(&m_library);
  if (error) {
    Debug::log("font",
               "TrueType::Init() failed: FT_Init_FreeType returned error code ",
               error);
    throw std::runtime_error("Failed to initialize FreeType2");
  }
  Debug::log("font", "TrueType::Init() successful.");
}

class TrueTypeLifecycleManager {
public:
  TrueTypeLifecycleManager() { PsyMP3::Core::TrueType::Init(); }
  ~TrueTypeLifecycleManager() { PsyMP3::Core::TrueType::Done(); }
};

static TrueTypeLifecycleManager s_truetype_manager;
