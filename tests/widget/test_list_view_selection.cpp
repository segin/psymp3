/*
 * test_list_view_selection.cpp - ListViewWidget range selection and block drag
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

#include <cstdlib>
#include <tuple>

using namespace TestFramework;
using namespace PsyMP3::Widget::UI;
using PsyMP3::Widget::Foundation::Widget;

namespace {

// Without a font the list uses its default 16-pixel rows inside a 1-pixel frame.
constexpr int kRow = 16;
constexpr int kBorder = 1;
constexpr int kRows = 10;

void ensureSDLVideo()
{
    static bool initialized = false;
    if (!initialized) {
        setenv("SDL_VIDEODRIVER", "dummy", 1);
        SDL_Init(SDL_INIT_VIDEO);
        initialized = true;
    }
}

SDL_MouseButtonEvent button(SDL_EventType type, Uint8 which = SDL_BUTTON_LEFT)
{
    SDL_MouseButtonEvent event{};
    event.type = type;
    event.button = which;
    return event;
}

/// A ten-row list recording the edits it reports.
struct Fixture {
    std::unique_ptr<ListViewWidget> list;
    std::vector<std::tuple<int, int, int>> reorders;
    std::vector<std::pair<int, int>> deletes;

    Fixture()
    {
        ensureSDLVideo();
        list = std::make_unique<ListViewWidget>(200, kRows * kRow + 2 * kBorder, nullptr);
        std::vector<TagLib::String> items;
        for (int i = 0; i < kRows; ++i) {
            items.push_back(TagLib::String(std::to_string(i)));
        }
        list->setItems(items);
        list->setOnReorder([this](int first, int last, int to) { reorders.emplace_back(first, last, to); });
        list->setOnDelete([this](int first, int last) { deletes.emplace_back(first, last); });
    }

    ~Fixture()
    {
        if (Widget::getMouseCapturedWidget()) {
            Widget::getMouseCapturedWidget()->releaseMouse();
        }
        ListViewWidget::clearFocusedWidget();
        SDL_SetModState(SDL_KMOD_NONE);
    }

    static int rowY(int row) { return kBorder + row * kRow + kRow / 2; }
    static int gapY(int gap) { return kBorder + gap * kRow; }

    void click(int row, bool shift = false, Uint8 which = SDL_BUTTON_LEFT)
    {
        SDL_SetModState(shift ? SDL_KMOD_LSHIFT : SDL_KMOD_NONE);
        list->handleMouseDown(button(SDL_EVENT_MOUSE_BUTTON_DOWN, which), 10, rowY(row));
        list->handleMouseUp(button(SDL_EVENT_MOUSE_BUTTON_UP, which), 10, rowY(row));
        SDL_SetModState(SDL_KMOD_NONE);
    }

    /// Press on a row, drag to an insertion gap, release there.
    void drag(int row, int gap)
    {
        list->handleMouseDown(button(SDL_EVENT_MOUSE_BUTTON_DOWN), 10, rowY(row));
        SDL_MouseMotionEvent motion{};
        motion.type = SDL_EVENT_MOUSE_MOTION;
        list->handleMouseMotion(motion, 10, gapY(gap));
        list->handleMouseUp(button(SDL_EVENT_MOUSE_BUTTON_UP), 10, gapY(gap));
    }

    void key(SDL_Keycode sym, bool shift = false)
    {
        Keysym k{};
        k.sym = sym;
        k.mod = shift ? SDL_KMOD_LSHIFT : SDL_KMOD_NONE;
        ListViewWidget::handleFocusedKeyPress(k);
    }

    bool selected(int first, int last) const
    {
        return list->getSelectionFirst() == first && list->getSelectionLast() == last &&
               list->getSelectionCount() == last - first + 1;
    }
};

class ShiftClickTest : public TestCase {
public:
    ShiftClickTest() : TestCase("Shift+click selects the run from the anchor to the clicked row") {}

protected:
    void runTest() override
    {
        Fixture f;
        f.click(2);
        ASSERT_TRUE(f.selected(2, 2), "a plain click selects one row");
        f.click(5, true);
        ASSERT_TRUE(f.selected(2, 5), "shift+click extends down");
        ASSERT_TRUE(f.list->getSelectionAnchor() == 2 && f.list->getSelectedIndex() == 5, "anchor 2, cursor 5");
        f.click(0, true);
        ASSERT_TRUE(f.selected(0, 2), "shift+click on the other side of the anchor reverses the run");
        ASSERT_TRUE(f.list->isRowSelected(1) && !f.list->isRowSelected(3), "membership follows the run");
        f.click(7);
        ASSERT_TRUE(f.selected(7, 7), "a plain click elsewhere starts over");
    }
};

class ShiftArrowTest : public TestCase {
public:
    ShiftArrowTest() : TestCase("Shift+Up/Down select and deselect rows from the anchor") {}

protected:
    void runTest() override
    {
        Fixture f;
        f.click(4); // also gives the list keyboard focus
        f.key(SDLK_DOWN, true);
        f.key(SDLK_DOWN, true);
        ASSERT_TRUE(f.selected(4, 6), "two rows added below");
        f.key(SDLK_UP, true);
        ASSERT_TRUE(f.selected(4, 5), "one deselected again");
        f.key(SDLK_UP, true);
        f.key(SDLK_UP, true);
        ASSERT_TRUE(f.selected(3, 4), "past the anchor the run grows upward");
        f.key(SDLK_DOWN);
        ASSERT_TRUE(f.selected(4, 4), "a plain arrow moves the cursor and drops the run");
    }
};

class DragBlockTest : public TestCase {
public:
    DragBlockTest() : TestCase("Dragging a selected row moves the whole run") {}

protected:
    void runTest() override
    {
        Fixture down;
        down.click(2);
        down.click(4, true);
        down.drag(3, 8);
        ASSERT_TRUE(down.reorders.size() == 1 && down.reorders[0] == std::make_tuple(2, 4, 5),
                    "rows 2..4 dropped before row 8 land at 5");

        Fixture up;
        up.click(5);
        up.click(6, true);
        up.drag(6, 1);
        ASSERT_TRUE(up.reorders.size() == 1 && up.reorders[0] == std::make_tuple(5, 6, 1),
                    "rows 5..6 dropped before row 1 land at 1");
    }
};

class DropOnItselfTest : public TestCase {
public:
    DropOnItselfTest() : TestCase("A run cannot be dropped inside or at the edges of itself") {}

protected:
    void runTest() override
    {
        Fixture f;
        f.click(2);
        f.click(5, true);
        f.drag(3, 4);   // between two selected rows
        f.drag(4, 2);   // at the block's top edge
        f.drag(5, 6);   // at the block's bottom edge
        ASSERT_TRUE(f.reorders.empty(), "no move reported");
        ASSERT_TRUE(f.selected(2, 5), "and the selection is left as it was");

        Fixture single;
        single.click(3);
        single.drag(3, 4);
        ASSERT_TRUE(single.reorders.empty(), "a single row dropped just below itself stays put");
    }
};

class ClickInsideRunTest : public TestCase {
public:
    ClickInsideRunTest() : TestCase("Clicking inside a run without dragging selects just that row") {}

protected:
    void runTest() override
    {
        Fixture f;
        f.click(2);
        f.click(5, true);
        SDL_MouseButtonEvent down = button(SDL_EVENT_MOUSE_BUTTON_DOWN);
        f.list->handleMouseDown(down, 10, Fixture::rowY(4));
        ASSERT_TRUE(f.selected(2, 5), "the run survives the press, so it could be dragged");
        f.list->handleMouseUp(button(SDL_EVENT_MOUSE_BUTTON_UP), 10, Fixture::rowY(4));
        ASSERT_TRUE(f.selected(4, 4), "the release without a drag selects the clicked row");
    }
};

class RightClickTest : public TestCase {
public:
    RightClickTest() : TestCase("Right-clicking inside the run keeps it for the context menu") {}

protected:
    void runTest() override
    {
        Fixture f;
        f.click(2);
        f.click(4, true);
        f.click(3, false, SDL_BUTTON_RIGHT);
        ASSERT_TRUE(f.selected(2, 4), "inside: kept");
        f.click(7, false, SDL_BUTTON_RIGHT);
        ASSERT_TRUE(f.selected(7, 7), "outside: that row alone");
    }
};

class DeleteAndHelpersTest : public TestCase {
public:
    DeleteAndHelpersTest() : TestCase("Delete and the edit helpers act on the whole run") {}

protected:
    void runTest() override
    {
        Fixture f;
        f.click(1);
        f.click(3, true);
        f.key(SDLK_DELETE);
        ASSERT_TRUE(f.deletes.size() == 1 && f.deletes[0] == std::make_pair(1, 3), "Delete reports 1..3");

        f.list->moveSelectedUp();
        ASSERT_TRUE(f.selected(0, 2), "run moved up one");
        f.list->moveSelectedUp();
        ASSERT_TRUE(f.selected(0, 2), "already at the top: no-op");
        f.list->moveSelectedDown();
        f.list->moveSelectedDown();
        ASSERT_TRUE(f.selected(2, 4), "run moved down two");
        f.list->removeSelected();
        ASSERT_TRUE(f.list->itemCount() == static_cast<size_t>(kRows - 3), "three rows removed");
        ASSERT_TRUE(f.selected(2, 2), "the row that took their place is selected");
    }
};

} // namespace

int test_list_view_selection_main()
{
    TestSuite suite("List View Selection Tests");
    suite.addTest(std::make_unique<ShiftClickTest>());
    suite.addTest(std::make_unique<ShiftArrowTest>());
    suite.addTest(std::make_unique<DragBlockTest>());
    suite.addTest(std::make_unique<DropOnItselfTest>());
    suite.addTest(std::make_unique<ClickInsideRunTest>());
    suite.addTest(std::make_unique<RightClickTest>());
    suite.addTest(std::make_unique<DeleteAndHelpersTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
