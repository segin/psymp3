/*
 * EqualizerCurveWidget.h - Smoothed EQ response-curve preview canvas.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef EQUALIZERCURVEWIDGET_H
#define EQUALIZERCURVEWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

using PsyMP3::Widget::Foundation::DrawableWidget;

// A canvas that plots the equalizer's gain control points as a smooth curve
// (Catmull-Rom spline through the points, drawn via cubic Bezier). Kept generic
// (band count + dB range are constructor parameters) so it does not depend on
// the DSP layer's headers, which are included later than the widgets.
class EqualizerCurveWidget : public DrawableWidget {
public:
    EqualizerCurveWidget(int width, int height, int num_bands, double min_db, double max_db);

    void setGains(const std::vector<double>& gains_db); // size == num_bands
    void setBandGain(int band, double db);

protected:
    void draw(Surface& surface) override;

private:
    int dbToY(double db, int plot_top, int plot_h) const;

    int    m_num;
    double m_min_db;
    double m_max_db;
    std::vector<double> m_gains;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // EQUALIZERCURVEWIDGET_H
