/*
 * sheenbidi_impl.c - compiles the vendored SheenBidi as one translation unit
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * SheenBidi itself is Apache-2.0; see third_party/sheenbidi/LICENSE. It is
 * built through upstream's own unity mode, which is why this wrapper exists at
 * all: the include paths it needs stay confined to one object instead of
 * leaking into every translation unit, the same treatment stb_vorbis and the
 * MLP decoder get.
 */

#ifndef SB_CONFIG_UNITY
#define SB_CONFIG_UNITY
#endif
#include "../../third_party/sheenbidi/Source/SheenBidi.c"
