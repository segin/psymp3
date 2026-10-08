/*
 * AlbumArtWidget.cpp - Shows a track's cover art, scaled to fit.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Widget {
namespace UI {

namespace {

// Box radius of each of the backdrop's three blur passes. Three passes of
// radius r come to a Gaussian of sigma = sqrt(r * (r + 1)), about 32 pixels.
constexpr int kAlbumArtBackdropBlurRadius = 32;

// Halve an RGBA image (alpha not premultiplied) each way, every output pixel
// the average of the 2x2 block it replaces. An odd last row or column is
// averaged with itself. Colour is weighted by alpha, as in scaleBilinear().
std::vector<uint8_t> albumArtHalve(const uint8_t* src, int src_w, int src_h,
                                   int& out_w, int& out_h)
{
    out_w = (src_w + 1) / 2;
    out_h = (src_h + 1) / 2;
    std::vector<uint8_t> out(static_cast<size_t>(out_w) * out_h * 4);

    for (int y = 0; y < out_h; ++y) {
        const int ys[2] = {y * 2, std::min(y * 2 + 1, src_h - 1)};
        for (int x = 0; x < out_w; ++x) {
            const int xs[2] = {x * 2, std::min(x * 2 + 1, src_w - 1)};
            uint32_t colour[3] = {0, 0, 0};
            uint32_t alpha = 0;
            for (int j = 0; j < 2; ++j) {
                for (int i = 0; i < 2; ++i) {
                    const uint8_t* p = src + (static_cast<size_t>(ys[j]) * src_w + xs[i]) * 4;
                    colour[0] += static_cast<uint32_t>(p[0]) * p[3];
                    colour[1] += static_cast<uint32_t>(p[1]) * p[3];
                    colour[2] += static_cast<uint32_t>(p[2]) * p[3];
                    alpha += p[3];
                }
            }
            uint8_t* o = &out[(static_cast<size_t>(y) * out_w + x) * 4];
            for (int c = 0; c < 3; ++c) {
                o[c] = alpha ? static_cast<uint8_t>((colour[c] + alpha / 2) / alpha) : 0;
            }
            o[3] = static_cast<uint8_t>((alpha + 2) / 4);
        }
    }
    return out;
}

// Resample RGBA to dst_w x dst_h, opaque over a background colour.
//
// A bilinear sample reads four source pixels however far apart the samples
// are, so shrinking a 3000-pixel cover to 300 with it alone would skip nine
// pixels in ten and come out gritty. Halve first, averaging, until what is
// left is less than twice the target; the bilinear pass then has every
// source pixel contributing.
std::vector<uint8_t> albumArtResample(const uint8_t* src, int src_w, int src_h,
                                      int dst_w, int dst_h, const uint8_t bg[3])
{
    std::vector<uint8_t> halved;
    while (src_w >= dst_w * 2 && src_h >= dst_h * 2) {
        int half_w = 0;
        int half_h = 0;
        std::vector<uint8_t> next = albumArtHalve(src, src_w, src_h, half_w, half_h);
        halved = std::move(next);
        src = halved.data();
        src_w = half_w;
        src_h = half_h;
    }
    return AlbumArtWidget::scaleBilinear(src, src_w, src_h, dst_w, dst_h, bg[0], bg[1], bg[2]);
}

// One box-blur pass along a line of `count` opaque RGBA pixels `stride`
// bytes apart, from `in` to `out`, with the end pixels repeated outward.
void albumArtBoxBlurLine(const uint8_t* in, uint8_t* out, int count, size_t stride, int radius)
{
    const uint32_t window = static_cast<uint32_t>(radius) * 2 + 1;
    for (int c = 0; c < 3; ++c) {
        auto at = [in, stride, count, c](int i) -> uint32_t {
            return in[static_cast<size_t>(std::clamp(i, 0, count - 1)) * stride + c];
        };
        uint32_t sum = 0;
        for (int i = -radius; i <= radius; ++i) {
            sum += at(i);
        }
        for (int i = 0; i < count; ++i) {
            out[static_cast<size_t>(i) * stride + c] =
                static_cast<uint8_t>((sum + window / 2) / window);
            sum += at(i + radius + 1);
            sum -= at(i - radius);
        }
    }
    for (int i = 0; i < count; ++i) {
        out[static_cast<size_t>(i) * stride + 3] = 255;
    }
}

} // namespace

AlbumArtWidget::AlbumArtWidget(int width, int height, Font* font)
    : DrawableWidget(width, height)
    , m_font(font)
    , m_box_w(width)
    , m_box_h(height)
{
}

void AlbumArtWidget::setArtBox(int width, int height)
{
    m_box_w = std::max(1, width);
    m_box_h = std::max(1, height);
}

void AlbumArtWidget::setBackgroundColor(uint8_t r, uint8_t g, uint8_t b)
{
    m_bg[0] = r;
    m_bg[1] = g;
    m_bg[2] = b;
    invalidate();
}

void AlbumArtWidget::setBackdrop(bool enabled)
{
    m_backdrop_enabled = enabled;
}

std::vector<uint8_t> AlbumArtWidget::scaleBilinear(const uint8_t* src, int src_w, int src_h,
                                                   int dst_w, int dst_h,
                                                   uint8_t bg_r, uint8_t bg_g, uint8_t bg_b)
{
    std::vector<uint8_t> out(static_cast<size_t>(dst_w) * dst_h * 4);
    const float bg[3] = {static_cast<float>(bg_r), static_cast<float>(bg_g),
                         static_cast<float>(bg_b)};

    // One source texel as premultiplied colour plus alpha, so that a
    // transparent pixel's colour does not bleed into its neighbours.
    auto texel = [src, src_w](int x, int y, float weight, float acc[4]) {
        const uint8_t* p = src + (static_cast<size_t>(y) * src_w + x) * 4;
        const float a = p[3] / 255.0f;
        acc[0] += weight * p[0] * a;
        acc[1] += weight * p[1] * a;
        acc[2] += weight * p[2] * a;
        acc[3] += weight * a;
    };

    const float x_ratio = static_cast<float>(src_w) / dst_w;
    const float y_ratio = static_cast<float>(src_h) / dst_h;

    for (int y = 0; y < dst_h; ++y) {
        // Sample at pixel centres, clamped to the image's edge.
        const float fy = std::clamp((y + 0.5f) * y_ratio - 0.5f, 0.0f,
                                    static_cast<float>(src_h - 1));
        const int y0 = static_cast<int>(fy);
        const int y1 = std::min(y0 + 1, src_h - 1);
        const float wy = fy - y0;

        for (int x = 0; x < dst_w; ++x) {
            const float fx = std::clamp((x + 0.5f) * x_ratio - 0.5f, 0.0f,
                                        static_cast<float>(src_w - 1));
            const int x0 = static_cast<int>(fx);
            const int x1 = std::min(x0 + 1, src_w - 1);
            const float wx = fx - x0;

            float acc[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            texel(x0, y0, (1.0f - wx) * (1.0f - wy), acc);
            texel(x1, y0, wx * (1.0f - wy), acc);
            texel(x0, y1, (1.0f - wx) * wy, acc);
            texel(x1, y1, wx * wy, acc);

            // Whatever the image does not cover shows the background.
            const float uncovered = 1.0f - acc[3];
            uint8_t* o = &out[(static_cast<size_t>(y) * dst_w + x) * 4];
            for (int c = 0; c < 3; ++c) {
                o[c] = static_cast<uint8_t>(
                    std::clamp(acc[c] + uncovered * bg[c] + 0.5f, 0.0f, 255.0f));
            }
            o[3] = 255;
        }
    }
    return out;
}

void AlbumArtWidget::blur(std::vector<uint8_t>& pixels, int width, int height, int radius)
{
    if (radius <= 0 || width <= 0 || height <= 0 ||
        pixels.size() < static_cast<size_t>(width) * height * 4) {
        return;
    }
    std::vector<uint8_t> scratch(pixels.size());
    const size_t row = static_cast<size_t>(width) * 4;
    for (int pass = 0; pass < 3; ++pass) {
        for (int y = 0; y < height; ++y) {
            albumArtBoxBlurLine(&pixels[y * row], &scratch[y * row], width, 4, radius);
        }
        for (int x = 0; x < width; ++x) {
            albumArtBoxBlurLine(&scratch[static_cast<size_t>(x) * 4],
                                &pixels[static_cast<size_t>(x) * 4], height, row, radius);
        }
    }
}

bool AlbumArtWidget::setImage(const uint8_t* data, size_t size)
{
    m_image.clear();
    m_backdrop.clear();
    m_image_w = m_image_h = 0;
    m_backdrop_w = m_backdrop_h = 0;
    m_undecodable = true;

    const int widget_w = getPos().width();
    const int widget_h = getPos().height();
    const int box_w = std::min(m_box_w, widget_w);
    const int box_h = std::min(m_box_h, widget_h);

    int src_w = 0;
    int src_h = 0;
    std::unique_ptr<unsigned char, void (*)(unsigned char*)> pixels(
        psymp3_image_decode_rgba(data, size, &src_w, &src_h), psymp3_image_free);

    if (pixels && box_w > 0 && box_h > 0) {
        // The largest size that fits the box with the image's proportions
        // kept, whether that means shrinking it or enlarging it.
        const double scale = std::min(static_cast<double>(box_w) / src_w,
                                      static_cast<double>(box_h) / src_h);
        m_image_w = std::clamp(static_cast<int>(std::lround(src_w * scale)), 1, box_w);
        m_image_h = std::clamp(static_cast<int>(std::lround(src_h * scale)), 1, box_h);
        m_image = albumArtResample(pixels.get(), src_w, src_h, m_image_w, m_image_h, m_bg);

        if (m_backdrop_enabled) {
            // The image at the widget's full width, proportions kept, then
            // only the rows about its centre that the widget has room for.
            const double full_scale = static_cast<double>(widget_w) / src_w;
            const int full_h = std::max(1, static_cast<int>(std::lround(src_h * full_scale)));
            // An image far taller than wide would come to an absurd height
            // here, nearly all of it cropped away again; resample only as
            // much of the source as the kept rows come from.
            const int kept_h = std::min(full_h, widget_h);
            const int src_rows = std::clamp(
                static_cast<int>(std::lround(kept_h / full_scale)), 1, src_h);
            const int src_top = (src_h - src_rows) / 2;
            m_backdrop_w = widget_w;
            m_backdrop_h = kept_h;
            m_backdrop = albumArtResample(
                pixels.get() + static_cast<size_t>(src_top) * src_w * 4,
                src_w, src_rows, m_backdrop_w, m_backdrop_h, m_bg);
            blur(m_backdrop, m_backdrop_w, m_backdrop_h, kAlbumArtBackdropBlurRadius);
        }
        m_undecodable = false;
    }

    redraw();
    return !m_undecodable;
}

void AlbumArtWidget::clearImage()
{
    m_image.clear();
    m_backdrop.clear();
    m_image_w = m_image_h = 0;
    m_backdrop_w = m_backdrop_h = 0;
    m_undecodable = false;
    redraw();
}

void AlbumArtWidget::drawMessage(Surface& surface, const char* text)
{
    if (!m_font) {
        return;
    }
    // Light text on a dark background, dark on a light one.
    const bool dark = (m_bg[0] + m_bg[1] + m_bg[2]) < 384;
    const uint8_t fg = dark ? 255 : 0;
    auto rendered = m_font->RenderLCD(TagLib::String(text), fg, fg, fg,
                                      m_bg[0], m_bg[1], m_bg[2]);
    if (!rendered) {
        return;
    }
    const int x = (getPos().width() - rendered->width()) / 2;
    const int y = (getPos().height() - rendered->height()) / 2;
    surface.Blit(*rendered, Rect(x, y, rendered->width(), rendered->height()));
}

void AlbumArtWidget::blitPixels(Surface& surface, std::vector<uint8_t>& pixels, int w, int h)
{
    // SDL converts from RGBA to whatever the widget's surface is.
    std::unique_ptr<SDL_Surface, void (*)(SDL_Surface*)> image(
        SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, pixels.data(), w * 4),
        SDL_DestroySurface);
    if (!image) {
        return;
    }
    SDL_SetSurfaceBlendMode(image.get(), SDL_BLENDMODE_NONE);
    SDL_Rect dst = {(getPos().width() - w) / 2, (getPos().height() - h) / 2, w, h};
    SDL_BlitSurface(image.get(), nullptr, surface.getHandle(), &dst);
}

void AlbumArtWidget::draw(Surface& surface)
{
    surface.FillRect(surface.MapRGB(m_bg[0], m_bg[1], m_bg[2]));

    if (m_image.empty()) {
        drawMessage(surface, m_undecodable ? "Album art could not be displayed"
                                           : "No album art");
        return;
    }

    if (!m_backdrop.empty()) {
        blitPixels(surface, m_backdrop, m_backdrop_w, m_backdrop_h);
    }
    blitPixels(surface, m_image, m_image_w, m_image_h);
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
