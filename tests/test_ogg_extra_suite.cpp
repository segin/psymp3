/*
 * test_ogg_extra_suite.cpp - ogg demuxer behaviour tests, in one program
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
 * @TEST_NAME: Ogg Demuxer Behaviour Tests
 * @TEST_AUTHOR: Kirn Gill II <segin2005@gmail.com>
 * @TEST_CREATED: 2026-09-18
 * @TEST_TIMEOUT: 120000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_TAGS: ogg,demuxer,integration
 * @TEST_METADATA_END
 */

#include <cstdio>

int test_ogg_aggressive_main();
int test_ogg_error_handling_main();
int test_ogg_integration_main();
int test_ogg_seeking_algorithms_main();

namespace {

struct Entry {
    const char* name;
    int (*run)();
};

const Entry kEntries[] = {
    {"test_ogg_aggressive",          test_ogg_aggressive_main},
    {"test_ogg_error_handling",      test_ogg_error_handling_main},
    {"test_ogg_integration",         test_ogg_integration_main},
    {"test_ogg_seeking_algorithms",  test_ogg_seeking_algorithms_main},
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
        if (failed > 0) {
            ++failed_files;
            failures += failed;
        }
    }

    std::printf("\n=== Ogg Demuxer Behaviour Tests: %zu files, %d failed test(s) in %d file(s), %d skipped ===\n",
                sizeof(kEntries) / sizeof(kEntries[0]), failures, failed_files, skipped_files);
    return failures;
}
