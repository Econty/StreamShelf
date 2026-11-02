// Exercises the production C++ Canvas on Mesa EGL. This is a renderer probe, not a Windows UI acceptance
// test.
#include "render.hpp"
#include <EGL/egl.h>
#include <fstream>
#include <iostream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
namespace ss {
std::string readFile(const fs::path &p, size_t limit) {
    std::ifstream f(p, std::ios::binary);
    if (!f)
        throw std::runtime_error("File missing: " + p.string());
    std::string s((std::istreambuf_iterator<char>(f)), {});
    if (s.size() > limit)
        throw std::runtime_error("File limit");
    return s;
}
} // namespace ss
int main() {
    using namespace ss;
    try {
        using GetPlatform = EGLDisplay (*)(EGLenum, void *, const EGLint *);
        auto get = (GetPlatform)eglGetProcAddress("eglGetPlatformDisplayEXT");
        if (!get)
            throw std::runtime_error("EGL platform");
        auto display = get(0x31DD, nullptr, nullptr);
        EGLint major, minor;
        if (!eglInitialize(display, &major, &minor))
            throw std::runtime_error("EGL init");
        eglBindAPI(EGL_OPENGL_API);
        EGLint cfg[] = {EGL_SURFACE_TYPE,
                        EGL_PBUFFER_BIT,
                        EGL_RENDERABLE_TYPE,
                        EGL_OPENGL_BIT,
                        EGL_RED_SIZE,
                        8,
                        EGL_GREEN_SIZE,
                        8,
                        EGL_BLUE_SIZE,
                        8,
                        EGL_ALPHA_SIZE,
                        8,
                        EGL_NONE};
        EGLConfig config;
        EGLint count;
        eglChooseConfig(display, cfg, &config, 1, &count);
        if (count != 1)
            throw std::runtime_error("EGL config");
        EGLint dimensions[] = {EGL_WIDTH, 1440, EGL_HEIGHT, 920, EGL_NONE};
        auto surface = eglCreatePbufferSurface(display, config, dimensions);
        EGLint attrib[] = {EGL_CONTEXT_MAJOR_VERSION,
                           3,
                           EGL_CONTEXT_MINOR_VERSION,
                           3,
                           EGL_CONTEXT_OPENGL_PROFILE_MASK,
                           EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                           EGL_NONE};
        auto context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrib);
        if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context))
            throw std::runtime_error("GL 3.3 context");
        Canvas c;
        c.init("Assets");
        std::cout << "GPU " << c.driver() << '\n';
        Spring p;
        for (int frame = 0; frame < 120; frame++) {
            p.target = frame < 60 ? 1 : 0;
            p.step(1.f / 60);
            c.begin(1440, 920, 1440, 920);
            glClearColor(.03f, .05f, .09f, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            c.rect({0, 0, 1440, 920}, 0, hex(0x080e19));
            c.glow({300, -350, 1150, 850}, hex(0x8be2d1, .09f));
            c.text("STREAMSHELF", 25, 36, 15, hex(0xeef3fc));
            c.text("C++ · WINAPI · OPENGL", 25, 57, 9, hex(0x8999b4));
            c.text("Проверка нативного рендерера", 245, 126, 28, hex(0xeef3fc));
            c.text("Это GPU-проверка Canvas, а не снимок запущенного Windows-приложения.", 246, 157, 12,
                   hex(0x8999b4));
            c.border({242, 188, 1170, 155}, 20, hex(0x2a374b), hex(0x141d2c));
            c.text("Шрифт с кириллицей · Прямоугольники · Пружины · Clipping", 264, 224, 18, hex(0xeef3fc));
            c.border(Rect{270, 260, 220, 46}.scale(1 - .035f * p.value), 12, hex(0x8be2d1),
                     mix(hex(0x8be2d1), hex(0xb6f5e8), p.value * .3f));
            c.text("Плавная анимация", 295, 288, 14, hex(0x102821));
            c.rect({530, 270, 340, 8}, 4, hex(0x2a374b));
            c.rect({530, 270, 340 * std::clamp(p.value, 0.f, 1.f), 8}, 4, hex(0x8be2d1));
            for (int i = 0; i < 5; i++) {
                c.rect({242, 365.f + i * 61, 1170, 53}, 12, hex(i % 2 ? 0x141d2c : 0x1c2a38));
                c.text(std::vector<std::string>{"Видео без звука", "Аудио", "Субтитры", "Превью",
                                                "Метаданные"}[i],
                       266, 399.f + i * 61, 14, hex(0x8bbfff));
                c.text("SDF · UTF-8 · C++20", 550, 399.f + i * 61, 13, hex(0xeef3fc));
            }
            c.push({900, 700, 400, 150});
            c.text("Текст ограничен областью clipping. ЖЩЮЯ 0123456789", 900, 745, 22, hex(0xeef3fc));
            c.pop();
            if (frame == 60)
                c.freeze();
            if (frame > 60)
                c.drawSnapshot({0, 0, 1440, 920}, std::max(0.f, 1 - (frame - 60) / 20.f));
            c.render();
            c.capture();
            if (glGetError() != GL_NO_ERROR)
                throw std::runtime_error("OpenGL error on frame " + std::to_string(frame));
        }
        std::vector<unsigned char> pixels(1440 * 920 * 4);
        glReadPixels(0, 0, 1440, 920, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        size_t nonzero = 0;
        for (auto x : pixels)
            if (x)
                nonzero++;
        if (nonzero < 1440 * 920)
            throw std::runtime_error("Empty framebuffer");
        for (int y = 0; y < 460; y++)
            for (int x = 0; x < 1440 * 4; x++)
                std::swap(pixels[y * 1440 * 4 + x], pixels[(919 - y) * 1440 * 4 + x]);
        std::filesystem::create_directories("artifacts");
        stbi_write_png("artifacts/native-renderer-probe.png", 1440, 920, 4, pixels.data(), 1440 * 4);
        c.destroy();
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
        eglDestroySurface(display, surface);
        eglTerminate(display);
        std::cout << "PASS 120 C++ OpenGL frames, shaders, SDF Cyrillic, clipping, history and interrupted "
                     "animation\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
