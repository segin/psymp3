/*
 * FocusAnts.cpp - The Windows 3.1 focus rectangle ("marching ants")
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Widget {
namespace UI {

void drawFocusAnts(Surface& surface, int x1, int y1, int x2, int y2, SDL_Color under)
{
    const uint8_t ir = static_cast<uint8_t>(255 - under.r);
    const uint8_t ig = static_cast<uint8_t>(255 - under.g);
    const uint8_t ib = static_cast<uint8_t>(255 - under.b);
    auto ant = [&](int x, int y) {
        if ((x + y) % 2 == 0) {
            surface.pixel(x, y, ir, ig, ib, 255);
        } else {
            surface.pixel(x, y, 0, 0, 0, 255);
        }
    };
    for (int x = x1; x <= x2; ++x) {
        ant(x, y1);
        ant(x, y2);
    }
    for (int y = y1 + 1; y < y2; ++y) {
        ant(x1, y);
        ant(x2, y);
    }
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
