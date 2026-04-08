/*
 * test_widget_rendering.cpp - SDL2 widget rendering regressions
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

#include <cstdlib>

using namespace TestFramework;
using namespace PsyMP3::Widget::Foundation;
using namespace PsyMP3::Widget::UI;
using namespace PsyMP3::Widget::Windowing;

namespace {

void ensureSDLVideo()
{
    static bool initialized = false;
    if (initialized) {
        return;
    }

    setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    initialized = true;
}

size_t countNonBlackPixels(::Surface& surface)
{
    SDL_Surface* handle = surface.getHandle();
    if (!handle || !handle->pixels) {
        return 0;
    }

    const bool must_lock = SDL_MUSTLOCK(handle);
    if (must_lock && SDL_LockSurface(handle) != 0) {
        throw std::runtime_error(std::string("SDL_LockSurface failed: ") + SDL_GetError());
    }

    size_t count = 0;
    for (int y = 0; y < handle->h; ++y) {
        auto* row = static_cast<uint8_t*>(handle->pixels) + y * handle->pitch;
        for (int x = 0; x < handle->w; ++x) {
            uint32_t pixel = 0;
            std::memcpy(&pixel, row + x * handle->format->BytesPerPixel, handle->format->BytesPerPixel);

            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            SDL_GetRGB(pixel, handle->format, &r, &g, &b);
            if (r != 0 || g != 0 || b != 0) {
                ++count;
            }
        }
    }

    if (must_lock) {
        SDL_UnlockSurface(handle);
    }

    return count;
}

size_t countNonTransparentPixels(::Surface& surface)
{
    SDL_Surface* handle = surface.getHandle();
    if (!handle || !handle->pixels) {
        return 0;
    }

    const bool must_lock = SDL_MUSTLOCK(handle);
    if (must_lock && SDL_LockSurface(handle) != 0) {
        throw std::runtime_error(std::string("SDL_LockSurface failed: ") + SDL_GetError());
    }

    size_t count = 0;
    for (int y = 0; y < handle->h; ++y) {
        auto* row = static_cast<uint8_t*>(handle->pixels) + y * handle->pitch;
        for (int x = 0; x < handle->w; ++x) {
            uint32_t pixel = 0;
            std::memcpy(&pixel, row + x * handle->format->BytesPerPixel, handle->format->BytesPerPixel);

            uint8_t a = 0;
            SDL_GetRGBA(pixel, handle->format, nullptr, nullptr, nullptr, &a);
            if (a != 0) {
                ++count;
            }
        }
    }

    if (must_lock) {
        SDL_UnlockSurface(handle);
    }

    return count;
}

size_t countNonBlackPixelsInRect(::Surface& surface, int x0, int y0, int width, int height)
{
    SDL_Surface* handle = surface.getHandle();
    if (!handle || !handle->pixels) {
        return 0;
    }

    const int x1 = std::min(x0 + width, handle->w);
    const int y1 = std::min(y0 + height, handle->h);
    if (x0 >= x1 || y0 >= y1) {
        return 0;
    }

    const bool must_lock = SDL_MUSTLOCK(handle);
    if (must_lock && SDL_LockSurface(handle) != 0) {
        throw std::runtime_error(std::string("SDL_LockSurface failed: ") + SDL_GetError());
    }

    size_t count = 0;
    for (int y = y0; y < y1; ++y) {
        auto* row = static_cast<uint8_t*>(handle->pixels) + y * handle->pitch;
        for (int x = x0; x < x1; ++x) {
            uint32_t pixel = 0;
            std::memcpy(&pixel, row + x * handle->format->BytesPerPixel, handle->format->BytesPerPixel);

            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            SDL_GetRGB(pixel, handle->format, &r, &g, &b);
            if (r != 0 || g != 0 || b != 0) {
                ++count;
            }
        }
    }

    if (must_lock) {
        SDL_UnlockSurface(handle);
    }

    return count;
}

class WidgetRenderingTest : public TestCase {
public:
    explicit WidgetRenderingTest(const std::string& name)
        : TestCase(name)
    {
    }

protected:
    void ensureVideoAndFont()
    {
        ensureSDLVideo();
        if (!m_font) {
            m_font = std::make_unique<Font>(TagLib::String("./res/vera.ttf"), 12);
        }
    }

    std::unique_ptr<Font> m_font;
};

class LabelRenderingTest : public WidgetRenderingTest {
public:
    LabelRenderingTest()
        : WidgetRenderingTest("Label blits visible text pixels to an RGB target surface")
    {
    }

protected:
    void runTest() override
    {
        ensureVideoAndFont();

        ::Surface target(320, 120);
        target.FillRect(target.MapRGB(0, 0, 0));

        Label label(m_font.get(),
                    Rect(10, 10, 0, 0),
                    TagLib::String("SDL2 Label Test"),
                    SDL_Color{255, 255, 255, 255},
                    SDL_Color{0, 0, 0, 255});
        label.BlitTo(target);

        ASSERT_TRUE(countNonBlackPixels(target) > 0,
                    "Rendering a label should produce visible non-black pixels");
    }
};

class WindowFrameRenderingTest : public WidgetRenderingTest {
public:
    WindowFrameRenderingTest()
        : WidgetRenderingTest("WindowFrameWidget blits visible frame pixels to an RGB target surface")
    {
    }

protected:
    void runTest() override
    {
        ensureVideoAndFont();

        ::Surface target(400, 240);
        target.FillRect(target.MapRGB(0, 0, 0));

        WindowFrameWidget frame(140, 70, "SDL2 Window", m_font.get());
        frame.setPos(Rect(30, 30, frame.getPos().width(), frame.getPos().height()));
        frame.BlitTo(target);

        ASSERT_TRUE(countNonBlackPixels(target) > 0,
                    "Rendering a framed window should produce visible non-black pixels");
    }
};

class OverflowLabelRenderingTest : public WidgetRenderingTest {
public:
    OverflowLabelRenderingTest()
        : WidgetRenderingTest("Overflowing labels render marquee text within a bounded viewport")
    {
    }

protected:
    void runTest() override
    {
        ensureVideoAndFont();

        ::Surface target(320, 120);
        target.FillRect(target.MapRGB(0, 0, 0));

        Label label(m_font.get(),
                    Rect(10, 10, 120, 16),
                    TagLib::String("Artist: This is a very long metadata value that should marquee"),
                    SDL_Color{255, 255, 255, 255},
                    SDL_Color{0, 0, 0, 255});
        label.setMarqueeEnabled(true);
        label.BlitTo(target);

        ASSERT_TRUE(countNonBlackPixels(target) > 0,
                    "Rendering an overflowing label should still produce visible pixels");
        ASSERT_TRUE(countNonBlackPixelsInRect(target, 10 + 120, 10, 160, 16) == 0,
                    "Overflowing marquee labels must not paint beyond their configured viewport");
    }
};

class ToastFadeRenderingTest : public WidgetRenderingTest {
public:
    ToastFadeRenderingTest()
        : WidgetRenderingTest("ToastWidget fades out instead of disappearing abruptly")
    {
    }

protected:
    void runTest() override
    {
        ensureVideoAndFont();

        ::Surface target(320, 120);
        ToastWidget toast("Toast fade regression", m_font.get(), ToastWidget::DURATION_LONG);
        toast.setPos(Rect(10, 10, toast.getPos().width(), toast.getPos().height()));

        toast.BlitTo(target);
        ASSERT_TRUE(countNonTransparentPixels(toast) > 0,
                    "A newly created toast should render visible pixels in its backing surface");

        toast.beginDismiss(ToastWidget::CROSSFADE_MS);
        toast.BlitTo(target);
        ASSERT_TRUE(countNonTransparentPixels(toast) > 0,
                    "A toast beginning dismissal should remain visible during the crossfade");

        SDL_Delay(ToastWidget::CROSSFADE_MS + 40);
        toast.BlitTo(target);
        ASSERT_TRUE(countNonTransparentPixels(toast) == 0,
                    "A toast should become fully transparent after its fade-out finishes");
    }
};

} // namespace

int main()
{
    TestSuite suite("Widget Rendering Regression Tests");
    suite.addTest(std::make_unique<LabelRenderingTest>());
    suite.addTest(std::make_unique<OverflowLabelRenderingTest>());
    suite.addTest(std::make_unique<ToastFadeRenderingTest>());
    suite.addTest(std::make_unique<WindowFrameRenderingTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
