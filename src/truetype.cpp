/*
 * truetype.cpp - FreeType2 TrueType font initialization
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

FT_Library TrueType::m_library;

/**
 * @brief Initialises the global FreeType library instance.
 *
 * Must be called once before any `Font` objects are created. Throws
 * `std::runtime_error` if FreeType initialisation fails.
 */
void TrueType::Init() {
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

/**
 * @brief Shuts down the global FreeType library instance, releasing all resources.
 *
 * Should only be called after all `Font` objects have been destroyed.
 */
void TrueType::Done() { FT_Done_FreeType(m_library); }

/**
 * @brief RAII wrapper that manages the lifetime of the global FreeType library.
 *
 * Its constructor calls `TrueType::Init()` and its destructor calls
 * `TrueType::Done()`, ensuring the library is initialised before any use
 * and cleaned up on process exit.
 */
class TrueTypeLifecycleManager {
public:
  /** @brief Initialises FreeType by calling `TrueType::Init()`. */
  TrueTypeLifecycleManager() { TrueType::Init(); }
  /** @brief Shuts down FreeType by calling `TrueType::Done()`. */
  ~TrueTypeLifecycleManager() { TrueType::Done(); }
};

// The global static instance that manages the library's lifetime.
static TrueTypeLifecycleManager s_truetype_manager;