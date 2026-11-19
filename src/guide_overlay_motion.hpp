#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ss {
struct GuideOverlayBox {
    double left{}, top{}, right{}, bottom{};
};
inline constexpr std::uint64_t guideOverlayRevealMs = 280, guideOverlayFadeMs = 240;

constexpr double guideOverlayEase(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * t * (10.0 + t * (-15.0 + t * 6.0));
}

inline std::uint64_t guideOverlayTravel(const GuideOverlayBox &from, const GuideOverlayBox &to, float scale) {
    const double distance = std::hypot((to.left + to.right - from.left - from.right) / 2,
                                       (to.top + to.bottom - from.top - from.bottom) / 2) / std::max(.1f, scale);
    return std::uint64_t(std::clamp(225.0 + distance * .19, 240.0, 425.0));
}

constexpr GuideOverlayBox guideOverlayInterpolate(const GuideOverlayBox &from, const GuideOverlayBox &to, double t) {
    const double progress = guideOverlayEase(t);
    const auto move = [progress](double a, double b) { return a + (b - a) * progress; };
    return {move(from.left,to.left), move(from.top,to.top), move(from.right,to.right), move(from.bottom,to.bottom)};
}

constexpr GuideOverlayBox guideOverlayOpeningBox(const GuideOverlayBox &box, double maxInset) {
    const auto shrink = [maxInset](double length) { return std::clamp(length * .035, 0.0, maxInset); };
    const double x = shrink(box.right - box.left), y = shrink(box.bottom - box.top);
    return {box.left + x, box.top + y, box.right - x, box.bottom - y};
}

constexpr unsigned guideOverlayReveal(std::uint64_t elapsed, unsigned initial = 0) {
    const auto eased = guideOverlayEase(double(elapsed) / double(guideOverlayRevealMs));
    return unsigned(initial + (255.0 - initial) * eased + .5);
}

constexpr unsigned guideOverlayFade(std::uint64_t elapsed, unsigned initial = 255) {
    return unsigned(initial * (1.0 - guideOverlayEase(double(elapsed) / double(guideOverlayFadeMs))) + .5);
}
}
