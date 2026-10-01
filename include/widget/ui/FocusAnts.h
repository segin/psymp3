/*
 * FocusAnts.h - The Windows 3.1 focus rectangle ("marching ants")
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef FOCUSANTS_H
#define FOCUSANTS_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

// Draw the focus rectangle on the edge of (x1, y1)-(x2, y2), corners
// included, over a fill of colour `under`: pixels where x + y is even are
// that colour inverted (navy becomes yellow, white becomes black), the others
// black. The combo box and the list view share it, so their ants match.
void drawFocusAnts(Surface& surface, int x1, int y1, int x2, int y2, SDL_Color under);

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // FOCUSANTS_H
