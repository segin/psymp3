#include <iostream>
#include <vector>
#include <chrono>

struct Window {
    int id;
    Window(int i) : id(i) {}
};

int main() {
    std::vector<Window> m_random_windows;
    for (int i = 0; i < 100000; i++) {
        m_random_windows.push_back(Window(i));
    }

    // Test without reserve
    auto start1 = std::chrono::high_resolution_clock::now();
    std::vector<Window*> sorted_windows1;
    for (int i = 0; i < 1000; i++) {
        for (const auto& window : m_random_windows) {
            sorted_windows1.push_back(const_cast<Window*>(&window));
        }
        sorted_windows1.clear();
    }
    auto end1 = std::chrono::high_resolution_clock::now();

    // Test with reserve
    auto start2 = std::chrono::high_resolution_clock::now();
    std::vector<Window*> sorted_windows2;
    for (int i = 0; i < 1000; i++) {
        sorted_windows2.reserve(m_random_windows.size());
        for (const auto& window : m_random_windows) {
            sorted_windows2.push_back(const_cast<Window*>(&window));
        }
        sorted_windows2.clear();
    }
    auto end2 = std::chrono::high_resolution_clock::now();

    std::cout << "Without reserve: " << std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1).count() << "ms\n";
    std::cout << "With reserve: " << std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2).count() << "ms\n";

    return 0;
}
