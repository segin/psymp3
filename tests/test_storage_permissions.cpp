#include "psymp3.h"
#include <iostream>
#include <sys/stat.h>
#include <cstdlib>
#include <cerrno>

int main() {
    const char* test_root = "/tmp/psymp3_test_perms";
    const char* storage_path = "/tmp/psymp3_test_perms/psymp3";

    // Clean up from previous runs if any
    system("rm -rf /tmp/psymp3_test_perms");
    if (mkdir(test_root, 0700) != 0) {
        perror("mkdir test_root");
        return 1;
    }

    setenv("XDG_CONFIG_HOME", test_root, 1);

    std::cout << "Target storage path: " << System::getStoragePath().to8Bit(true) << std::endl;

    if (!System::createStoragePath()) {
        std::cerr << "Failed to create storage path" << std::endl;
        return 1;
    }

    struct stat st;
    if (stat(storage_path, &st) != 0) {
        perror("stat");
        return 1;
    }

    mode_t actual_mode = st.st_mode & 0777;
    std::cout << "Actual permissions: 0" << std::oct << actual_mode << std::dec << std::endl;

    if (actual_mode != 0700) {
        std::cerr << "VULNERABILITY DETECTED: Insecure permissions 0" << std::oct << actual_mode << std::dec << " (expected 0700)" << std::endl;
        // Clean up
        system("rm -rf /tmp/psymp3_test_perms");
        return 1;
    }

    std::cout << "SUCCESS: Permissions are restricted to 0700" << std::endl;

    // Cleanup
    system("rm -rf /tmp/psymp3_test_perms");

    return 0;
}
