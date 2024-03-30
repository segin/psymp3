#include <iostream>
#include <cmath>

float logarithmicScale(float x) {
    // Ensure x is in the range [0, 1]
    x = std::max(0.0f, std::min(1.0f, x));
    
    // Apply logarithmic scaling
    return std::log(1.0f + 9.0f * x) / std::log(10.0f);
}

int main() {
    // Example: Scaling numbers in the range [0, 1] using logarithmic scaling
    const int size = 10;
    float numbers[size] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 0.9f, 0.95f, 0.99f, 1.0f};
    
    std::cout << "Original numbers:\n";
    for (int i = 0; i < size; ++i) {
        std::cout << numbers[i] << " ";
    }
    std::cout << "\n\n";
    
    std::cout << "Scaled numbers (logarithmic scaling towards 1):\n";
    for (int i = 0; i < size; ++i) {
        std::cout << logarithmicScale(numbers[i]) << " ";
    }
    std::cout << std::endl;
    
    return 0;
}
