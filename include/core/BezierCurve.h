/*
 * BezierCurve.h - Cubic Bezier evaluation and Catmull-Rom curve smoothing.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PSYMP3_CORE_BEZIERCURVE_H
#define PSYMP3_CORE_BEZIERCURVE_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Core {

struct CurvePoint {
    double x = 0.0;
    double y = 0.0;
};

// Evaluate a cubic Bezier defined by control points p0..p3 at t in [0,1].
inline CurvePoint cubicBezier(const CurvePoint& p0, const CurvePoint& p1,
                              const CurvePoint& p2, const CurvePoint& p3, double t)
{
    const double u = 1.0 - t;
    const double w0 = u * u * u;
    const double w1 = 3.0 * u * u * t;
    const double w2 = 3.0 * u * t * t;
    const double w3 = t * t * t;
    return { w0 * p0.x + w1 * p1.x + w2 * p2.x + w3 * p3.x,
             w0 * p0.y + w1 * p1.y + w2 * p2.y + w3 * p3.y };
}

// Smooth a sequence of control points that the curve must pass through into a
// dense polyline, using a uniform Catmull-Rom spline expressed as cubic Bezier
// segments (tangent at each interior point = (next - prev)/6). The endpoints
// are duplicated as phantom neighbours so the curve begins and ends exactly on
// the first and last point. samples_per_segment is clamped to >= 1.
//
// The returned polyline can be drawn gap-free by connecting successive points
// with Surface::line().
inline std::vector<CurvePoint> smoothCurve(const std::vector<CurvePoint>& pts,
                                           int samples_per_segment = 16)
{
    std::vector<CurvePoint> out;
    const int n = static_cast<int>(pts.size());
    if (n == 0) return out;
    if (n == 1) { out.push_back(pts[0]); return out; }
    if (samples_per_segment < 1) samples_per_segment = 1;

    out.reserve(static_cast<size_t>(n - 1) * samples_per_segment + 1);
    out.push_back(pts[0]);
    for (int i = 0; i < n - 1; ++i) {
        const CurvePoint& p0 = pts[i == 0 ? 0 : i - 1];
        const CurvePoint& p1 = pts[i];
        const CurvePoint& p2 = pts[i + 1];
        const CurvePoint& p3 = pts[i + 2 >= n ? n - 1 : i + 2];
        // Catmull-Rom -> cubic Bezier control points.
        const CurvePoint b1 { p1.x + (p2.x - p0.x) / 6.0, p1.y + (p2.y - p0.y) / 6.0 };
        const CurvePoint b2 { p2.x - (p3.x - p1.x) / 6.0, p2.y - (p3.y - p1.y) / 6.0 };
        for (int s = 1; s <= samples_per_segment; ++s) {
            const double t = static_cast<double>(s) / samples_per_segment;
            out.push_back(cubicBezier(p1, b1, b2, p2, t));
        }
    }
    return out;
}

} // namespace Core
} // namespace PsyMP3

#endif // PSYMP3_CORE_BEZIERCURVE_H
