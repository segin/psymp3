/*
 * test_widget_event_routing.cpp - Regression tests for widget event routing
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

class RecordingWidget : public Widget {
public:
    explicit RecordingWidget(bool capture_on_down = false)
        : m_capture_on_down(capture_on_down)
    {
    }

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override
    {
        down_count++;
        last_down_x = relative_x;
        last_down_y = relative_y;
        if (m_capture_on_down && event.button == SDL_BUTTON_LEFT) {
            captureMouse();
        }
        return true;
    }

    bool handleMouseMotion(const SDL_MouseMotionEvent&, int relative_x, int relative_y) override
    {
        motion_count++;
        last_motion_x = relative_x;
        last_motion_y = relative_y;
        return true;
    }

    bool handleMouseUp(const SDL_MouseButtonEvent&, int relative_x, int relative_y) override
    {
        up_count++;
        last_up_x = relative_x;
        last_up_y = relative_y;
        if (hasMouseCapture()) {
            releaseMouse();
        }
        return true;
    }

    int down_count = 0;
    int motion_count = 0;
    int up_count = 0;
    int last_down_x = 0;
    int last_down_y = 0;
    int last_motion_x = 0;
    int last_motion_y = 0;
    int last_up_x = 0;
    int last_up_y = 0;

private:
    bool m_capture_on_down;
};

SDL_MouseButtonEvent makeMouseButtonEvent(Uint32 type, Uint8 button = SDL_BUTTON_LEFT)
{
    SDL_MouseButtonEvent event{};
    event.type = type;
    event.button = button;
    return event;
}

SDL_MouseMotionEvent makeMouseMotionEvent()
{
    SDL_MouseMotionEvent event{};
    event.type = SDL_MOUSEMOTION;
    return event;
}

void releaseCapturedWidgetIfNeeded()
{
    if (Widget::getMouseCapturedWidget()) {
        Widget::getMouseCapturedWidget()->releaseMouse();
    }
}

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

Display& getTestDisplay()
{
    ensureSDLVideo();
    static Display* display = new Display();
    return *display;
}

class WidgetEventRoutingTest : public TestCase {
public:
    explicit WidgetEventRoutingTest(const std::string& name)
        : TestCase(name)
    {
    }

    void tearDown() override
    {
        releaseCapturedWidgetIfNeeded();
    }
};

class ClippedChildCoordinatesTest : public WidgetEventRoutingTest {
public:
    ClippedChildCoordinatesTest()
        : WidgetEventRoutingTest("Clipped child hit testing preserves child-local coordinates")
    {
    }

protected:
    void runTest() override
    {
        auto parent = std::make_unique<Widget>();
        parent->setPos(Rect(0, 0, 100, 50));

        auto child = std::make_unique<RecordingWidget>();
        RecordingWidget* child_ptr = child.get();
        child->setPos(Rect(-10, 0, 40, 20));
        parent->addChild(std::move(child));

        SDL_MouseButtonEvent down = makeMouseButtonEvent(SDL_MOUSEBUTTONDOWN);
        bool handled = parent->handleMouseDown(down, 0, 5);

        ASSERT_TRUE(handled, "Parent should dispatch to the partially visible child");
        ASSERT_EQUALS(1, child_ptr->down_count, "Child should receive the mouse-down event");
        ASSERT_EQUALS(10, child_ptr->last_down_x, "Child-local X should be computed from the child origin, not the clipped edge");
        ASSERT_EQUALS(5, child_ptr->last_down_y, "Child-local Y should be preserved");
    }
};

class NestedCaptureRoutingTest : public WidgetEventRoutingTest {
public:
    NestedCaptureRoutingTest()
        : WidgetEventRoutingTest("Nested captured widget keeps receiving motion and button-up events")
    {
    }

protected:
    void runTest() override
    {
        auto root = std::make_unique<Widget>();
        root->setPos(Rect(0, 0, 200, 200));

        auto panel = std::make_unique<Widget>();
        panel->setPos(Rect(50, 50, 100, 100));

        auto child = std::make_unique<RecordingWidget>(true);
        RecordingWidget* child_ptr = child.get();
        child->setPos(Rect(20, 10, 40, 30));
        panel->addChild(std::move(child));
        root->addChild(std::move(panel));

        SDL_MouseButtonEvent down = makeMouseButtonEvent(SDL_MOUSEBUTTONDOWN);
        SDL_MouseMotionEvent motion = makeMouseMotionEvent();
        SDL_MouseButtonEvent up = makeMouseButtonEvent(SDL_MOUSEBUTTONUP);

        ASSERT_TRUE(root->handleMouseDown(down, 75, 65), "Root should deliver the mouse-down event to the nested child");
        ASSERT_TRUE(child_ptr->hasMouseCapture(), "Nested child should capture the mouse during the drag");

        ASSERT_TRUE(root->handleMouseMotion(motion, 5, 5), "Root should route motion to the captured nested child");
        ASSERT_EQUALS(1, child_ptr->motion_count, "Captured child should receive motion events even outside its bounds");
        ASSERT_EQUALS(-65, child_ptr->last_motion_x, "Motion X should be translated through every ancestor");
        ASSERT_EQUALS(-55, child_ptr->last_motion_y, "Motion Y should be translated through every ancestor");

        ASSERT_TRUE(root->handleMouseUp(up, 6, 6), "Root should route button-up to the captured nested child");
        ASSERT_EQUALS(1, child_ptr->up_count, "Captured child should receive the button-up event");
        ASSERT_FALSE(child_ptr->hasMouseCapture(), "Child should release capture after button-up");
    }
};

class WindowFrameClientDispatchTest : public WidgetEventRoutingTest {
public:
    WindowFrameClientDispatchTest()
        : WidgetEventRoutingTest("WindowFrameWidget forwards client-area events into the widget tree")
    {
    }

protected:
    void runTest() override
    {
        WindowFrameWidget frame(120, 60, "Test", nullptr);

        auto client = std::make_unique<RecordingWidget>(true);
        RecordingWidget* client_ptr = client.get();
        frame.setClientArea(std::move(client));

        const Rect client_pos = frame.getClientArea()->getPos();
        SDL_MouseButtonEvent down = makeMouseButtonEvent(SDL_MOUSEBUTTONDOWN);
        SDL_MouseMotionEvent motion = makeMouseMotionEvent();
        SDL_MouseButtonEvent up = makeMouseButtonEvent(SDL_MOUSEBUTTONUP);

        ASSERT_TRUE(frame.handleMouseDown(down, client_pos.x() + 7, client_pos.y() + 9),
                    "Frame should forward client-area mouse-down events");
        ASSERT_EQUALS(1, client_ptr->down_count, "Client area should receive the mouse-down event");
        ASSERT_EQUALS(7, client_ptr->last_down_x, "Client-area X should be frame-relative minus client origin");
        ASSERT_EQUALS(9, client_ptr->last_down_y, "Client-area Y should be frame-relative minus client origin");

        ASSERT_TRUE(frame.handleMouseMotion(motion, client_pos.x() + 20, client_pos.y() + 25),
                    "Frame should keep forwarding motion to the captured client-area widget");
        ASSERT_EQUALS(1, client_ptr->motion_count, "Client area should receive motion events after capture");
        ASSERT_EQUALS(20, client_ptr->last_motion_x, "Motion X should stay relative to the client area");
        ASSERT_EQUALS(25, client_ptr->last_motion_y, "Motion Y should stay relative to the client area");

        ASSERT_TRUE(frame.handleMouseUp(up, client_pos.x() + 18, client_pos.y() + 21),
                    "Frame should forward button-up to the captured client-area widget");
        ASSERT_EQUALS(1, client_ptr->up_count, "Client area should receive button-up events");
    }
};

class ApplicationWindowCaptureRoutingTest : public WidgetEventRoutingTest {
public:
    ApplicationWindowCaptureRoutingTest()
        : WidgetEventRoutingTest("ApplicationWidget routes captured events back through the owning window")
    {
    }

protected:
    void runTest() override
    {
        Display& display = getTestDisplay();
        ApplicationWidget& app = ApplicationWidget::getInstance(display);

        auto window = std::make_unique<Widget>();
        window->setPos(Rect(40, 40, 200, 160));

        auto panel = std::make_unique<Widget>();
        panel->setPos(Rect(30, 20, 100, 80));

        auto child = std::make_unique<RecordingWidget>(true);
        RecordingWidget* child_ptr = child.get();
        child->setPos(Rect(10, 5, 30, 20));

        panel->addChild(std::move(child));
        window->addChild(std::move(panel));
        app.addWindow(std::move(window));

        SDL_MouseButtonEvent down = makeMouseButtonEvent(SDL_MOUSEBUTTONDOWN);
        SDL_MouseMotionEvent motion = makeMouseMotionEvent();
        SDL_MouseButtonEvent up = makeMouseButtonEvent(SDL_MOUSEBUTTONUP);

        ASSERT_TRUE(app.handleMouseDown(down, 84, 71), "Application should deliver the initial mouse-down to the window subtree");
        ASSERT_TRUE(child_ptr->hasMouseCapture(), "Nested window child should capture the mouse");

        ASSERT_TRUE(app.handleMouseMotion(motion, 10, 10), "Application should route motion back through the owning window");
        ASSERT_EQUALS(1, child_ptr->motion_count, "Captured child inside a window should receive motion events");
        ASSERT_EQUALS(-70, child_ptr->last_motion_x, "Window routing should preserve descendant-relative X translation");
        ASSERT_EQUALS(-55, child_ptr->last_motion_y, "Window routing should preserve descendant-relative Y translation");

        ASSERT_TRUE(app.handleMouseUp(up, 11, 12), "Application should route button-up back through the owning window");
        ASSERT_EQUALS(1, child_ptr->up_count, "Captured child inside a window should receive button-up events");
    }
};

} // namespace

int main()
{
    TestSuite suite("Widget Event Routing Tests");
    suite.addTest(std::make_unique<ClippedChildCoordinatesTest>());
    suite.addTest(std::make_unique<NestedCaptureRoutingTest>());
    suite.addTest(std::make_unique<WindowFrameClientDispatchTest>());
    suite.addTest(std::make_unique<ApplicationWindowCaptureRoutingTest>());

    auto results = suite.runAll();
    suite.printResults(results);

    return suite.getFailureCount(results) == 0 ? 0 : 1;
}
