#include <iostream>
#include <random>
#include <cassert>
#include <vector>

int main() {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<int> width_dist(100, 299);
    std::uniform_int_distribution<int> height_dist(80, 229);
    std::uniform_int_distribution<int> x_dist(0, 399);
    std::uniform_int_distribution<int> y_dist(0, 299);

    std::cout << "Running 1000 iterations to verify random distribution ranges..." << std::endl;

    for (int i = 0; i < 1000; ++i) {
        int width = width_dist(gen);
        int height = height_dist(gen);
        int x = x_dist(gen);
        int y = y_dist(gen);

        if (width < 100 || width > 299) {
            std::cerr << "Width out of range: " << width << std::endl;
            return 1;
        }
        if (height < 80 || height > 229) {
            std::cerr << "Height out of range: " << height << std::endl;
            return 1;
        }
        if (x < 0 || x > 399) {
            std::cerr << "X out of range: " << x << std::endl;
            return 1;
        }
        if (y < 0 || y > 299) {
            std::cerr << "Y out of range: " << y << std::endl;
            return 1;
        }
    }

    std::cout << "All values within expected ranges." << std::endl;

    // Verify that it's not always the same (basic check for randomness)
    int first_width = width_dist(gen);
    bool changed = false;
    for (int i = 0; i < 100; ++i) {
        if (width_dist(gen) != first_width) {
            changed = true;
            break;
        }
    }
    assert(changed && "Random generator seems stuck or non-random");

    return 0;
}
