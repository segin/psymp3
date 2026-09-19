/*
 * test_playlist_suite.cpp - playlist and track tests, in one program
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Each of these tests keeps its own file and its own suite; what they share
 * is this program's link, which without them numbered one per test and cost
 * more than every one of them takes to run. Every entry returns its failure
 * count, as its main did.
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Playlist and Track Tests
 * @TEST_AUTHOR: Kirn Gill II <segin2005@gmail.com>
 * @TEST_CREATED: 2026-09-18
 * @TEST_TIMEOUT: 120000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_TAGS: playlist,track
 * @TEST_METADATA_END
 */

#include <cstdio>

int test_playlist_block_edits_main();
int test_playlist_load_main();
int test_playlist_shuffle_main();
int test_track_utils_main();

namespace {

struct Entry {
    const char* name;
    int (*run)();
};

const Entry kEntries[] = {
    {"test_playlist_block_edits",  test_playlist_block_edits_main},
    {"test_playlist_load",         test_playlist_load_main},
    {"test_playlist_shuffle",      test_playlist_shuffle_main},
    {"test_track_utils",           test_track_utils_main},
};

} // namespace

int main()
{
    int failures = 0;
    int failed_files = 0;
    int skipped_files = 0;
    for (const Entry& entry : kEntries) {
        // The name goes out before the tests run, so that a crash says which
        // file was in the middle of it.
        std::printf("\n=== %s ===\n", entry.name);
        std::fflush(stdout);
        const int failed = entry.run();
        // 77 is the skip status, which a test returns when what it needs is
        // not there -- a fixture, usually. It is not a failure, and it is not
        // a count of them either.
        if (failed == 77) {
            ++skipped_files;
            std::printf("--- %s skipped\n", entry.name);
        } else if (failed > 0) {
            ++failed_files;
            failures += failed;
            std::printf("--- %s reported %d failure(s)\n", entry.name, failed);
        }
    }

    // A program every one of whose tests skipped has skipped.
    if (skipped_files == static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]))) {
        std::printf("\n=== all %d file(s) skipped ===\n", skipped_files);
        return 77;
    }
    std::printf("\n=== Playlist and Track Tests: %zu files, %d failed test(s) in %d file(s), %d skipped ===\n",
                sizeof(kEntries) / sizeof(kEntries[0]), failures, failed_files, skipped_files);
    return failures;
}
