/*
 * test_matroska_suite.cpp - the Matroska and WebM demuxer tests, in one program
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * The pieces MatroskaDemuxer sits on are tested one file each -- the EBML
 * reader, the segment parser, block parsing and lacing, the cue index,
 * seeking, and the Tags reader -- and they share this program's link. Every
 * entry returns its failure count, as its main did.
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Matroska / WebM Tests
 * @TEST_DESCRIPTION: EBML element grammar, Segment and Tracks parsing, block
 *   lacing, the cue index, seeking, and Tags
 * @TEST_AUTHOR: Kirn Gill II <segin2005@gmail.com>
 * @TEST_CREATED: 2026-09-18
 * @TEST_TIMEOUT: 120000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_TAGS: matroska, webm, demuxer, unit
 * @TEST_METADATA_END
 */

#include <cstdio>

int test_ebml_reader_main();
int test_matroska_segment_main();
int test_matroska_block_main();
int test_matroska_cues_main();
int test_matroska_seek_main();
int test_matroska_tags_main();

namespace {

struct Entry {
    const char* name;
    int (*run)();
};

const Entry kEntries[] = {
    {"test_ebml_reader",        test_ebml_reader_main},
    {"test_matroska_segment",   test_matroska_segment_main},
    {"test_matroska_block",     test_matroska_block_main},
    {"test_matroska_cues",      test_matroska_cues_main},
    {"test_matroska_seek",      test_matroska_seek_main},
    {"test_matroska_tags",      test_matroska_tags_main},
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
    std::printf("\n=== Matroska: %zu files, %d failed test(s) in %d file(s), %d skipped ===\n",
                sizeof(kEntries) / sizeof(kEntries[0]), failures, failed_files, skipped_files);
    return failures;
}
