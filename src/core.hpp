#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include "json.hpp"
namespace ss {
using Json = nlohmann::json;
struct PreviewImage { int width=0,height=0; std::vector<unsigned char> rgba; };
struct Rect {
    float x, y, w, h;
    bool contains(float a, float b) const {
        return a >= x && a < x + w && b >= y && b < y + h;
    }
    Rect inset(float d) const {
        return {x + d, y + d, std::max(0.f, w - 2 * d), std::max(0.f, h - 2 * d)};
    }
    Rect move(float a, float b) const {
        return {x + a, y + b, w, h};
    }
    Rect scale(float s) const {
        return {x + w * (1 - s) / 2, y + h * (1 - s) / 2, w * s, h * s};
    }
    Rect intersect(Rect b) const {
        float a = std::max(x, b.x), c = std::max(y, b.y);
        return {a, c, std::max(0.f, std::min(x + w, b.x + b.w) - a),
                std::max(0.f, std::min(y + h, b.y + b.h) - c)};
    }
};
struct Ink {
    float r, g, b, a = 1;
    Ink alpha(float v) const {
        return {r, g, b, a * v};
    }
};
inline Ink hex(unsigned v, float a = 1) {
    return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, a};
}
inline Ink mix(Ink a, Ink b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}
struct Spring {
    float value = 0, velocity = 0, target = 0;
    Spring(float v = 0) : value(v), target(v) {}
    void step(float dt, float f = 4.5f, float damping = .86f) {
        if (!std::isfinite(dt) || dt <= 0)
            return;
        dt = std::min(dt, .1f);
        int n = std::max(1, int(std::ceil(dt * 240)));
        float h = dt / n, w = f * 6.283185307f;
        for (int i = 0; i < n; i++) {
            velocity += (w * w * (target - value) - 2 * damping * w * velocity) * h;
            value += velocity * h;
        }
        if (std::abs(value - target) < .0001f && std::abs(velocity) < .0001f) {
            value = target;
            velocity = 0;
        }
    }
};
struct Scroll {
    Spring p;
    float maximum = 0;
    void bounds(float c, float h) {
        maximum = std::max(0.f, c - h);
        p.target = std::clamp(p.target, 0.f, maximum);
    }
    void wheel(float delta) {
        p.target = std::clamp(p.target - delta * 65, 0.f, maximum);
    }
    void step(float dt) {
        p.step(dt, 2.1f, 1);
    }
};
struct Media {
    std::string key, id, title, category, ext, quality, vcodec, acodec, language, protocol, url, page,
          credentialUrl, thumbnail, sourceTitle;
    double size = 0, fps = 0, rate = 0;
    bool selected = false, drm = false, format = false, videoOnly = false, automatic = false, approx = false;
    Json data;
};
struct Analysis {
    std::string title, page;
    Json raw;
    std::vector<Media> items;
};
std::string str(const Json &j, const char *key, const std::string &fallback = "");
double number(const Json &j, const char *key, double fallback = 0);
Analysis parseAnalysis(const Json &j, const std::string &url);
std::string quoteArg(const std::wstring &arg);
std::wstring quoteWide(const std::wstring &arg);
std::string redact(std::string s);
bool httpUrl(const std::string &s);
std::string cookies(const Json &array);
std::string validateCookies(const std::string &s);
std::string sizeText(double size, bool approx = false);
std::vector<uint32_t> codepoints(const std::string &s);
std::string sanitizeMetadata(const Json &j);
std::string dataSourceContent(const Media &item);
constexpr const char *ExtensionId = "cjhiagnpgpmaagnoinjnkipimjlplcpe";
constexpr const char *FirefoxId = "connector@streamshelf.local";
constexpr uint32_t MaxFrame = 1024 * 1024;
} // namespace ss
