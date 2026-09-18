/*
 * test_flac_suite.cpp - flac tests, in one program
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
 * @TEST_NAME: FLAC Tests
 * @TEST_AUTHOR: Kirn Gill II <segin2005@gmail.com>
 * @TEST_CREATED: 2026-09-18
 * @TEST_TIMEOUT: 120000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_TAGS: flac,codec,demuxer,rfc9639
 * @TEST_METADATA_END
 */

#include <cstdio>

int test_flac_audio_output_main();
int test_flac_audio_output_debug_main();
int test_flac_backward_compatibility_validation_main();
int test_flac_binary_search_limitations_main();
int test_flac_bisection_real_files_main();
int test_flac_bit_depth_validation_simple_main();
int test_flac_codec_compatibility_main();
int test_flac_codec_container_agnostic_main();
int test_flac_codec_demuxer_integration_main();
int test_flac_codec_error_handling_main();
int test_flac_codec_integration_main();
int test_flac_codec_minimal_container_agnostic_main();
int test_flac_codec_multithreaded_main();
int test_flac_codec_performance_comprehensive_main();
int test_flac_codec_simple_debug_main();
int test_flac_codec_unit_comprehensive_main();
int test_flac_codec_unit_minimal_main();
int test_flac_compatibility_integration_main();
int test_flac_comprehensive_validation_main();
int test_flac_conditional_compilation_main();
int test_flac_crc_validation_main();
int test_flac_demuxer_compatibility_main();
int test_flac_demuxer_integration_comprehensive_main();
int test_flac_demuxer_performance_main();
int test_flac_demuxer_real_files_main();
int test_flac_demuxer_rfc9639_regressions_main();
int test_flac_demuxer_simple_main();
int test_flac_demuxer_thread_safety_main();
int test_flac_demuxer_threading_main();
int test_flac_demuxer_unit_comprehensive_main();
int test_flac_demuxer_unit_fixed_main();
int test_flac_diagnostic_main();
int test_flac_index_based_seeking_main();
int test_flac_mini_player_main();
int test_flac_performance_benchmarking_main();
int test_flac_performance_optimization_main();
int test_flac_performance_with_real_files_main();
int test_flac_quality_simple_main();
int test_flac_real_file_main();
int test_flac_rfc9639_error_handling_main();
int test_flac_rfc_bit_depth_validation_main();
int test_flac_rfc_block_size_sample_rate_validation_main();
int test_flac_rfc_compliance_main();
int test_flac_rfc_entropy_coding_validation_main();
int test_flac_rfc_subframe_validation_main();
int test_flac_sample_format_rfc9639_compliance_main();
int test_flac_security_validation_main();
int test_flac_seek_landing_main();
int test_flac_seeking_crash_main();
int test_flac_test_data_validation_main();
int test_flac_thread_safety_validation_main();
int test_flac_variable_block_size_main();

namespace {

struct Entry {
    const char* name;
    int (*run)();
};

const Entry kEntries[] = {
    {"test_flac_audio_output",                           test_flac_audio_output_main},
    {"test_flac_audio_output_debug",                     test_flac_audio_output_debug_main},
    {"test_flac_backward_compatibility_validation",      test_flac_backward_compatibility_validation_main},
    {"test_flac_binary_search_limitations",              test_flac_binary_search_limitations_main},
    {"test_flac_bisection_real_files",                   test_flac_bisection_real_files_main},
    {"test_flac_bit_depth_validation_simple",            test_flac_bit_depth_validation_simple_main},
    {"test_flac_codec_compatibility",                    test_flac_codec_compatibility_main},
    {"test_flac_codec_container_agnostic",               test_flac_codec_container_agnostic_main},
    {"test_flac_codec_demuxer_integration",              test_flac_codec_demuxer_integration_main},
    {"test_flac_codec_error_handling",                   test_flac_codec_error_handling_main},
    {"test_flac_codec_integration",                      test_flac_codec_integration_main},
    {"test_flac_codec_minimal_container_agnostic",       test_flac_codec_minimal_container_agnostic_main},
    {"test_flac_codec_multithreaded",                    test_flac_codec_multithreaded_main},
    {"test_flac_codec_performance_comprehensive",        test_flac_codec_performance_comprehensive_main},
    {"test_flac_codec_simple_debug",                     test_flac_codec_simple_debug_main},
    {"test_flac_codec_unit_comprehensive",               test_flac_codec_unit_comprehensive_main},
    {"test_flac_codec_unit_minimal",                     test_flac_codec_unit_minimal_main},
    {"test_flac_compatibility_integration",              test_flac_compatibility_integration_main},
    {"test_flac_comprehensive_validation",               test_flac_comprehensive_validation_main},
    {"test_flac_conditional_compilation",                test_flac_conditional_compilation_main},
    {"test_flac_crc_validation",                         test_flac_crc_validation_main},
    {"test_flac_demuxer_compatibility",                  test_flac_demuxer_compatibility_main},
    {"test_flac_demuxer_integration_comprehensive",      test_flac_demuxer_integration_comprehensive_main},
    {"test_flac_demuxer_performance",                    test_flac_demuxer_performance_main},
    {"test_flac_demuxer_real_files",                     test_flac_demuxer_real_files_main},
    {"test_flac_demuxer_rfc9639_regressions",            test_flac_demuxer_rfc9639_regressions_main},
    {"test_flac_demuxer_simple",                         test_flac_demuxer_simple_main},
    {"test_flac_demuxer_thread_safety",                  test_flac_demuxer_thread_safety_main},
    {"test_flac_demuxer_threading",                      test_flac_demuxer_threading_main},
    {"test_flac_demuxer_unit_comprehensive",             test_flac_demuxer_unit_comprehensive_main},
    {"test_flac_demuxer_unit_fixed",                     test_flac_demuxer_unit_fixed_main},
    {"test_flac_diagnostic",                             test_flac_diagnostic_main},
    {"test_flac_index_based_seeking",                    test_flac_index_based_seeking_main},
    {"test_flac_mini_player",                            test_flac_mini_player_main},
    {"test_flac_performance_benchmarking",               test_flac_performance_benchmarking_main},
    {"test_flac_performance_optimization",               test_flac_performance_optimization_main},
    {"test_flac_performance_with_real_files",            test_flac_performance_with_real_files_main},
    {"test_flac_quality_simple",                         test_flac_quality_simple_main},
    {"test_flac_real_file",                              test_flac_real_file_main},
    {"test_flac_rfc9639_error_handling",                 test_flac_rfc9639_error_handling_main},
    {"test_flac_rfc_bit_depth_validation",               test_flac_rfc_bit_depth_validation_main},
    {"test_flac_rfc_block_size_sample_rate_validation",  test_flac_rfc_block_size_sample_rate_validation_main},
    {"test_flac_rfc_compliance",                         test_flac_rfc_compliance_main},
    {"test_flac_rfc_entropy_coding_validation",          test_flac_rfc_entropy_coding_validation_main},
    {"test_flac_rfc_subframe_validation",                test_flac_rfc_subframe_validation_main},
    {"test_flac_sample_format_rfc9639_compliance",       test_flac_sample_format_rfc9639_compliance_main},
    {"test_flac_security_validation",                    test_flac_security_validation_main},
    {"test_flac_seek_landing",                           test_flac_seek_landing_main},
    {"test_flac_seeking_crash",                          test_flac_seeking_crash_main},
    {"test_flac_test_data_validation",                   test_flac_test_data_validation_main},
    {"test_flac_thread_safety_validation",               test_flac_thread_safety_validation_main},
    {"test_flac_variable_block_size",                    test_flac_variable_block_size_main},
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

    std::printf("\n=== FLAC Tests: %zu files, %d failed test(s) in %d file(s), %d skipped ===\n",
                sizeof(kEntries) / sizeof(kEntries[0]), failures, failed_files, skipped_files);
    return failures;
}
