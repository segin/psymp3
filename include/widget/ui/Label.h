/*
 * Label.h - A text label widget.
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

#ifndef LABEL_H
#define LABEL_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

class Label : public Widget
{
    public:
        // Horizontal justification of the text within the label's width.
        enum class Align { Left, Center, Right };

        Label(Font* font, const Rect& position, const TagLib::String& initial_text = "",
              SDL_Color color = {255, 255, 255, 255}, SDL_Color background_color = {0, 0, 0, 255});
        virtual ~Label() = default;

        void setText(const TagLib::String& text);
        void setBackgroundColor(SDL_Color background_color);
        void setAlignment(Align align);
        void setMarqueeEnabled(bool enabled);
        void BlitTo(Surface& target) override;
        void recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos) override;

    private:
        void blitWithBackgroundClear(Surface& target, const Rect& absolute_pos);
        std::unique_ptr<Surface> createViewportSurface(int viewport_width, int viewport_height) const;
        int calculateMarqueeOffset(uint32_t tick_ms) const;
        bool isInMarqueeHomePause(uint32_t tick_ms) const;
        float calculateLeftEdgeFadeStrength(uint32_t tick_ms) const;
        void applyEdgeFade(Surface& surface, float left_fade_strength) const;

        Font* m_font; // Non-owning pointer to the global font
        TagLib::String m_text;
        SDL_Color m_color;
        SDL_Color m_background_color;
        Align m_align{Align::Left};
        std::unique_ptr<Surface> m_text_surface;
        int m_last_drawn_width{0};
        int m_last_drawn_height{0};
        bool m_marquee_enabled{false};
        static constexpr int kEdgeFadeWidth = 14;
        static constexpr int kMarqueeGapPixels = 48;
        static constexpr int kMarqueePauseMs = 2000;
        static constexpr int kMarqueePixelsPerSecond = 36;
        static constexpr int kGradientTransitionMs = 220;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // LABEL_H
