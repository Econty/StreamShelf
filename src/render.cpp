#include "render.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#include <cstddef>
#ifdef STREAMSHELF_GPU_TEST
#include <dlfcn.h>
using PROC = void (*)();
#endif
namespace ss {
#define GL_PROC_LIST(X)                                                                                      \
    X(PFNGLCREATESHADERPROC, CreateShader)                                                                   \
    X(PFNGLSHADERSOURCEPROC, ShaderSource) X(PFNGLCOMPILESHADERPROC, CompileShader) X(                       \
        PFNGLGETSHADERIVPROC, GetShaderiv) X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog)                    \
        X(PFNGLCREATEPROGRAMPROC, CreateProgram) X(PFNGLATTACHSHADERPROC, AttachShader) X(                   \
            PFNGLLINKPROGRAMPROC, LinkProgram) X(PFNGLGETPROGRAMIVPROC, GetProgramiv)                        \
            X(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog) X(PFNGLDELETESHADERPROC, DeleteShader) X(       \
                PFNGLGENVERTEXARRAYSPROC, GenVertexArrays) X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray)      \
                X(PFNGLGENBUFFERSPROC, GenBuffers) X(PFNGLBINDBUFFERPROC, BindBuffer) X(                     \
                    PFNGLENABLEVERTEXATTRIBARRAYPROC,                                                        \
                    EnableVertexAttribArray) X(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer)            \
                    X(PFNGLBUFFERDATAPROC, BufferData) X(PFNGLUSEPROGRAMPROC, UseProgram)                    \
                        X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) X(PFNGLUNIFORM1IPROC, Uniform1i)  \
                            X(PFNGLUNIFORM2FPROC, Uniform2f) X(PFNGLACTIVETEXTUREPROC, ActiveTexture)        \
                                X(PFNGLDELETEBUFFERSPROC, DeleteBuffers)                                     \
                                    X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays)                       \
                                        X(PFNGLDELETEPROGRAMPROC, DeleteProgram)                             \
                                            X(PFNGLGENFRAMEBUFFERSPROC, GenFramebuffers)                     \
                                                X(PFNGLBINDFRAMEBUFFERPROC, BindFramebuffer)                 \
                                                    X(PFNGLFRAMEBUFFERTEXTURE2DPROC, FramebufferTexture2D)   \
                                                        X(PFNGLDELETEFRAMEBUFFERSPROC, DeleteFramebuffers)   \
                                                            X(PFNGLCHECKFRAMEBUFFERSTATUSPROC,               \
                                                              CheckFramebufferStatus)
#define DECL(T, N) static T p##N;
GL_PROC_LIST(DECL)
#undef DECL
static PROC getProc(const char *n) {
#ifdef STREAMSHELF_GPU_TEST
    static void *lib = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    auto p = (PROC)dlsym(lib, n);
#else
    auto p = wglGetProcAddress(n);
    if (!p || p == (PROC)1 || p == (PROC)2 || p == (PROC)3 || p == (PROC)-1)
        p = GetProcAddress(GetModuleHandleW(L"opengl32.dll"), n);
#endif
    if (!p)
        throw std::runtime_error(std::string("OpenGL 3.3: ") + n);
    return p;
}
void Canvas::init(const fs::path &assets) {
#define LOAD(T, N) p##N = reinterpret_cast<T>(getProc("gl" #N));
    GL_PROC_LIST(LOAD)
#undef LOAD
    auto shader = [&](GLenum kind, const fs::path &name) {
        auto s = readFile(assets / L"shaders" / name);
        auto c = s.c_str();
        unsigned id = pCreateShader(kind);
        pShaderSource(id, 1, &c, nullptr);
        pCompileShader(id);
        int ok = 0;
        pGetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[4096];
            pGetShaderInfoLog(id, sizeof(log), nullptr, log);
            throw std::runtime_error(log);
        }
        return id;
    };
    auto vs = shader(GL_VERTEX_SHADER, L"desktop.vert"), fs = shader(GL_FRAGMENT_SHADER, L"desktop.frag");
    program = pCreateProgram();
    pAttachShader(program, vs);
    pAttachShader(program, fs);
    pLinkProgram(program);
    int linked;
    pGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[4096];
        pGetProgramInfoLog(program, sizeof(log), nullptr, log);
        throw std::runtime_error(log);
    }
    pDeleteShader(vs);
    pDeleteShader(fs);
    pGenVertexArrays(1, &vao);
    pGenBuffers(1, &vbo);
    pBindVertexArray(vao);
    pBindBuffer(GL_ARRAY_BUFFER, vbo);
    for (int i = 0; i < 4; i++) {
        pEnableVertexAttribArray(i);
        pVertexAttribPointer(i, i < 2 ? 2 : 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void *)(uintptr_t)(i == 0   ? 0
                                                 : i == 1 ? 8
                                                 : i == 2 ? 16
                                                          : 32));
    }
    auto png = readFile(assets / L"fonts" / L"ui-sdf.png");
    int w, h, ch;
    auto pixels = stbi_load_from_memory((stbi_uc *)png.data(), int(png.size()), &w, &h, &ch, 4);
    if (!pixels)
        throw std::runtime_error("Не удалось загрузить SDF-атлас");
    glGenTextures(1, &font);
    glBindTexture(GL_TEXTURE_2D, font);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    std::istringstream in(readFile(assets / L"fonts" / L"ui-font.txt"));
    std::string header;
    std::getline(in, header);
    uint32_t code;
    Glyph g;
    while (in >> code >> g.advance >> g.left >> g.top >> g.x >> g.y >> g.w >> g.h)
        glyphs[code] = g;
    std::istringstream pairs(readFile(assets / L"fonts" / L"ui-kern.txt"));
    uint32_t a, b;
    float amount;
    while (pairs >> a >> b >> amount)
        kern[(uint64_t(a) << 32) | b] = amount;
    pGenFramebuffers(1, &fbo);
    glGenTextures(1, &history);
    glGenTextures(1, &snapshot);
}
void Canvas::begin(float w, float h, int pw, int ph, bool deferSnapshots) {
    width = w;
    height = h;
    vertices.clear();
    batches.clear();
    clips = {{0, 0, w, h}};
    batchStart = 0;
    opacity = 1;
    offsetX = offsetY = 0;
    pixelW = pw;
    pixelH = ph;
    if (snapshotW != pw || snapshotH != ph) {
        frozen = false;
        if (deferSnapshots) return;
        snapshotW = pw; snapshotH = ph;
        glBindTexture(GL_TEXTURE_2D, history);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pw, ph, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        frozen = false;
    }
}
void Canvas::seal() {
    int n = int(vertices.size()) - batchStart;
    if (n)
        batches.push_back({clips.back(), batchStart, n,batchImage});
    batchStart = int(vertices.size());
}
void Canvas::push(Rect r) {
    seal();
    clips.push_back(clips.back().intersect(r.move(offsetX, offsetY)));
}
void Canvas::pop() {
    seal();
    if (clips.size() > 1)
        clips.pop_back();
}
void Canvas::vertex(float x, float y, float u, float v, Ink c, float p, float q, float r, float kind) {
    vertices.push_back({x + offsetX, y + offsetY, u, v, c.r, c.g, c.b, c.a * opacity, p, q, r, kind});
}
void Canvas::quad(Rect r, float u0, float v0, float u1, float v1, Ink c, float p, float q, float z,
                  float kind) {
    vertex(r.x, r.y, u0, v0, c, p, q, z, kind);
    vertex(r.x + r.w, r.y, u1, v0, c, p, q, z, kind);
    vertex(r.x + r.w, r.y + r.h, u1, v1, c, p, q, z, kind);
    vertex(r.x, r.y, u0, v0, c, p, q, z, kind);
    vertex(r.x + r.w, r.y + r.h, u1, v1, c, p, q, z, kind);
    vertex(r.x, r.y + r.h, u0, v1, c, p, q, z, kind);
}
void Canvas::rect(Rect r, float radius, Ink c) {
    if (r.w <= 0 || r.h <= 0 || c.a * opacity < .001f)
        return;
    quad(r, 0, 0, 1, 1, c, r.w, r.h, std::min(radius, std::min(r.w, r.h) / 2), 1);
}
void Canvas::border(Rect r, float z, Ink e, Ink f, float t) {
    rect(r, z, e);
    rect(r.inset(t), std::max(0.f, z - t), f);
}
void Canvas::circle(float x, float y, float r, Ink c) {
    quad({x - r, y - r, r * 2, r * 2}, 0, 0, 1, 1, c, r * 2, r * 2, r, 2);
}
void Canvas::glow(Rect r, Ink c) {
    quad(r, 0, 0, 1, 1, c, 0, 0, 0, 4);
}
void Canvas::line(float x, float y, float a, float b, float t, Ink c) {
    float dx = a - x, dy = b - y, len = std::hypot(dx, dy);
    if (len < .01f)
        return;
    float nx = -dy / len * t / 2, ny = dx / len * t / 2;
    vertex(x + nx, y + ny, 0, 0, c, 0, 0, 0, 0);
    vertex(a + nx, b + ny, 1, 0, c, 0, 0, 0, 0);
    vertex(a - nx, b - ny, 1, 1, c, 0, 0, 0, 0);
    vertex(x + nx, y + ny, 0, 0, c, 0, 0, 0, 0);
    vertex(a - nx, b - ny, 1, 1, c, 0, 0, 0, 0);
    vertex(x - nx, y - ny, 0, 1, c, 0, 0, 0, 0);
}
float Canvas::measure(const std::string &s, float size) {
    float x = 0, max = 0;
    uint32_t prev = 0;
    for (auto r : codepoints(s)) {
        if (r == '\n') {
            max = std::max(max, x);
            x = 0;
            prev = 0;
            continue;
        }
        auto i = glyphs.find(r);
        if (i == glyphs.end())
            i = glyphs.find('?');
        if (i == glyphs.end())
            continue;
        auto k = kern.find((uint64_t(prev) << 32) | r);
        if (k != kern.end())
            x += k->second * size / 48;
        x += i->second.advance * size / 48;
        prev = r;
    }
    return std::max(max, x);
}
void Canvas::text(const std::string &s, float x, float y, float size, Ink c, float maxWidth) {
    float start = x, scale = size / 48;
    uint32_t prev = 0;
    for (auto r : codepoints(s)) {
        if (r == '\n') {
            y += size * 1.6f;
            x = start;
            prev = 0;
            continue;
        }
        auto i = glyphs.find(r);
        if (i == glyphs.end())
            i = glyphs.find('?');
        if (i == glyphs.end())
            continue;
        auto g = i->second;
        auto k = kern.find((uint64_t(prev) << 32) | r);
        if (k != kern.end())
            x += k->second * scale;
        if (x + g.advance * scale > start + maxWidth) {
            if (r != 0x2026 && maxWidth > size) {
                text("…", std::min(x, start + maxWidth - size), y, size, c, size * 2);
            }
            break;
        }
        if (r != ' ')
            quad({x + g.left * scale, y + g.top * scale, g.w * scale, g.h * scale}, g.x / 1024.f,
                 g.y / 1024.f, (g.x + g.w) / 1024.f, (g.y + g.h) / 1024.f, c, 0, 0, 0, 3);
        x += g.advance * scale;
        prev = r;
    }
}
void Canvas::drawSnapshot(Rect r, float alpha, bool blur) {
    if (frozen)
        quad(r, 0, 1, 1, 0, hex(0xffffff, alpha), 0, 0, 0, blur ? 6 : 5);
}
unsigned Canvas::createImage(const PreviewImage &image) {
    if(image.rgba.empty())return 0;
    unsigned texture=0;glGenTextures(1,&texture);
    pActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    return texture;
}
void Canvas::deleteImage(unsigned texture){if(texture)glDeleteTextures(1,&texture);}
void Canvas::image(unsigned texture,Rect r,int w,int h,float alpha){
    if(!texture||!w||!h)return;
    float fit=std::min(r.w/w,r.h/h);
    Rect fitted{r.x+(r.w-w*fit)/2,r.y+(r.h-h*fit)/2,w*fit,h*fit};
    seal();batchImage=texture;
    quad(fitted,0,0,1,1,hex(0xffffff,alpha),0,0,0,7);
    seal();batchImage=0;
}
void Canvas::freeze() {
    if (!pixelW || !pixelH || snapshotW != pixelW || snapshotH != pixelH)
        return;
    pBindFramebuffer(GL_FRAMEBUFFER, fbo);
    pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, history, 0);
    if (pCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        glBindTexture(GL_TEXTURE_2D, snapshot);
        if (frozenW != pixelW || frozenH != pixelH) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pixelW, pixelH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            frozenW = pixelW; frozenH = pixelH;
        }
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, pixelW, pixelH);
        frozen = true;
    }
    pBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void Canvas::render() {
    seal();
    glViewport(0, 0, pixelW, pixelH);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    pUseProgram(program);
    pBindVertexArray(vao);
    pBindBuffer(GL_ARRAY_BUFFER, vbo);
    pBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STREAM_DRAW);
    pUniform2f(pGetUniformLocation(program, "uScreen"), width, height);
    pActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font);
    pUniform1i(pGetUniformLocation(program, "uFont"), 0);
    pActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, snapshot);
    pUniform1i(pGetUniformLocation(program, "uSnapshot"), 1);
    pUniform1i(pGetUniformLocation(program, "uImage"), 2);
    glEnable(GL_SCISSOR_TEST);
    for (auto &b : batches) {
        auto r = b.clip;
        pActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,b.image);
        if (r.w <= 0 || r.h <= 0)
            continue;
        float sx = pixelW / width, sy = pixelH / height;
        glScissor(std::max(0, int(std::floor(r.x * sx))),
                  std::max(0, int(std::floor((height - r.y - r.h) * sy))), int(std::ceil(r.w * sx)),
                  int(std::ceil(r.h * sy)));
        glDrawArrays(GL_TRIANGLES, b.start, b.count);
    }
    glDisable(GL_SCISSOR_TEST);
}
void Canvas::capture() {
    if (snapshotW != pixelW || snapshotH != pixelH) return;
    pActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, history);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, pixelW, pixelH);
}
std::string Canvas::driver() {
    return std::string((const char *)glGetString(GL_VENDOR)) + " / " +
           (const char *)glGetString(GL_RENDERER) + " / " + (const char *)glGetString(GL_VERSION);
}
void Canvas::destroy() {
    if (program) {
        glDeleteTextures(1, &font);
        glDeleteTextures(1, &history);
        glDeleteTextures(1, &snapshot);
        pDeleteFramebuffers(1, &fbo);
        pDeleteBuffers(1, &vbo);
        pDeleteVertexArrays(1, &vao);
        pDeleteProgram(program);
        program = 0;
    }
}
} // namespace ss
