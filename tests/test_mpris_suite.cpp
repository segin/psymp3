/*
 * test_mpris_suite.cpp - mpris tests, in one program
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
 * @TEST_NAME: MPRIS Tests
 * @TEST_AUTHOR: Kirn Gill II <segin2005@gmail.com>
 * @TEST_CREATED: 2026-09-18
 * @TEST_TIMEOUT: 600000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_TAGS: mpris,dbus,integration
 * @TEST_METADATA_END
 */

#include <cstdio>

int test_mpris_concurrent_clients_main();
int test_mpris_error_handling_main();
int test_mpris_error_integration_main();
int test_mpris_logger_basic_main();
int test_mpris_logger_dbus_tracing_main();
int test_mpris_logger_performance_main();
int test_mpris_manager_basic_integration_main();
int test_mpris_manager_comprehensive_main();
int test_mpris_manager_integration_main();
int test_mpris_manager_minimal_main();
int test_mpris_manager_simple_main();
int test_mpris_memory_validation_main();
int test_mpris_mock_framework_main();
int test_mpris_performance_profiler_main();
int test_mpris_reconnection_behavior_main();
int test_mpris_regression_validation_main();
int test_mpris_spec_compliance_main();
int test_mpris_stress_testing_main();
int test_mpris_types_main();

namespace {

struct Entry {
    const char* name;
    int (*run)();
};

const Entry kEntries[] = {
    {"test_mpris_concurrent_clients",         test_mpris_concurrent_clients_main},
    {"test_mpris_error_handling",             test_mpris_error_handling_main},
    {"test_mpris_error_integration",          test_mpris_error_integration_main},
    {"test_mpris_logger_basic",               test_mpris_logger_basic_main},
    {"test_mpris_logger_dbus_tracing",        test_mpris_logger_dbus_tracing_main},
    {"test_mpris_logger_performance",         test_mpris_logger_performance_main},
    {"test_mpris_manager_basic_integration",  test_mpris_manager_basic_integration_main},
    {"test_mpris_manager_comprehensive",      test_mpris_manager_comprehensive_main},
    {"test_mpris_manager_integration",        test_mpris_manager_integration_main},
    {"test_mpris_manager_minimal",            test_mpris_manager_minimal_main},
    {"test_mpris_manager_simple",             test_mpris_manager_simple_main},
    {"test_mpris_memory_validation",          test_mpris_memory_validation_main},
    {"test_mpris_mock_framework",             test_mpris_mock_framework_main},
    {"test_mpris_performance_profiler",       test_mpris_performance_profiler_main},
    {"test_mpris_regression_validation",      test_mpris_regression_validation_main},
    {"test_mpris_spec_compliance",            test_mpris_spec_compliance_main},
    {"test_mpris_stress_testing",             test_mpris_stress_testing_main},
    {"test_mpris_types",                      test_mpris_types_main},
    // Last, and it has to be: this one runs a D-Bus session of its own and
    // kills it to watch the manager reconnect. libdbus caches the session
    // connection process-wide, so whatever runs after it reaches for a bus
    // that is no longer there -- putting DBUS_SESSION_BUS_ADDRESS back is
    // not enough to clear that cache.
    {"test_mpris_reconnection_behavior",      test_mpris_reconnection_behavior_main},
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
    std::printf("\n=== MPRIS Tests: %zu files, %d failed test(s) in %d file(s), %d skipped ===\n",
                sizeof(kEntries) / sizeof(kEntries[0]), failures, failed_files, skipped_files);
    return failures;
}
