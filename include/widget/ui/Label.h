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

        // Make this label a mnemonic for `target` (the control it names), the
        // Windows way: an '&' in the text marks the next character, which is
        // drawn underlined, and Alt+that key in the label's window focuses
        // `target`; "&&" is a literal '&'. Only a label with a target reads
        // '&' this way, so other labels show it as it is. nullptr undoes it.
        void setMnemonicTarget(Widget* target);
        Widget* mnemonicTarget() const { return m_mnemonic_target; }
        // The mnemonic key, lowercased (an SDL keycode for printable ASCII),
        // or 0 when there is none.
        int mnemonicKey() const;

        // Enable multi-line reflow: the label word-wraps its text to `wrap_width`
        // pixels and grows its height to fit the wrapped lines (instead of the
        // default single-line render). Re-applies to the current text; call
        // again with a new width (e.g. on container resize) to re-flow.
        void setReflow(bool enabled, int wrap_width);

        // Greedy word-wrap: split `text` into lines no wider than `max_width` px
        // when rendered with `font`. Whitespace-delimited; a single word wider
        // than max_width is placed on its own (overflowing) line rather than
        // split mid-word. Returns at least one (possibly empty) line.
        static std::vector<std::string> wrapText(Font* font, const std::string& text, int max_width);
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
        TagLib::String m_text;      // as displayed (mnemonic marker removed)
        TagLib::String m_raw_text;  // as given to setText
        Widget* m_mnemonic_target = nullptr;
        size_t m_mnemonic_index = std::string::npos; // byte offset in m_text's UTF-8
        void underlineMnemonic();   // onto m_text_surface, after each render
        SDL_Color m_color;
        SDL_Color m_background_color;
        Align m_align{Align::Left};
        std::unique_ptr<Surface> m_text_surface;
        int m_last_drawn_width{0};
        int m_last_drawn_height{0};
        bool m_marquee_enabled{false};
        /// Constructed with a zero-sized rect: adopt the rendered text's size
        /// on every setText so centering math based on getPos() works.
        bool m_auto_size{false};
        bool m_reflow{false};
        int m_reflow_width{0};
        void renderReflowed(); // multi-line wrapped render into the widget surface
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
