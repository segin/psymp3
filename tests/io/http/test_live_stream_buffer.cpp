/*
 * test_live_stream_buffer.cpp - LiveStreamIOHandler's buffer, driven directly
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * A scripted producer hands the buffer exactly the chunks a test gives it, so
 * the reader's position relative to what has arrived is under the test's
 * control -- which a real connection, with its own timing, cannot promise.
 */

#include "psymp3.h"
#include "test_framework.h"

using PsyMP3::IO::HTTP::LiveStreamIOHandler;

namespace {

/**
 * @brief The byte at stream offset @p i: a pattern with no short period, so
 *        data at the wrong offset shows.
 * @param i Offset into the stream.
 * @return The byte there.
 */
char liveByte(int64_t i)
{
    return static_cast<char>((i * 7 + i / 251) & 0xFF);
}

/**
 * @brief A live handler whose producer appends what the test feeds it.
 */
class ScriptedLiveHandler : public LiveStreamIOHandler {
public:
    /** @brief Start the producer. */
    ScriptedLiveHandler() { startWorker(); }

    /**
     * @brief Stop the producer before this class's members go. It waits on
     *        the script's own condition, which abort() does not signal, so
     *        it is ended through the script first.
     */
    ~ScriptedLiveHandler() override
    {
        end();
        stopWorker();
    }

    /**
     * @brief Have the producer append the next @p len bytes of the pattern,
     *        and wait until it has.
     * @param len How many.
     */
    void feed(size_t len)
    {
        std::unique_lock<std::mutex> lock(m_script_mutex);
        m_pending += len;
        m_script_cv.notify_all();
        m_script_cv.wait(lock, [this] { return m_pending == 0 || m_stop; });
    }

    /** @brief End the stream cleanly. */
    void end()
    {
        std::lock_guard<std::mutex> lock(m_script_mutex);
        m_stop = true;
        m_script_cv.notify_all();
    }

protected:
    /** @brief Append fed bytes as they are asked for, until ended or aborted. */
    void runWorker() override
    {
        int64_t offset = 0;
        for (;;) {
            size_t len;
            {
                std::unique_lock<std::mutex> lock(m_script_mutex);
                m_script_cv.wait(lock, [this] { return m_pending > 0 || m_stop || m_abort; });
                if (m_stop || m_abort) break;
                len = m_pending;
            }
            std::string chunk(len, '\0');
            for (size_t i = 0; i < len; ++i) chunk[i] = liveByte(offset + static_cast<int64_t>(i));
            if (!appendAudio(chunk.data(), chunk.size())) break;
            offset += static_cast<int64_t>(len);
            std::lock_guard<std::mutex> lock(m_script_mutex);
            m_pending = 0;
            m_script_cv.notify_all();
        }
        {
            std::lock_guard<std::mutex> lock(m_script_mutex);
            m_stop = true;
            m_script_cv.notify_all();
        }
        finishWorker(false, std::string());
    }

private:
    std::mutex m_script_mutex;
    std::condition_variable m_script_cv;
    size_t m_pending = 0;
    bool m_stop = false;
};

/**
 * @brief A reader that seeks ahead of what has arrived (as skipping a large
 *        ID3v2 tag does) gets the right bytes there, and the buffer survives
 *        the data arriving: trimming the history used to measure from the
 *        reader's position, which past the end of the data made it erase more
 *        than the buffer held.
 */
class SeekAheadOfDataTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    SeekAheadOfDataTest() : TestCase("A seek ahead of the data received reads the right bytes when they come") {}
protected:
    /** @brief Seek 900 KiB ahead of 100 KiB received, then feed past it. */
    void runTest() override {
        ScriptedLiveHandler handler;
        handler.feed(100 * 1024);
        const int64_t target = 1000 * 1024;
        ASSERT_EQUALS(0, handler.seek(target, SEEK_SET), "a seek within the forward window is allowed");
        // Arrives in pieces, the first of which leaves the reader still ahead.
        handler.feed(300 * 1024);
        handler.feed(700 * 1024);
        std::string got(4096, '\0');
        ASSERT_EQUALS(got.size(), handler.read(&got[0], 1, got.size()), "the bytes at the target are read");
        bool match = true;
        for (size_t i = 0; i < got.size(); ++i) {
            if (got[i] != liveByte(target + static_cast<int64_t>(i))) { match = false; break; }
        }
        ASSERT_TRUE(match, "they are the stream's bytes at that offset");
        ASSERT_EQUALS(target + static_cast<int64_t>(got.size()), handler.tell(), "tell follows the read");
        // The history behind the reader is still there for a short seek back.
        ASSERT_EQUALS(0, handler.seek(target - 64 * 1024, SEEK_SET), "a short seek back still works");
        char c = 0;
        ASSERT_EQUALS(size_t(1), handler.read(&c, 1, 1), "and reads");
        ASSERT_EQUALS(liveByte(target - 64 * 1024), c, "the right byte");
        handler.end();
    }
};

/**
 * @brief Once aborted, a reader that has caught up sees the end at once:
 *        eof() used to stay false until the producer had wound up, so a
 *        read-until-eof loop spun meanwhile. And a SEEK_CUR far enough to
 *        overflow is refused rather than computed.
 */
class AbortEofAndSeekRangeTest : public TestFramework::TestCase {
public:
    /** @brief Name the test. */
    AbortEofAndSeekRangeTest() : TestCase("eof() right after abort; SEEK_CUR overflow refused") {}
protected:
    /** @brief Read what came, abort, check eof; then the huge seeks. */
    void runTest() override {
        ScriptedLiveHandler handler;
        handler.feed(1000);
        std::string got(1000, '\0');
        ASSERT_EQUALS(got.size(), handler.read(&got[0], 1, got.size()), "what arrived is read");
        ASSERT_EQUALS(-1, handler.seek(std::numeric_limits<int64_t>::max(), SEEK_CUR),
                      "a SEEK_CUR past the largest offset is refused");
        ASSERT_EQUALS(int64_t(1000), handler.tell(), "and moves nothing");
        handler.abort();
        ASSERT_TRUE(handler.eof(), "aborted and caught up: at the end");
    }
};

} // namespace

/**
 * @brief Run the live buffer tests; the entry test_http_suite calls.
 * @return Number of failed tests.
 */
int test_live_stream_buffer_main()
{
    TestFramework::TestSuite suite("Live Stream Buffer Tests");
    suite.addTest(std::make_unique<SeekAheadOfDataTest>());
    suite.addTest(std::make_unique<AbortEofAndSeekRangeTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
