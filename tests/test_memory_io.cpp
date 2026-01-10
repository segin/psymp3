#include "psymp3.h"
#include "io/MemoryIOHandler.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>

using namespace PsyMP3::IO;

void test_memory_io() {
    std::cout << "Testing MemoryIOHandler..." << std::endl;

    MemoryIOHandler handler;
    const char* data1 = "Hello";
    const char* data2 = " World";

    handler.write(data1, 5);
    handler.write(data2, 6);

    char buffer[20];

    // Test basic read
    handler.seek(0, SEEK_SET);
    size_t read = handler.read(buffer, 1, 6); // Read "Hello "
    assert(read == 6);
    assert(strncmp(buffer, "Hello ", 6) == 0);
    std::cout << "Read OK" << std::endl;

    // Logical pos should be 6
    assert(handler.tell() == 6);

    // Discard what we read
    handler.discardRead();

    // Logical pos should still be 6
    assert(handler.tell() == 6);

    // Physical buffer should now contain "World" (5 chars)
    // Physical pos should be 0.

    // Read next part
    memset(buffer, 0, 20);
    read = handler.read(buffer, 1, 5);
    assert(read == 5);
    assert(strncmp(buffer, "World", 5) == 0);
    std::cout << "DiscardRead + Subsequent Read OK" << std::endl;

    // Logical pos should be 11
    assert(handler.tell() == 11);
}

int main() {
    try {
        test_memory_io();
        std::cout << "All tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
