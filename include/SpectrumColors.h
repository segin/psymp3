#ifndef SPECTRUM_COLORS_H
#define SPECTRUM_COLORS_H

#include <cstdint>
#include <cmath>

namespace PsyMP3 {

struct SpectrumColorConfig {
    static constexpr uint16_t TOTAL_BINS = 320;

    // Frequency range boundaries
    static constexpr uint16_t LOW_CUTOFF = 106;
    static constexpr uint16_t MID_CUTOFF = 214; // Actually > 213 means >= 214

    // Low frequency range constants (x < 106)
    struct Low {
        static constexpr uint8_t BASE_R = 128;
        static constexpr uint8_t BASE_G = 255;
        static constexpr float BLUE_FACTOR = 2.398f;
    };

    // Mid frequency range constants (106 <= x <= 213)
    struct Mid {
        static constexpr uint8_t BASE_B = 255;
        static constexpr uint8_t START_R = 128;
        static constexpr uint8_t START_G = 255;
        static constexpr float RED_DECAY = 1.1962615f;
        static constexpr float GREEN_DECAY = 2.383177f;
    };

    // High frequency range constants (x > 213)
    struct High {
        static constexpr uint8_t BASE_G = 0;
        static constexpr uint8_t BASE_B = 255;
        static constexpr float RED_GROWTH = 2.4f;
    };

    // Calculate RGB values for a given bin index
    static void getRGB(uint16_t x, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (x >= MID_CUTOFF) {
            // High frequency range
            r = static_cast<uint8_t>((x - MID_CUTOFF) * High::RED_GROWTH);
            g = High::BASE_G;
            b = High::BASE_B;
        } else if (x < LOW_CUTOFF) {
            // Low frequency range
            r = Low::BASE_R;
            g = Low::BASE_G;
            b = static_cast<uint8_t>(x * Low::BLUE_FACTOR);
        } else {
            // Mid frequency range
            r = static_cast<uint8_t>(Mid::START_R - ((x - LOW_CUTOFF) * Mid::RED_DECAY));
            g = static_cast<uint8_t>(Mid::START_G - ((x - LOW_CUTOFF) * Mid::GREEN_DECAY));
            b = Mid::BASE_B;
        }
    }
};

} // namespace PsyMP3

#endif // SPECTRUM_COLORS_H
