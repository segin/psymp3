#include "psymp3.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>

using namespace PsyMP3::IO::File;
using namespace PsyMP3::Core;

void test_path(const std::string& path_str, bool expect_success) {
    std::cout << "Testing path: [" << path_str << "] - Expecting " << (expect_success ? "SUCCESS" : "FAILURE") << "... ";
    try {
        // Create the file if we expect success, to ensure weakly_canonical doesn't fail
        // (though it shouldn't fail if the path is just non-existent).
        if (expect_success) {
             std::ofstream(path_str) << "dummy" << std::endl;
        }

        FileIOHandler handler{TagLib::String(path_str)};

        if (expect_success) {
             std::filesystem::remove(path_str);
        }

        if (!expect_success) {
            std::cout << "FAILED (Allowed insecure path!)" << std::endl;
            exit(1);
        }
        std::cout << "PASSED" << std::endl;
    } catch (const InvalidMediaException& e) {
        if (expect_success) {
            std::cout << "FAILED (Blocked secure path!): " << e.what() << std::endl;
            exit(1);
        }
        std::cout << "PASSED (Caught: " << e.what() << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "FAILED (Unknown exception): " << e.what() << std::endl;
        exit(1);
    }
}

int main() {
    // Basic tests
    test_path("test_ok.mp3", true);
    test_path("./test_ok2.mp3", true);
    test_path("test..txt", true);

    // Traversal tests
    test_path("../secret.txt", false);
    test_path("dir/../../secret.txt", false);

    // Symlink test - escape CWD
    std::filesystem::create_directory("safe_dir");
    try {
        // Points to / (outside current directory hierarchy)
        std::filesystem::create_directory_symlink("/", "safe_dir/root_link");
        test_path("safe_dir/root_link/etc/passwd", false);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cout << "Skipping symlink test (filesystem does not support symlinks here): " << e.what() << std::endl;
    }

    // Absolute path traversal attempt (obfuscated with ..)
    test_path("/tmp/../etc/passwd", false);

    // Cleanup
    std::filesystem::remove_all("safe_dir");

    std::cout << "\nAll path security tests passed!" << std::endl;
    return 0;
}
