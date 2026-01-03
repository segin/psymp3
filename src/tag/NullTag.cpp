/*
 * NullTag.cpp - Null object pattern implementation for Tag interface
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FINAL_BUILD
// Standalone build - include only what we need
#include "psymp3.h"

#include "tag/NullTag.h"
#endif // !FINAL_BUILD

// NullTag is fully implemented in the header as inline methods.
// This file exists to ensure the compilation unit is present for the build system
// and to provide a place for any future non-inline implementations.

namespace PsyMP3 {
namespace Tag {

// Explicit template instantiation or any future non-inline methods would go here.
// Currently, NullTag is entirely header-only, but having this .cpp file
// ensures proper build system integration and allows for future expansion.

} // namespace Tag
} // namespace PsyMP3

