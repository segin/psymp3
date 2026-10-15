/*
 * test_progress_bar_seekable.cpp - the progress bar refuses to seek when told to
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * A track with no length (internet radio) cannot be seeked, so the player
 * draws the bar full and calls setSeekable(false): clicks and drags must
 * then do nothing, and a drag already under way must end without a seek.
 */

#include "psymp3.h"
#include "test_framework.h"

using PsyMP3::Widget::Foundation::Widget;
using PsyMP3::Widget::UI::PlayerProgressBarWidget;

namespace {

/**
 * @brief A left-button event of @p type.
 * @param type SDL_EVENT_MOUSE_BUTTON_DOWN or _UP.
 * @return The event.
 */
SDL_MouseButtonEvent leftButton(SDL_EventType type)
{
    SDL_MouseButtonEvent event{};
    event.type = type;
    event.button = SDL_BUTTON_LEFT;
    return event;
}

/** @brief Start SDL's dummy video driver once, which surfaces need. */
void ensureSDLVideo()
{
    static bool initialized = false;
    if (initialized) return;
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    initialized = true;
}

/** @brief Counts the bar's seek callbacks. */
struct SeekCalls {
    int start = 0;   ///< seek-start callbacks
    int end = 0;     ///< seek-end callbacks
    double last_end = -1.0;  ///< progress passed to the last seek-end
};

/**
 * @brief A 220x10 bar wired to @p calls.
 * @param calls Where the callbacks count.
 * @return The bar.
 */
std::unique_ptr<PlayerProgressBarWidget> makeBar(SeekCalls& calls)
{
    ensureSDLVideo();
    auto bar = std::make_unique<PlayerProgressBarWidget>(220, 10);
    bar->setOnSeekStart([&calls](double) { ++calls.start; });
    bar->setOnSeekEnd([&calls](double p) { ++calls.end; calls.last_end = p; });
    return bar;
}

/** @brief Base for these tests: lets go of any mouse capture afterwards. */
class SeekableTestBase : public TestFramework::TestCase {
public:
    /**
     * @brief Name the test.
     * @param name Its name.
     */
    explicit SeekableTestBase(const std::string& name) : TestCase(name) {}

    /** @brief Release a capture a failed test may have left. */
    void tearDown() override
    {
        if (Widget::getMouseCapturedWidget()) {
            Widget::getMouseCapturedWidget()->releaseMouse();
        }
    }
};

/** @brief Seekable, the bar seeks as before. */
class SeekableClickTest : public SeekableTestBase {
public:
    /** @brief Name the test. */
    SeekableClickTest() : SeekableTestBase("Seekable bar: a click seeks") {}
protected:
    /** @brief Click and release; check the seek callbacks fire. */
    void runTest() override {
        SeekCalls calls;
        auto bar = makeBar(calls);
        ASSERT_TRUE(bar->isSeekable(), "seekable by default");
        ASSERT_TRUE(bar->handleMouseDown(leftButton(SDL_EVENT_MOUSE_BUTTON_DOWN), 110, 5), "click taken");
        ASSERT_TRUE(bar->handleMouseUp(leftButton(SDL_EVENT_MOUSE_BUTTON_UP), 110, 5), "release taken");
        ASSERT_EQUALS(1, calls.start, "seek started");
        ASSERT_EQUALS(1, calls.end, "seek ended");
    }
};

/** @brief Not seekable: clicks are taken and ignored. */
class UnseekableClickTest : public SeekableTestBase {
public:
    /** @brief Name the test. */
    UnseekableClickTest() : SeekableTestBase("Unseekable bar: clicks and drags do nothing") {}
protected:
    /** @brief Click, drag and release; check nothing seeks and nothing is captured. */
    void runTest() override {
        SeekCalls calls;
        auto bar = makeBar(calls);
        bar->setSeekable(false);
        bar->setProgress(1.0);
        ASSERT_TRUE(bar->handleMouseDown(leftButton(SDL_EVENT_MOUSE_BUTTON_DOWN), 50, 5),
                    "a click on the bar is taken, so nothing behind it acts");
        ASSERT_FALSE(bar->isDragging(), "no drag starts");
        ASSERT_NULL(Widget::getMouseCapturedWidget(), "the mouse is not captured");
        SDL_MouseMotionEvent motion{};
        motion.type = SDL_EVENT_MOUSE_MOTION;
        ASSERT_FALSE(bar->handleMouseMotion(motion, 80, 5), "motion is not a drag");
        ASSERT_FALSE(bar->handleMouseUp(leftButton(SDL_EVENT_MOUSE_BUTTON_UP), 80, 5), "release is not a seek");
        ASSERT_EQUALS(0, calls.start, "no seek started");
        ASSERT_EQUALS(0, calls.end, "no seek ended");
        ASSERT_EQUALS(1.0, bar->getProgress(), "still drawn full");
        ASSERT_FALSE(bar->handleMouseDown(leftButton(SDL_EVENT_MOUSE_BUTTON_DOWN), 500, 5),
                     "a click outside the bar is not taken");
    }
};

/** @brief A drag under way when seeking is forbidden. */
class DragCancelledTest : public SeekableTestBase {
public:
    /** @brief Name the test. */
    DragCancelledTest() : SeekableTestBase("Forbidding seeks mid-drag drops the drag without a seek") {}
protected:
    /** @brief Start a drag, forbid seeking, release; check no seek ends. */
    void runTest() override {
        SeekCalls calls;
        auto bar = makeBar(calls);
        ASSERT_TRUE(bar->handleMouseDown(leftButton(SDL_EVENT_MOUSE_BUTTON_DOWN), 30, 5), "drag starts");
        ASSERT_TRUE(bar->isDragging(), "dragging");
        bar->setSeekable(false);
        ASSERT_FALSE(bar->isDragging(), "drag dropped");
        ASSERT_NULL(Widget::getMouseCapturedWidget(), "capture released");
        ASSERT_FALSE(bar->handleMouseUp(leftButton(SDL_EVENT_MOUSE_BUTTON_UP), 30, 5), "release ignored");
        ASSERT_EQUALS(0, calls.end, "the dropped drag does not seek");
        bar->setSeekable(true);
        ASSERT_TRUE(bar->isSeekable(), "seekable again");
    }
};

} // namespace

/**
 * @brief Run the progress bar seekability tests; the entry test_widget_suite calls.
 * @return Number of failed tests.
 */
int test_progress_bar_seekable_main()
{
    TestFramework::TestSuite suite("Progress Bar Seekability Tests");
    suite.addTest(std::make_unique<SeekableClickTest>());
    suite.addTest(std::make_unique<UnseekableClickTest>());
    suite.addTest(std::make_unique<DragCancelledTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
