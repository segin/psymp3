/*
 * test_flac_parser_suite.cpp - flac parser tests, in one program
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Each of these tests keeps its own file and its own suite; what they share
 * is this program's link. The halves of this pair are built under different
 * conditions, so each is declared and listed only where it is compiled.
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: FLAC Parser Tests
 * @TEST_AUTHOR: Kirn Gill II <segin2005@gmail.com>
 * @TEST_CREATED: 2026-09-19
 * @TEST_TIMEOUT: 120000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_TAGS: flac,parser,ogg
 * @TEST_METADATA_END
 */

#include "config.h"

#include <cstdio>

#ifdef HAVE_NATIVE_FLAC
int test_flac_parse_coded_number_main();
#endif
#ifdef HAVE_OGGDEMUXER
int test_flac_parser_main();
#endif

namespace {

struct Entry {
    const char* name;
    int (*run)();
};

const Entry kEntries[] = {
#ifdef HAVE_NATIVE_FLAC
    {"test_flac_parse_coded_number", test_flac_parse_coded_number_main},
#endif
#ifdef HAVE_OGGDEMUXER
    {"test_flac_parser",            test_flac_parser_main},
#endif
};

} // namespace

int main()
{
    int failures = 0;
    int failed_files = 0;
    int skipped_files = 0;
    for (const Entry& entry : kEntries) {
        std::printf("\n=== %s ===\n", entry.name);
        std::fflush(stdout);
        const int failed = entry.run();
        if (failed == 77) {
            ++skipped_files;
            std::printf("--- %s skipped\n", entry.name);
        } else if (failed > 0) {
            ++failed_files;
            failures += failed;
            std::printf("--- %s reported %d failure(s)\n", entry.name, failed);
        }
    }

    if (skipped_files == static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]))) {
        std::printf("\n=== all %d file(s) skipped ===\n", skipped_files);
        return 77;
    }

    std::printf("\n=== FLAC Parser Tests: %zu files, %d failed test(s) in %d file(s), %d skipped ===\n",
                sizeof(kEntries) / sizeof(kEntries[0]), failures, failed_files, skipped_files);
    return failures;
}
