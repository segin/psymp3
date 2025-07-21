/**
 * Integration tests for the test harness itself
 * Tests basic functionality and validates the test suite
 */

#include "test_framework.h"
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace TestFramework;

class TestHarnessValidationTest : public TestCase {
public:
    TestHarnessValidationTest() : TestCase("TestHarnessValidation") {}

protected:
    void runTest() override {
        testBasicFunctionality();
        testIndividualTestExecution();
        testBuildSystemIntegration();
        testFileSystemValidation();
    }

private:
    void testBasicFunctionality() {
        std::cout << "Testing basic test harness functionality..." << std::endl;
        
        // Test that test harness executable exists
        ASSERT_TRUE(std::filesystem::exists("./test-harness"), "Test harness executable should exist");
        
        // Test that test harness shows help
        std::string help_output = executeCommand("./test-harness --help");
        ASSERT_TRUE(help_output.find("Usage:") != std::string::npos, "Help should show usage information");
        ASSERT_TRUE(help_output.find("--verbose") != std::string::npos, "Help should show verbose option");
        ASSERT_TRUE(help_output.find("--filter") != std::string::npos, "Help should show filter option");
        
        // Test that test harness can list tests
        std::string list_output = executeCommand("./test-harness --list");
        ASSERT_TRUE(list_output.find("rect") != std::string::npos, "Should list rect tests");
    }
    
    void testIndividualTestExecution() {
        std::cout << "Testing individual test execution..." << std::endl;
        
        // Test that individual tests exist and are properly built
        std::vector<std::string> expected_tests = {
            "test_rect_containment",
            "test_rect_intersection", 
            "test_rect_union",
            "test_rect_area_validation"
        };
        
        for (const auto& test_name : expected_tests) {
            if (std::filesystem::exists("./" + test_name)) {
                std::cout << "  Found test executable: " << test_name << std::endl;
                ASSERT_TRUE(std::filesystem::is_regular_file("./" + test_name), 
                           "Test should be a regular file: " + test_name);
            }
        }
        
        // Test that make check target uses the test harness
        std::string make_check = executeCommand("make -n check");
        ASSERT_TRUE(make_check.find("test-harness") != std::string::npos, "make check should use test harness");
    }
    
    void testBuildSystemIntegration() {
        std::cout << "Testing build system integration..." << std::endl;
        
        // Test that Makefile.am exists and has proper structure
        ASSERT_TRUE(std::filesystem::exists("./Makefile.am"), "Makefile.am should exist");
        
        std::ifstream makefile("./Makefile.am");
        std::string makefile_content((std::istreambuf_iterator<char>(makefile)),
                                   std::istreambuf_iterator<char>());
        
        ASSERT_TRUE(makefile_content.find("test-harness") != std::string::npos, 
                   "Makefile.am should reference test-harness");
        ASSERT_TRUE(makefile_content.find("check_PROGRAMS") != std::string::npos,
                   "Makefile.am should have check_PROGRAMS");
        
        // Test that test utilities library exists
        ASSERT_TRUE(std::filesystem::exists("./libtest_utilities.a"), 
                   "Test utilities library should exist");
    }
    
    void testFileSystemValidation() {
        std::cout << "Testing file system validation..." << std::endl;
        
        // Test that all expected test framework files exist
        std::vector<std::string> expected_files = {
            "test_framework.h",
            "test_framework.cpp", 
            "test_discovery.h",
            "test_discovery.cpp",
            "test_executor.h", 
            "test_executor.cpp",
            "test_reporter.h",
            "test_reporter.cpp",
            "test_harness.cpp"
        };
        
        for (const auto& file : expected_files) {
            ASSERT_TRUE(std::filesystem::exists("./" + file), 
                       "Test framework file should exist: " + file);
        }
        
        // Test that validation scripts exist
        std::vector<std::string> validation_scripts = {
            "validate_test_suite.sh",
            "verify_all_tests.sh"
        };
        
        for (const auto& script : validation_scripts) {
            if (std::filesystem::exists("./" + script)) {
                std::cout << "  Found validation script: " << script << std::endl;
            }
        }
        
        // Test that README has been updated with testing information
        if (std::filesystem::exists("../README")) {
            std::ifstream readme("../README");
            std::string readme_content((std::istreambuf_iterator<char>(readme)),
                                     std::istreambuf_iterator<char>());
            
            ASSERT_TRUE(readme_content.find("make check") != std::string::npos,
                       "README should document make check command");
            ASSERT_TRUE(readme_content.find("test-harness") != std::string::npos,
                       "README should document test harness usage");
        }
    }
    
    std::string executeCommand(const std::string& command) {
        std::string result;
        char buffer[128];
        
        // Use popen instead of system() to capture output
        FILE* pipe = popen((command + " 2>&1").c_str(), "r");
        if (pipe) {
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }
            pclose(pipe);
        }
        
        return result;
    }
};

int main() {
    TestSuite suite("Test Harness Validation");
    suite.addTest(std::make_unique<TestHarnessValidationTest>());
    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results) > 0 ? 1 : 0;
}