#include "app.hpp"
#include "media_display.hpp"
#include <windowsx.h>
#include <set>
#include <numeric>
namespace ss {
float UI::to(const std::string &id, float target, float initial, float freq, float damping) {
    auto [i, newly] = motion.try_emplace(id, initial);
    auto &s = i->second;
    s.target = target;
    s.step(dt, app.reduced ? 14 : freq, app.reduced ? 1 : damping);
    return s.value;
}
void UI::hit(const std::string &id, Rect r, std::function<void()> f, bool edit) {
    r = r.move(c.offsetX, c.offsetY).intersect(c.clip());
    if (r.w > 0 && r.h > 0)
        hits.push_back({id, r, std::move(f), edit});
}
float UI::hover(const std::string &id, Rect r) {
    bool enabled = overlayInput || (app.modalTitle.empty() && !menuOpen);
    return to(id + ":hover",
              enabled && r.move(c.offsetX, c.offsetY).intersect(c.clip()).contains(mx, my) ? 1 : 0);
}
void UI::label(const std::string &id, const std::string &value, float x, float y, float size, Ink color,
               float max) {
    auto [i, created] = labels.try_emplace(id);
    auto &v = i->second;
    if (created)
        v.current = value;
    if (v.current != value) {
        v.old = v.mix.value > .5f ? v.current : v.old;
        v.current = value;
        v.mix.value = 0;
        v.mix.velocity = 0;
    }
    v.mix.target = 1;
    v.mix.step(dt, app.reduced ? 14 : 6, 1);
    float t = std::clamp(v.mix.value, 0.f, 1.f);
    if (t < .999f)
        c.text(v.old, x, y - 3 * t, size, color.alpha(1 - t), max);
    c.text(value, x, y + 4 * (1 - t), size, color.alpha(t), max);
}
void UI::button(const std::string &id, Rect r, const std::string &s, std::function<void()> f, bool primary,
                bool enabled) {
    float h = hover(id, r), p = to(id + ":press", down && capture && capture->id == id ? 1 : 0),
          a = to(id + ":enabled", enabled ? 1 : .35f, enabled ? 1 : .35f);
    auto box = r.scale(1 - .035f * p).move(0, -h + p * .6f);
    auto color = primary ? mix(accent, hex(0xb6f5e8), h * .3f) : mix(panel, hex(0x2a3b52), h * .5f);
    c.glow({box.x - 10, box.y - 8, box.w + 20, box.h + 22}, accent.alpha(h * (primary ? .12f : .04f) * a));
    c.border(box, 12, primary ? accent.alpha(a) : mix(edge, accent, h * .4f).alpha(a), color.alpha(a));
    float x = box.x + std::max(9.f, (box.w - c.measure(s, 12.5f)) / 2);
    label(id + ":label", s, x, box.y + box.h / 2 + 4.5f, 12.5f, (primary ? hex(0x102821) : text).alpha(a),
          box.w - 18);
    if (enabled)
        hit(id, r, std::move(f));
}
void UI::check(const std::string &id, Rect r, bool on, std::function<void()> f) {
    float h = hover(id, r), p = to(id + ":press", down && capture && capture->id == id ? 1 : 0),
          v = to(id + ":check", on ? 1 : 0, on ? 1 : 0, 5, .8f);
    auto box = Rect{r.x + 4, r.y + (r.h - 19) / 2, 19, 19}.scale(1 - p * .1f + h * .06f);
    c.border(box, 6, mix(edge, accent, v), mix(panel, accent, v));
    if (v > .001f) {
        float t = std::clamp(v * 2, 0.f, 1.f);
        c.line(box.x + 4, box.y + 10, box.x + 4 + 4 * t, box.y + 10 + 4 * t, 2, hex(0x112c25, v));
        if (v > .5f) {
            t = std::clamp(v * 2 - 1, 0.f, 1.f);
            c.line(box.x + 8, box.y + 14, box.x + 8 + 7 * t, box.y + 14 - 9 * t, 2, hex(0x112c25, v));
        }
    }
    hit(id, r, std::move(f));
}
void UI::toggle(const std::string &id, Rect r, bool on, std::function<void()> f) {
    float h = hover(id, r), v = to(id + ":toggle", on ? 1 : 0, on ? 1 : 0, 5, .8f),
          p = to(id + ":press", down && capture && capture->id == id ? 1 : 0);
    c.glow({r.x - 8, r.y - 8, r.w + 16, r.h + 16}, accent.alpha(h * .1f));
    c.rect(r, r.h / 2, mix(hex(0x334158), accent, v));
    c.circle(r.x + r.h / 2 + v * (r.w - r.h), r.y + r.h / 2, (r.h / 2 - 3) * (1 + p * .08f), hex(0xf4fffd));
    hit(id, r, std::move(f));
}
void UI::input(const std::string &id, Rect r, std::string &value, const std::string &placeholder) {
    bool f = focus == id;
    float focused = to(id + ":focus", f ? 1 : 0), h = hover(id, r);
    c.glow({r.x - 10, r.y - 10, r.w + 20, r.h + 20}, accent.alpha(focused * .08f));
    c.border(r, 13, mix(edge, accent, std::max(focused, h * .3f)), panel);
    auto w = wide(value);
    auto &caret = carets[id];
    auto &anchor = anchors[id];
    caret = std::min(caret, w.size());
    anchor = std::min(anchor, w.size());
    float caretX = c.measure(utf8(w.substr(0, caret)), 14),
          scroll = to(id + ":scroll", f ? std::max(0.f, caretX - r.w + 36) : 0);
    float x = r.x + 13 - scroll;
    auto clip = r.inset(10);
    c.push(clip);
    size_t a = std::min(caret, anchor), b = std::max(caret, anchor);
    float left = c.measure(utf8(w.substr(0, a)), 14), right = c.measure(utf8(w.substr(0, b)), 14);
    float sl = to(id + ":selectionLeft", left, left), sr = to(id + ":selectionRight", right, right),
          sa = to(id + ":selection", f && a != b ? 1 : 0);
    c.rect({x + sl, r.y + 9, std::max(0.f, sr - sl), r.h - 18}, 4, accent.alpha(sa * .25f));
    c.text(value.empty() ? placeholder : value, x, r.y + r.h / 2 + 5, 14, value.empty() ? muted : text);
    if (focused > .001f) {
        float cx = to(id + ":caret", caretX, caretX, 8, 1);
        float blink = .55f + .45f * std::cos(float(clock * 5));
        c.rect({x + cx, r.y + 10, 1.5f, r.h - 20}, 1, accent.alpha(focused * blink));
    }
    c.pop();
    float offset = c.offsetX;
    auto place = [this, id, &value, x, offset] {
        focus = id;
        editing = &value;
        auto w = wide(value);
        size_t best = 0;
        float d = 1e9;
        for (size_t i = 0; i <= w.size(); i++) {
            if (i > 0 && i < w.size() && IS_LOW_SURROGATE(w[i]))
                continue;
            float n = std::abs(c.measure(utf8(w.substr(0, i)), 14) - (mx - offset - x));
            if (n < d) {
                d = n;
                best = i;
            }
        }
        carets[id] = best;
        if (!down || !capture || capture->id != id || std::hypot(mx - downX, my - downY) < 3)
            anchors[id] = best;
    };
    hit(id, r, place, true);
    if (f && down && capture && capture->id == id && std::hypot(mx - downX, my - downY) > 3)
        place();
}
void UI::choice(const std::string &id, Rect r, const std::vector<std::string> &options, int selected,
                std::function<void(int)> select) {
    float h = hover(id, r), p = to(id + ":press", down && capture && capture->id == id ? 1 : 0);
    c.border(r.scale(1 - .015f * p), 12, mix(edge, accent, h * .4f), panel);
    auto s = selected >= 0 && selected < int(options.size()) ? options[selected] : "Не выбрано";
    label(id + ":value", s, r.x + 13, r.y + r.h / 2 + 4, 12, text, r.w - 42);
    float v = to(id + ":arrow", menuOpen && menuId == id ? 1 : 0);
    c.line(r.x + r.w - 23, r.y + r.h / 2 - 2 + v * 4, r.x + r.w - 18, r.y + r.h / 2 + 3 - v * 6, 1.5f, muted);
    c.line(r.x + r.w - 18, r.y + r.h / 2 + 3 - v * 6, r.x + r.w - 13, r.y + r.h / 2 - 2 + v * 4, 1.5f, muted);
    auto anchor = r.move(c.offsetX, c.offsetY);
    hit(id, r, [this, id, options, select, anchor] {
        if (menuOpen && menuId == id) {
            menuOpen = false;
            return;
        }
        menuId = id;
        menuItems = options;
        menuSelect = select;
        menuAnchor = anchor;
        menuOpen = true;
        menuScroll.p.target = 0;
    });
}
void UI::card(Rect r) {
    c.glow({r.x - 20, r.y - 6, r.w + 40, r.h + 40}, hex(0, app.light ? .05f : .12f));
    c.border(r, 20, edge.alpha(.65f), panel.alpha(.95f));
    c.rect({r.x + 20, r.y + 1, r.w - 40, 1}, .5f, hex(0xffffff, .05f));
}
void UI::title(Rect r, const std::string &a, const std::string &b) {
    c.text(a, r.x, r.y + 28, 28, text, r.w);
    c.text(b, r.x + 1, r.y + 55, 12, muted, r.w);
}
void UI::scrollbar(const std::string &id, Rect r, Scroll &s) {
    if (s.maximum <= 0 || r.h <= 1)
        return;
    float h = hover(id, r);
    float alpha = to(id + ":alpha", h > 0 || std::abs(s.p.velocity) > 1 ? .9f : .6f, .6f);
    float size = std::min(r.h - 1.f, std::max(32.f, r.h * r.h / (r.h + s.maximum)));
    float y = r.y + (r.h - size) * std::clamp(s.p.value / s.maximum, 0.f, 1.f);
    c.rect({r.x+r.w-9,r.y,6,r.h},3,edge.alpha(.45f));
    c.rect({r.x + r.w - 9, y, 6, size}, 3, muted.alpha(alpha));
    hit(id, {r.x + r.w - 12, r.y, 12, r.h}, [this, &s, r, size] {
        s.p.target = std::clamp((my - r.y - size / 2) / (r.h - size) * s.maximum, 0.f, s.maximum);
    });
    if (down && capture && capture->id == id)
        s.p.target = std::clamp((my - r.y - size / 2) / (r.h - size) * s.maximum, 0.f, s.maximum);
}
void UI::init() {
    c.init(executable().parent_path() / L"Assets");
    theme.value = theme.target = app.light ? 1 : 0;
    if (app.splashEnabled) splashStarted = GetTickCount64();
}
void UI::navigate(int i) {
    if (i == page)
        return;
    c.freeze();
    page = i;
    transition.value = 0;
    transition.velocity = 0;
    menuOpen = false;
    cancelInput();
}
void UI::frame(int pw, int ph, float elapsed, float dpi, bool resizing) {
    if (pw < 1 || ph < 1)
        return;
    dt = std::clamp(elapsed, .001f, .08f);
    clock += elapsed;
    scale = std::max(.5f, std::min({dpi, pw / 880.f, ph / 590.f}));
    width = pw / scale;
    height = ph / scale;
    if (splashStarted && GetTickCount64() - splashStarted < ULONGLONG(app.splashSeconds) * 1000) {
        hits.clear();
        float elapsedSeconds = float(GetTickCount64() - splashStarted) / 1000.f;
        float fade = std::clamp(std::min(elapsedSeconds * 2.6f, (app.splashSeconds - elapsedSeconds) * 3.f), 0.f, 1.f);
        float intro = 1.f - std::pow(1.f - std::clamp(elapsedSeconds / .8f, 0.f, 1.f), 3.f);
        float drift = (1.f - intro) * 18.f;
        c.begin(width, height, pw, ph, resizing);
        glClearColor(.025f, .047f, .078f, 1); glClear(GL_COLOR_BUFFER_BIT);
        c.opacity = 1;
        c.rect({0, 0, width, height}, 0, hex(0x080e19));
        float pulse = .5f + .5f * std::sin(elapsedSeconds * 2.2f);
        c.glow({width / 2 - 360, height / 2 - 300, 720, 600}, hex(0x57d5bf, (.08f + .035f * pulse) * fade));
        c.opacity = fade;
        auto centered = [&](const std::string &value, float y, float size, Ink color) {
            c.text(value, (width - c.measure(value, size)) / 2, y + drift, size, color);
        };
        float cx = width / 2, cy = height / 2;
        c.border({cx - 29, cy - 142 + drift, 58, 58}, 18, hex(0x8be2d1, .75f), hex(0x142c35), 1.5f);
        c.line(cx - 12, cy - 113 + drift, cx + 12, cy - 113 + drift, 2.5f, hex(0x8be2d1));
        c.line(cx - 12, cy - 105 + drift, cx + 5, cy - 105 + drift, 2.5f, hex(0x8be2d1));
        c.circle(cx + 11, cy - 105 + drift, 2.5f + pulse, hex(0x8be2d1));
        centered("KRECHETOV LABS", cy - 48, 13, hex(0x8be2d1));
        centered("StreamShelf", cy + 5, 36, hex(0xeef3fc));
        centered("Версия 2.2 Beta", cy + 41, 12, hex(0x8999b4));
        c.rect({cx - 50, cy + 69, 100, 2}, 1, hex(0x2a374b));
        c.rect({cx - 50, cy + 69, 100 * std::clamp(elapsedSeconds / float(app.splashSeconds), 0.f, 1.f), 2}, 1, hex(0x8be2d1));
        c.opacity = 1; c.render(); return;
    }
    splashStarted = 0;
    if (app.requestedPage >= 0) {
        navigate(app.requestedPage);
        app.requestedPage = -1;
    }
    for (auto *s :
         {&mediaScroll, &jobsScroll, &browserScroll, &logScroll, &settingsScroll, &aboutScroll, &menuScroll, &modalScroll, &poolScroll, &draftScroll, &propertyScroll})
        s->step(dt);
    nav.target = 111 + 57 * page;
    nav.step(dt, 4, .9f);
    transition.target = 1;
    transition.step(dt, app.reduced ? 14 : 4.7f, 1);
    appearance.target = app.exitRequested ? 0 : 1;
    appearance.step(dt, 4, 1);
    theme.target = app.light ? 1 : 0;
    theme.step(dt, 3.5f, 1);
    float t = theme.value;
    bg = mix(hex(0x080e19), hex(0xeff3f9), t);
    panel = mix(hex(0x141d2c), hex(0xffffff), t);
    edge = mix(hex(0x2a374b), hex(0xd9e2ef), t);
    text = mix(hex(0xeef3fc), hex(0x192b43), t);
    muted = mix(hex(0x8999b4), hex(0x61748c), t);
    accent = mix(hex(0x8be2d1), hex(0x278c78), t);
    if (!app.modalTitle.empty() && !modalWas) {
        c.freeze();
        cancelInput();
        modalScroll.p.target = 0;
    }
    modalWas = !app.modalTitle.empty();
    c.begin(width, height, pw, ph, resizing);
    hits.clear();
    glClearColor(bg.r, bg.g, bg.b, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    c.opacity = std::clamp(appearance.value, 0.f, 1.f);
    c.rect({0, 0, width, height}, 0, bg);
    c.glow({width * .25f, -360, width * .9f, 820}, accent.alpha(.075f));
    c.glow({-250, height * .4f, 600, 650}, hex(0x577cff, .07f));
    c.text("STREAMSHELF", 25, 36, 15, text);
    c.circle(260, 34, 3.5f, app.busy ? hex(0xffd697) : accent);
    if(app.busy||app.scanning){
        float pulse=app.reduced?1.f:float(.5+.5*std::sin(clock*5));
        c.circle(260,34,3.5f+2*pulse,accent.alpha(.55f+.45f*pulse));
        c.text((app.busy?"Анализ страницы":"Поиск медиа")+std::string(" · найдено ")+std::to_string(app.media.size()),273,39,11,accent,width-585);
    } else label("status", app.status, 273, 39, 11, muted, width - 585);
    button("theme", {width - 234, 17, 92, 31}, app.light ? "Темнее" : "Светлее",
           [this] { app.light = !app.light; });
    button("min", {width - 126, 17, 32, 31}, "−", [this] { ShowWindow(app.hwnd, SW_MINIMIZE); });
    button("max", {width - 88, 17, 32, 31}, "",
           [this] { PostMessageW(app.hwnd, WM_APP + 20, 0, 0); });
    if (IsZoomed(app.hwnd)) {
        c.border({width - 77, 26, 12, 10}, 1, text, panel, 1.3f);
        c.border({width - 80, 29, 12, 10}, 1, text, panel, 1.3f);
    } else c.border({width - 80, 26, 14, 12}, 1, text, panel, 1.3f);
    button("close", {width - 50, 17, 32, 31}, "×", [this] { app.closeRequest(); });
    bool compactMenu=width<1200;
    c.rect({0, 65, width, 1}, 0, edge.alpha(.5f));
    c.rect({17, nav.value, compactMenu?48.f:199.f, 44}, 13, accent.alpha(.11f));
    c.rect({17, nav.value + 13, 3, 18}, 2, accent);
    const char *names[] = {"Обзор медиа", "Подключение", "Загрузки", "Терминал", "Настройки", "О программе"};
    for (int i = 0; i < 6; i++) {
        Rect r{23, 111.f + i * 57, compactMenu?42.f:186.f, 44};
        float h = hover("nav:" + std::to_string(i), r),
              p = to("nav:" + std::to_string(i) + ":press",
                     down && capture && capture->id == "nav:" + std::to_string(i) ? 1 : 0),
              sel = to("nav:" + std::to_string(i) + ":sel", i == page ? 1 : 0, i == page ? 1 : 0);
        c.rect(r.scale(1 - p * .025f), 12, accent.alpha(h * .055f));
        Ink col = mix(muted, accent, sel);
        float x = 40, y = r.y + 15;
        if (i == 0)
            c.border({x, y, 15, 14}, 3, col, panel, 2);
        if (i == 1) {
            c.circle(x + 7, y + 7, 7, col);
            c.circle(x + 7, y + 7, 4, panel);
            c.circle(x + 7, y + 7, 1.4f, col);
        }
        if (i == 2) {
            c.line(x + 7, y, x + 7, y + 11, 1.8f, col);
            c.line(x + 2, y + 7, x + 7, y + 12, 1.8f, col);
            c.line(x + 7, y + 12, x + 12, y + 7, 1.8f, col);
            c.line(x, y + 15, x + 14, y + 15, 1.8f, col);
        }
        if (i == 3) {
            c.line(x, y, x + 5, y + 5, 1.8f, col);
            c.line(x + 5, y + 5, x, y + 10, 1.8f, col);
            c.line(x + 8, y + 12, x + 15, y + 12, 1.8f, col);
        }
        if (i == 4)
            for (int k = 0; k < 3; k++) {
                c.line(x, y + k * 6, x + 15, y + k * 6, 1.5f, col);
                c.circle(x + 3 + (k % 2) * 8, y + k * 6, 2.7f, col);
            }
        if (i == 5) {
            c.circle(x + 7, y + 7, 7, col);
            c.circle(x + 7, y + 7, 5.5f, panel);
            c.circle(x + 7, y + 4, 1, col);
            c.line(x + 7, y + 7, x + 7, y + 11, 1.5f, col);
        }
        if(!compactMenu)c.text(names[i], 69, r.y + 27, 13, mix(muted, text, std::max(sel, h * .7f)));
        hit("nav:" + std::to_string(i), r, [this, i] { navigate(i); });
    }
    c.circle(40, height - 28, 3, app.session() && app.session()->alive && app.session()->verified && app.session()->setupReady ? accent : muted);
    if(!compactMenu)c.text(app.session() && app.session()->alive && app.session()->verified && app.session()->setupReady?"Браузер подключён":"Нет соединения",52,height-24,10,muted,150);
    if (!compactMenu) {
        c.text("KRECHETOV LABS · StreamShelf 2.2 Beta", 25, height - 80, 9, muted, 195);
        c.text("Разработчик: Арсений Кречетов", 25, height - 59, 9, muted, 195);
    }
    float contentLeft=compactMenu?76.f:226.f;
    Rect area{contentLeft, 74, width - contentLeft - 12, height - 94};
    float entry = (1 - std::clamp(transition.value, 0.f, 1.f)) * 42;
    c.offsetX = entry;
    c.push(area.move(-entry, 0));
    switch (page) {
    case 0:
        mediaPage(area);
        break;
    case 1:
        browserPage(area);
        break;
    case 2:
        downloadsPage(area);
        break;
    case 3:
        logsPage(area);
        break;
    case 4:
        settingsPage(area);
        break;
    default:
        aboutPage(area);
        break;
    }
    c.pop();
    c.offsetX = 0;
    if (transition.value < .999f && c.frozen) {
        c.push(area);
        c.drawSnapshot({0, 0, width, height}, 1 - transition.value);
        c.pop();
    }
    c.text(std::to_string(app.media.size()) + " ресурсов", width - 170, height - 15, 9, muted);
    overlays();
    c.opacity = 1;
    c.render();
    c.capture();
    bool isEdit = false, isHit = false;
    for (auto &h : hits)
        if (h.rect.contains(mx, my)) {
            isHit = true;
            isEdit = h.edit;
        }
    POINT pointer{};
    GetCursorPos(&pointer);
    bool overWindow = WindowFromPoint(pointer) == app.hwnd;
    ScreenToClient(app.hwnd, &pointer);
    int border = IsZoomed(app.hwnd) ? 0 : int(8 * dpi);
    // Leave the native sizing cursor alone at the resize borders.
    if (overWindow && pointer.x >= border && pointer.y >= border && pointer.x < pw - border && pointer.y < ph - border)
      SetCursor(LoadCursorW(nullptr,
                          isEdit  ? IDC_IBEAM
                          : isHit ? IDC_HAND
                                  : IDC_ARROW)); // Frame-local caches are bounded even across long sessions.
    if (motion.size() > 30000)
        motion.clear();
    if (labels.size() > 10000)
        labels.clear();
}
std::vector<Media *> UI::filtered() {
    static std::vector<std::string> categories = {
        "Все категории", "Видео", "Аудио", "Субтитры", "Автосубтитры", "Превью",
        "HLS", "DASH", "Данные", "Изображения", "Защищённые", "Требуют проверки"};
    std::vector<Media *> out;
    auto lower = [](std::string s) {
        auto w = wide(s);
        std::transform(w.begin(), w.end(), w.begin(), ::towlower);
        return utf8(w);
    };
    std::string q = lower(app.search);
    for (auto &m : app.media)
        if (matchesMediaCategory(m, categories[std::clamp<int>(app.category, 0, int(categories.size() - 1))]) &&
            (q.empty() ||
             lower(m.title + " " + m.language + " " + m.id + " " + m.vcodec + " " + m.acodec).find(q) !=
                 std::string::npos))
            out.push_back(&m);
    auto field = [&](Media *m) {
        switch (sortColumn) {
        case 0:
            return m->category;
        case 1:
            return m->title;
        case 2:
            return m->quality;
        case 4:
            return m->vcodec;
        case 5:
            return m->acodec;
        case 6:
            return m->language;
        case 9:
            return m->protocol;
        default:
            return m->id;
        }
    };
    std::stable_sort(out.begin(), out.end(), [&](Media *a, Media *b) {
        bool less;
        if (sortColumn == 3)
            less = a->fps < b->fps;
        else if (sortColumn == 7)
            less = a->rate < b->rate;
        else if (sortColumn == 8)
            less = a->size < b->size;
        else
            less = field(a) < field(b);
        if (descending) {
            if (sortColumn == 3)
                return a->fps > b->fps;
            if (sortColumn == 7)
                return a->rate > b->rate;
            if (sortColumn == 8)
                return a->size > b->size;
            return field(a) > field(b);
        }
        return less;
    });
    return out;
}
void UI::startMediaAnalysis(bool deep) {
    app.category = 1; cardView = true;
    mediaScroll.p.target = mediaScroll.p.value = 0;
    app.action(deep ? "deepAnalyze" : "analyze");
}
void UI::mediaPage(Rect r) {
    if(app.constructorOpen){constructorPage(r);return;}
    if(app.downloadChoice){downloadChoicePage(r);return;}
    float saved=198;
    Rect search{r.x, r.y, r.w, 78};
    card(search);
    input("url", {r.x + 14, r.y + 10, r.w - 524, 36}, app.url, "Ссылка на страницу или поток");
    button("deepAnalyze", {r.x + r.w - 500, r.y + 10, 154, 36}, "Глубокий анализ", [this] { startMediaAnalysis(true); });
    button("clearMedia", {r.x + r.w - 336, r.y + 10, 144, 36}, "Очистить список", [this] { app.action("clearMedia"); });
    button(
        "analyze", {r.x + r.w - 182, r.y + 10, 168, 36}, app.busy ? "Отменить" : "Анализировать",
        [this] { if (app.busy) app.action("cancelAnalysis"); else startMediaAnalysis(false); }, true);
    c.circle(r.x + 18, r.y + 61, 3, app.session() && app.session()->alive && app.session()->verified && app.session()->setupReady ? accent : muted);
    label("connection", app.connection, r.x + 29, r.y + 65, 11, muted, std::max(80.f, r.w - 500));
    if (app.busy || app.scanning) {
        float pulse=app.reduced?1.f:float(.55+.45*std::sin(clock*5));
        c.circle(r.x+r.w-447,r.y+61,3+2*pulse,accent.alpha(.5f+.5f*pulse));
        c.text((app.busy ? "Анализируем страницу" : "Ищем медиа во вкладке") + std::string(" · найдено ") + std::to_string(app.media.size()), r.x + r.w - 435, r.y + 65, 11, accent, 290);
    }
    else if (app.browserIndex >= 0 && app.browserIndex < int(app.browsers.size())) {
        auto &browser = app.browsers[app.browserIndex];
        if (app.profileIndex >= 0 && app.profileIndex < int(browser.profiles.size()))
            c.text(browser.name + " · " + browser.profiles[app.profileIndex].status, r.x + r.w - 450, r.y + 65, 11, muted, 310);
    }
    int selected = 0;
    for (auto &m : app.media)
        if (m.selected)
            selected++;
    c.text(std::to_string(selected) + " выбрано", r.x + r.w - 130, r.y + 65, 11, accent);
    input("media:search", {r.x, r.y + 291 - saved, r.w - 410, 36}, app.search, "Поиск файлов…");
    std::vector<std::string> categories = {"Все категории", "Видео", "Аудио", "Субтитры", "Автосубтитры", "Превью", "HLS", "DASH", "Данные", "Изображения", "Защищённые", "Требуют проверки"};
    choice("category", {r.x + r.w - 400, r.y + 291 - saved, 166, 36}, categories, app.category,
           [this](int i) { app.category = i; });
    button("selectall", {r.x + r.w - 224, r.y + 291 - saved, 94, 36}, "Выбрать все", [this] {
        auto v = filtered();
        bool all = std::all_of(v.begin(), v.end(), [](auto *m) { return m->selected; });
        for (auto *m : v)
            if (!m->drm)
                m->selected = !all;
    });
    button("view",{r.x+r.w-120,r.y+291-saved,120,36},cardView?"Подробности":"Карточки",[this]{cardView=!cardView;mediaScroll.p.target=0;});
    Rect table{r.x, r.y + 342 - saved, r.w, std::max(100.f, r.h - 462 + saved)};
    card(table);
    if(cardView){
        auto rows=filtered();
        int columns=std::max(1,int((table.w-18)/224));
        float cellWidth=(table.w-22)/columns,cellHeight=height<760?234.f:258.f;
        Rect body=table.inset(10);body.w-=12;
        mediaScroll.bounds(std::ceil(float(rows.size())/columns)*cellHeight,body.h);
        c.push(body);
        for(size_t i=0;i<rows.size();++i){
            auto &m=*rows[i];Rect box{body.x+int(i%columns)*cellWidth,body.y+int(i/columns)*cellHeight-mediaScroll.p.value,cellWidth-10,cellHeight-10};
            if(box.y+box.h<body.y||box.y>body.y+body.h)continue;
            auto id="tile:"+m.key;
            c.border(box,12,m.selected?accent:edge,panel,m.selected?3.5f:1.f);
            Rect image{box.x+6,box.y+6,box.w-12,box.h-102};
            previewTile(m,image);
            if (m.selected) c.rect(box.inset(3.5f), 9, accent.alpha(.12f));
            auto name = m.format && m.page == app.analysis.page ? app.analysis.title : m.title;
            if (name.empty()) name = "Медиа " + std::to_string(i + 1);
            c.text(name,box.x+10,box.y+box.h-75,12,text,box.w-20);
            double seconds = number(m.data, "duration");
            if (seconds <= 0 && m.page == app.analysis.page) seconds = number(app.analysis.raw, "duration");
            std::string duration;
            if (seconds > 0) { auto whole = int(seconds); duration = " · " + std::to_string(whole / 60) + ":" + (whole % 60 < 10 ? "0" : "") + std::to_string(whole % 60); }
            bool video = matchesMediaCategory(m, "Видео");
            std::string info = (video ? "Видео" : m.category) + (m.quality.empty() ? "" : " · " + m.quality) + duration;
            c.text(info,box.x+10,box.y+box.h-54,10,muted,box.w-20);
            std::string properties = m.ext.empty() ? "" : m.ext;
            if (m.fps > 0) properties += (properties.empty() ? "" : " · ") + std::to_string(int(m.fps)) + " FPS";
            if (m.size > 0) properties += (properties.empty() ? "" : " · ") + sizeText(m.size, m.approx);
            if (properties.empty()) properties = "Размер и формат не указаны";
            c.text(properties,box.x+10,box.y+box.h-36,10,muted,box.w-20);
            std::string codecs = m.vcodec.empty() || m.vcodec == "none" ? "" : m.vcodec;
            if (!m.acodec.empty() && m.acodec != "none") codecs += (codecs.empty() ? "" : " · ") + m.acodec;
            if (!m.language.empty()) codecs += (codecs.empty() ? "" : " · ") + m.language;
            if (codecs.empty()) codecs = "Кодеки не указаны";
            c.text(codecs,box.x+10,box.y+box.h-18,10,muted,box.w-20);
            if (video) {
                auto sound = videoSoundLabel(m);
                float badgeWidth = std::min(image.w - 40.f, c.measure(sound, 10) + 18.f);
                Rect badge{image.x + image.w - badgeWidth - 5, image.y + 5, badgeWidth, 22};
                c.rect(badge, 7, hex(m.videoOnly || m.category == "Видео без звука" ? 0x8f462a : 0x263a48, .94f));
                c.text(sound, badge.x + 9, badge.y + 15, 10, hex(0xffffff), badge.w - 14);
            }
            hit(id,box,[this,key=m.key]{for(auto &item:app.media)if(item.key==key){app.selectedMedia=key;if(!item.drm)item.selected=!item.selected;break;}});
            check(id+":check",{box.x+6,box.y+7,30,30},m.selected,[this,key=m.key]{for(auto &item:app.media)if(item.key==key&&!item.drm){item.selected=!item.selected;app.selectedMedia=key;}});
        }
        c.pop();scrollbar("media:scroll",{table.x,table.y+6,table.w-5,table.h-12},mediaScroll);
        if(rows.empty())c.text(app.scanning?"Поиск включён — запустите медиа в браузере":"Нет файлов. Добавьте ссылку или включите поиск во вкладке «Подключение».",table.x+20,table.y+40,12,muted,table.w-40);
    }else{
    float contentWidth = std::max(1120.f, table.w - 28);
    tableScroll.step(dt);
    float hx = tableScroll.p.value; // horizontal wheel is handled below via keyboard-free Shift gesture.
    auto columns = std::vector<float>{42, 205, 125, 102, 55, 116, 110, 80, 75, 118, 110};
    float total = std::accumulate(columns.begin(), columns.end(), 0.f);
    tableScroll.bounds(total, table.w - 28);
    float ratio = std::max(1.f, (table.w - 20) / total);
    for (auto &v : columns)
        v *= ratio;
    Rect body{table.x + 4, table.y + 42, table.w - 8, table.h - 59};
    mediaScroll.bounds(filtered().size() * 49, body.h);
    Rect scrollArea = body;
    if (mediaScroll.maximum > 0) body.w -= 18;
    Rect headerClip = table.inset(3);
    if (mediaScroll.maximum > 0) headerClip.w -= 18;
    c.push(headerClip);
    c.rect({table.x + 1, table.y + 1, table.w - 2, 41}, 16, mix(panel, hex(0x26354b), .3f));
    const char *heads[] = {"Тип",   "Название / ID", "Качество", "FPS",    "Видео",
                           "Аудио", "Язык",          "kbps",     "Размер", "Протокол"};
    float x = table.x + columns[0] - hx;
    for (int i = 0; i < 10; i++) {
        float cw = columns[i + 1];
        std::string id = "sort:" + std::to_string(i);
        float h = hover(id, {x, table.y, cw, 42});
        c.rect({x, table.y + 3, cw - 4, 35}, 7, accent.alpha(h * .07f));
        c.text(std::string(heads[i]) + (sortColumn == i ? (descending ? " ↓" : " ↑") : ""), x + 10,
               table.y + 27, 10, sortColumn == i ? accent : muted, cw - 14);
        hit(id, {x, table.y, cw, 42}, [this, i] {
            if (sortColumn == i)
                descending = !descending;
            else {
                sortColumn = i;
                descending = false;
            }
        });
        x += cw;
    }
    c.pop();
    auto rows = filtered();
    c.push(body);
    for (size_t i = 0; i < rows.size(); i++) {
        auto *m = rows[i];
        float target = float(i) * 49;
        auto [y, created] = rowY.try_emplace(m->key, target);
        float a = 1 - std::exp(-dt * 18);
        y->second += (target - y->second) * a;
        Rect row{body.x, body.y + y->second - mediaScroll.p.value, body.w, 46};
        if (row.y + 49 < body.y || row.y > body.y + body.h)
            continue;
        auto id = "row:" + m->key;
        float h = hover(id, row), sel = to(id + ":selected", app.selectedMedia == m->key ? 1 : 0);
        c.rect(row, 9, mix(panel, accent, .025f + (i % 2) * .025f + h * .05f + sel * .06f));
        hit(id, row, [this, key = m->key] { app.selectedMedia = key; });
        check(id + ":check", {row.x + 5, row.y, 30, row.h}, m->selected, [this, key = m->key] {
            for (auto &m : app.media)
                if (m.key == key && !m.drm) {
                    m.selected = !m.selected;
                    app.selectedMedia = key;
                    break;
                }
        });
        Ink category = m->drm                                             ? hex(0xff8d8d)
                       : m->category == "Аудио"                           ? hex(0xc9a7ff)
                       : m->category.find("Субтитры") != m->category.npos ? hex(0xffd18d)
                                                                          : hex(0x8bbfff);
        std::vector<std::string> values = {m->category,
                                           m->title.empty() ? m->id : m->title,
                                           m->quality,
                                           m->fps > 0 ? std::to_string(int(m->fps)) : "—",
                                           m->vcodec.empty() ? "—" : m->vcodec,
                                           m->acodec.empty() ? "—" : m->acodec,
                                           m->language.empty() ? "—" : m->language,
                                           m->rate > 0 ? std::to_string(int(m->rate)) : "—",
                                           sizeText(m->size, m->approx),
                                           m->protocol};
        c.push({table.x + columns[0], body.y, std::max(0.f, body.x + body.w - table.x - columns[0]), body.h});
        x = table.x + columns[0] - hx;
        for (int k = 0; k < 10; k++) {
            c.text(values[k], x + 10, row.y + 28, 11.5f, k == 0 ? category : text, columns[k + 1] - 16);
            x += columns[k + 1];
        }
        c.pop();
    }
    c.pop();
    scrollbar("media:scroll", scrollArea, mediaScroll);
    if (tableScroll.maximum > 0) {
        Rect track{table.x + 12, table.y + table.h - 6, table.w - 28, 4};
        float thumb = std::max(30.f, track.w * track.w / (track.w + tableScroll.maximum)),
              pos = track.x +
                    (track.w - thumb) * std::clamp(tableScroll.p.value / tableScroll.maximum, 0.f, 1.f);
        c.rect(track, 2, edge.alpha(.4f));
        c.rect({pos, track.y, thumb, 4}, 2, muted.alpha(.55f));
        auto adjust = [this, track, thumb] {
            tableScroll.p.target =
                std::clamp((mx - track.x - thumb / 2) / (track.w - thumb) * tableScroll.maximum, 0.f,
                           tableScroll.maximum);
        };
        hit("media:hscroll", {track.x, track.y - 5, track.w, 12}, adjust);
        if (down && capture && capture->id == "media:hscroll")
            adjust();
    }
    }
    float bottom = r.y + r.h - 98;
    c.text("Выбранные файлы сохраняются раздельно в одну папку", r.x, bottom + 3, 10, muted);
    input("destination", {r.x, bottom + 15, r.w - 250, 36}, app.destination, "Папка сохранения");
    button("folder", {r.x + r.w - 238, bottom + 15, 78, 36}, "Папка", [this] { app.action("folder"); });
    button(
        "enqueue", {r.x + r.w - 150, bottom + 15, 150, 36}, "Скачать ↓", [this] { cancelInput(); app.downloadChoice=true; },
        true, selected > 0);
    button(
        "resource", {r.x, bottom + 63, 166, 32}, "Анализ ресурса", [this] { app.action("analyzeResource"); },
        false, !app.selectedMedia.empty());
    button("json", {r.x+178,bottom+63,126,32},"Метаданные",[this]{app.action("json");});
}
void UI::previewTile(const Media &media,Rect box){
    c.rect(box,7,hex(app.light?0xe5eaf1:0x0b121d));
    auto preview=app.requestPreview(media);
    auto key=std::to_string(app.previewGeneration) + "|" + (media.thumbnail.empty()?media.key:media.thumbnail);
    if(preview&&!preview->image.rgba.empty()){
        if(!previewTextures.count(key)){
            if(previewTextures.size()>=32){auto old=previewTextures.begin();c.deleteImage(old->second);previewTextures.erase(old);}
            previewTextures[key]=c.createImage(preview->image);
        }
        c.image(previewTextures[key],box,preview->image.width,preview->image.height);
    }else{
        c.text(preview?preview->message:"Ожидание превью…",box.x+12,box.y+box.h/2,11,muted,box.w-24);
    }
}
void UI::browserPage(Rect r) {
    Rect view{r.x, r.y, r.w, r.h};
    browserScroll.bounds(578, view.h);
    if (browserScroll.maximum > 0) r.w -= 18;
    c.push(view);
    float y = view.y - browserScroll.p.value;
    card({r.x, y, r.w, 200});
    c.text("01  ВЫБЕРИТЕ БРАУЗЕР", r.x + 22, y + 29, 10, accent);
    std::vector<std::string> options;
    for (auto &b : app.browsers)
        options.push_back(b.name + (b.running ? " · открыт" : " · закрыт"));
    choice("browser", {r.x + 22, y + 48, r.w - 174, 40}, options, app.browserIndex, [this](int i) {
        app.browserIndex = i;
        app.profileIndex = 0;
    });
    button("browser:open", {r.x + r.w - 140, y + 48, 119, 40}, "Открыть",
           [this] { app.action("openBrowser"); });
    std::vector<std::string> profiles;
    std::string info = "Браузеры не найдены. Откройте установленный браузер и обновите список.";
    if (app.browserIndex < int(app.browsers.size())) {
        auto &b = app.browsers[app.browserIndex];
        for (auto &p : b.profiles)
            profiles.push_back(p.name);
        if (app.profileIndex < int(b.profiles.size()))
            info = b.profiles[app.profileIndex].status + " · " + b.profiles[app.profileIndex].path;
    }
    choice("profile", {r.x + 22, y + 103, r.w - 44, 38}, profiles, app.profileIndex,
           [this](int i) { app.profileIndex = i; });
    c.text(info, r.x + 22, y + 174, 11, muted, r.w - 470);
    button("browser:custom", {r.x + r.w - 430, y + 151, 199, 32}, "Указать EXE",
           [this] { app.action("customBrowser"); });
    button("browser:customProfile", {r.x + r.w - 220, y + 151, 199, 32}, "Папка профиля",
           [this] { app.action("customProfile"); });
    y += 216;
    card({r.x, y, r.w, 158});
    c.text("02  УСТАНОВИТЕ ЛОКАЛЬНОЕ РАСШИРЕНИЕ", r.x + 22, y + 30, 10, accent);
    bool guided = app.browserIndex >= 0 && app.browserIndex < int(app.browsers.size()) &&
        guidedBrowser(app.browsers[app.browserIndex]) != GuidedBrowser::None;
    c.text(guided ? "Мастер покажет шаги установки и подсветит доступные элементы браузера."
                  : "Для этого браузера расширение устанавливается вручную. Папку подготовит программа.", r.x + 22, y + 57, 12, muted,
           r.w - 44);
    button(
        "prepare", {r.x + 22, y + 83, 288, 40}, app.preparingInstaller ? "Подготовка…" : guided ? "Установить с подсказками" : "Подготовить для ручной установки",
        [this] { app.action("prepareConnector"); }, true, !app.preparingInstaller);
    button("ext:folder", {r.x + 326, y + 83, 213, 40}, "Папка расширения",
           [this] { app.action("extensionFolder"); });
    y += 174;
    card({r.x, y, r.w, 157});
    c.text("COOKIES И АВТОРИЗАЦИЯ", r.x + 22, y + 30, 10, accent);
    c.text(fs::exists(dataRoot() / L"cookies.dpapi")
               ? "Импорт cookies подключён · защищён Windows DPAPI"
               : "Cookies запрашиваются у расширения только для выбранного ресурса.",
           r.x + 22, y + 59, 12, muted, r.w - 44);
    button("cookie:import", {r.x + 22, y + 83, 210, 40}, "Импорт cookies.txt",
           [this] { app.action("importCookies"); });
    button("cookie:delete", {r.x + 245, y + 83, 184, 40}, "Удалить импорт",
           [this] { app.action("deleteCookies"); });
    c.pop();
    scrollbar("browser:scroll", view, browserScroll);
}
void UI::downloadsPage(Rect r) {
    c.text("Загрузки · "+std::to_string(app.jobs.size()), r.x, r.y + 28, 20, text, r.w - 400);
    button("jobs:view", {r.x + r.w - 390, r.y + 8, 140, 38}, compactJobs ? "Карточки" : "Компактно", [this] { compactJobs = !compactJobs; jobsScroll.p.target = jobsScroll.p.value = 0; });
    choice(
        "parallel", {r.x + r.w - 240, r.y + 8, 240, 38},
        {"1 задача одновременно", "2 задачи одновременно", "3 задачи одновременно", "4 задачи одновременно"},
        app.getParallel() - 1, [this](int i) { app.setParallel(i + 1); });
    float topWidth = (r.w - 40) / 5;
    button("jobs:selectAll", {r.x, r.y + 54, topWidth, 34}, "Выбрать все", [this] { app.action("selectAllJobs"); }, false, !app.jobs.empty());
    button("jobs:clearSelected", {r.x + topWidth + 10, r.y + 54, topWidth, 34}, "Убрать выбранные", [this] { app.action("clearSelectedJobs"); }, false, !app.selectedJobs.empty());
    button("jobs:clearFinished", {r.x + 2*(topWidth + 10), r.y + 54, topWidth, 34}, "Очистить завершённые", [this] { app.action("clearFinishedJobs"); }, false, !app.jobs.empty());
    button("jobs:constructor", {r.x + 3*(topWidth + 10), r.y + 54, topWidth, 34}, "Конструктор", [this] { cancelInput(); app.openConstructor(true,true); });
    button("jobs:addLocal", {r.x + 4*(topWidth + 10), r.y + 54, topWidth, 34}, "Добавить файлы", [this] { app.action("addLocalDownloads"); });
    float viewTop = 96;
    for(const auto &job:app.jobs)if(job->id==app.selectedJob&&job->assembly&&job->decisionNeeded){
        card({r.x,r.y+96,r.w,100});
        c.text("Не все источники доступны. Сохранённые файлы оставлены отдельно.",r.x+14,r.y+121,12,text,r.w-28);
        button("assembly:retry",{r.x+14,r.y+140,210,38},"Повторить загрузки",[this]{app.action("retryAssemblySources");});
        bool canAssemble=false;
        for(const auto &source:job->sources)if(source.role<2){
            if(source.dependency.empty()&&!source.path.empty()&&fs::is_regular_file(wide(source.path)))canAssemble=true;
            for(const auto &child:app.jobs)if(child->id==source.dependency&&child->done&&child->state=="Готово"&&!child->output.empty()&&fs::is_regular_file(wide(child->output)))canAssemble=true;
        }
        button("assembly:partial",{r.x+238,r.y+140,220,38},"Собрать доступное",[this]{app.action("assemblePartial");},true,canAssemble);
        viewTop=206;break;
    }
    if(viewTop==96)for(const auto &job:app.jobs)if(job->state=="Сборка контейнера"||job->state=="Проверка результата"){
        card({r.x,r.y+96,r.w,122});
        c.text(job->state+" · "+job->title,r.x+14,r.y+119,13,text,r.w-220);
        c.text(job->detail,r.x+14,r.y+141,11,muted,r.w-220);
        int active=0,waiting=0;for(const auto &entry:app.jobs)if(!entry->done){if(entry->claimed)++active;else ++waiting;}
        c.text(job->metrics,r.x+14,r.y+160,10,muted,r.w-220);
        std::string device="CPU (автоматически)";for(const auto &source:job->sources)if(source.role==0&&source.codec>=5){device=source.codec<=6?"NVIDIA GPU":source.codec<=8?"Intel GPU":"AMD GPU";break;}
        c.text("Задач: "+std::to_string(active)+" выполняется · "+std::to_string(waiting)+" в очереди · "+device+" · логических ядер: "+std::to_string(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)),r.x+14,r.y+181,10,muted,r.w-220);
        Rect track{r.x+14,r.y+200,r.w-230,5};c.rect(track,3,edge);
        if(job->percent<0){float segment=track.w*.2f,offset=(track.w-segment)*float(.5+.5*std::sin(clock*2.4));c.rect({track.x+offset,track.y,segment,track.h},3,accent);}
        else c.rect({track.x,track.y,track.w*std::clamp(float(job->percent/100.),0.f,1.f),track.h},3,accent);
        auto id=job->id;button("assembly:active:cancel",{r.x+r.w-196,r.y+120,180,38},"Отменить",[this,id]{app.selectedJob=id;app.action("cancelJob");});
        viewTop=228;break;
    }
    Rect view{r.x, r.y + viewTop, r.w, std::max(40.f, r.h - viewTop - 58)};
    float rowHeight = compactJobs ? 54.f : height < 900 ? 106.f : 124.f;
    struct QueueRow {int group;float y;std::shared_ptr<Job> job;};
    std::vector<QueueRow> rows;float totalHeight=0;
    const std::string groupNames[]={"Загрузки", "Конвертация", "Файлы с компьютера", "Сборка контейнеров"};
    auto groupFor=[](const std::shared_ptr<Job> &job){
        if(job->localFile)return 2;
        if(job->conversionSource||(job->assembly&&std::any_of(job->sources.begin(),job->sources.end(),[](const auto &source){return source.enabled&&source.codec>0;})))return 1;
        return job->assembly||job->mux?3:0;
    };
    for(int group=0;group<4;++group){
        bool first=true;for(const auto &job:app.jobs)if(groupFor(job)==group){
            if(first){rows.push_back({group,totalHeight,{}});totalHeight+=34;first=false;}
            rows.push_back({group,totalHeight,job});totalHeight+=rowHeight;
        }
    }
    jobsScroll.bounds(totalHeight, view.h);
    c.push(view);
    if (app.jobs.empty()) {
        card({r.x, view.y + 4, r.w, 170});
        c.text("Здесь появятся ваши загрузки", r.x + 28, view.y + 55, 22, text);
        c.text("Выберите медиа в обзоре или перетащите сюда файлы из Проводника.", r.x + 28, view.y + 91, 13, muted);
    }
    for (const auto &entry:rows) {
        if(!entry.job){float y=view.y+entry.y-jobsScroll.p.value;if(y+34>=view.y&&y<view.y+view.h)c.text(groupNames[entry.group],r.x+12,y+24,12,accent,r.w-40);continue;}
        auto j = entry.job;
        Rect row{r.x, view.y + entry.y - jobsScroll.p.value, r.w - (jobsScroll.maximum > 0 ? 18.f : 0.f), rowHeight - 12};
        if (row.y + row.h < view.y || row.y > view.y + view.h)
            continue;
        float h = hover("job:" + j->id, row),
              sel = to("job:" + j->id + ":sel", app.selectedJobs.count(j->id) ? 1 : 0);
        card(row);
        bool checked=app.selectedJobs.count(j->id)!=0;
        c.border(row, 20, checked?accent:edge, panel, checked?3.5f:1.f);
        if (!compactJobs) previewTile(j->item,{row.x+12,row.y+10,90,row.h-38});
        if (sel > .001f) c.rect(row.inset(3.5f), 16, accent.alpha(.12f * sel));
        label("job:" + j->id + ":title", j->title, row.x + (compactJobs ? 48 : 114), row.y + (compactJobs ? 25 : 31), compactJobs ? 12 : 14, text, row.w - (compactJobs ? 230 : 295));
        label("job:" + j->id + ":state", j->state, row.x + row.w - 160, row.y + (compactJobs ? 25 : 31), 12,
              j->state == "Ошибка" ? hex(0xff9292) : accent, 140);
        if (!compactJobs) c.text(j->detail.empty() && !j->done ? "Обработка…" : j->detail, row.x + 114, row.y + 59, 11, muted, row.w - 135);
        bool processing=j->state=="Сборка контейнера"||j->state=="Проверка результата"||j->state=="Объединение"||j->state=="Скачивание";
        float progress = to("job:" + j->id + ":progress", std::clamp(float(j->percent / 100),0.f,1.f), 0, 4, 1);
        Rect track{row.x + 22, row.y + row.h - (compactJobs ? 5 : 19), row.w - 44, compactJobs ? 2.f : 5.f};
        c.rect(track,3,edge);
        if(processing && j->percent<0) {
            float segment=track.w*.2f,offset=(track.w-segment)*float(.5+.5*std::sin(clock*2.4));
            c.rect({track.x+offset,track.y,segment,track.h},3,accent);
        } else if(progress>0)c.rect({track.x,track.y,track.w*progress,track.h},3,accent);
        hit("job:" + j->id, row, [this, j] { app.selectedJob = j->id; if (app.selectedJobs.count(j->id)) app.selectedJobs.erase(j->id); else app.selectedJobs.insert(j->id); });
        check("job:" + j->id + ":check", {row.x + (compactJobs ? 7 : 15), row.y + (compactJobs ? 6 : 10), 30, 30}, app.selectedJobs.count(j->id) != 0, [this, j] {
            app.selectedJob = j->id;
            if (app.selectedJobs.count(j->id)) app.selectedJobs.erase(j->id); else app.selectedJobs.insert(j->id);
        });
    }
    c.pop();
    scrollbar("jobs:scroll", view, jobsScroll);
    float y = r.y + r.h - 52;
    std::vector<std::pair<std::string, std::string>> actions = {{"cancelJob", "Отменить"},
                                                                {"retryJob", "Повторить"},
                                                                {"openJobFolder", "Открыть папку"},
                                                                {"openJobFile", "Открыть файл"},
                                                                {"removeJob", "Убрать запись"}};
    float x = r.x;
    float actionWidth = std::min(145.f, (r.w - 4 * 10) / 5);
    for (auto &[cmd, name] : actions) {
        button(
            "jobs:" + cmd, {x, y, actionWidth, 38}, name, [this, cmd] { app.action(cmd); }, false,
            !app.selectedJob.empty());
        x += actionWidth + 10;
    }
}
void UI::logsPage(Rect r) {
    input("logs:search", {r.x, r.y, r.w - 450, 40}, app.logSearch, "Поиск в журнале…");
    button("logs:copy", {r.x + r.w - 435, r.y, 137, 40}, "Скопировать",
           [this] { app.action("copyLogs"); });
    button("logs:export", {r.x + r.w - 286, r.y, 137, 40}, "Экспорт",
           [this] { app.action("exportLogs"); });
    button("logs:clear", {r.x + r.w - 137, r.y, 137, 40}, "Очистить",
           [this] { app.action("clearLogs"); });
    Rect box{r.x, r.y + 54, r.w, r.h - 104};
    card(box);
    Rect view = box.inset(17);
    std::vector<const std::string *> lines;
    for (auto &line : app.logs)
        if (app.logSearch.empty() || line.find(app.logSearch) != line.npos)
            lines.push_back(&line);
    if (lines.size() > 1500)
        lines.erase(lines.begin(), lines.end() - 1500);
    float old = logScroll.maximum;
    bool follow = logScroll.p.target >= old - 2;
    logScroll.bounds(lines.size() * 25, view.h);
    if (follow)
        logScroll.p.target = logScroll.maximum;
    std::vector<std::wstring> wideLines;std::vector<size_t> offsets;std::wstring buffer;
    for(const auto *line:lines){
        offsets.push_back(buffer.size());
        // A legacy or external log entry must never terminate the UI frame.
        int n=MultiByteToWideChar(CP_UTF8,0,line->data(),int(line->size()),nullptr,0);
        std::wstring decoded(size_t(std::max(n,0)),L'\0');
        if(n>0)MultiByteToWideChar(CP_UTF8,0,line->data(),int(line->size()),decoded.data(),n);
        wideLines.push_back(std::move(decoded));buffer+=wideLines.back();buffer+=L'\n';
    }
    logSelectionBuffer=std::move(buffer);logAnchor=std::min(logAnchor,logSelectionBuffer.size());logCaret=std::min(logCaret,logSelectionBuffer.size());
    auto position=[this,view,offsets,wideLines](float x,float y){
        if(wideLines.empty())return size_t(0);
        int line=std::clamp(int(std::floor((y-view.y+logScroll.p.value)/25.f)),0,int(wideLines.size())-1);
        float dx=x-view.x-4;size_t best=0;float distance=1e9;
        for(size_t n=0;n<=wideLines[line].size();++n){if(n>0&&n<wideLines[line].size()&&IS_LOW_SURROGATE(wideLines[line][n]))continue;float d=std::abs(c.measure(utf8(wideLines[line].substr(0,n)),12)-dx);if(d<distance){distance=d;best=n;}}
        return offsets[line]+best;
    };
    if(down&&capture&&capture->id=="log:select"&&std::hypot(mx-downX,my-downY)>3)logCaret=position(mx,my);
    c.push(view);
    if(lines.empty())c.text("Журнал пока пуст",view.x+3,view.y+26,13,muted);
    size_t lo=std::min(logAnchor,logCaret),hi=std::max(logAnchor,logCaret);
    for(size_t i=0;i<lines.size();++i){float y=view.y+20+i*25-logScroll.p.value;if(y<view.y-15||y>=view.y+view.h+20)continue;
        size_t start=offsets[i],end=start+wideLines[i].size(),a=std::max(lo,start),b=std::min(hi,end);
        if(focus=="log:selection"&&a<b){float l=c.measure(utf8(wideLines[i].substr(0,a-start)),12),rr=c.measure(utf8(wideLines[i].substr(0,b-start)),12);c.rect({view.x+4+l,y-15,rr-l,19},3,accent.alpha(.25f));}
        c.text(*lines[i],view.x+4,y,12,text,view.w-20);
    }
    hit("log:select",view,[this,position]{editing=nullptr;focus="log:selection";logAnchor=logCaret=position(mx,my);},true);
    c.pop();scrollbar("logs:scroll",view,logScroll);
    input("terminal:command",{r.x,r.y+r.h-38,r.w-126,36},app.terminalCommand,"Команда Windows…");
    button("terminal:run",{r.x+r.w-116,r.y+r.h-38,116,36},"Выполнить",[this]{app.action("runTerminal");},true,!app.terminalCommand.empty());
}
void UI::settingsPage(Rect r) {
    Rect view{r.x, r.y, r.w, r.h};
    settingsScroll.bounds(1000, view.h);
    if (settingsScroll.maximum > 0) r.w -= 18;
    c.push(view);
    float y = view.y - settingsScroll.p.value;
    card({r.x, y, r.w, 233});
    c.text("ОФОРМЛЕНИЕ И ДВИЖЕНИЕ", r.x + 24, y + 32, 10, accent);
    c.text("Светлая тема", r.x + 24, y + 77, 15, text);
    c.text("Мягкий переход между светлой и тёмной палитрой", r.x + 24, y + 101, 11, muted);
    toggle("settings:light", {r.x + r.w - 80, y + 62, 51, 28}, app.light, [this] { app.light = !app.light; });
    c.rect({r.x + 24, y + 124, r.w - 48, 1}, 0, edge);
    c.text("Уменьшить движение", r.x + 24, y + 162, 15, text);
    c.text("Более короткие переходы с минимальным пружинным эффектом", r.x + 24, y + 188, 11, muted);
    toggle("settings:reduced", {r.x + r.w - 80, y + 148, 51, 28}, app.reduced,
           [this] { app.reduced = !app.reduced; });
    y += 249;
    card({r.x, y, r.w, 237});
    c.text("ДВИЖОК И ИНСТРУМЕНТЫ", r.x + 24, y + 32, 10, accent);
    label("engine:status", app.status, r.x + 24, y + 71, 14, text, r.w - 60);
    c.text("Проверять официальный стабильный выпуск yt-dlp при запуске", r.x + 24, y + 111, 13, text,
           r.w - 140);
    toggle("settings:update", {r.x + r.w - 80, y + 93, 51, 28}, app.autoUpdate,
           [this] { app.autoUpdate = !app.autoUpdate; });
    button(
        "settings:updateNow", {r.x + 24, y + 146, 237, 40}, "Проверить обновление",
        [this] { app.action("update"); }, true);
    button("settings:data", {r.x + 276, y + 146, 210, 40}, "Папка данных",
           [this] { app.action("dataFolder"); });
    c.text("yt-dlp · FFmpeg · Deno. Обновления проверяются по SHA-256.", r.x + 24, y + 218, 11, muted,
           r.w - 48);
    y += 253;
    card({r.x, y, r.w, 217});
    c.text("ЗАГРУЗКИ И ПОДКЛЮЧЕНИЕ", r.x + 24, y + 32, 10, accent);
    c.text("Параллельные фрагменты на одну загрузку", r.x + 24, y + 77, 13, text);
    choice("settings:fragments", {r.x + r.w - 230, y + 53, 200, 40},
           {"1 фрагмент", "2 фрагмента", "4 фрагмента", "8 фрагментов"},
           app.fragments == 1   ? 0
           : app.fragments == 2 ? 1
           : app.fragments == 4 ? 2
                                : 3,
           [this](int i) { app.fragments = 1 << i; });
    button("settings:unregister", {r.x + 24, y + 112, 293, 38}, "Удалить регистрацию подключения",
           [this] { app.action("unregisterConnector"); });
    c.text("DRM не обходится. Авторизация используется только для вашего контента.", r.x + 24, y + 188, 11,
           muted, r.w - 48);
    y += 233;
    card({r.x, y, r.w, 90});
    c.text("Сохранять предыдущие результаты анализа", r.x + 24, y + 48, 13, text, r.w - 130);
    toggle("settings:keepResults", {r.x + r.w - 80, y + 28, 51, 28}, app.keepResults,
           [this] { app.keepResults = !app.keepResults; });
    y += 106;
    card({r.x, y, r.w, 138});
    c.text("ЗАСТАВКА ПРИ ЗАПУСКЕ", r.x + 24, y + 30, 10, accent);
    c.text("Показывать заставку", r.x + 24, y + 70, 13, text);
    toggle("settings:splash", {r.x + r.w - 80, y + 50, 51, 28}, app.splashEnabled,
           [this] { app.splashEnabled = !app.splashEnabled; });
    c.text("Длительность", r.x + 24, y + 113, 13, text);
    choice("settings:splashDuration", {r.x + r.w - 230, y + 86, 200, 38},
           {"1 секунда", "2 секунды", "3 секунды", "4 секунды", "5 секунд", "6 секунд", "7 секунд", "8 секунд", "9 секунд", "10 секунд"},
           app.splashSeconds - 1, [this](int i) { app.splashSeconds = i + 1; });
    c.pop();
    scrollbar("settings:scroll", view, settingsScroll);
}
void UI::aboutPage(Rect r) {
    Rect view{r.x, r.y, r.w, r.h};
    aboutScroll.bounds(880, view.h);
    if (aboutScroll.maximum > 0) r.w -= 18;
    c.push(view);
    float y = r.y - aboutScroll.p.value;
    card({r.x, y, r.w, 152});
    c.text("KRECHETOV LABS", r.x + 24, y + 30, 11, accent, r.w - 48);
    c.text("StreamShelf", r.x + 24, y + 69, 25, text, r.w - 48);
    c.text("Версия 2.2 Beta · Windows x64", r.x + 24, y + 100, 12, muted, r.w - 48);
    c.text("Разработчик: Арсений Кречетов", r.x + 24, y + 126, 12, muted, r.w - 48);
    y += 168;
    card({r.x, y, r.w, 220});
    c.text("О ПРОГРАММЕ", r.x + 24, y + 30, 10, accent, r.w - 48);
    c.text("StreamShelf помогает находить и сохранять доступные медиа со страниц браузера.", r.x + 24, y + 62, 12, text, r.w - 48);
    c.text("Расширение передаёт активную вкладку и доступные данные для анализа.", r.x + 24, y + 87, 12, text, r.w - 48);
    c.text("Обычный анализ использует yt-dlp; глубокий дополняет его данными страницы.", r.x + 24, y + 112, 12, text, r.w - 48);
    c.text("Результаты доступны в карточках и подробном списке с выбором форматов.", r.x + 24, y + 137, 12, text, r.w - 48);
    c.text("Для динамических потоков может понадобиться запустить воспроизведение.", r.x + 24, y + 162, 12, muted, r.w - 48);
    c.text("Доступность скачивания зависит от источника и его ограничений.", r.x + 24, y + 187, 12, muted, r.w - 48);
    y += 236;
    card({r.x, y, r.w, 205});
    c.text("ЗАГРУЗКА И ОБРАБОТКА", r.x + 24, y + 30, 10, accent, r.w - 48);
    c.text("Очередь загрузок поддерживает выбор файлов, повтор и открытие результата.", r.x + 24, y + 62, 12, text, r.w - 48);
    c.text("FFmpeg объединяет скачанные видео, аудио, субтитры и главы в контейнер.", r.x + 24, y + 87, 12, text, r.w - 48);
    c.text("Доступны MKV, MP4, MOV и WebM, копирование потоков и выбор кодеков.", r.x + 24, y + 112, 12, text, r.w - 48);
    c.text("FFprobe уточняет параметры и проверяет обработанные файлы.", r.x + 24, y + 137, 12, text, r.w - 48);
    c.text("Поддержка конкретного формата зависит от контейнера, кодеков и источника.", r.x + 24, y + 170, 12, muted, r.w - 48);
    y += 221;
    card({r.x, y, r.w, 238});
    c.text("РАЗРАБОТКА И ПОДКЛЮЧЕНИЕ", r.x + 24, y + 30, 10, accent, r.w - 48);
    c.text("Нативное приложение на C++ и WinAPI; интерфейс отрисовывается через OpenGL.", r.x + 24, y + 62, 12, text, r.w - 48);
    c.text("Мастер установки расширения доступен для Chrome и Яндекс Браузера.", r.x + 24, y + 87, 12, text, r.w - 48);
    c.text("Связь с расширением использует локальный Native Messaging.", r.x + 24, y + 112, 12, text, r.w - 48);
    c.text("Для поиска на сайтах расширению требуется подтверждённый доступ в браузере.", r.x + 24, y + 137, 12, text, r.w - 48);
    c.text("В работе используются yt-dlp, FFmpeg, FFprobe и Deno.", r.x + 24, y + 162, 12, text, r.w - 48);
    c.text("Информация о лицензиях сторонних компонентов находится в папке программы.", r.x + 24, y + 194, 12, muted, r.w - 48);
    c.pop();
    scrollbar("about:scroll", view, aboutScroll);
}
void UI::overlays() {
    float menu = to("menu:visibility", menuOpen ? 1 : 0, 0, 6, 1);
    if (menu > .002f && !menuId.empty()) {
        hits.clear();
        hit("menu:outside", {0, 0, width, height}, [this] { menuOpen = false; });
        float h = std::min(300.f, float(menuItems.size() * 38 + 16)),
              top = std::min(menuAnchor.y + menuAnchor.h + 8, height - h - 20);
        Rect box{std::min(menuAnchor.x, width - 260), top, std::max(240.f, menuAnchor.w), h};
        overlayInput = true;
        c.opacity *= menu;
        c.offsetY = -9 * (1 - menu);
        c.glow({box.x - 32, box.y - 20, box.w + 64, box.h + 70}, hex(0, .4f));
        c.border(box, 16, edge, panel);
        c.push(box.inset(5));
        menuScroll.bounds(menuItems.size() * 38, box.h - 16);
        for (int i = 0; i < int(menuItems.size()); i++) {
            Rect row{box.x + 7, box.y + 8 + i * 38 - menuScroll.p.value, box.w - 14, 35};
            if (row.y + row.h < box.y || row.y > box.y + box.h)
                continue;
            button("menu:item:" + std::to_string(i), row, menuItems[i], [this, i] {
                if (menuSelect)
                    menuSelect(i);
                menuOpen = false;
            });
        }
        c.pop();
        c.offsetY = 0;
        c.opacity = appearance.value;
        overlayInput = false;
    }
    float mv = to("modal:visibility", app.modalTitle.empty() ? 0 : 1, 0, 4.5f, 1);
    static std::string lastTitle, lastBody;
    if (!app.modalTitle.empty()) {
        lastTitle = app.modalTitle;
        lastBody = app.modalBody;
    }
    if (mv > .002f) {
        hits.clear();
        hit("modal:outside", {0, 0, width, height}, [this] {
            app.modalTitle.clear();
            app.modalConfirm = nullptr;
        });
        c.drawSnapshot({0, 0, width, height}, mv, true);
        c.rect({0, 0, width, height}, 0, hex(0x020711, mv * .67f));
        float w = std::min(810.f, width - 130), h = std::min(610.f, height - 150);
        Rect box = Rect{(width - w) / 2, (height - h) / 2 + (1 - mv) * 20, w, h}.scale(.97f + .03f * mv);
        overlayInput = true;
        c.opacity *= mv;
        card(box);
        hit("modal:block", box, [] {});
        c.text(lastTitle, box.x + 28, box.y + 44, 23, text, box.w - 80);
        Rect body{box.x + 26, box.y + 68, box.w - 52, box.h - 146};
        static std::string cachedBody;
        static float cachedWidth = 0;
        static std::vector<std::string> lines;
        if (cachedBody != lastBody || cachedWidth != body.w) {
            cachedBody = lastBody;
            cachedWidth = body.w;
            lines.clear();
            std::istringstream input(lastBody.substr(0, 1024 * 1024));
            std::string line;
            while (std::getline(input, line)) {
                if (line.empty()) {
                    lines.push_back("");
                    continue;
                }
                std::string current;
                float lineWidth = 0;
                for (auto cp : codepoints(line)) {
                    std::wstring w;
                    if (cp > 0xffff) {
                        uint32_t v = cp - 0x10000;
                        w += wchar_t(0xd800 + (v >> 10));
                        w += wchar_t(0xdc00 + (v & 1023));
                    } else
                        w += wchar_t(cp);
                    auto piece = utf8(w);
                    float advance = c.measure(piece, 13);
                    if (lineWidth + advance > body.w - 24 && !current.empty()) {
                        lines.push_back(current);
                        current.clear();
                        lineWidth = 0;
                    }
                    current += piece;
                    lineWidth += advance;
                }
                lines.push_back(current);
                if (lines.size() > 10000) {
                    lines.push_back("…");
                    break;
                }
            }
        }
        modalScroll.bounds(lines.size() * 21, body.h);
        c.push(body);
        for (size_t i = 0; i < lines.size(); i++) {
            float y = body.y + 17 + i * 21 - modalScroll.p.value;
            if (y >= body.y - 20 && y < body.y + body.h + 20)
            c.text(lines[i], body.x, y, 13, muted, body.w - 20);
        }
        c.pop();
        scrollbar("modal:scroll", body, modalScroll);
        if (app.modalConfirm)
            button(
                "modal:confirm", {box.x + box.w - 390, box.y + box.h - 60, 210, 38}, app.modalConfirmLabel,
                [this] {
                    auto f = app.modalConfirm;
                    app.modalTitle.clear();
                    app.modalConfirm = nullptr;
                    if (f)
                        f();
                },
                true);
        button("modal:close", {box.x + box.w - 164, box.y + box.h - 60, 136, 38}, "Закрыть", [this] {
            app.modalTitle.clear();
            app.modalConfirm = nullptr;
        });
        c.opacity = appearance.value;
        overlayInput = false;
    }
    if (lastToast != app.toastSerial) {
        lastToast = app.toastSerial;
        toastTime = clock;
    }
    float tv = to("toast:visibility",
                  !app.toast.empty() && clock - toastTime < 6 && app.modalTitle.empty() && mv < .01f ? 1 : 0,
                  0, 5, 1);
    if (tv > .002f) {
        Rect r{width / 2 - 260, height - 78 + (1 - tv) * 16, 520, 43};
        c.opacity *= tv;
        c.border(r, 14, accent.alpha(.35f), panel);
        c.text(app.toast, r.x + 16, r.y + 27, 12, text, r.w - 50);
        if (mv < .01f)
            hit("toast", r, [this] { toastTime = -100; });
        c.opacity = appearance.value;
    }
}
void UI::mouse(float x, float y) {
    mx = x / scale;
    my = y / scale;
}
void UI::rightClick(float x,float y) {
    mouse(x,y);if(!app.modalTitle.empty())return;
    for(auto it=hits.rbegin();it!=hits.rend();++it)if(it->edit&&it->rect.contains(mx,my)){
        if(focus!=it->id && !(it->id=="log:select"&&focus=="log:selection")){if(it->action)it->action();}
        bool logFocus=focus=="log:selection";
        menuId="context:text";menuAnchor={mx,my,1,1};menuItems=logFocus?std::vector<std::string>{"Копировать","Выделить всё"}:std::vector<std::string>{"Копировать","Вырезать","Вставить","Выделить всё"};
        menuSelect=[this,logFocus](int n){if(logFocus){key(n==0?'C':'A',true,false);}else{const int keys[]={'C','X','V','A'};if(n>=0&&n<4)key(keys[n],true,false);}};
        menuScroll.p.target=menuScroll.p.value=0;menuOpen=true;return;
    }
}
void UI::press() {
    down = true;
    downX = mx;
    downY = my;
    capture.reset();
    for (auto i = hits.rbegin(); i != hits.rend(); ++i)
        if (i->rect.contains(mx, my)) {
            capture = *i;
            break;
        }
    bool contextAction = menuOpen && menuId == "context:text" && capture && capture->id.rfind("menu:item:", 0) == 0;
    if ((!capture || !capture->edit) && !contextAction) {
        focus.clear();
        editing = nullptr;
    }
    if (capture && capture->edit && capture->action)
        capture->action();
}
void UI::release() {
    auto old = capture;
    down = false;
    capture.reset();
    // Release OS capture before an action maximizes/minimizes the window.
    if (GetCapture() == app.hwnd) ReleaseCapture();
    if (old && app.constructorOpen && finishDraftDrag(*old)) return;
    if (old && !old->edit && old->rect.contains(mx, my) && std::hypot(mx - downX, my - downY) < 9 &&
        old->action)
        old->action();
}
void UI::lostCapture() {
    down = false;
    capture.reset();
}
void UI::cancelInput() {
    lostCapture();
    focus.clear();
    editing = nullptr;
}
void UI::wheel(float d, bool shift) {
    lostCapture();
    if (!app.modalTitle.empty()) {
        modalScroll.wheel(d);
        return;
    }
    if (menuOpen) {
        menuScroll.wheel(d);
        return;
    }
    if(page==0&&app.constructorOpen){
        if(poolView.contains(mx,my))poolScroll.wheel(d);
        else if(propertyView.contains(mx,my))propertyScroll.wheel(d);
        else draftScroll.wheel(d);
        return;
    }
    if (page == 0) {
        if (shift)
            tableScroll.wheel(d);
        else
            mediaScroll.wheel(d);
    } else if (page == 1)
        browserScroll.wheel(d);
    else if (page == 2)
        jobsScroll.wheel(d);
    else if (page == 3)
        logScroll.wheel(d);
    else if (page == 4)
        settingsScroll.wheel(d);
    else
        aboutScroll.wheel(d);
}
void UI::character(wchar_t ch) {
    static wchar_t high = 0;
    if (!editing || focus.empty() || ch < 32)
        return;
    if (IS_HIGH_SURROGATE(ch)) {
        high = ch;
        return;
    }
    std::wstring value;
    if (high && IS_LOW_SURROGATE(ch)) {
        value += high;
        value += ch;
        high = 0;
    } else {
        high = 0;
        value += ch;
    }
    auto w = wide(*editing);
    auto &a = anchors[focus];
    auto &p = carets[focus];
    size_t left = std::min(a, p), right = std::max(a, p);
    if (w.size() + value.size() - (right - left) > 32768)
        return;
    w.replace(left, right - left, value);
    p = a = left + value.size();
    *editing = utf8(w);
}
void UI::key(int k, bool ctrl, bool shift) {
    if (k == VK_ESCAPE) {
        menuOpen = false;
        if (!app.modalTitle.empty()) {
            app.modalTitle.clear();
            app.modalConfirm = nullptr;
        } else
            cancelInput();
        return;
    }
    if (!app.modalTitle.empty()) {
        if (k == VK_RETURN) {
            auto f = app.modalConfirm;
            app.modalTitle.clear();
            app.modalConfirm = nullptr;
            if (f)
                f();
        }
        return;
    }
    if (!editing || focus.empty()) {
        if(focus=="log:selection" && ctrl && k=='A'){logAnchor=0;logCaret=logSelectionBuffer.size();return;}
        if(focus=="log:selection" && ctrl && k=='C'){auto a=std::min(logAnchor,logCaret),b=std::max(logAnchor,logCaret);if(a<b)copyText(utf8(logSelectionBuffer.substr(a,b-a)));return;}
        if (k == VK_RETURN && page == 0)
            startMediaAnalysis(false);
        return;
    }
    if(focus=="terminal:command" && k==VK_RETURN){app.action("runTerminal");return;}
    auto w = wide(*editing);
    auto &p = carets[focus];
    auto &a = anchors[focus];
    p = std::min(p, w.size());
    a = std::min(a, w.size());
    size_t left = std::min(p, a), right = std::max(p, a);
    if (ctrl && k == 'A') {
        a = 0;
        p = w.size();
        return;
    }
    if (ctrl && (k == 'C' || k == 'X')) {
        if (left == right)
            return;
        if (left != right)
            copyText(utf8(w.substr(left, right - left)));
        if (k == 'C')
            return;
        k = VK_DELETE;
    }
    if (ctrl && k == 'V') {
        if (OpenClipboard(app.hwnd)) {
            auto h = GetClipboardData(CF_UNICODETEXT);
            if (h) {
                auto *s = (const wchar_t *)GlobalLock(h);
                if (s) {
                    std::wstring paste = s;
                    GlobalUnlock(h);
                    paste.erase(std::remove_if(paste.begin(), paste.end(), [](wchar_t c) { return c < 32; }),
                                paste.end());
                    if (w.size() + paste.size() - (right - left) <= 32768) {
                        w.replace(left, right - left, paste);
                        p = a = left + paste.size();
                        *editing = utf8(w);
                    }
                }
            }
            CloseClipboard();
        }
        return;
    }
    if (k == VK_BACK || k == VK_DELETE) {
        if (left == right) {
            if (k == VK_BACK && left > 0) {
                left--;
                if (left > 0 && IS_LOW_SURROGATE(w[left]))
                    left--;
            }
            if (k == VK_DELETE && right < w.size()) {
                right++;
                if (right < w.size() && IS_LOW_SURROGATE(w[right]))
                    right++;
            }
        }
        w.erase(left, right - left);
        p = a = left;
        *editing = utf8(w);
        return;
    }
    if (k == VK_LEFT && p > 0) {
        p--;
        if (p > 0 && IS_LOW_SURROGATE(w[p]))
            p--;
    }
    if (k == VK_RIGHT && p < w.size()) {
        p++;
        if (p < w.size() && IS_LOW_SURROGATE(w[p]))
            p++;
    }
    if (k == VK_HOME)
        p = 0;
    if (k == VK_END)
        p = w.size();
    if (!shift && (k == VK_LEFT || k == VK_RIGHT || k == VK_HOME || k == VK_END))
        a = p;
}
bool UI::draggingTitle(float x, float y) const {
    return app.modalTitle.empty() && !menuOpen && y / scale < 74 && x / scale < width - 250;
}
void UI::setDialog(const std::string &t, const std::string &b) {
    app.modalTitle = t;
    app.modalBody = b;
    app.modalConfirm = nullptr;
}
} // namespace ss
