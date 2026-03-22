with open("tests/test_tag_picture_properties.cpp", "r") as f:
    content = f.read()

target = """        try {
            test.run();
            if (test.passed()) {
                passed++;
                std::cout << "  " << test.name() << ": PASSED\\n";
            } else {
                failed++;
                std::cout << "  " << test.name() << ": FAILED - " << test.failureMessage() << "\\n";
            }
        } catch (const std::exception& e) {
            failed++;
            std::cout << "  " << test.name() << ": EXCEPTION - " << e.what() << "\\n";
        }"""

replacement = """        try {
            auto info = test.run();
            if (info.result == TestFramework::TestResult::PASSED) {
                passed++;
                std::cout << "  " << info.name << ": PASSED\\n";
            } else {
                failed++;
                std::cout << "  " << info.name << ": FAILED - " << info.failure_message << "\\n";
            }
        } catch (const std::exception& e) {
            failed++;
            std::cout << "  Unknown: EXCEPTION - " << e.what() << "\\n";
        }"""

content = content.replace(target, replacement)
with open("tests/test_tag_picture_properties.cpp", "w") as f:
    f.write(content)
