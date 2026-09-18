/*
 * test_audio_buffer_cursor.cpp - Audio's PCM queue and its read cursor
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

// Every standard header psymp3.h uses comes first, so that the access
// override below reaches only PsyMP3's own classes.
#include <algorithm>
#include <atomic>
#include <bitset>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <complex>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <queue>
#include <random>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

// The SDL callback and the queue cursor are private; these tests drive them
// directly, because the device path offers no way to read the output back.
#define private public
#include "psymp3.h"
#undef private

#include "test_framework.h"

using namespace TestFramework;

namespace {

constexpr int kRate = 48000;
constexpr int kChannels = 2;

/// Consecutive sample values starting at @p first, @p frames frames long.
std::vector<AudioSample> ramp(AudioSample first, size_t frames)
{
    std::vector<AudioSample> samples(frames * kChannels);
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = first + static_cast<AudioSample>(i);
    }
    return samples;
}

/// A stream with nothing left to give, so the decoder thread stays idle and
/// the queue holds exactly what a test primed it with.
class ExhaustedStream : public Stream {
public:
    size_t getData(size_t, void*) override { return 0; }
    void seekTo(unsigned long) override {}
    bool eof() override { return true; }
    unsigned int getRate() override { return kRate; }
    unsigned int getChannels() override { return kChannels; }
};

/// A stream counting up from 1, one sample value at a time, forever.
class CountingStream : public Stream {
public:
    size_t getData(size_t len, void* buf) override
    {
        const size_t count = len / sizeof(AudioSample);
        auto* out = static_cast<AudioSample*>(buf);
        for (size_t i = 0; i < count; ++i) {
            out[i] = m_next++;
        }
        return count * sizeof(AudioSample);
    }
    void seekTo(unsigned long) override {}
    bool eof() override { return false; }
    unsigned int getRate() override { return kRate; }
    unsigned int getChannels() override { return kChannels; }

private:
    AudioSample m_next = 1;
};

/// Runs Audio's SDL callback into a stream bound to no device and reads back
/// what it produced. Both ends use the device format, so SDL passes the bytes
/// through unchanged. The Audio's own device stays paused, as SDL opens it,
/// so nothing else drains the queue.
class Puller {
public:
    explicit Puller(Audio& audio) : m_audio(audio)
    {
        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_S32;
        spec.channels = kChannels;
        spec.freq = kRate;
        m_stream = SDL_CreateAudioStream(&spec, &spec);
        m_audio.m_playing = true; // the callback only copies while playing
    }
    ~Puller()
    {
        if (m_stream) {
            SDL_DestroyAudioStream(m_stream);
        }
    }

    bool ok() const { return m_stream != nullptr; }

    std::vector<AudioSample> pull(size_t frames)
    {
        const int bytes = static_cast<int>(frames * kChannels * sizeof(AudioSample));
        Audio::callback(&m_audio, m_stream, bytes, bytes);
        std::vector<AudioSample> out(frames * kChannels);
        const int got = SDL_GetAudioStreamData(m_stream, out.data(), bytes);
        out.resize(got > 0 ? static_cast<size_t>(got) / sizeof(AudioSample) : 0);
        return out;
    }

private:
    Audio& m_audio;
    SDL_AudioStream* m_stream = nullptr;
};

size_t buffered(Audio& audio)
{
    std::lock_guard<std::mutex> lock(audio.m_buffer_mutex);
    return audio.bufferedSamples_unlocked();
}

class CursorConsumptionTest : public TestCase {
public:
    CursorConsumptionTest() : TestCase("The callback consumes the queue in order without erasing it") {}

protected:
    void runTest() override
    {
        FastFourier fft(512);
        std::mutex player_mutex;
        Audio audio(std::make_unique<ExhaustedStream>(), &fft, &player_mutex, ramp(1, 1000), true);
        Puller puller(audio);
        ASSERT_TRUE(puller.ok(), "a device-less SDL stream can be created");

        ASSERT_TRUE(puller.pull(300) == ramp(1, 300), "the first pull is the head of the queue");
        {
            std::lock_guard<std::mutex> lock(audio.m_buffer_mutex);
            ASSERT_EQUALS(size_t{600}, audio.m_buffer_read, "the cursor moved past what was played");
            ASSERT_EQUALS(size_t{2000}, audio.m_buffer.size(),
                          "nothing was erased: the callback no longer shifts the queue");
        }
        ASSERT_EQUALS(size_t{1400}, buffered(audio), "the rest is still queued");
        ASSERT_EQUALS(uint64_t{14}, audio.getBufferLatencyMs(),
                      "latency counts only unplayed frames: 700 frames at 48 kHz");
        ASSERT_FALSE(audio.isFinished(), "a stream at EOF is not finished while audio is queued");

        ASSERT_TRUE(puller.pull(700) == ramp(601, 700), "the second pull continues where the first stopped");
        {
            std::lock_guard<std::mutex> lock(audio.m_buffer_mutex);
            ASSERT_TRUE(audio.m_buffer.empty(), "a fully played queue is emptied");
            ASSERT_EQUALS(size_t{0}, audio.m_buffer_read, "and its cursor starts over");
        }
        ASSERT_TRUE(audio.isFinished(), "EOF with nothing queued is finished");

        const std::vector<AudioSample> silence = puller.pull(10);
        ASSERT_EQUALS(size_t{20}, silence.size(), "an underrun still fills the request");
        ASSERT_TRUE(std::all_of(silence.begin(), silence.end(), [](AudioSample s) { return s == 0; }),
                    "an underrun plays silence");
    }
};

class CompactionTest : public TestCase {
public:
    CompactionTest() : TestCase("Compaction reclaims the played prefix without reordering") {}

protected:
    void runTest() override
    {
        FastFourier fft(512);
        std::mutex player_mutex;
        Audio audio(std::make_unique<ExhaustedStream>(), &fft, &player_mutex, ramp(1, 1000), true);
        Puller puller(audio);
        ASSERT_TRUE(puller.ok(), "a device-less SDL stream can be created");

        ASSERT_TRUE(puller.pull(100) == ramp(1, 100), "the head of the queue plays first");
        {
            std::lock_guard<std::mutex> lock(audio.m_buffer_mutex);
            audio.compactBuffer_unlocked();
            ASSERT_EQUALS(size_t{200}, audio.m_buffer_read,
                          "a prefix shorter than the queue is left alone");
        }

        ASSERT_TRUE(puller.pull(500) == ramp(201, 500), "playback continues across the cursor");
        {
            std::lock_guard<std::mutex> lock(audio.m_buffer_mutex);
            audio.compactBuffer_unlocked();
            ASSERT_EQUALS(size_t{0}, audio.m_buffer_read, "a prefix as long as the queue is reclaimed");
            ASSERT_EQUALS(size_t{800}, audio.m_buffer.size(), "only the unplayed samples remain");
            ASSERT_EQUALS(AudioSample{1201}, audio.m_buffer.front(), "starting with the next one due");

            // What the decoder thread does next: append after compacting.
            std::vector<AudioSample> carry;
            const std::vector<AudioSample> more = ramp(2001, 500);
            Audio::appendWholeFrames(audio.m_buffer, carry, more.data(), more.size(), kChannels);
        }

        ASSERT_TRUE(puller.pull(900) == ramp(1201, 900),
                    "old and newly appended samples play back as one continuous run");
        ASSERT_EQUALS(size_t{0}, buffered(audio), "everything was played");
    }
};

class ResetTest : public TestCase {
public:
    ResetTest() : TestCase("A seek or a stream swap restarts the cursor") {}

protected:
    void runTest() override
    {
        FastFourier fft(512);
        std::mutex player_mutex;
        Audio audio(std::make_unique<ExhaustedStream>(), &fft, &player_mutex, ramp(1, 1000), true);
        Puller puller(audio);
        ASSERT_TRUE(puller.ok(), "a device-less SDL stream can be created");

        ASSERT_TRUE(puller.pull(300) == ramp(1, 300), "part of the queue is played");
        audio.resetBuffer();
        {
            std::lock_guard<std::mutex> lock(audio.m_buffer_mutex);
            ASSERT_TRUE(audio.m_buffer.empty(), "a seek empties the queue");
            ASSERT_EQUALS(size_t{0}, audio.m_buffer_read, "and resets the cursor");
        }
        ASSERT_EQUALS(uint64_t{0}, audio.getBufferLatencyMs(), "nothing is queued after a seek");

        const std::vector<AudioSample> after_seek = puller.pull(10);
        ASSERT_TRUE(std::all_of(after_seek.begin(), after_seek.end(), [](AudioSample s) { return s == 0; }),
                    "nothing from before the seek plays after it");

        audio.setStream(std::make_unique<ExhaustedStream>(), ramp(5001, 200), true);
        ASSERT_TRUE(puller.pull(200) == ramp(5001, 200),
                    "a swapped-in stream's primed audio plays from its first sample");

        // A swap in the middle of a partly played queue must not inherit its
        // cursor either.
        audio.setStream(std::make_unique<ExhaustedStream>(), ramp(9001, 400), true);
        ASSERT_TRUE(puller.pull(100) == ramp(9001, 100), "the next queue is partly played");
        audio.setStream(std::make_unique<ExhaustedStream>(), ramp(7001, 50), true);
        ASSERT_EQUALS(size_t{100}, buffered(audio), "the new queue is exactly the primed audio");
        ASSERT_TRUE(puller.pull(50) == ramp(7001, 50), "and plays from its start");
    }
};

class DecoderContinuityTest : public TestCase {
public:
    DecoderContinuityTest() : TestCase("Audio fed by the decoder thread plays back without gaps or repeats") {}

protected:
    void runTest() override
    {
        FastFourier fft(512);
        std::mutex player_mutex;
        Audio audio(std::make_unique<CountingStream>(), &fft, &player_mutex);
        Puller puller(audio);
        ASSERT_TRUE(puller.ok(), "a device-less SDL stream can be created");

        // Five seconds in 10 ms pulls. The decoder refills between pulls and
        // compacts as it goes; waiting for enough to be queued keeps underrun
        // silence out of the comparison.
        constexpr size_t kPullFrames = 480;
        constexpr size_t kPulls = 500;
        AudioSample expected = 1;
        for (size_t pull = 0; pull < kPulls; ++pull) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (buffered(audio) < kPullFrames * kChannels &&
                   std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            ASSERT_TRUE(buffered(audio) >= kPullFrames * kChannels,
                        "the decoder thread keeps the queue filled");

            const std::vector<AudioSample> out = puller.pull(kPullFrames);
            ASSERT_EQUALS(kPullFrames * kChannels, out.size(), "a full pull comes back");
            if (out != ramp(expected, kPullFrames)) {
                std::ostringstream msg;
                msg << "pull " << pull << " should start at " << expected << " but starts at "
                    << out.front();
                ASSERT_TRUE(false, msg.str());
            }
            expected += static_cast<AudioSample>(kPullFrames * kChannels);
        }

        // The decoder appends only below the high water mark (a quarter of a
        // second) and compacts first whenever the played prefix is at least as
        // long as the queue. So the vector never exceeds two high water marks
        // plus one read (1024 frames). Without compaction, it would hold all
        // five seconds.
        const size_t high_water = static_cast<size_t>(kRate) * kChannels / 4;
        const size_t one_read = 1024 * static_cast<size_t>(kChannels);
        std::lock_guard<std::mutex> lock(audio.m_buffer_mutex);
        ASSERT_TRUE(audio.m_buffer.size() <= 2 * high_water + one_read,
                    "compaction keeps the vector within twice the queue");
    }
};

} // namespace

int test_audio_buffer_cursor_main()
{
    // No real device is needed, and a test must not open the desktop's.
    setenv("SDL_AUDIO_DRIVER", "dummy", 0);
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        std::cout << "SKIP: SDL audio initialization failed: " << SDL_GetError() << std::endl;
        return 77;
    }

    TestSuite suite("Audio Buffer Cursor Tests");
    suite.addTest(std::make_unique<CursorConsumptionTest>());
    suite.addTest(std::make_unique<CompactionTest>());
    suite.addTest(std::make_unique<ResetTest>());
    suite.addTest(std::make_unique<DecoderContinuityTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    // Count every test that did not pass. getFailureCount() skips tests that
    // ended in an unexpected exception, such as a device that failed to open.
    const int not_passed = static_cast<int>(results.size()) - suite.getPassedCount(results);
    SDL_Quit();
    return not_passed;
}
