/*
 * test_metadata_template.h - Template for test metadata annotations
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef TEST_METADATA_TEMPLATE_H
#define TEST_METADATA_TEMPLATE_H

/*
 * TEST METADATA ANNOTATION FORMAT
 * 
 * Each test file should include metadata annotations in the following format:
 * 
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: [Human readable test name]
 * @TEST_DESCRIPTION: [Brief description of what this test validates]
 * @TEST_REQUIREMENTS: [Comma-separated list of requirement IDs from requirements.md]
 * @TEST_AUTHOR: [Author name and email]
 * @TEST_CREATED: [Creation date]
 * @TEST_TIMEOUT: [Maximum execution time in milliseconds, default: 5000]
 * @TEST_PARALLEL_SAFE: [true/false - can this test run in parallel with others]
 * @TEST_DEPENDENCIES: [Comma-separated list of required object files or libraries]
 * @TEST_TAGS: [Comma-separated list of tags for categorization]
 * @TEST_METADATA_END
 * 
 * Example:
 * @TEST_METADATA_BEGIN
 * @TEST_NAME: Rectangle Area Validation Tests
 * @TEST_DESCRIPTION: Tests area calculation, isEmpty, and isValid methods for Rect class
 * @TEST_REQUIREMENTS: 6.1, 6.3, 6.6
 * @TEST_AUTHOR: Kirn Gill <segin2005@gmail.com>
 * @TEST_CREATED: 2025-01-19
 * @TEST_TIMEOUT: 3000
 * @TEST_PARALLEL_SAFE: true
 * @TEST_DEPENDENCIES: rect.o
 * @TEST_TAGS: rect, area, validation, basic
 * @TEST_METADATA_END
 */

#endif // TEST_METADATA_TEMPLATE_H