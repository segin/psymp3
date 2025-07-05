/*
 * ProgressBarBracketWidget.cpp - Frame bracket widgets for progress bar
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"

void ProgressBarLeftBracketWidget::draw(Surface& surface)
{
    // Fill with transparent background
    uint32_t transparent = surface.MapRGBA(0, 0, 0, 0);
    surface.FillRect(transparent);
    
    // Draw left bracket (white lines only)
    uint32_t white = surface.MapRGBA(255, 255, 255, 255);
    
    // Original coordinates translated to widget space:
    // vline(399,370,385) -> vline(0,0,15) - left vertical line
    // hline(399,402,370) -> hline(0,3,0) - left top horizontal
    // hline(399,402,385) -> hline(0,3,15) - left bottom horizontal
    
    surface.vline(0, 0, 15, white);       // Left vertical line
    surface.hline(0, 3, 0, white);       // Left top horizontal bracket
    surface.hline(0, 3, 15, white);      // Left bottom horizontal bracket
}

void ProgressBarRightBracketWidget::draw(Surface& surface)
{
    // Fill with transparent background
    uint32_t transparent = surface.MapRGBA(0, 0, 0, 0);
    surface.FillRect(transparent);
    
    // Draw right bracket (white lines only)
    uint32_t white = surface.MapRGBA(255, 255, 255, 255);
    
    // Original coordinates translated to widget space:
    // vline(621,370,385) -> vline(3,0,15) - right vertical line (621-618=3)
    // hline(618,621,370) -> hline(0,3,0) - right top horizontal  
    // hline(618,621,385) -> hline(0,3,15) - right bottom horizontal
    
    surface.vline(3, 0, 15, white);       // Right vertical line
    surface.hline(0, 3, 0, white);       // Right top horizontal bracket
    surface.hline(0, 3, 15, white);      // Right bottom horizontal bracket
}