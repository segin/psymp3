/*
 * test_native_flac_suite.cpp - native flac tests, in one program
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
 * @TEST_NAME: Native FLAC Tests
 * @TEST_AUTHOR: Kirn Gill II <segin2005@gmail.com>
 * @TEST_CREATED: 2026-09-18
 * @TEST_TIMEOUT: 120000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_TAGS: flac,native,codec
 * @TEST_METADATA_END
 */

#include <cstdio>

int test_native_flac_containers_main();
int test_native_flac_edge_cases_main();
int test_native_flac_memory_usage_main();
int test_native_flac_performance_benchmark_main();
int test_native_flac_real_files_main();
int test_native_flac_threading_main();

namespace {

struct Entry {
    const char* name;
    int (*run)();
};

const Entry kEntries[] = {
    {"test_native_flac_containers",             test_native_flac_containers_main},
    {"test_native_flac_edge_cases",             test_native_flac_edge_cases_main},
    {"test_native_flac_memory_usage",           test_native_flac_memory_usage_main},
    {"test_native_flac_performance_benchmark",  test_native_flac_performance_benchmark_main},
    {"test_native_flac_real_files",             test_native_flac_real_files_main},
    {"test_native_flac_threading",              test_native_flac_threading_main},
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

    std::printf("\n=== Native FLAC Tests: %zu files, %d failed test(s) in %d file(s), %d skipped ===\n",
                sizeof(kEntries) / sizeof(kEntries[0]), failures, failed_files, skipped_files);
    return failures;
}
