#pragma once
#include <algorithm>
#include <cmath>

namespace ss {
// Visibility is independent of the actual keyboard/accessibility focus.
struct FocusVisual {
    double shownAt = -1;
    float opacity = 0;
    void show(double now) { shownAt = now; }
    void hide() { shownAt = -1; }
    float step(double now, float elapsed, bool reduced) {
        const float target = shownAt >= 0 && now >= shownAt && now - shownAt < 3 ? 1.f : 0.f;
        if (reduced) opacity = target;
        else if (std::isfinite(elapsed) && elapsed > 0) {
            const float duration = target > opacity ? .065f : .12f;
            opacity += (target - opacity) * (1.f - std::exp(-std::min(elapsed, .1f) / duration));
            if (std::abs(opacity - target) < .001f) opacity = target;
        }
        return opacity;
    }
};
} // namespace ss
