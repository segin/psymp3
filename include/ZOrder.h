/*
 * ZOrder.h - Z-order constants for layered UI system
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

#ifndef ZORDER_H
#define ZORDER_H

/**
 * @brief Z-order constants for proper UI layering.
 * 
 * This namespace defines the standard Z-order levels used throughout
 * the application to ensure consistent layering behavior.
 */
namespace ZOrder {
    /// Desktop elements: FFT spectrum, main UI, progress bars, labels
    constexpr int DESKTOP = 0;
    
    /// UI overlays: LyricsWidget, seek indicators (just above desktop)
    constexpr int UI = 1;
    
    /// Low priority windows: Tool palettes, secondary panels
    constexpr int LOW = 10;
    
    /// Normal floating windows: Settings, file browser, etc.
    constexpr int NORMAL = 50;
    
    /// High priority windows: Modal dialogs, error messages
    constexpr int HIGH = 100;
    
    /// Maximum priority: ToastWidget (always on top, mouse-transparent)
    constexpr int MAX = 999;
}

#endif // ZORDER_H