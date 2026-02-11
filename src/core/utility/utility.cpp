/*
 * utility.cpp - General utility functions implementation.
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <algorithm>
#include <cmath>

#ifndef FINAL_BUILD
#include "psymp3.h"
#else
#include "core/utility/utility.h"
#endif

namespace {
    /**
     * @brief Internal lookup table for logarithmic scaling.
     * Precomputes scale factors 0-4 for values 0.0 to 1.0.
     * Uses 4096 entries to ensure high precision even for scale factor 4.
     */
    struct LogScaleLUT {
        static constexpr int TABLE_SIZE = 4096;
        float data[5][TABLE_SIZE];
        LogScaleLUT() {
            for (int f = 0; f <= 4; ++f) {
                for (int i = 0; i < TABLE_SIZE; ++i) {
                    float val = static_cast<float>(i) / static_cast<float>(TABLE_SIZE - 1);
                    float x = val;
                    if (f > 0) {
                        for (int j = 0; j < f; ++j) {
                            x = std::log10(1.0f + 9.0f * x);
                        }
                    }
                    data[f][i] = x;
                }
            }
        }
    };
}

/**
 * @brief Applies logarithmic scaling to a value.
 * Optimized with a lookup table and linear interpolation for common scale factors (0-4).
 * @param f The scale factor (number of times to apply log10(1 + 9x)).
 * @param x The input value, clamped to [0.0, 1.0].
 * @return The scaled value.
 */
float PsyMP3::Core::Utility::logarithmicScale(const int f, float x) {
    static const LogScaleLUT lut;
    x = std::clamp(x, 0.0f, 1.0f);

    if (f >= 0 && f <= 4) {
        float f_index = x * static_cast<float>(LogScaleLUT::TABLE_SIZE - 1);
        int index = static_cast<int>(f_index);
        if (index >= LogScaleLUT::TABLE_SIZE - 1) {
            return lut.data[f][LogScaleLUT::TABLE_SIZE - 1];
        }
        float fraction = f_index - static_cast<float>(index);
        return lut.data[f][index] * (1.0f - fraction) + lut.data[f][index + 1] * fraction;
    }

    // Fallback for f > 4 or f < 0
    if (f > 0) {
        for (auto i = 0; i < f; i++) {
            x = std::log10(1.0f + 9.0f * x);
        }
    }
    return x;
}
