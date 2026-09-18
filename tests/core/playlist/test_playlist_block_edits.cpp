/*
 * test_playlist_block_edits.cpp - Moving and removing runs of playlist tracks
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

namespace {

/// A playlist of tracks named 0..n-1, so the order reads as a string.
std::unique_ptr<Playlist> numbered(int n)
{
    auto playlist = std::make_unique<Playlist>();
    for (int i = 0; i < n; ++i) {
        playlist->addFile(TagLib::String("/test/track" + std::to_string(i) + ".mp3"),
                          TagLib::String("Artist"), TagLib::String("Title"), 180);
    }
    return playlist;
}

/// The playlist order as track numbers, e.g. "01452367".
std::string order(const Playlist& playlist)
{
    std::string s;
    for (long i = 0; i < playlist.entries(); ++i) {
        const std::string path = playlist.getTrack(i).to8Bit(true);
        const size_t dot = path.rfind(".mp3");
        s += path.substr(path.rfind("track") + 5, dot - path.rfind("track") - 5);
    }
    return s;
}

class MoveBlockTest : public TestCase {
public:
    MoveBlockTest() : TestCase("A block moves as a unit, in order, up or down") {}

protected:
    void runTest() override
    {
        auto down = numbered(8);
        ASSERT_TRUE(down->moveTracks(2, 3, 5), "move 2..3 down to 5");
        ASSERT_EQUALS(std::string("01456237"), order(*down), "block lands at 5, order kept");

        auto up = numbered(8);
        ASSERT_TRUE(up->moveTracks(5, 6, 1), "move 5..6 up to 1");
        ASSERT_EQUALS(std::string("05612347"), order(*up), "block lands at 1, order kept");

        auto to_end = numbered(6);
        ASSERT_TRUE(to_end->moveTracks(0, 2, 3), "move the first three to the end");
        ASSERT_EQUALS(std::string("345012"), order(*to_end), "to the very end");
    }
};

class RefusedMoveTest : public TestCase {
public:
    RefusedMoveTest() : TestCase("Moves that change nothing or leave the list are refused") {}

protected:
    void runTest() override
    {
        auto p = numbered(8);
        ASSERT_FALSE(p->moveTracks(2, 4, 2), "onto itself");
        ASSERT_FALSE(p->moveTracks(2, 4, 6), "past the end: 3 tracks cannot start at 6 of 8");
        ASSERT_FALSE(p->moveTracks(4, 2, 0), "reversed span");
        ASSERT_FALSE(p->moveTracks(6, 8, 0), "span past the end");
        ASSERT_FALSE(p->moveTracks(-1, 1, 3), "negative start");
        ASSERT_EQUALS(std::string("01234567"), order(*p), "nothing changed");
    }
};

class MoveKeepsCursorTest : public TestCase {
public:
    MoveKeepsCursorTest() : TestCase("The playing track stays current through a block move") {}

protected:
    void runTest() override
    {
        auto inside = numbered(8);
        inside->setPosition(3);
        inside->moveTracks(2, 3, 5);
        ASSERT_EQUALS(6L, inside->getPosition(), "track 3 moved with its block to index 6");

        auto between = numbered(8);
        between->setPosition(5);
        between->moveTracks(2, 3, 5);
        ASSERT_EQUALS(3L, between->getPosition(), "track 5 shifts up past the block");

        auto outside = numbered(8);
        outside->setPosition(0);
        outside->moveTracks(2, 3, 5);
        ASSERT_EQUALS(0L, outside->getPosition(), "track 0 untouched");
    }
};

class RemoveBlockTest : public TestCase {
public:
    RemoveBlockTest() : TestCase("A block is removed whole and the cursor follows removeTrack's rule") {}

protected:
    void runTest() override
    {
        auto inside = numbered(8);
        inside->setPosition(3);
        ASSERT_TRUE(inside->removeTracks(2, 4), "remove 2..4");
        ASSERT_EQUALS(std::string("01567"), order(*inside), "three tracks gone");
        ASSERT_EQUALS(2L, inside->getPosition(), "cursor on the track that took the block's place");

        auto after = numbered(8);
        after->setPosition(6);
        after->removeTracks(2, 4);
        ASSERT_EQUALS(3L, after->getPosition(), "a later cursor shifts up by the block's length");

        auto tail = numbered(5);
        tail->setPosition(4);
        tail->removeTracks(3, 4);
        ASSERT_EQUALS(2L, tail->getPosition(), "removing the tail clamps the cursor to the new end");

        auto all = numbered(4);
        ASSERT_TRUE(all->removeTracks(0, 3), "remove everything");
        ASSERT_EQUALS(0L, all->entries(), "empty");

        auto bad = numbered(4);
        ASSERT_FALSE(bad->removeTracks(2, 4), "span past the end");
        ASSERT_FALSE(bad->removeTracks(3, 2), "reversed span");
        ASSERT_EQUALS(4L, bad->entries(), "untouched");
    }
};

class ShuffleTest : public TestCase {
public:
    ShuffleTest() : TestCase("Block edits keep a shuffled playlist consistent") {}

protected:
    void runTest() override
    {
        auto p = numbered(12);
        p->setShuffle(true);
        ASSERT_TRUE(p->moveTracks(3, 7, 0), "move under shuffle");
        ASSERT_TRUE(p->removeTracks(0, 4), "remove under shuffle");
        ASSERT_EQUALS(7L, p->entries(), "seven left");
        for (int i = 0; i < 20; ++i) {
            p->next();
            ASSERT_TRUE(p->getPosition() >= 0 && p->getPosition() < 7, "shuffle stays within the list");
        }
    }
};

} // namespace

int test_playlist_block_edits_main()
{
    TestSuite suite("Playlist Block Edit Tests");
    suite.addTest(std::make_unique<MoveBlockTest>());
    suite.addTest(std::make_unique<RefusedMoveTest>());
    suite.addTest(std::make_unique<MoveKeepsCursorTest>());
    suite.addTest(std::make_unique<RemoveBlockTest>());
    suite.addTest(std::make_unique<ShuffleTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
