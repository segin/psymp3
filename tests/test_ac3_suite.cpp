/*
 * test_ac3_suite.cpp - the AC-3 and E-AC-3 unit tests, in one program
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Each of these tests used to be a program of its own, and each one linked
 * the whole static stack -- around 150 MB of binary and ten seconds of
 * compiler and linker for a file that runs in milliseconds. The tests
 * themselves are unchanged and still keep their own suites and their own
 * files: what is shared is the link. Every entry returns its failure count,
 * as its main did.
 */

/*
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: AC-3 / E-AC-3 Unit Tests
 * @TEST_DESCRIPTION: Frame headers, exponents, bit allocation, mantissas, the
 *   inverse transform, and the E-AC-3 tools: SPX, TPNP, AHT, ECPL, DRC and
 *   downmixing
 * @TEST_AUTHOR: Kirn Gill II <segin2005@gmail.com>
 * @TEST_CREATED: 2026-09-18
 * @TEST_TIMEOUT: 120000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_TAGS: ac3, eac3, codec, unit
 * @TEST_METADATA_END
 */

#include <cstdio>

int test_ac3_frame_header_main();
int test_ac3_exponents_main();
int test_ac3_bit_allocation_main();
int test_ac3_mantissas_main();
int test_ac3_transform_main();
int test_eac3_frame_main();
int test_eac3_spx_main();
int test_eac3_tpnp_main();
int test_eac3_aht_main();
int test_eac3_ecpl_main();
int test_ac3_drc_main();
int test_ac3_downmix_main();

namespace {

struct Entry {
    const char* name;
    int (*run)();
};

const Entry kEntries[] = {
    {"test_ac3_frame_header",   test_ac3_frame_header_main},
    {"test_ac3_exponents",      test_ac3_exponents_main},
    {"test_ac3_bit_allocation", test_ac3_bit_allocation_main},
    {"test_ac3_mantissas",      test_ac3_mantissas_main},
    {"test_ac3_transform",      test_ac3_transform_main},
    {"test_eac3_frame",         test_eac3_frame_main},
    {"test_eac3_spx",           test_eac3_spx_main},
    {"test_eac3_tpnp",          test_eac3_tpnp_main},
    {"test_eac3_aht",           test_eac3_aht_main},
    {"test_eac3_ecpl",          test_eac3_ecpl_main},
    {"test_ac3_drc",            test_ac3_drc_main},
    {"test_ac3_downmix",        test_ac3_downmix_main},
};

} // namespace

int main()
{
    int failures = 0;
    int failed_files = 0;
    int skipped_files = 0;
    for (const Entry& entry : kEntries) {
        // The name is printed first so that a crash says which file was in
        // the middle of running, which a program per test used to say for
        // free.
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
        }
    }

    // A program every one of whose tests skipped has skipped.
    if (skipped_files == static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]))) {
        std::printf("\n=== all %d file(s) skipped ===\n", skipped_files);
        return 77;
    }

    std::printf("\n=== AC-3 / E-AC-3: %zu files, %d failed test(s) in %d file(s), %d skipped ===\n",
                sizeof(kEntries) / sizeof(kEntries[0]), failures, failed_files, skipped_files);
    return failures;
}
