/*
 * Label.cpp - A text label widget implementation.
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
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

namespace PsyMP3 {
namespace Widget {
namespace UI {

std::vector<std::string> Label::wrapText(Font* font, const std::string& text, int max_width)
{
    std::vector<std::string> lines;
    if (!font || max_width <= 0) {
        lines.push_back(text);
        return lines;
    }
    // Whitespace is structural in preformatted text, so it is laid out rather
    // than collapsed. Leading indentation is preserved and reused as the hanging
    // indent for any continuation lines, and runs of spaces between words are
    // kept verbatim. Without this the About text's third-party licenses lose
    // their shape entirely: nested license clauses flatten against the left
    // margin, and the contents listing collapses from a two-column table to
    // "1. PsyMP3 ISC License".
    //
    // Widths accumulate rather than re-measuring the whole candidate line per
    // word, which was quadratic in a line's characters. The running total is
    // exact, not an approximation: measureWidth sums glyph advances and applies
    // no kerning, so the width of a concatenation is the sum of the widths.
    static constexpr char kWhitespace[] = " \t\n\r\v\f";

    // A run is reproduced verbatim only when it is all spaces; a tab has no
    // defined width here, so a run containing one collapses to a single space
    // (which is what the previous whitespace-collapsing implementation did).
    const auto layoutRun = [](const std::string& run) {
        return run.find_first_not_of(' ') == std::string::npos ? run : std::string(" ");
    };

    const std::size_t indent_end = text.find_first_not_of(kWhitespace);
    if (indent_end == std::string::npos) {
        lines.push_back(""); // empty or whitespace-only: nothing to lay out
        return lines;
    }
    const std::string indent = layoutRun(text.substr(0, indent_end));
    const int indent_width = font->measureWidth(indent);

    std::string current = indent;
    int current_width = indent_width;
    bool have_word = false;
    // Whitespace seen since the last word, held back so that a run landing on a
    // wrap point becomes the line break instead of trailing space.
    std::string pending_space;
    int pending_space_width = 0;

    std::size_t pos = indent_end;
    while (pos < text.size()) {
        const std::size_t run_end = text.find_first_not_of(kWhitespace, pos);
        if (run_end != pos) {
            if (run_end == std::string::npos) {
                break; // trailing whitespace, dropped
            }
            pending_space = layoutRun(text.substr(pos, run_end - pos));
            pending_space_width = font->measureWidth(pending_space);
            pos = run_end;
            continue;
        }

        std::size_t word_end = text.find_first_of(kWhitespace, pos);
        if (word_end == std::string::npos) {
            word_end = text.size();
        }
        // Cheap advance-only measure (no rasterization). The UTF-8 overload is
        // deliberate: `text` is already UTF-8, and going via TagLib::String
        // would convert it to UTF-16 and straight back, per word.
        const std::string word = text.substr(pos, word_end - pos);
        const int word_width = font->measureWidth(word);
        pos = word_end;

        if (!have_word) {
            // First word on the line is kept whether or not it fits on its own.
            current += word;
            current_width += word_width;
            have_word = true;
        } else if (current_width + pending_space_width + word_width > max_width) {
            lines.push_back(current); // flush the line before this word overflows
            current = indent + word;
            current_width = indent_width + word_width;
        } else {
            current += pending_space;
            current += word;
            current_width += pending_space_width + word_width;
        }
        pending_space.clear();
        pending_space_width = 0;
    }

    lines.push_back(have_word ? current : std::string());
    return lines;
}

Label::Label(Font* font, const Rect& position, const TagLib::String& initial_text, SDL_Color color, SDL_Color background_color)
    : Widget(Surface(), position), // Initialize base Widget with an empty surface and a position
      m_font(font),
      m_text(), // Will be set by setText
      m_color(color),
      m_background_color(background_color)
{
    // A zero-sized construction rect means "size to the text": without this,
    // getPos() reported 0x0 forever and centering math placed the label's
    // top-left at the intended midpoint (the PAUSED overlay did exactly that).
    m_auto_size = (position.width() == 0 && position.height() == 0);

    // The initial render is done by calling setText.
    setText(initial_text);
}

void Label::setBackgroundColor(SDL_Color background_color)
{
    if (background_color.r == m_background_color.r
        && background_color.g == m_background_color.g
        && background_color.b == m_background_color.b
        && background_color.a == m_background_color.a) {
        return;
    }
    m_background_color = background_color;

    // The LCD render path bakes the bg colour into each glyph's edge pixels,
    // so a bg change requires re-rendering the text — invalidate alone leaves
    // glyphs blended against the previous bg and shows colour fringing.
    if (!m_text.isEmpty() && m_font) {
        m_text_surface = m_font->RenderLCD(m_text,
                                           m_color.r, m_color.g, m_color.b,
                                           m_background_color.r,
                                           m_background_color.g,
                                           m_background_color.b);
        if (m_text_surface) {
            auto widget_surface = std::make_unique<Surface>(m_text_surface->width(),
                                                            m_text_surface->height(),
                                                            true);
            widget_surface->FillRect(widget_surface->MapRGBA(0, 0, 0, 0));
            widget_surface->Blit(*m_text_surface,
                                 Rect(0, 0, m_text_surface->width(), m_text_surface->height()));
            setSurface(std::move(widget_surface));
        }
    }
    invalidate();
}

void Label::setAlignment(Align align)
{
    if (m_align == align) {
        return;
    }
    m_align = align;
    invalidate();
}

void Label::setMarqueeEnabled(bool enabled)
{
    if (m_marquee_enabled == enabled) {
        return;
    }

    m_marquee_enabled = enabled;
    invalidate();
}

void Label::setText(const TagLib::String& text)
{
    // Avoid re-rendering if the text hasn't changed.
    if (text == m_text) {
        return;
    }

    m_text = text;

    // A Label may be constructed with a null font; guard the render paths that
    // dereference it rather than crash.
    if (!m_font) {
        return;
    }

    // Empty text means "show nothing": drop the text surface entirely, so the
    // blit path clears the old glyphs once and then stops painting. (RenderLCD
    // of "" yields a valid 1x1 surface, which used to keep the full opaque
    // background box painted every frame.)
    if (m_text.isEmpty()) {
        m_text_surface.reset();
        setSurface(nullptr);
        invalidate();
        return;
    }

    // Multi-line reflow path: word-wrap to the configured width and stack lines.
    if (m_reflow && m_reflow_width > 0) {
        renderReflowed();
        invalidate();
        return;
    }

    // Use the ClearType (LCD subpixel) path so the rendered glyphs are
    // pre-blended against this label's background colour, avoiding the
    // single-alpha quality loss of compositing the text surface afterwards.
    m_text_surface = m_font->RenderLCD(m_text,
                                       m_color.r, m_color.g, m_color.b,
                                       m_background_color.r,
                                       m_background_color.g,
                                       m_background_color.b);
    if (!m_text_surface) {
        std::cerr << "Failed to render text surface for label." << std::endl;
        return;
    }

    auto widget_surface = std::make_unique<Surface>(m_text_surface->width(), m_text_surface->height(), true);
    widget_surface->FillRect(widget_surface->MapRGBA(0, 0, 0, 0));
    widget_surface->Blit(*m_text_surface, Rect(0, 0, m_text_surface->width(), m_text_surface->height()));
    setSurface(std::move(widget_surface));

    if (m_auto_size) {
        Rect pos = getPos();
        setPos(Rect(pos.x(), pos.y(),
                    m_text_surface->width(), m_text_surface->height()));
    }

    // Notify parent that this widget needs repainting
    invalidate();
}

void Label::setReflow(bool enabled, int wrap_width)
{
    m_reflow = enabled;
    m_reflow_width = wrap_width;
    if (!m_font) {
        return;
    }
    // Re-render the current text with the new wrap settings.
    if (m_reflow && m_reflow_width > 0) {
        renderReflowed();
    } else if (!m_text.isEmpty()) {
        m_text_surface = m_font->RenderLCD(m_text, m_color.r, m_color.g, m_color.b,
                                           m_background_color.r, m_background_color.g,
                                           m_background_color.b);
        if (m_text_surface) {
            auto ws = std::make_unique<Surface>(m_text_surface->width(), m_text_surface->height(), true);
            ws->FillRect(ws->MapRGBA(0, 0, 0, 0));
            ws->Blit(*m_text_surface, Rect(0, 0, m_text_surface->width(), m_text_surface->height()));
            setSurface(std::move(ws));
        }
    }
    invalidate();
}

void Label::renderReflowed()
{
    // Word-wrap the (UTF-8) text to m_reflow_width and stack the rendered lines
    // into a single tall surface, growing the label's height to fit.
    const std::vector<std::string> lines = wrapText(m_font, m_text.to8Bit(true), m_reflow_width);

    std::vector<std::unique_ptr<Surface>> rendered;
    rendered.reserve(lines.size());
    int max_w = 0;
    int line_h = 0;
    for (const std::string& ln : lines) {
        if (ln.empty()) {
            rendered.push_back(nullptr); // blank line: preserve spacing
            continue;
        }
        auto s = m_font->RenderLCD(TagLib::String(ln, TagLib::String::UTF8),
                                   m_color.r, m_color.g, m_color.b,
                                   m_background_color.r, m_background_color.g, m_background_color.b);
        if (s && s->isValid()) {
            max_w = std::max(max_w, static_cast<int>(s->width()));
            line_h = std::max(line_h, static_cast<int>(s->height()));
        }
        rendered.push_back(std::move(s));
    }
    if (line_h <= 0) {
        line_h = 14;
    }
    const int surf_w = std::max(max_w, m_reflow_width);
    const int surf_h = std::max(line_h, static_cast<int>(rendered.size()) * line_h);

    auto ws = std::make_unique<Surface>(surf_w, surf_h, true);
    ws->FillRect(ws->MapRGBA(0, 0, 0, 0));
    int y = 0;
    for (const auto& s : rendered) {
        if (s && s->isValid()) {
            ws->Blit(*s, Rect(0, y, s->width(), s->height()));
        }
        y += line_h;
    }
    setSurface(std::move(ws));

    Rect p = getPos();
    setPos(Rect(p.x(), p.y(), surf_w, surf_h));
}

void Label::BlitTo(Surface& target)
{
    if (!m_visible) {
        return;
    }
    blitWithBackgroundClear(target, m_pos);

    // Blit children (if any)
    for (const auto& child : m_children) {
        child->recursiveBlitTo(target, m_pos);
    }
}

void Label::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    if (!m_visible) {
        return;
    }
    Rect absolute_pos(parent_absolute_pos.x() + m_pos.x(),
                      parent_absolute_pos.y() + m_pos.y(),
                      m_pos.width(), m_pos.height());

    blitWithBackgroundClear(target, absolute_pos);

    // Blit children (if any)
    for (const auto& child : m_children) {
        child->recursiveBlitTo(target, absolute_pos);
    }
}

void Label::blitWithBackgroundClear(Surface& target, const Rect& absolute_pos)
{
    if (!m_text_surface || !m_text_surface->isValid()) {
        // Text was cleared: erase the previous glyphs from the persistent
        // target once, then paint nothing at all.
        if (m_last_drawn_width > 0 || m_last_drawn_height > 0) {
            target.box(absolute_pos.x(), absolute_pos.y(),
                       absolute_pos.x() + m_last_drawn_width - 1,
                       absolute_pos.y() + m_last_drawn_height - 1,
                       target.MapRGB(m_background_color.r, m_background_color.g,
                                     m_background_color.b));
            m_last_drawn_width = 0;
            m_last_drawn_height = 0;
        }
        return;
    }

    const int viewport_width = absolute_pos.width() > 0 ? absolute_pos.width() : m_text_surface->width();
    const int viewport_height = absolute_pos.height() > 0 ? absolute_pos.height() : m_text_surface->height();
    const bool should_marquee = m_marquee_enabled &&
                                viewport_width > 0 &&
                                m_text_surface->width() > viewport_width;

    // Labels render onto a persistent graph surface, so the old glyph bounds
    // must be cleared before alpha-blending the newly rendered text.
    int clear_w = std::max(viewport_width, m_last_drawn_width);
    int clear_h = std::max(viewport_height, m_last_drawn_height);

    target.box(absolute_pos.x(), absolute_pos.y(),
               absolute_pos.x() + clear_w - 1,
               absolute_pos.y() + clear_h - 1,
               target.MapRGB(m_background_color.r, m_background_color.g, m_background_color.b));

    if (should_marquee) {
        auto viewport_surface = createViewportSurface(viewport_width, viewport_height);
        if (viewport_surface && viewport_surface->isValid()) {
            target.Blit(*viewport_surface,
                        Rect(absolute_pos.x(), absolute_pos.y(), viewport_width, viewport_height));
        }
    } else {
        // Justify the text horizontally within the label's width. A text
        // surface wider than the viewport falls back to left alignment so it
        // never renders to the left of the label's origin.
        int off_x = 0;
        const int slack = viewport_width - m_text_surface->width();
        if (slack > 0) {
            if (m_align == Align::Center) off_x = slack / 2;
            else if (m_align == Align::Right) off_x = slack;
        }
        // Clip to the label's own viewport for the blit: Surface::Blit
        // ignores the destination rect's size, so a text surface wider than
        // the label used to bleed into the neighboring widgets' areas.
        SDL_Surface* handle = target.getHandle();
        SDL_Rect saved_clip{};
        if (handle) {
            SDL_GetSurfaceClipRect(handle, &saved_clip);
            SDL_Rect label_clip{absolute_pos.x(), absolute_pos.y(),
                                viewport_width, viewport_height};
            SDL_Rect merged{};
            SDL_GetRectIntersection(&saved_clip, &label_clip, &merged);
            SDL_SetSurfaceClipRect(handle, &merged);
        }
        target.Blit(*m_text_surface,
                    Rect(absolute_pos.x() + off_x, absolute_pos.y(),
                         m_text_surface->width(), m_text_surface->height()));
        if (handle) {
            SDL_SetSurfaceClipRect(handle, &saved_clip);
        }
    }

    m_last_drawn_width = viewport_width;
    m_last_drawn_height = viewport_height;
}

std::unique_ptr<Surface> Label::createViewportSurface(int viewport_width, int viewport_height) const
{
    auto viewport_surface = std::make_unique<Surface>(viewport_width, viewport_height, true);
    viewport_surface->FillRect(viewport_surface->MapRGBA(0, 0, 0, 0));

    const uint32_t tick_ms = SDL_GetTicks();
    const int marquee_offset = calculateMarqueeOffset(tick_ms);
    const int text_y = std::max(0, (viewport_height - m_text_surface->height()) / 2);

    viewport_surface->Blit(*m_text_surface,
                           Rect(-marquee_offset, text_y,
                                m_text_surface->width(), m_text_surface->height()));
    viewport_surface->Blit(*m_text_surface,
                           Rect(m_text_surface->width() + kMarqueeGapPixels - marquee_offset,
                                text_y,
                                m_text_surface->width(), m_text_surface->height()));
    applyEdgeFade(*viewport_surface, calculateLeftEdgeFadeStrength(tick_ms));
    return viewport_surface;
}

int Label::calculateMarqueeOffset(uint32_t tick_ms) const
{
    if (!m_text_surface || !m_text_surface->isValid()) {
        return 0;
    }

    const int loop_width = m_text_surface->width() + kMarqueeGapPixels;
    if (loop_width <= 0) {
        return 0;
    }

    const int scroll_duration_ms = std::max((loop_width * 1000) / kMarqueePixelsPerSecond, 1);
    const int cycle_ms = kMarqueePauseMs + scroll_duration_ms;
    const int phase = static_cast<int>(tick_ms % static_cast<uint32_t>(cycle_ms));
    if (phase < kMarqueePauseMs) {
        return 0;
    }

    const int scroll_phase_ms = phase - kMarqueePauseMs;
    return std::min((scroll_phase_ms * kMarqueePixelsPerSecond) / 1000, loop_width - 1);
}

bool Label::isInMarqueeHomePause(uint32_t tick_ms) const
{
    if (!m_text_surface || !m_text_surface->isValid()) {
        return false;
    }

    const int loop_width = m_text_surface->width() + kMarqueeGapPixels;
    if (loop_width <= 0) {
        return false;
    }

    const int scroll_duration_ms = std::max((loop_width * 1000) / kMarqueePixelsPerSecond, 1);
    const int cycle_ms = kMarqueePauseMs + scroll_duration_ms;
    const int phase = static_cast<int>(tick_ms % static_cast<uint32_t>(cycle_ms));
    return phase < kMarqueePauseMs;
}

float Label::calculateLeftEdgeFadeStrength(uint32_t tick_ms) const
{
    if (!m_text_surface || !m_text_surface->isValid()) {
        return 0.0f;
    }

    const int loop_width = m_text_surface->width() + kMarqueeGapPixels;
    if (loop_width <= 0) {
        return 0.0f;
    }

    const int scroll_duration_ms = std::max((loop_width * 1000) / kMarqueePixelsPerSecond, 1);
    const int cycle_ms = kMarqueePauseMs + scroll_duration_ms;
    const int phase = static_cast<int>(tick_ms % static_cast<uint32_t>(cycle_ms));

    if (phase < kMarqueePauseMs) {
        if (phase >= kMarqueePauseMs - kGradientTransitionMs) {
            const int transition_phase = phase - (kMarqueePauseMs - kGradientTransitionMs);
            return std::clamp(static_cast<float>(transition_phase) /
                                  static_cast<float>(kGradientTransitionMs),
                              0.0f, 1.0f);
        }
        return 0.0f;
    }

    const int scroll_phase = phase - kMarqueePauseMs;
    if (scroll_phase >= scroll_duration_ms - kGradientTransitionMs) {
        const int transition_phase = scroll_phase - (scroll_duration_ms - kGradientTransitionMs);
        return std::clamp(1.0f - (static_cast<float>(transition_phase) /
                                      static_cast<float>(kGradientTransitionMs)),
                          0.0f, 1.0f);
    }

    return 1.0f;
}

void Label::applyEdgeFade(Surface& surface, float left_fade_strength) const
{
    SDL_Surface* handle = surface.getHandle();
    if (!handle || !handle->pixels) {
        return;
    }

    const int fade_width = std::min(kEdgeFadeWidth, surface.width() / 2);
    if (fade_width <= 0) {
        return;
    }

    const bool must_lock = SDL_MUSTLOCK(handle);
    // SDL3: SDL_LockSurface returns bool (true on success); bail on failure.
    if (must_lock && !SDL_LockSurface(handle)) {
        return;
    }

    // SDL3: unpack/pack pixels via an SDL_PixelFormatDetails* resolved from the
    // surface's format enum.
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(handle->format);
    const int bpp = SDL_BYTESPERPIXEL(handle->format);

    for (int y = 0; y < surface.height(); ++y) {
        auto* row = static_cast<uint8_t*>(handle->pixels) + y * handle->pitch;
        for (int x = 0; x < surface.width(); ++x) {
            float fade = 1.0f;
            if (left_fade_strength > 0.0f && x < fade_width) {
                const float edge_fade = static_cast<float>(x) / static_cast<float>(fade_width);
                fade = ((1.0f - left_fade_strength) * 1.0f) + (left_fade_strength * edge_fade);
            } else if (x >= surface.width() - fade_width) {
                fade = static_cast<float>((surface.width() - 1) - x) / static_cast<float>(fade_width);
            }

            if (fade >= 1.0f) {
                continue;
            }

            fade = std::clamp(fade, 0.0f, 1.0f);
            uint32_t pixel = 0;
            std::memcpy(&pixel, row + x * bpp, bpp);

            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            uint8_t a = 0;
            SDL_GetRGBA(pixel, fmt, nullptr, &r, &g, &b, &a);
            if (a == 0) {
                continue;
            }

            a = static_cast<uint8_t>(static_cast<float>(a) * fade);
            pixel = SDL_MapRGBA(fmt, nullptr, r, g, b, a);
            std::memcpy(row + x * bpp, &pixel, bpp);
        }
    }

    if (must_lock) {
        SDL_UnlockSurface(handle);
    }
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
