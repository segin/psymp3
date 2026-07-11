/*
 * EqualizerCurveWidget.cpp - Smoothed EQ response-curve preview canvas.
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

EqualizerCurveWidget::EqualizerCurveWidget(int width, int height, int num_bands,
                                           double min_db, double max_db)
    : DrawableWidget(width, height)
    , m_num(num_bands > 0 ? num_bands : 1)
    , m_min_db(min_db)
    , m_max_db(max_db > min_db ? max_db : min_db + 1.0)
    , m_gains(static_cast<size_t>(m_num), 0.0)
{
}

void EqualizerCurveWidget::setGains(const std::vector<double>& gains_db)
{
    for (int i = 0; i < m_num && i < static_cast<int>(gains_db.size()); ++i)
        m_gains[i] = gains_db[i];
    redraw();
}

void EqualizerCurveWidget::setBandGain(int band, double db)
{
    if (band < 0 || band >= m_num) return;
    m_gains[band] = db;
    redraw();
}

int EqualizerCurveWidget::dbToY(double db, int plot_top, int plot_h) const
{
    // max_db -> plot_top, min_db -> plot_top + plot_h - 1
    const double span = m_max_db - m_min_db;
    double frac = (m_max_db - db) / span; // 0 at top
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;
    return plot_top + static_cast<int>(std::round(frac * (plot_h - 1)));
}

void EqualizerCurveWidget::draw(Surface& surface)
{
    const int w = getPos().width();
    const int h = getPos().height();

    // Sunken bezel around a dark plot field.
    surface.box(0, 0, w - 1, h - 1, 0, 0, 0, 255);
    surface.hline(0, w - 1, 0, 128, 128, 128, 255);
    surface.vline(0, 0, h - 1, 128, 128, 128, 255);
    surface.hline(0, w - 1, h - 1, 255, 255, 255, 255);
    surface.vline(w - 1, 0, h - 1, 255, 255, 255, 255);

    const int m = 3;                 // inner margin
    const int plot_x = m;
    const int plot_top = m;
    const int plot_w = w - 2 * m;
    const int plot_h = h - 2 * m;
    if (plot_w < 4 || plot_h < 4 || m_num < 1) { return; }

    // 0 dB reference line.
    const int y0 = dbToY(0.0, plot_top, plot_h);
    surface.hline(plot_x, plot_x + plot_w - 1, y0, 64, 64, 96, 255);

    // Vertical band gridlines at each control-point x.
    auto bandX = [&](int i) -> int {
        if (m_num == 1) return plot_x + plot_w / 2;
        return plot_x + (i * (plot_w - 1)) / (m_num - 1);
    };
    for (int i = 0; i < m_num; ++i)
        surface.vline(bandX(i), plot_top, plot_top + plot_h - 1, 32, 32, 48, 255);

    // Build the control points (band x, gain y) and smooth them.
    std::vector<PsyMP3::Core::CurvePoint> pts;
    pts.reserve(m_num);
    for (int i = 0; i < m_num; ++i)
        pts.push_back({ static_cast<double>(bandX(i)),
                        static_cast<double>(dbToY(m_gains[i], plot_top, plot_h)) });

    std::vector<PsyMP3::Core::CurvePoint> poly = PsyMP3::Core::smoothCurve(pts, 20);
    for (size_t i = 1; i < poly.size(); ++i) {
        surface.line(static_cast<Sint16>(poly[i - 1].x), static_cast<Sint16>(poly[i - 1].y),
                     static_cast<Sint16>(poly[i].x),     static_cast<Sint16>(poly[i].y),
                     0, 224, 96, 255);
    }

    // Control-point markers.
    for (int i = 0; i < m_num; ++i) {
        int cx = static_cast<int>(pts[i].x);
        int cy = static_cast<int>(pts[i].y);
        surface.box(cx - 1, cy - 1, cx + 1, cy + 1, 255, 255, 160, 255);
    }
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
