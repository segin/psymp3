/*
 * PlaybackIndicatorsWidget.cpp - Repeat/Shuffle status indicators.
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

namespace {
// Bright when a mode is active, dim grey when it is off.
constexpr uint8_t kOnR = 0,  kOnG = 220, kOnB = 100;
constexpr uint8_t kOffR = 78, kOffG = 78, kOffB = 78;
}

PlaybackIndicatorsWidget::PlaybackIndicatorsWidget(int width, int height)
    : DrawableWidget(width, height)
{
}

void PlaybackIndicatorsWidget::setState(LoopMode loop, bool shuffle)
{
    if (loop == m_loop && shuffle == m_shuffle) {
        return;
    }
    m_loop = loop;
    m_shuffle = shuffle;
    invalidate();
}

// A rectangular loop with a right-pointing arrowhead breaking out of the
// top-right and a left-pointing one out of the bottom-left — reads as "repeat".
// With show_one, a small "1" is drawn inside for repeat-one.
void PlaybackIndicatorsWidget::drawRepeat(Surface& s, int xoff, uint8_t r, uint8_t g, uint8_t b, bool show_one)
{
    const uint32_t packed = s.MapRGB(r, g, b);
    auto L = [&](int x1, int y1, int x2, int y2) {
        s.line(static_cast<int16_t>(xoff + x1), static_cast<int16_t>(y1),
               static_cast<int16_t>(xoff + x2), static_cast<int16_t>(y2), r, g, b, 255);
    };
    auto HL = [&](int x1, int x2, int y) {
        s.hline(static_cast<int16_t>(xoff + x1), static_cast<int16_t>(xoff + x2),
                static_cast<int16_t>(y), packed);
    };
    auto VL = [&](int x, int y1, int y2) {
        s.vline(static_cast<int16_t>(xoff + x), static_cast<int16_t>(y1),
                static_cast<int16_t>(y2), packed);
    };

    // Loop body (rectangle 4,3 .. 12,10).
    HL(4, 12, 3);   // top
    HL(4, 12, 10);  // bottom
    VL(4, 3, 10);   // left
    VL(12, 3, 10);  // right
    // Top-right arrowhead, pointing right.
    L(10, 1, 12, 3);
    L(10, 5, 12, 3);
    // Bottom-left arrowhead, pointing left.
    L(6, 8, 4, 10);
    L(6, 12, 4, 10);

    if (show_one) {
        // A tiny "1" inside the loop.
        VL(8, 4, 9);   // stem
        L(7, 5, 8, 4); // top flag
        HL(7, 9, 9);   // base serif
    }
}

// Two arrows that cross once and both point right — reads as "shuffle".
void PlaybackIndicatorsWidget::drawShuffle(Surface& s, int xoff, uint8_t r, uint8_t g, uint8_t b)
{
    auto L = [&](int x1, int y1, int x2, int y2) {
        s.line(static_cast<int16_t>(xoff + x1), static_cast<int16_t>(y1),
               static_cast<int16_t>(xoff + x2), static_cast<int16_t>(y2), r, g, b, 255);
    };

    // Path A: top-left down through the centre to the lower-right.
    L(1, 2, 9, 7);
    L(9, 7, 14, 10);
    // Path B: bottom-left up through the centre to the upper-right.
    L(1, 10, 9, 5);
    L(9, 5, 14, 2);
    // Arrowhead on A (lower-right), pointing right.
    L(12, 8, 14, 10);
    L(12, 12, 14, 10);
    // Arrowhead on B (upper-right), pointing right.
    L(12, 0, 14, 2);
    L(12, 4, 14, 2);
}

void PlaybackIndicatorsWidget::draw(Surface& surface)
{
    surface.FillRect(surface.MapRGBA(0, 0, 0, 0)); // transparent over the HUD

    const bool repeat_on = m_loop != LoopMode::None;
    if (repeat_on) {
        drawRepeat(surface, kRepeatX, kOnR, kOnG, kOnB, m_loop == LoopMode::One);
    } else {
        drawRepeat(surface, kRepeatX, kOffR, kOffG, kOffB, false);
    }
    if (m_shuffle) {
        drawShuffle(surface, kShuffleX, kOnR, kOnG, kOnB);
    } else {
        drawShuffle(surface, kShuffleX, kOffR, kOffG, kOffB);
    }
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
