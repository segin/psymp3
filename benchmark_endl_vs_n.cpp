#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <cstdio>

int main() {
    const int num_iterations = 100000;
    const std::string message = "12:34:56.789012 [channel]: This is a test message to benchmark the performance difference between std::endl and newlines.";

    // Test std::endl
    auto start_endl = std::chrono::high_resolution_clock::now();
    std::ofstream file_endl("test_endl.log");
    for (int i = 0; i < num_iterations; ++i) {
        file_endl << message << std::endl;
    }
    file_endl.close();
    auto end_endl = std::chrono::high_resolution_clock::now();
    auto duration_endl = std::chrono::duration_cast<std::chrono::milliseconds>(end_endl - start_endl).count();

    // Test \n
    auto start_n = std::chrono::high_resolution_clock::now();
    std::ofstream file_n("test_n.log");
    for (int i = 0; i < num_iterations; ++i) {
        file_n << message << '\n';
    }
    file_n.close();
    auto end_n = std::chrono::high_resolution_clock::now();
    auto duration_n = std::chrono::duration_cast<std::chrono::milliseconds>(end_n - start_n).count();

    std::cout << "std::endl duration: " << duration_endl << " ms\n";
    std::cout << "'\\n' duration: " << duration_n << " ms\n";
    std::cout << "Improvement: " << (double)duration_endl / duration_n << "x\n";

    std::remove("test_endl.log");
    std::remove("test_n.log");

    return 0;
}