/*
 * AlbumArtWidget.h - Shows a track's cover art, scaled to fit.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef ALBUMARTWIDGET_H
#define ALBUMARTWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

using PsyMP3::Widget::Foundation::DrawableWidget;

// A canvas showing one JPEG or PNG image, scaled bilinearly to the largest
// size that fits a box with its proportions kept (enlarging a small image to
// reach it), and centred. An image more than twice that size is first halved
// by averaging, as often as it takes, so that no source pixel is skipped.
//
// With a backdrop (setBackdrop), what the image leaves uncovered is filled by
// the image itself: scaled to the widget's full width, cropped top and bottom
// about its centre, and blurred heavily.
//
// It takes the encoded bytes rather than a Tag::Picture so that it does not
// depend on the tag framework's headers, which are included later than the
// widgets. Only the scaled copies are kept: the decoded original, which for a
// large cover is tens of megabytes, is released before setImage() returns.
class AlbumArtWidget : public DrawableWidget {
public:
    AlbumArtWidget(int width, int height, Font* font);

    // The box the image is fitted to, centred in the widget. The whole widget
    // unless set.
    void setArtBox(int width, int height);

    // The colour behind the image, shown where nothing else is drawn and
    // through a transparent image. White unless set.
    void setBackgroundColor(uint8_t r, uint8_t g, uint8_t b);

    // Fill the area around the image with a blurred copy of it.
    void setBackdrop(bool enabled);

    // Decode and show an image. Returns false, and shows a message in its
    // place, if the data is not a JPEG or PNG that can be decoded.
    bool setImage(const uint8_t* data, size_t size);

    // Show the "no album art" message.
    void clearImage();

    bool hasImage() const { return !m_image.empty(); }

    // Bilinear resampling of 8-bit RGBA (alpha not premultiplied) to opaque
    // RGBA over a background colour. Both sizes must be positive.
    static std::vector<uint8_t> scaleBilinear(const uint8_t* src, int src_w, int src_h,
                                              int dst_w, int dst_h,
                                              uint8_t bg_r = 255, uint8_t bg_g = 255,
                                              uint8_t bg_b = 255);

    // An approximate Gaussian blur, in place, of opaque RGBA: three passes of
    // a box blur of the given radius each way, edges repeated.
    static void blur(std::vector<uint8_t>& pixels, int width, int height, int radius);

protected:
    void draw(Surface& surface) override;

private:
    void drawMessage(Surface& surface, const char* text);
    void blitPixels(Surface& surface, std::vector<uint8_t>& pixels, int w, int h);

    Font* m_font;
    int m_box_w;
    int m_box_h;
    uint8_t m_bg[3] = {255, 255, 255};
    bool m_backdrop_enabled = false;
    // The scaled image, opaque RGBA, m_image_w x m_image_h; empty when there
    // is none to show.
    std::vector<uint8_t> m_image;
    int m_image_w = 0;
    int m_image_h = 0;
    // The blurred backdrop, likewise; empty without setBackdrop(true).
    std::vector<uint8_t> m_backdrop;
    int m_backdrop_w = 0;
    int m_backdrop_h = 0;
    // An image was given and could not be decoded.
    bool m_undecodable = false;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // ALBUMARTWIDGET_H
