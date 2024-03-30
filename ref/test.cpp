/* This code was used to suss out the weird teal column at the end of the middle third.
 * The colorful FFT graph is rendered in thirds, as you can see from the if statement. 
 * The third case handles the middle; the factors originally used were 2.4 and 1.2, but 
 * differences in how signs were handled between the original FreeBASIC and C++ led to a
 * integer underflow condition. 
 */

#include <iostream>

int main() {
    for(int x=0; x < 320; x++) {
        if (x > 213) {
            std::cout << "RGB value #" << x << ": " << ((x - 214) * 2.4) << ", " << 0 << ", " << 255 << ", " << 255 << std::endl;
        } else if (x < 106) {
            std::cout << "RGB value #" << x << ": " << 128 << ", " << 255 << ", " << (x * 2.398) << ", " << 255 << std::endl;
        } else {
            std::cout << "RGB value #" << x << ": " << (128 - ((x - 106) * 1.1962615)) << ", " << (255 - ((x - 106) * 2.383177)) << ", " << 255 << ", " << 255 << std::endl;
        }
    };
}