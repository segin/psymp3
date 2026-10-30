/*
 * display.h - SDL_Surface wrapper implementation for display window.
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
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

#ifndef DISPLAY_H
#define DISPLAY_H

namespace PsyMP3::Core {

class Display : public Surface
{
    public:
        static constexpr int LOGICAL_WIDTH = 640;
        static constexpr int LOGICAL_HEIGHT = 404;

        Display();
        ~Display() override;
        void SetCaption(TagLib::String, TagLib::String);
        // Set the window/taskbar icon from a tightly-packed RGBA8888 buffer
        // (byte order R,G,B,A). Best-effort; a null/invalid buffer is ignored.
        void setWindowIcon(const uint8_t* rgba, int width, int height);
        void Flip() override;
        SDL_Window* getWindowHandle() const;
        bool handleWindowEvent(const SDL_WindowEvent& event);
        bool handleRenderDeviceReset();
        void handleRenderTargetsReset();

        /*
         * The underlay: a texture drawn on the GPU beneath the software-drawn
         * frame, which is then premultiplied ARGB and lets it show through
         * where transparent. The spectrum draws there. Only with a renderer.
         */
        bool hasUnderlay() const { return m_renderer != nullptr; }
        SDL_Renderer* beginUnderlay(int width, int height, bool& fresh);
        void endUnderlay();
        void showUnderlay(const Rect& at);
        static SDL_BlendMode fadeBlendMode();

        int getLogicalScale() const { return m_logical_scale; }
        void setLogicalScale(int scale);
        // Re-assert the client-area size at the current scale. Call after the
        // native window gains/loses a menu bar so the drawable stays logical.
        void reapplyWindowSize();

    private:
        SDL_Window* m_window = nullptr;
        SDL_Surface* m_window_surface = nullptr; // non-owned, refreshed from SDL; only without a renderer
        SDL_Renderer* m_renderer = nullptr;      // presents the frame; null means the window-surface path
        SDL_Texture* m_texture = nullptr;        // the logical frame, uploaded each Flip()
        std::vector<uint8_t> m_uploaded;         // the frame as last uploaded, to upload only changed rows
        SDL_Texture* m_underlay = nullptr;       // render target beneath the frame
        bool m_underlay_lost = false;            // its contents were lost (render targets reset)
        bool m_underlay_shown = false;           // drawn this frame: a visible widget asked for it
        Rect m_underlay_at;                      // where, in logical pixels
        int m_logical_scale = 1;
        std::string m_caption; // last title given to SDL_SetWindowTitle

        bool createRenderer();
        bool createTexture();
        void uploadChangedRows();
        void refreshWindowSurface();
        void allocateLogicalSurface();
};

} // namespace PsyMP3::Core

#endif // DISPLAY_H
