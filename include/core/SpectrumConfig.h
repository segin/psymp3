#ifndef SPECTRUM_CONFIG_H
#define SPECTRUM_CONFIG_H

#include <cstdint>

namespace PsyMP3 {
namespace Core {

/**
 * @brief Configuration for the spectrum analyzer visualization.
 *
 * This struct defines constants and helper functions for generating the
 * spectrum visualization, ensuring consistency across different components
 * (Player, SpectrumAnalyzerWidget).
 */
struct SpectrumConfig {
    /**
     * @brief The number of frequency bands in the spectrum.
     */
    static constexpr uint16_t NumBands = 320;

    /**
     * @brief The end index of the first color zone (exclusive).
     * Range: [0, Zone1End)
     */
    static constexpr uint16_t Zone1End = 106;   // Exclusive (< 106)

    /**
     * @brief The start index of the third color zone (inclusive).
     * Range: [Zone3Start, NumBands)
     */
    static constexpr uint16_t Zone3Start = 214; // Inclusive (>= 214, so > 213)

    /**
     * @brief Represents an RGB color.
     */
    struct Color {
        uint8_t r, g, b;
    };

    /**
     * @brief Calculates the color for a given frequency band index.
     *
     * This function implements the gradient logic used for the spectrum bars.
     *
     * @param band_index The index of the frequency band (0 to NumBands-1).
     * @return The RGB color for the bar.
     */
    static constexpr Color getBarColor(uint16_t band_index) {
        if (band_index >= Zone3Start) {
             // Zone 3: Higher frequencies (Blue -> Purple/Red-ish)
             // Original logic: r = (x - 214) * 2.4, g = 0, b = 255
             uint8_t r = static_cast<uint8_t>((band_index - Zone3Start) * 2.4);
             return {r, 0, 255};
        } else if (band_index < Zone1End) {
             // Zone 1: Lower frequencies (Cyan -> Blue)
             // Original logic: r = 128, g = 255, b = x * 2.398
             uint8_t b = static_cast<uint8_t>(band_index * 2.398);
             return {128, 255, b};
        } else {
             // Zone 2: Middle frequencies (Green/Cyan -> Blue)
             // Original logic:
             // r = 128 - ((x - 106) * 1.1962615)
             // g = 255 - ((x - 106) * 2.383177)
             // b = 255
             uint8_t r = static_cast<uint8_t>(128 - ((band_index - Zone1End) * 1.1962615));
             uint8_t g = static_cast<uint8_t>(255 - ((band_index - Zone1End) * 2.383177));
             return {r, g, 255};
        }
    }
};

} // namespace Core
} // namespace PsyMP3

#endif // SPECTRUM_CONFIG_H
