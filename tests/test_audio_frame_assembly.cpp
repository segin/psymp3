/*
 * test_audio_frame_assembly.cpp - Decoded PCM stays whole-frame and gapless
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The decoder thread reads from a stream in fixed-size requests and queues
 * the result for the audio callback. A request that is not a whole number of
 * frames ends inside one; the queue must neither drop those samples (a gap
 * and a channel rotation after every read) nor queue them unfinished.
 */

#include "psymp3.h"
#include "test_framework.h"

using namespace TestFramework;

namespace {

/// Sample value identifying frame @p frame, channel @p channel.
AudioSample tag(size_t frame, size_t channel)
{
    return static_cast<AudioSample>((frame << 4) | channel);
}

/// A stream of @p frames frames, interleaved.
std::vector<AudioSample> source(size_t frames, size_t channels)
{
    std::vector<AudioSample> v;
    v.reserve(frames * channels);
    for (size_t f = 0; f < frames; ++f) {
        for (size_t c = 0; c < channels; ++c) {
            v.push_back(tag(f, c));
        }
    }
    return v;
}

/// Feed @p pcm through appendWholeFrames in reads of the given sizes
/// (cycled), then check the queue holds exactly the stream's frames in order.
void assemble(size_t channels, std::initializer_list<size_t> read_sizes, size_t frames,
              const std::string& what)
{
    const auto pcm = source(frames, channels);
    std::vector<AudioSample> queue, carry;
    size_t offset = 0;
    std::vector<size_t> sizes(read_sizes);
    for (size_t i = 0; offset < pcm.size(); ++i) {
        const size_t n = std::min(sizes[i % sizes.size()], pcm.size() - offset);
        const size_t before = queue.size();
        Audio::appendWholeFrames(queue, carry, pcm.data() + offset, n, channels);
        offset += n;
        ASSERT_TRUE(queue.size() % channels == 0, what + ": queue grows by whole frames only");
        ASSERT_TRUE(carry.size() < channels, what + ": at most a partial frame is held");
        (void)before;
    }
    ASSERT_TRUE(carry.empty(), what + ": a stream of whole frames leaves nothing behind");
    ASSERT_TRUE(queue == pcm, what + ": every sample queued, in order, on the right channel");
}

class DecoderReadSizeTest : public TestCase {
public:
    DecoderReadSizeTest() : TestCase("Reads that end inside a frame lose nothing") {}

protected:
    void runTest() override
    {
        // The decoder thread's old request, 4096 samples, ends 4 samples into
        // a 5.1 frame on every read.
        assemble(6, { 4096 }, 5000, "5.1 in 4096-sample reads");
        assemble(3, { 4096 }, 5000, "3 channels in 4096-sample reads");
        assemble(7, { 4096 }, 5000, "7 channels in 4096-sample reads");
        assemble(2, { 4096 }, 5000, "stereo in 4096-sample reads");
    }
};

class IrregularReadTest : public TestCase {
public:
    IrregularReadTest() : TestCase("Short and odd-sized reads reassemble into whole frames") {}

protected:
    void runTest() override
    {
        assemble(6, { 7, 1, 13, 5, 4095, 2 }, 3000, "5.1 in irregular reads");
        assemble(8, { 3, 3, 3, 1 }, 500, "7.1 in reads smaller than a frame");
        assemble(1, { 5, 1 }, 500, "mono");
    }
};

class CarryAcrossCallsTest : public TestCase {
public:
    CarryAcrossCallsTest() : TestCase("A partial frame waits until the next read completes it") {}

protected:
    void runTest() override
    {
        const auto pcm = source(2, 6);
        std::vector<AudioSample> queue, carry;
        Audio::appendWholeFrames(queue, carry, pcm.data(), 4, 6);
        ASSERT_TRUE(queue.empty() && carry.size() == 4, "four samples of frame 0 held back");
        Audio::appendWholeFrames(queue, carry, pcm.data() + 4, 3, 6);
        ASSERT_TRUE(queue.size() == 6 && carry.size() == 1, "frame 0 completed, one sample of frame 1 held");
        ASSERT_TRUE(queue[4] == tag(0, 4) && queue[5] == tag(0, 5), "completed frame keeps its channel order");
        Audio::appendWholeFrames(queue, carry, pcm.data() + 7, 5, 6);
        ASSERT_TRUE(queue == pcm && carry.empty(), "both frames queued");
    }
};

} // namespace

int main()
{
    TestSuite suite("Audio Frame Assembly Tests");
    suite.addTest(std::make_unique<DecoderReadSizeTest>());
    suite.addTest(std::make_unique<IrregularReadTest>());
    suite.addTest(std::make_unique<CarryAcrossCallsTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
