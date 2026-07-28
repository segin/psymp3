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
// White when a mode is active, dim grey when it is off.
constexpr uint8_t kOnR = 255, kOnG = 255, kOnB = 255;
constexpr uint8_t kOffR = 78, kOffG = 78, kOffB = 78;

// Pixel-exact glyphs traced from the reference art. One row per entry, MSB is
// the leftmost column. Repeat is 16 wide x 14 tall; shuffle 17 wide x 11 tall.
// Base "repeat" loop, WITHOUT the little "1" — this is the repeat-all glyph.
// The reference art carries a "1" (the 1-2-1-1-wide stroke at col 9 above the
// arrowhead); those pixels live in kRepeatOneBits and are only OR'd in for
// repeat-one mode so the two states are distinguishable.
constexpr int kRepeatW = 16, kRepeatH = 14;
constexpr uint32_t kRepeatBits[kRepeatH] = {
    0,     // ................
    0,     // ................
    1024,  // .....#..........
    1536,  // .....##.........
    16156, // ..######...###..
    32670, // .########..####.
    65311, // ########...#####
    58887, // ###..##......###
    58375, // ###..#.......###
    57351, // ###..........###
    57351, // ###..........###
    65535, // ################
    32766, // .##############.
    16380, // ..############..
};

// The "1" carved into the loop's top (cols 8-9, rows 0-3) — added for repeat-one.
constexpr int kRepeatOneH = 4;
constexpr uint32_t kRepeatOneBits[kRepeatOneH] = {
    64,  // .........#......
    192, // ........##......
    64,  // .........#......
    64,  // .........#......
};

constexpr int kShuffleW = 17, kShuffleH = 15;
constexpr uint32_t kShuffleBits[kShuffleH] = {
    8,      // .............#...
    12,     // .............##..
    122942, // ####.......#####.
    127103, // #####.....#######
    129278, // ######...#######.
    7628,   // ....###.###..##..
    3976,   // .....#####...#...
    1792,   // ......###........
    3976,   // .....#####...#...
    7628,   // ....###.###..##..
    129278, // ######...#######.
    127103, // #####.....#######
    122942, // ####.......#####.
    12,     // .............##..
    8,      // .............#...
};

// Vertical offsets inside the widget, matching the reference: the taller
// shuffle glyph rides 1px lower than the repeat loop.
constexpr int kRepeatY = 0;
constexpr int kShuffleY = 1;
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

void PlaybackIndicatorsWidget::setCallbacks(std::function<void()> on_repeat,
                                            std::function<void()> on_shuffle)
{
    m_on_repeat_click = std::move(on_repeat);
    m_on_shuffle_click = std::move(on_shuffle);
}

bool PlaybackIndicatorsWidget::handleMouseDown(const SDL_MouseButtonEvent& event,
                                               int relative_x, int /*relative_y*/)
{
    if (event.button != SDL_BUTTON_LEFT) {
        return false;
    }
    // Split the strip: left half is the repeat glyph, right half the shuffle
    // glyph. kShuffleX is where the shuffle glyph begins.
    if (relative_x < kShuffleX) {
        if (m_on_repeat_click) {
            m_on_repeat_click();
        }
    } else {
        if (m_on_shuffle_click) {
            m_on_shuffle_click();
        }
    }
    return true;
}

// Blit a packed 1bpp glyph (MSB = leftmost column) at (xoff, yoff).
static void blitGlyph(Surface& s, const uint32_t* bits, int w, int h,
                      int xoff, int yoff, uint32_t packed)
{
    for (int row = 0; row < h; ++row) {
        const uint32_t v = bits[row];
        for (int col = 0; col < w; ++col) {
            if (v & (1u << (w - 1 - col))) {
                s.pixel(static_cast<int16_t>(xoff + col),
                        static_cast<int16_t>(yoff + row), packed);
            }
        }
    }
}

// The reference "repeat" loop glyph. With show_one, the "1" carved into the top
// of the loop (from the reference art) is added back for repeat-one mode.
void PlaybackIndicatorsWidget::drawRepeat(Surface& s, int xoff, uint8_t r, uint8_t g, uint8_t b, bool show_one)
{
    const uint32_t packed = s.MapRGB(r, g, b);
    blitGlyph(s, kRepeatBits, kRepeatW, kRepeatH, xoff, kRepeatY, packed);
    if (show_one) {
        blitGlyph(s, kRepeatOneBits, kRepeatW, kRepeatOneH, xoff, kRepeatY, packed);
    }
}

// The reference "shuffle" crossed-arrows glyph.
void PlaybackIndicatorsWidget::drawShuffle(Surface& s, int xoff, uint8_t r, uint8_t g, uint8_t b)
{
    const uint32_t packed = s.MapRGB(r, g, b);
    blitGlyph(s, kShuffleBits, kShuffleW, kShuffleH, xoff, kShuffleY, packed);
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
