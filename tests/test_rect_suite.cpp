/*
 * test_rect_suite.cpp - rect tests, in one program
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
 * @TEST_NAME: Rect Tests
 * @TEST_AUTHOR: Kirn Gill II <segin2005@gmail.com>
 * @TEST_CREATED: 2026-09-18
 * @TEST_TIMEOUT: 120000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_TAGS: rect,core,geometry,unit
 * @TEST_METADATA_END
 */

#include <cstdio>

int test_rect_area_validation_main();
int test_rect_backward_compatibility_main();
int test_rect_centering_main();
int test_rect_centering_overflow_main();
int test_rect_containment_main();
int test_rect_expansion_main();
int test_rect_integration_main();
int test_rect_intersection_main();
int test_rect_modern_cpp_main();
int test_rect_normalization_main();
int test_rect_pair_variants_main();
int test_rect_performance_main();
int test_rect_transformation_main();
int test_rect_union_main();
int test_rect_unit_main();
int test_widget_rect_integration_main();

namespace {

struct Entry {
    const char* name;
    int (*run)();
};

const Entry kEntries[] = {
    {"test_rect_area_validation",         test_rect_area_validation_main},
    {"test_rect_backward_compatibility",  test_rect_backward_compatibility_main},
    {"test_rect_centering",               test_rect_centering_main},
    {"test_rect_centering_overflow",      test_rect_centering_overflow_main},
    {"test_rect_containment",             test_rect_containment_main},
    {"test_rect_expansion",               test_rect_expansion_main},
    {"test_rect_integration",             test_rect_integration_main},
    {"test_rect_intersection",            test_rect_intersection_main},
    {"test_rect_modern_cpp",              test_rect_modern_cpp_main},
    {"test_rect_normalization",           test_rect_normalization_main},
    {"test_rect_pair_variants",           test_rect_pair_variants_main},
    {"test_rect_performance",             test_rect_performance_main},
    {"test_rect_transformation",          test_rect_transformation_main},
    {"test_rect_union",                   test_rect_union_main},
    {"test_rect_unit",                    test_rect_unit_main},
    {"test_widget_rect_integration",      test_widget_rect_integration_main},
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
    std::printf("\n=== Rect Tests: %zu files, %d failed test(s) in %d file(s), %d skipped ===\n",
                sizeof(kEntries) / sizeof(kEntries[0]), failures, failed_files, skipped_files);
    return failures;
}
