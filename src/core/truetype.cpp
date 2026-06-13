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

  // Enable the default 3-tap LCD filter so subpixel-rendered glyphs come out
  // without rainbow fringing. Required for Font::RenderLCD to produce
  // ClearType-quality output.
  if (FT_Library_SetLcdFilter(m_library, FT_LCD_FILTER_DEFAULT)) {
    Debug::log("font", "TrueType::Init() warning: LCD filter unavailable; "
                       "subpixel text may show color fringing");
  }

  Debug::log("font", "TrueType::Init() successful.");
}

namespace {
class TrueTypeLifecycleManager {
public:
  TrueTypeLifecycleManager() { PsyMP3::Core::TrueType::Init(); }
  ~TrueTypeLifecycleManager() { PsyMP3::Core::TrueType::Done(); }
};
} // namespace

FT_Library PsyMP3::Core::TrueType::getLibrary() {
  // Construct-on-first-use: FreeType is initialized the first time a font is
  // needed (at runtime), not during static initialization. This removes the
  // cross-TU static-order dependency (Init() logs via Debug, whose state lives
  // in another TU) and makes an FT_Init_FreeType failure throw from a normal
  // call site a caller can catch, instead of escaping a static initializer
  // uncatchably into std::terminate. FT_Done_FreeType runs at program exit.
  static TrueTypeLifecycleManager manager;
  (void)manager;
  return m_library;
}
