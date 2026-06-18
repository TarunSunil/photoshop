#pragma once

#include "shared-types/Layer.hpp"

namespace lumen::blend {
    inline double normal(double a, double b) { return b; }
    inline double multiply(double a, double b) { return a * b; }
    inline double screen(double a, double b) { return 1.0 - (1.0 - a) * (1.0 - b); }
    inline double overlay(double a, double b) { return a < 0.5 ? 2 * a * b : 1 - 2 * (1 - a) * (1 - b); }
    inline double softLight(double a, double b) {
        if (b <= 0.5) return a - (1 - 2 * b) * a * (1 - a);
        double d = (a <= 0.25) ? ((16 * a - 12) * a + 4) * a : std::sqrt(a);
        return a + (2 * b - 1) * (d - a);
    }
    inline double hardLight(double a, double b) { return overlay(b, a); }
    inline double difference(double a, double b) { return std::abs(a - b); }
    inline double compose(double base, double blend, double opacity, BlendMode mode)
    {
        double blended;
        switch (mode) {
            case BlendMode::Multiply:   blended = multiply(base, blend); break;
            case BlendMode::Screen:     blended = screen(base, blend); break;
            case BlendMode::Overlay:    blended = overlay(base, blend); break;
            case BlendMode::SoftLight:  blended = softLight(base, blend); break;
            case BlendMode::HardLight:  blended = hardLight(base, blend); break;
            case BlendMode::Difference: blended = difference(base, blend); break;
            default:                    blended = normal(base, blend); break;
        }
        return base * (1.0 - opacity) + blended * opacity;
    }
} // namespace lumen::blend
```